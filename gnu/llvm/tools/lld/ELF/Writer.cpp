//===- Writer.cpp ---------------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Writer.h"
#include "Config.h"
#include "LinkerScript.h"
#include "OutputSections.h"
#include "Relocations.h"
#include "Strings.h"
#include "SymbolTable.h"
#include "Target.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::object;

using namespace lld;
using namespace lld::elf;

namespace {
// The writer writes a SymbolTable result to a file.
template <class ELFT> class Writer {
public:
  typedef typename ELFT::uint uintX_t;
  typedef typename ELFT::Shdr Elf_Shdr;
  typedef typename ELFT::Ehdr Elf_Ehdr;
  typedef typename ELFT::Phdr Elf_Phdr;
  typedef typename ELFT::Sym Elf_Sym;
  typedef typename ELFT::SymRange Elf_Sym_Range;
  typedef typename ELFT::Rela Elf_Rela;
  Writer(SymbolTable<ELFT> &S) : Symtab(S) {}
  void run();

private:
  // This describes a program header entry.
  // Each contains type, access flags and range of output sections that will be
  // placed in it.
  struct Phdr {
    Phdr(unsigned Type, unsigned Flags) {
      H.p_type = Type;
      H.p_flags = Flags;
    }
    Elf_Phdr H = {};
    OutputSectionBase<ELFT> *First = nullptr;
    OutputSectionBase<ELFT> *Last = nullptr;
  };

  void copyLocalSymbols();
  void addReservedSymbols();
  void createSections();
  void addPredefinedSections();
  bool needsGot();

  void createPhdrs();
  void assignAddresses();
  void assignFileOffsets();
  void setPhdrs();
  void fixHeaders();
  void fixSectionAlignments();
  void fixAbsoluteSymbols();
  void openFile();
  void writeHeader();
  void writeSections();
  void writeBuildId();
  bool needsInterpSection() const {
    return !Symtab.getSharedFiles().empty() && !Config->DynamicLinker.empty();
  }
  bool isOutputDynamic() const {
    return !Symtab.getSharedFiles().empty() || Config->Pic;
  }

  void addCommonSymbols(std::vector<DefinedCommon *> &Syms);

  std::unique_ptr<FileOutputBuffer> Buffer;

  BumpPtrAllocator Alloc;
  std::vector<OutputSectionBase<ELFT> *> OutputSections;
  std::vector<std::unique_ptr<OutputSectionBase<ELFT>>> OwningSections;

  void addRelIpltSymbols();
  void addStartEndSymbols();
  void addStartStopSymbols(OutputSectionBase<ELFT> *Sec);

  SymbolTable<ELFT> &Symtab;
  std::vector<Phdr> Phdrs;

  uintX_t FileSize;
  uintX_t SectionHeaderOff;
};
} // anonymous namespace

template <class ELFT>
StringRef elf::getOutputSectionName(InputSectionBase<ELFT> *S) {
  StringRef Dest = Script<ELFT>::X->getOutputSection(S);
  if (!Dest.empty())
    return Dest;

  StringRef Name = S->getSectionName();
  for (StringRef V : {".text.", ".rodata.", ".data.rel.ro.", ".data.", ".bss.",
                      ".init_array.", ".fini_array.", ".ctors.", ".dtors.",
                      ".tbss.", ".gcc_except_table.", ".tdata."})
    if (Name.startswith(V))
      return V.drop_back();
  return Name;
}

template <class ELFT>
void elf::reportDiscarded(InputSectionBase<ELFT> *IS,
                          const std::unique_ptr<elf::ObjectFile<ELFT>> &File) {
  if (!Config->PrintGcSections || !IS || IS->Live)
    return;
  errs() << "removing unused section from '" << IS->getSectionName()
         << "' in file '" << File->getName() << "'\n";
}

template <class ELFT> void elf::writeResult(SymbolTable<ELFT> *Symtab) {
  typedef typename ELFT::uint uintX_t;
  typedef typename ELFT::Ehdr Elf_Ehdr;

  // Create singleton output sections.
  OutputSection<ELFT> Bss(".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE);
  DynamicSection<ELFT> Dynamic;
  EhOutputSection<ELFT> EhFrame;
  GotSection<ELFT> Got;
  InterpSection<ELFT> Interp;
  PltSection<ELFT> Plt;
  RelocationSection<ELFT> RelaDyn(Config->Rela ? ".rela.dyn" : ".rel.dyn",
                                  Config->ZCombreloc);
  StringTableSection<ELFT> DynStrTab(".dynstr", true);
  StringTableSection<ELFT> ShStrTab(".shstrtab", false);
  SymbolTableSection<ELFT> DynSymTab(DynStrTab);
  VersionTableSection<ELFT> VerSym;
  VersionNeedSection<ELFT> VerNeed;

  OutputSectionBase<ELFT> ElfHeader("", 0, SHF_ALLOC);
  ElfHeader.setSize(sizeof(Elf_Ehdr));
  OutputSectionBase<ELFT> ProgramHeaders("", 0, SHF_ALLOC);
  ProgramHeaders.updateAlignment(sizeof(uintX_t));

  // Instantiate optional output sections if they are needed.
  std::unique_ptr<BuildIdSection<ELFT>> BuildId;
  std::unique_ptr<EhFrameHeader<ELFT>> EhFrameHdr;
  std::unique_ptr<GnuHashTableSection<ELFT>> GnuHashTab;
  std::unique_ptr<GotPltSection<ELFT>> GotPlt;
  std::unique_ptr<HashTableSection<ELFT>> HashTab;
  std::unique_ptr<RelocationSection<ELFT>> RelaPlt;
  std::unique_ptr<StringTableSection<ELFT>> StrTab;
  std::unique_ptr<SymbolTableSection<ELFT>> SymTabSec;
  std::unique_ptr<OutputSection<ELFT>> MipsRldMap;
  std::unique_ptr<VersionDefinitionSection<ELFT>> VerDef;

  if (Config->BuildId == BuildIdKind::Fnv1)
    BuildId.reset(new BuildIdFnv1<ELFT>);
  else if (Config->BuildId == BuildIdKind::Md5)
    BuildId.reset(new BuildIdMd5<ELFT>);
  else if (Config->BuildId == BuildIdKind::Sha1)
    BuildId.reset(new BuildIdSha1<ELFT>);
  else if (Config->BuildId == BuildIdKind::Hexstring)
    BuildId.reset(new BuildIdHexstring<ELFT>);

  if (Config->EhFrameHdr)
    EhFrameHdr.reset(new EhFrameHeader<ELFT>);

  if (Config->GnuHash)
    GnuHashTab.reset(new GnuHashTableSection<ELFT>);
  if (Config->SysvHash)
    HashTab.reset(new HashTableSection<ELFT>);
  StringRef S = Config->Rela ? ".rela.plt" : ".rel.plt";
  GotPlt.reset(new GotPltSection<ELFT>);
  RelaPlt.reset(new RelocationSection<ELFT>(S, false /*Sort*/));
  if (!Config->StripAll) {
    StrTab.reset(new StringTableSection<ELFT>(".strtab", false));
    SymTabSec.reset(new SymbolTableSection<ELFT>(*StrTab));
  }
  if (Config->EMachine == EM_MIPS && !Config->Shared) {
    // This is a MIPS specific section to hold a space within the data segment
    // of executable file which is pointed to by the DT_MIPS_RLD_MAP entry.
    // See "Dynamic section" in Chapter 5 in the following document:
    // ftp://www.linux-mips.org/pub/linux/mips/doc/ABI/mipsabi.pdf
    MipsRldMap.reset(new OutputSection<ELFT>(".rld_map", SHT_PROGBITS,
                                             SHF_ALLOC | SHF_WRITE));
    MipsRldMap->setSize(sizeof(uintX_t));
    MipsRldMap->updateAlignment(sizeof(uintX_t));
  }
  if (!Config->VersionDefinitions.empty())
    VerDef.reset(new VersionDefinitionSection<ELFT>());

  Out<ELFT>::Bss = &Bss;
  Out<ELFT>::BuildId = BuildId.get();
  Out<ELFT>::DynStrTab = &DynStrTab;
  Out<ELFT>::DynSymTab = &DynSymTab;
  Out<ELFT>::Dynamic = &Dynamic;
  Out<ELFT>::EhFrame = &EhFrame;
  Out<ELFT>::EhFrameHdr = EhFrameHdr.get();
  Out<ELFT>::GnuHashTab = GnuHashTab.get();
  Out<ELFT>::Got = &Got;
  Out<ELFT>::GotPlt = GotPlt.get();
  Out<ELFT>::HashTab = HashTab.get();
  Out<ELFT>::Interp = &Interp;
  Out<ELFT>::Plt = &Plt;
  Out<ELFT>::RelaDyn = &RelaDyn;
  Out<ELFT>::RelaPlt = RelaPlt.get();
  Out<ELFT>::ShStrTab = &ShStrTab;
  Out<ELFT>::StrTab = StrTab.get();
  Out<ELFT>::SymTab = SymTabSec.get();
  Out<ELFT>::VerDef = VerDef.get();
  Out<ELFT>::VerSym = &VerSym;
  Out<ELFT>::VerNeed = &VerNeed;
  Out<ELFT>::MipsRldMap = MipsRldMap.get();
  Out<ELFT>::Opd = nullptr;
  Out<ELFT>::OpdBuf = nullptr;
  Out<ELFT>::TlsPhdr = nullptr;
  Out<ELFT>::ElfHeader = &ElfHeader;
  Out<ELFT>::ProgramHeaders = &ProgramHeaders;

  Writer<ELFT>(*Symtab).run();
}

// The main function of the writer.
template <class ELFT> void Writer<ELFT>::run() {
  if (!Config->DiscardAll)
    copyLocalSymbols();
  addReservedSymbols();
  createSections();
  if (HasError)
    return;

  if (Config->Relocatable) {
    assignFileOffsets();
  } else {
    createPhdrs();
    fixHeaders();
    if (ScriptConfig->DoLayout) {
      Script<ELFT>::X->assignAddresses(OutputSections);
    } else {
      fixSectionAlignments();
      assignAddresses();
    }
    assignFileOffsets();
    setPhdrs();
    fixAbsoluteSymbols();
  }

  openFile();
  if (HasError)
    return;
  writeHeader();
  writeSections();
  writeBuildId();
  if (HasError)
    return;
  if (auto EC = Buffer->commit())
    error(EC, "failed to write to the output file");
}

template <class ELFT>
static void reportUndefined(SymbolTable<ELFT> &Symtab, SymbolBody *Sym) {
  if (Config->UnresolvedSymbols == UnresolvedPolicy::Ignore)
    return;

  if (Config->Shared && Sym->symbol()->Visibility == STV_DEFAULT &&
      Config->UnresolvedSymbols != UnresolvedPolicy::NoUndef)
    return;

  std::string Msg = "undefined symbol: " + Sym->getName().str();
  if (Sym->File)
    Msg += " in " + getFilename(Sym->File);
  if (Config->UnresolvedSymbols == UnresolvedPolicy::Warn)
    warning(Msg);
  else
    error(Msg);
}

template <class ELFT>
static bool shouldKeepInSymtab(InputSectionBase<ELFT> *Sec, StringRef SymName,
                               const SymbolBody &B) {
  if (B.isFile())
    return false;

  // We keep sections in symtab for relocatable output.
  if (B.isSection())
    return Config->Relocatable;

  // If sym references a section in a discarded group, don't keep it.
  if (Sec == &InputSection<ELFT>::Discarded)
    return false;

  if (Config->DiscardNone)
    return true;

  // In ELF assembly .L symbols are normally discarded by the assembler.
  // If the assembler fails to do so, the linker discards them if
  // * --discard-locals is used.
  // * The symbol is in a SHF_MERGE section, which is normally the reason for
  //   the assembler keeping the .L symbol.
  if (!SymName.startswith(".L") && !SymName.empty())
    return true;

  if (Config->DiscardLocals)
    return false;

  return !(Sec->getSectionHdr()->sh_flags & SHF_MERGE);
}

template <class ELFT> static bool includeInSymtab(const SymbolBody &B) {
  if (!B.isLocal() && !B.symbol()->IsUsedInRegularObj)
    return false;

  if (auto *D = dyn_cast<DefinedRegular<ELFT>>(&B)) {
    // Always include absolute symbols.
    if (!D->Section)
      return true;
    // Exclude symbols pointing to garbage-collected sections.
    if (!D->Section->Live)
      return false;
    if (auto *S = dyn_cast<MergeInputSection<ELFT>>(D->Section))
      if (!S->getSectionPiece(D->Value)->Live)
        return false;
  }
  return true;
}

// Local symbols are not in the linker's symbol table. This function scans
// each object file's symbol table to copy local symbols to the output.
template <class ELFT> void Writer<ELFT>::copyLocalSymbols() {
  if (!Out<ELFT>::SymTab)
    return;
  for (const std::unique_ptr<elf::ObjectFile<ELFT>> &F :
       Symtab.getObjectFiles()) {
    const char *StrTab = F->getStringTable().data();
    for (SymbolBody *B : F->getLocalSymbols()) {
      auto *DR = dyn_cast<DefinedRegular<ELFT>>(B);
      // No reason to keep local undefined symbol in symtab.
      if (!DR)
        continue;
      if (!includeInSymtab<ELFT>(*B))
        continue;
      StringRef SymName(StrTab + B->getNameOffset());
      InputSectionBase<ELFT> *Sec = DR->Section;
      if (!shouldKeepInSymtab<ELFT>(Sec, SymName, *B))
        continue;
      ++Out<ELFT>::SymTab->NumLocals;
      if (Config->Relocatable)
        B->DynsymIndex = Out<ELFT>::SymTab->NumLocals;
      F->KeptLocalSyms.push_back(
          std::make_pair(DR, Out<ELFT>::SymTab->StrTabSec.addString(SymName)));
    }
  }
}

// PPC64 has a number of special SHT_PROGBITS+SHF_ALLOC+SHF_WRITE sections that
// we would like to make sure appear is a specific order to maximize their
// coverage by a single signed 16-bit offset from the TOC base pointer.
// Conversely, the special .tocbss section should be first among all SHT_NOBITS
// sections. This will put it next to the loaded special PPC64 sections (and,
// thus, within reach of the TOC base pointer).
static int getPPC64SectionRank(StringRef SectionName) {
  return StringSwitch<int>(SectionName)
      .Case(".tocbss", 0)
      .Case(".branch_lt", 2)
      .Case(".toc", 3)
      .Case(".toc1", 4)
      .Case(".opd", 5)
      .Default(1);
}

template <class ELFT> static bool isRelroSection(OutputSectionBase<ELFT> *Sec) {
  if (!Config->ZRelro)
    return false;
  typename ELFT::uint Flags = Sec->getFlags();
  if (!(Flags & SHF_ALLOC) || !(Flags & SHF_WRITE))
    return false;
  if (Flags & SHF_TLS)
    return true;
  uint32_t Type = Sec->getType();
  if (Type == SHT_INIT_ARRAY || Type == SHT_FINI_ARRAY ||
      Type == SHT_PREINIT_ARRAY)
    return true;
  if (Sec == Out<ELFT>::GotPlt)
    return Config->ZNow;
  if (Sec == Out<ELFT>::Dynamic || Sec == Out<ELFT>::Got)
    return true;
  StringRef S = Sec->getName();
  return S == ".data.rel.ro" || S == ".ctors" || S == ".dtors" || S == ".jcr" ||
         S == ".eh_frame";
}

// Output section ordering is determined by this function.
template <class ELFT>
static bool compareSections(OutputSectionBase<ELFT> *A,
                            OutputSectionBase<ELFT> *B) {
  typedef typename ELFT::uint uintX_t;

  int Comp = Script<ELFT>::X->compareSections(A->getName(), B->getName());
  if (Comp != 0)
    return Comp < 0;

  uintX_t AFlags = A->getFlags();
  uintX_t BFlags = B->getFlags();

  // Allocatable sections go first to reduce the total PT_LOAD size and
  // so debug info doesn't change addresses in actual code.
  bool AIsAlloc = AFlags & SHF_ALLOC;
  bool BIsAlloc = BFlags & SHF_ALLOC;
  if (AIsAlloc != BIsAlloc)
    return AIsAlloc;

  // We don't have any special requirements for the relative order of
  // two non allocatable sections.
  if (!AIsAlloc)
    return false;

  // We want the read only sections first so that they go in the PT_LOAD
  // covering the program headers at the start of the file.
  bool AIsWritable = AFlags & SHF_WRITE;
  bool BIsWritable = BFlags & SHF_WRITE;
  if (AIsWritable != BIsWritable)
    return BIsWritable;

  // For a corresponding reason, put non exec sections first (the program
  // header PT_LOAD is not executable).
  bool AIsExec = AFlags & SHF_EXECINSTR;
  bool BIsExec = BFlags & SHF_EXECINSTR;
  if (AIsExec != BIsExec)
    return BIsExec;

  // If we got here we know that both A and B are in the same PT_LOAD.

  // The TLS initialization block needs to be a single contiguous block in a R/W
  // PT_LOAD, so stick TLS sections directly before R/W sections. The TLS NOBITS
  // sections are placed here as they don't take up virtual address space in the
  // PT_LOAD.
  bool AIsTls = AFlags & SHF_TLS;
  bool BIsTls = BFlags & SHF_TLS;
  if (AIsTls != BIsTls)
    return AIsTls;

  // The next requirement we have is to put nobits sections last. The
  // reason is that the only thing the dynamic linker will see about
  // them is a p_memsz that is larger than p_filesz. Seeing that it
  // zeros the end of the PT_LOAD, so that has to correspond to the
  // nobits sections.
  bool AIsNoBits = A->getType() == SHT_NOBITS;
  bool BIsNoBits = B->getType() == SHT_NOBITS;
  if (AIsNoBits != BIsNoBits)
    return BIsNoBits;

  // We place RelRo section before plain r/w ones.
  bool AIsRelRo = isRelroSection(A);
  bool BIsRelRo = isRelroSection(B);
  if (AIsRelRo != BIsRelRo)
    return AIsRelRo;

  // Some architectures have additional ordering restrictions for sections
  // within the same PT_LOAD.
  if (Config->EMachine == EM_PPC64)
    return getPPC64SectionRank(A->getName()) <
           getPPC64SectionRank(B->getName());

  return false;
}

// Until this function is called, common symbols do not belong to any section.
// This function adds them to end of BSS section.
template <class ELFT>
void Writer<ELFT>::addCommonSymbols(std::vector<DefinedCommon *> &Syms) {
  if (Syms.empty())
    return;

  // Sort the common symbols by alignment as an heuristic to pack them better.
  std::stable_sort(Syms.begin(), Syms.end(),
                   [](const DefinedCommon *A, const DefinedCommon *B) {
                     return A->Alignment > B->Alignment;
                   });

  uintX_t Off = Out<ELFT>::Bss->getSize();
  for (DefinedCommon *C : Syms) {
    Off = alignTo(Off, C->Alignment);
    Out<ELFT>::Bss->updateAlignment(C->Alignment);
    C->OffsetInBss = Off;
    Off += C->Size;
  }

  Out<ELFT>::Bss->setSize(Off);
}

template <class ELFT>
static Symbol *addOptionalSynthetic(SymbolTable<ELFT> &Table, StringRef Name,
                                    OutputSectionBase<ELFT> *Sec,
                                    typename ELFT::uint Val) {
  SymbolBody *S = Table.find(Name);
  if (!S)
    return nullptr;
  if (!S->isUndefined() && !S->isShared())
    return S->symbol();
  return Table.addSynthetic(Name, Sec, Val);
}

// The beginning and the ending of .rel[a].plt section are marked
// with __rel[a]_iplt_{start,end} symbols if it is a statically linked
// executable. The runtime needs these symbols in order to resolve
// all IRELATIVE relocs on startup. For dynamic executables, we don't
// need these symbols, since IRELATIVE relocs are resolved through GOT
// and PLT. For details, see http://www.airs.com/blog/archives/403.
template <class ELFT> void Writer<ELFT>::addRelIpltSymbols() {
  if (isOutputDynamic() || !Out<ELFT>::RelaPlt)
    return;
  StringRef S = Config->Rela ? "__rela_iplt_start" : "__rel_iplt_start";
  addOptionalSynthetic(Symtab, S, Out<ELFT>::RelaPlt, 0);

  S = Config->Rela ? "__rela_iplt_end" : "__rel_iplt_end";
  addOptionalSynthetic(Symtab, S, Out<ELFT>::RelaPlt,
                       DefinedSynthetic<ELFT>::SectionEnd);
}

// The linker is expected to define some symbols depending on
// the linking result. This function defines such symbols.
template <class ELFT> void Writer<ELFT>::addReservedSymbols() {
  if (Config->EMachine == EM_MIPS) {
    // Define _gp for MIPS. st_value of _gp symbol will be updated by Writer
    // so that it points to an absolute address which is relative to GOT.
    // See "Global Data Symbols" in Chapter 6 in the following document:
    // ftp://www.linux-mips.org/pub/linux/mips/doc/ABI/mipsabi.pdf
    Symtab.addSynthetic("_gp", Out<ELFT>::Got, MipsGPOffset);

    // On MIPS O32 ABI, _gp_disp is a magic symbol designates offset between
    // start of function and 'gp' pointer into GOT.
    Symbol *Sym =
        addOptionalSynthetic(Symtab, "_gp_disp", Out<ELFT>::Got, MipsGPOffset);
    if (Sym)
      ElfSym<ELFT>::MipsGpDisp = Sym->body();

    // The __gnu_local_gp is a magic symbol equal to the current value of 'gp'
    // pointer. This symbol is used in the code generated by .cpload pseudo-op
    // in case of using -mno-shared option.
    // https://sourceware.org/ml/binutils/2004-12/msg00094.html
    addOptionalSynthetic(Symtab, "__gnu_local_gp", Out<ELFT>::Got,
                         MipsGPOffset);
  }

  // In the assembly for 32 bit x86 the _GLOBAL_OFFSET_TABLE_ symbol
  // is magical and is used to produce a R_386_GOTPC relocation.
  // The R_386_GOTPC relocation value doesn't actually depend on the
  // symbol value, so it could use an index of STN_UNDEF which, according
  // to the spec, means the symbol value is 0.
  // Unfortunately both gas and MC keep the _GLOBAL_OFFSET_TABLE_ symbol in
  // the object file.
  // The situation is even stranger on x86_64 where the assembly doesn't
  // need the magical symbol, but gas still puts _GLOBAL_OFFSET_TABLE_ as
  // an undefined symbol in the .o files.
  // Given that the symbol is effectively unused, we just create a dummy
  // hidden one to avoid the undefined symbol error.
  if (!Config->Relocatable)
    Symtab.addIgnored("_GLOBAL_OFFSET_TABLE_");

  // __tls_get_addr is defined by the dynamic linker for dynamic ELFs. For
  // static linking the linker is required to optimize away any references to
  // __tls_get_addr, so it's not defined anywhere. Create a hidden definition
  // to avoid the undefined symbol error.
  if (!isOutputDynamic())
    Symtab.addIgnored("__tls_get_addr");

  auto Define = [this](StringRef S, DefinedRegular<ELFT> *&Sym1,
                       DefinedRegular<ELFT> *&Sym2) {
    Sym1 = Symtab.addIgnored(S, STV_DEFAULT);

    // The name without the underscore is not a reserved name,
    // so it is defined only when there is a reference against it.
    assert(S.startswith("_"));
    S = S.substr(1);
    if (SymbolBody *B = Symtab.find(S))
      if (B->isUndefined())
        Sym2 = Symtab.addAbsolute(S, STV_DEFAULT);
  };

  Define("_end", ElfSym<ELFT>::End, ElfSym<ELFT>::End2);
  Define("_etext", ElfSym<ELFT>::Etext, ElfSym<ELFT>::Etext2);
  Define("_edata", ElfSym<ELFT>::Edata, ElfSym<ELFT>::Edata2);
}

// Sort input sections by section name suffixes for
// __attribute__((init_priority(N))).
template <class ELFT> static void sortInitFini(OutputSectionBase<ELFT> *S) {
  if (S)
    reinterpret_cast<OutputSection<ELFT> *>(S)->sortInitFini();
}

// Sort input sections by the special rule for .ctors and .dtors.
template <class ELFT> static void sortCtorsDtors(OutputSectionBase<ELFT> *S) {
  if (S)
    reinterpret_cast<OutputSection<ELFT> *>(S)->sortCtorsDtors();
}

// Create output section objects and add them to OutputSections.
template <class ELFT> void Writer<ELFT>::createSections() {
  // Create output sections for input object file sections.
  std::vector<OutputSectionBase<ELFT> *> RegularSections;
  OutputSectionFactory<ELFT> Factory;
  for (const std::unique_ptr<elf::ObjectFile<ELFT>> &F :
       Symtab.getObjectFiles()) {
    for (InputSectionBase<ELFT> *C : F->getSections()) {
      if (isDiscarded(C)) {
        reportDiscarded(C, F);
        continue;
      }
      OutputSectionBase<ELFT> *Sec;
      bool IsNew;
      std::tie(Sec, IsNew) = Factory.create(C, getOutputSectionName(C));
      if (IsNew) {
        OwningSections.emplace_back(Sec);
        OutputSections.push_back(Sec);
        RegularSections.push_back(Sec);
      }
      Sec->addSection(C);
    }
  }

  // If we have a .opd section (used under PPC64 for function descriptors),
  // store a pointer to it here so that we can use it later when processing
  // relocations.
  Out<ELFT>::Opd = Factory.lookup(".opd", SHT_PROGBITS, SHF_WRITE | SHF_ALLOC);

  Out<ELFT>::Dynamic->PreInitArraySec = Factory.lookup(
      ".preinit_array", SHT_PREINIT_ARRAY, SHF_WRITE | SHF_ALLOC);
  Out<ELFT>::Dynamic->InitArraySec =
      Factory.lookup(".init_array", SHT_INIT_ARRAY, SHF_WRITE | SHF_ALLOC);
  Out<ELFT>::Dynamic->FiniArraySec =
      Factory.lookup(".fini_array", SHT_FINI_ARRAY, SHF_WRITE | SHF_ALLOC);

  // Sort section contents for __attribute__((init_priority(N)).
  sortInitFini(Out<ELFT>::Dynamic->InitArraySec);
  sortInitFini(Out<ELFT>::Dynamic->FiniArraySec);
  sortCtorsDtors(Factory.lookup(".ctors", SHT_PROGBITS, SHF_WRITE | SHF_ALLOC));
  sortCtorsDtors(Factory.lookup(".dtors", SHT_PROGBITS, SHF_WRITE | SHF_ALLOC));

  // The linker needs to define SECNAME_start, SECNAME_end and SECNAME_stop
  // symbols for sections, so that the runtime can get the start and end
  // addresses of each section by section name. Add such symbols.
  if (!Config->Relocatable) {
    addStartEndSymbols();
    for (OutputSectionBase<ELFT> *Sec : RegularSections)
      addStartStopSymbols(Sec);
  }

  // Add _DYNAMIC symbol. Unlike GNU gold, our _DYNAMIC symbol has no type.
  // It should be okay as no one seems to care about the type.
  // Even the author of gold doesn't remember why gold behaves that way.
  // https://sourceware.org/ml/binutils/2002-03/msg00360.html
  if (isOutputDynamic())
    Symtab.addSynthetic("_DYNAMIC", Out<ELFT>::Dynamic, 0);

  // Define __rel[a]_iplt_{start,end} symbols if needed.
  addRelIpltSymbols();

  // Add scripted symbols with zero values now.
  // Real values will be assigned later
  Script<ELFT>::X->addScriptedSymbols();

  if (!Out<ELFT>::EhFrame->empty()) {
    OutputSections.push_back(Out<ELFT>::EhFrame);
    Out<ELFT>::EhFrame->finalize();
  }

  // Scan relocations. This must be done after every symbol is declared so that
  // we can correctly decide if a dynamic relocation is needed.
  for (const std::unique_ptr<elf::ObjectFile<ELFT>> &F :
       Symtab.getObjectFiles()) {
    for (InputSectionBase<ELFT> *C : F->getSections()) {
      if (isDiscarded(C))
        continue;
      if (auto *S = dyn_cast<InputSection<ELFT>>(C)) {
        scanRelocations(*S);
        continue;
      }
      if (auto *S = dyn_cast<EhInputSection<ELFT>>(C))
        if (S->RelocSection)
          scanRelocations(*S, *S->RelocSection);
    }
  }

  for (OutputSectionBase<ELFT> *Sec : OutputSections)
    Sec->assignOffsets();

  // Now that we have defined all possible symbols including linker-
  // synthesized ones. Visit all symbols to give the finishing touches.
  std::vector<DefinedCommon *> CommonSymbols;
  for (Symbol *S : Symtab.getSymbols()) {
    SymbolBody *Body = S->body();

    // We only report undefined symbols in regular objects. This means that we
    // will accept an undefined reference in bitcode if it can be optimized out.
    if (S->IsUsedInRegularObj && Body->isUndefined() && !S->isWeak())
      reportUndefined<ELFT>(Symtab, Body);

    if (auto *C = dyn_cast<DefinedCommon>(Body))
      CommonSymbols.push_back(C);

    if (!includeInSymtab<ELFT>(*Body))
      continue;
    if (Out<ELFT>::SymTab)
      Out<ELFT>::SymTab->addSymbol(Body);

    if (isOutputDynamic() && S->includeInDynsym()) {
      Out<ELFT>::DynSymTab->addSymbol(Body);
      if (auto *SS = dyn_cast<SharedSymbol<ELFT>>(Body))
        if (SS->file()->isNeeded())
          Out<ELFT>::VerNeed->addSymbol(SS);
    }
  }

  // Do not proceed if there was an undefined symbol.
  if (HasError)
    return;

  addCommonSymbols(CommonSymbols);

  // So far we have added sections from input object files.
  // This function adds linker-created Out<ELFT>::* sections.
  addPredefinedSections();

  std::stable_sort(OutputSections.begin(), OutputSections.end(),
                   compareSections<ELFT>);

  unsigned I = 1;
  for (OutputSectionBase<ELFT> *Sec : OutputSections) {
    Sec->SectionIndex = I++;
    Sec->setSHName(Out<ELFT>::ShStrTab->addString(Sec->getName()));
  }

  // Finalizers fix each section's size.
  // .dynsym is finalized early since that may fill up .gnu.hash.
  if (isOutputDynamic())
    Out<ELFT>::DynSymTab->finalize();

  // Fill other section headers. The dynamic table is finalized
  // at the end because some tags like RELSZ depend on result
  // of finalizing other sections. The dynamic string table is
  // finalized once the .dynamic finalizer has added a few last
  // strings. See DynamicSection::finalize()
  for (OutputSectionBase<ELFT> *Sec : OutputSections)
    if (Sec != Out<ELFT>::DynStrTab && Sec != Out<ELFT>::Dynamic)
      Sec->finalize();

  if (isOutputDynamic())
    Out<ELFT>::Dynamic->finalize();

  // Now that all output offsets are fixed. Finalize mergeable sections
  // to fix their maps from input offsets to output offsets.
  for (OutputSectionBase<ELFT> *Sec : OutputSections)
    Sec->finalizePieces();
}

template <class ELFT> bool Writer<ELFT>::needsGot() {
  if (!Out<ELFT>::Got->empty())
    return true;

  // We add the .got section to the result for dynamic MIPS target because
  // its address and properties are mentioned in the .dynamic section.
  if (Config->EMachine == EM_MIPS)
    return true;

  // If we have a relocation that is relative to GOT (such as GOTOFFREL),
  // we need to emit a GOT even if it's empty.
  return Out<ELFT>::Got->HasGotOffRel;
}

// This function add Out<ELFT>::* sections to OutputSections.
template <class ELFT> void Writer<ELFT>::addPredefinedSections() {
  auto Add = [&](OutputSectionBase<ELFT> *C) {
    if (C)
      OutputSections.push_back(C);
  };

  // A core file does not usually contain unmodified segments except
  // the first page of the executable. Add the build ID section to beginning of
  // the file so that the section is included in the first page.
  if (Out<ELFT>::BuildId)
    OutputSections.insert(OutputSections.begin(), Out<ELFT>::BuildId);

  // Add .interp at first because some loaders want to see that section
  // on the first page of the executable file when loaded into memory.
  if (needsInterpSection())
    OutputSections.insert(OutputSections.begin(), Out<ELFT>::Interp);

  // This order is not the same as the final output order
  // because we sort the sections using their attributes below.
  Add(Out<ELFT>::SymTab);
  Add(Out<ELFT>::ShStrTab);
  Add(Out<ELFT>::StrTab);
  if (isOutputDynamic()) {
    Add(Out<ELFT>::DynSymTab);

    bool HasVerNeed = Out<ELFT>::VerNeed->getNeedNum() != 0;
    if (Out<ELFT>::VerDef || HasVerNeed)
      Add(Out<ELFT>::VerSym);
    Add(Out<ELFT>::VerDef);
    if (HasVerNeed)
      Add(Out<ELFT>::VerNeed);

    Add(Out<ELFT>::GnuHashTab);
    Add(Out<ELFT>::HashTab);
    Add(Out<ELFT>::Dynamic);
    Add(Out<ELFT>::DynStrTab);
    if (Out<ELFT>::RelaDyn->hasRelocs())
      Add(Out<ELFT>::RelaDyn);
    Add(Out<ELFT>::MipsRldMap);
  }

  // We always need to add rel[a].plt to output if it has entries.
  // Even during static linking it can contain R_[*]_IRELATIVE relocations.
  if (Out<ELFT>::RelaPlt && Out<ELFT>::RelaPlt->hasRelocs()) {
    Add(Out<ELFT>::RelaPlt);
    Out<ELFT>::RelaPlt->Static = !isOutputDynamic();
  }

  if (needsGot())
    Add(Out<ELFT>::Got);
  if (Out<ELFT>::GotPlt && !Out<ELFT>::GotPlt->empty())
    Add(Out<ELFT>::GotPlt);
  if (!Out<ELFT>::Plt->empty())
    Add(Out<ELFT>::Plt);
  if (!Out<ELFT>::EhFrame->empty())
    Add(Out<ELFT>::EhFrameHdr);
  if (Out<ELFT>::Bss->getSize() > 0)
    Add(Out<ELFT>::Bss);
}

// The linker is expected to define SECNAME_start and SECNAME_end
// symbols for a few sections. This function defines them.
template <class ELFT> void Writer<ELFT>::addStartEndSymbols() {
  auto Define = [&](StringRef Start, StringRef End,
                    OutputSectionBase<ELFT> *OS) {
    if (OS) {
      this->Symtab.addSynthetic(Start, OS, 0);
      this->Symtab.addSynthetic(End, OS, DefinedSynthetic<ELFT>::SectionEnd);
    } else {
      addOptionalSynthetic(this->Symtab, Start,
                           (OutputSectionBase<ELFT> *)nullptr, 0);
      addOptionalSynthetic(this->Symtab, End,
                           (OutputSectionBase<ELFT> *)nullptr, 0);
    }
  };

  Define("__preinit_array_start", "__preinit_array_end",
         Out<ELFT>::Dynamic->PreInitArraySec);
  Define("__init_array_start", "__init_array_end",
         Out<ELFT>::Dynamic->InitArraySec);
  Define("__fini_array_start", "__fini_array_end",
         Out<ELFT>::Dynamic->FiniArraySec);
}

// If a section name is valid as a C identifier (which is rare because of
// the leading '.'), linkers are expected to define __start_<secname> and
// __stop_<secname> symbols. They are at beginning and end of the section,
// respectively. This is not requested by the ELF standard, but GNU ld and
// gold provide the feature, and used by many programs.
template <class ELFT>
void Writer<ELFT>::addStartStopSymbols(OutputSectionBase<ELFT> *Sec) {
  StringRef S = Sec->getName();
  if (!isValidCIdentifier(S))
    return;
  StringSaver Saver(Alloc);
  StringRef Start = Saver.save("__start_" + S);
  StringRef Stop = Saver.save("__stop_" + S);
  if (SymbolBody *B = Symtab.find(Start))
    if (B->isUndefined())
      Symtab.addSynthetic(Start, Sec, 0);
  if (SymbolBody *B = Symtab.find(Stop))
    if (B->isUndefined())
      Symtab.addSynthetic(Stop, Sec, DefinedSynthetic<ELFT>::SectionEnd);
}

template <class ELFT> static bool needsPtLoad(OutputSectionBase<ELFT> *Sec) {
  if (!(Sec->getFlags() & SHF_ALLOC))
    return false;

  // Don't allocate VA space for TLS NOBITS sections. The PT_TLS PHDR is
  // responsible for allocating space for them, not the PT_LOAD that
  // contains the TLS initialization image.
  if (Sec->getFlags() & SHF_TLS && Sec->getType() == SHT_NOBITS)
    return false;
  return true;
}

static uint32_t toPhdrFlags(uint64_t Flags) {
  uint32_t Ret = PF_R;
  if (Flags & SHF_WRITE)
    Ret |= PF_W;
  if (Flags & SHF_EXECINSTR)
    Ret |= PF_X;
  return Ret;
}

// Decide which program headers to create and which sections to include in each
// one.
template <class ELFT> void Writer<ELFT>::createPhdrs() {
  auto AddHdr = [this](unsigned Type, unsigned Flags) {
    return &*Phdrs.emplace(Phdrs.end(), Type, Flags);
  };

  auto AddSec = [](Phdr &Hdr, OutputSectionBase<ELFT> *Sec) {
    Hdr.Last = Sec;
    if (!Hdr.First)
      Hdr.First = Sec;
    Hdr.H.p_align = std::max<uintX_t>(Hdr.H.p_align, Sec->getAlignment());
  };

  // The first phdr entry is PT_PHDR which describes the program header itself.
  Phdr &Hdr = *AddHdr(PT_PHDR, PF_R);
  AddSec(Hdr, Out<ELFT>::ProgramHeaders);

  // PT_INTERP must be the second entry if exists.
  if (needsInterpSection()) {
    Phdr &Hdr = *AddHdr(PT_INTERP, toPhdrFlags(Out<ELFT>::Interp->getFlags()));
    AddSec(Hdr, Out<ELFT>::Interp);
  }

  // Add the first PT_LOAD segment for regular output sections.
  uintX_t Flags = PF_R;
  Phdr *Load = AddHdr(PT_LOAD, Flags);
  AddSec(*Load, Out<ELFT>::ElfHeader);
  AddSec(*Load, Out<ELFT>::ProgramHeaders);

  Phdr TlsHdr(PT_TLS, PF_R);
  Phdr RelRo(PT_GNU_RELRO, PF_R);
  Phdr Note(PT_NOTE, PF_R);
  for (OutputSectionBase<ELFT> *Sec : OutputSections) {
    if (!(Sec->getFlags() & SHF_ALLOC))
      break;

    // If we meet TLS section then we create TLS header
    // and put all TLS sections inside for futher use when
    // assign addresses.
    if (Sec->getFlags() & SHF_TLS)
      AddSec(TlsHdr, Sec);

    if (!needsPtLoad<ELFT>(Sec))
      continue;

    // If flags changed then we want new load segment.
    uintX_t NewFlags = toPhdrFlags(Sec->getFlags());
    if (Flags != NewFlags) {
      Load = AddHdr(PT_LOAD, NewFlags);
      Flags = NewFlags;
    }

    AddSec(*Load, Sec);

    if (isRelroSection(Sec))
      AddSec(RelRo, Sec);
    if (Sec->getType() == SHT_NOTE)
      AddSec(Note, Sec);
  }

  // Add the TLS segment unless it's empty.
  if (TlsHdr.First)
    Phdrs.push_back(std::move(TlsHdr));

  // Add an entry for .dynamic.
  if (isOutputDynamic()) {
    Phdr &H = *AddHdr(PT_DYNAMIC, toPhdrFlags(Out<ELFT>::Dynamic->getFlags()));
    AddSec(H, Out<ELFT>::Dynamic);
  }

  // PT_GNU_RELRO includes all sections that should be marked as
  // read-only by dynamic linker after proccessing relocations.
  if (RelRo.First)
    Phdrs.push_back(std::move(RelRo));

  // PT_GNU_EH_FRAME is a special section pointing on .eh_frame_hdr.
  if (!Out<ELFT>::EhFrame->empty() && Out<ELFT>::EhFrameHdr) {
    Phdr &Hdr = *AddHdr(PT_GNU_EH_FRAME,
                        toPhdrFlags(Out<ELFT>::EhFrameHdr->getFlags()));
    AddSec(Hdr, Out<ELFT>::EhFrameHdr);
  }

  // PT_GNU_STACK is a special section to tell the loader to make the
  // pages for the stack non-executable.
  if (!Config->ZExecStack)
    AddHdr(PT_GNU_STACK, PF_R | PF_W);

  if (Note.First)
    Phdrs.push_back(std::move(Note));

  Out<ELFT>::ProgramHeaders->setSize(sizeof(Elf_Phdr) * Phdrs.size());
}

// The first section of each PT_LOAD and the first section after PT_GNU_RELRO
// have to be page aligned so that the dynamic linker can set the permissions.
template <class ELFT> void Writer<ELFT>::fixSectionAlignments() {
  for (const Phdr &P : Phdrs)
    if (P.H.p_type == PT_LOAD)
      P.First->PageAlign = true;

  for (const Phdr &P : Phdrs) {
    if (P.H.p_type != PT_GNU_RELRO)
      continue;
    // Find the first section after PT_GNU_RELRO. If it is in a PT_LOAD we
    // have to align it to a page.
    auto End = OutputSections.end();
    auto I = std::find(OutputSections.begin(), End, P.Last);
    if (I == End || (I + 1) == End)
      continue;
    OutputSectionBase<ELFT> *Sec = *(I + 1);
    if (needsPtLoad(Sec))
      Sec->PageAlign = true;
  }
}

// We should set file offsets and VAs for elf header and program headers
// sections. These are special, we do not include them into output sections
// list, but have them to simplify the code.
template <class ELFT> void Writer<ELFT>::fixHeaders() {
  uintX_t BaseVA = ScriptConfig->DoLayout ? 0 : Config->ImageBase;
  Out<ELFT>::ElfHeader->setVA(BaseVA);
  uintX_t Off = Out<ELFT>::ElfHeader->getSize();
  Out<ELFT>::ProgramHeaders->setVA(Off + BaseVA);
}

// Assign VAs (addresses at run-time) to output sections.
template <class ELFT> void Writer<ELFT>::assignAddresses() {
  uintX_t VA = Config->ImageBase + Out<ELFT>::ElfHeader->getSize() +
               Out<ELFT>::ProgramHeaders->getSize();

  uintX_t ThreadBssOffset = 0;
  for (OutputSectionBase<ELFT> *Sec : OutputSections) {
    uintX_t Alignment = Sec->getAlignment();
    if (Sec->PageAlign)
      Alignment = std::max<uintX_t>(Alignment, Target->PageSize);

    // We only assign VAs to allocated sections.
    if (needsPtLoad<ELFT>(Sec)) {
      VA = alignTo(VA, Alignment);
      Sec->setVA(VA);
      VA += Sec->getSize();
    } else if (Sec->getFlags() & SHF_TLS && Sec->getType() == SHT_NOBITS) {
      uintX_t TVA = VA + ThreadBssOffset;
      TVA = alignTo(TVA, Alignment);
      Sec->setVA(TVA);
      ThreadBssOffset = TVA - VA + Sec->getSize();
    }
  }
}

// Adjusts the file alignment for a given output section and returns
// its new file offset. The file offset must be the same with its
// virtual address (modulo the page size) so that the loader can load
// executables without any address adjustment.
template <class ELFT, class uintX_t>
static uintX_t getFileAlignment(uintX_t Off, OutputSectionBase<ELFT> *Sec) {
  uintX_t Alignment = Sec->getAlignment();
  if (Sec->PageAlign)
    Alignment = std::max<uintX_t>(Alignment, Target->PageSize);
  Off = alignTo(Off, Alignment);

  // Relocatable output does not have program headers
  // and does not need any other offset adjusting.
  if (Config->Relocatable || !(Sec->getFlags() & SHF_ALLOC))
    return Off;
  return alignTo(Off, Target->PageSize, Sec->getVA());
}

// Assign file offsets to output sections.
template <class ELFT> void Writer<ELFT>::assignFileOffsets() {
  uintX_t Off = 0;

  auto Set = [&](OutputSectionBase<ELFT> *Sec) {
    if (Sec->getType() == SHT_NOBITS) {
      Sec->setFileOffset(Off);
      return;
    }

    Off = getFileAlignment<ELFT>(Off, Sec);
    Sec->setFileOffset(Off);
    Off += Sec->getSize();
  };

  Set(Out<ELFT>::ElfHeader);
  Set(Out<ELFT>::ProgramHeaders);
  for (OutputSectionBase<ELFT> *Sec : OutputSections)
    Set(Sec);

  SectionHeaderOff = alignTo(Off, sizeof(uintX_t));
  FileSize = SectionHeaderOff + (OutputSections.size() + 1) * sizeof(Elf_Shdr);
}

// Finalize the program headers. We call this function after we assign
// file offsets and VAs to all sections.
template <class ELFT> void Writer<ELFT>::setPhdrs() {
  for (Phdr &P : Phdrs) {
    Elf_Phdr &H = P.H;
    OutputSectionBase<ELFT> *First = P.First;
    OutputSectionBase<ELFT> *Last = P.Last;
    if (First) {
      H.p_filesz = Last->getFileOff() - First->getFileOff();
      if (Last->getType() != SHT_NOBITS)
        H.p_filesz += Last->getSize();
      H.p_memsz = Last->getVA() + Last->getSize() - First->getVA();
      H.p_offset = First->getFileOff();
      H.p_vaddr = First->getVA();
    }
    if (H.p_type == PT_LOAD)
      H.p_align = Target->PageSize;
    else if (H.p_type == PT_GNU_RELRO)
      H.p_align = 1;
    H.p_paddr = H.p_vaddr;

    // The TLS pointer goes after PT_TLS. At least glibc will align it,
    // so round up the size to make sure the offsets are correct.
    if (H.p_type == PT_TLS) {
      Out<ELFT>::TlsPhdr = &H;
      H.p_memsz = alignTo(H.p_memsz, H.p_align);
    }
  }
}

static uint32_t getMipsEFlags(bool Is64Bits) {
  // FIXME: In fact ELF flags depends on ELF flags of input object files
  // and selected emulation. For now just use hard coded values.
  if (Is64Bits)
    return EF_MIPS_CPIC | EF_MIPS_PIC | EF_MIPS_ARCH_64R2;

  uint32_t V = EF_MIPS_CPIC | EF_MIPS_ABI_O32 | EF_MIPS_ARCH_32R2;
  if (Config->Shared)
    V |= EF_MIPS_PIC;
  return V;
}

template <class ELFT> static typename ELFT::uint getEntryAddr() {
  if (Symbol *S = Config->EntrySym)
    return S->body()->getVA<ELFT>();
  if (Config->EntryAddr != uint64_t(-1))
    return Config->EntryAddr;
  return 0;
}

template <class ELFT> static uint8_t getELFEncoding() {
  if (ELFT::TargetEndianness == llvm::support::little)
    return ELFDATA2LSB;
  return ELFDATA2MSB;
}

static uint16_t getELFType() {
  if (Config->Pic)
    return ET_DYN;
  if (Config->Relocatable)
    return ET_REL;
  return ET_EXEC;
}

// This function is called after we have assigned address and size
// to each section. This function fixes some predefined absolute
// symbol values that depend on section address and size.
template <class ELFT> void Writer<ELFT>::fixAbsoluteSymbols() {
  auto Set = [](DefinedRegular<ELFT> *S1, DefinedRegular<ELFT> *S2, uintX_t V) {
    if (S1)
      S1->Value = V;
    if (S2)
      S2->Value = V;
  };

  // _etext is the first location after the last read-only loadable segment.
  // _edata is the first location after the last read-write loadable segment.
  // _end is the first location after the uninitialized data region.
  for (Phdr &P : Phdrs) {
    Elf_Phdr &H = P.H;
    if (H.p_type != PT_LOAD)
      continue;
    Set(ElfSym<ELFT>::End, ElfSym<ELFT>::End2, H.p_vaddr + H.p_memsz);

    uintX_t Val = H.p_vaddr + H.p_filesz;
    if (H.p_flags & PF_W)
      Set(ElfSym<ELFT>::Edata, ElfSym<ELFT>::Edata2, Val);
    else
      Set(ElfSym<ELFT>::Etext, ElfSym<ELFT>::Etext2, Val);
  }
}

template <class ELFT> void Writer<ELFT>::writeHeader() {
  uint8_t *Buf = Buffer->getBufferStart();
  memcpy(Buf, "\177ELF", 4);

  auto &FirstObj = cast<ELFFileBase<ELFT>>(*Config->FirstElf);

  // Write the ELF header.
  auto *EHdr = reinterpret_cast<Elf_Ehdr *>(Buf);
  EHdr->e_ident[EI_CLASS] = ELFT::Is64Bits ? ELFCLASS64 : ELFCLASS32;
  EHdr->e_ident[EI_DATA] = getELFEncoding<ELFT>();
  EHdr->e_ident[EI_VERSION] = EV_CURRENT;
  EHdr->e_ident[EI_OSABI] = FirstObj.getOSABI();
  EHdr->e_type = getELFType();
  EHdr->e_machine = FirstObj.EMachine;
  EHdr->e_version = EV_CURRENT;
  EHdr->e_entry = getEntryAddr<ELFT>();
  EHdr->e_shoff = SectionHeaderOff;
  EHdr->e_ehsize = sizeof(Elf_Ehdr);
  EHdr->e_phnum = Phdrs.size();
  EHdr->e_shentsize = sizeof(Elf_Shdr);
  EHdr->e_shnum = OutputSections.size() + 1;
  EHdr->e_shstrndx = Out<ELFT>::ShStrTab->SectionIndex;

  if (Config->EMachine == EM_MIPS)
    EHdr->e_flags = getMipsEFlags(ELFT::Is64Bits);

  if (!Config->Relocatable) {
    EHdr->e_phoff = sizeof(Elf_Ehdr);
    EHdr->e_phentsize = sizeof(Elf_Phdr);
  }

  // Write the program header table.
  auto *HBuf = reinterpret_cast<Elf_Phdr *>(Buf + EHdr->e_phoff);
  for (Phdr &P : Phdrs)
    *HBuf++ = P.H;

  // Write the section header table. Note that the first table entry is null.
  auto *SHdrs = reinterpret_cast<Elf_Shdr *>(Buf + EHdr->e_shoff);
  for (OutputSectionBase<ELFT> *Sec : OutputSections)
    Sec->writeHeaderTo(++SHdrs);
}

template <class ELFT> void Writer<ELFT>::openFile() {
  ErrorOr<std::unique_ptr<FileOutputBuffer>> BufferOrErr =
      FileOutputBuffer::create(Config->OutputFile, FileSize,
                               FileOutputBuffer::F_executable);
  if (auto EC = BufferOrErr.getError())
    error(EC, "failed to open " + Config->OutputFile);
  else
    Buffer = std::move(*BufferOrErr);
}

// Write section contents to a mmap'ed file.
template <class ELFT> void Writer<ELFT>::writeSections() {
  uint8_t *Buf = Buffer->getBufferStart();

  // PPC64 needs to process relocations in the .opd section before processing
  // relocations in code-containing sections.
  if (OutputSectionBase<ELFT> *Sec = Out<ELFT>::Opd) {
    Out<ELFT>::OpdBuf = Buf + Sec->getFileOff();
    Sec->writeTo(Buf + Sec->getFileOff());
  }

  for (OutputSectionBase<ELFT> *Sec : OutputSections)
    if (Sec != Out<ELFT>::Opd)
      Sec->writeTo(Buf + Sec->getFileOff());
}

template <class ELFT> void Writer<ELFT>::writeBuildId() {
  if (!Out<ELFT>::BuildId)
    return;

  // Compute a hash of all sections except .debug_* sections.
  // We skip debug sections because they tend to be very large
  // and their contents are very likely to be the same as long as
  // other sections are the same.
  uint8_t *Start = Buffer->getBufferStart();
  uint8_t *Last = Start;
  std::vector<ArrayRef<uint8_t>> Regions;
  for (OutputSectionBase<ELFT> *Sec : OutputSections) {
    uint8_t *End = Start + Sec->getFileOff();
    if (!Sec->getName().startswith(".debug_"))
      Regions.push_back({Last, End});
    Last = End;
  }
  Regions.push_back({Last, Start + FileSize});
  Out<ELFT>::BuildId->writeBuildId(Regions);
}

template void elf::writeResult<ELF32LE>(SymbolTable<ELF32LE> *Symtab);
template void elf::writeResult<ELF32BE>(SymbolTable<ELF32BE> *Symtab);
template void elf::writeResult<ELF64LE>(SymbolTable<ELF64LE> *Symtab);
template void elf::writeResult<ELF64BE>(SymbolTable<ELF64BE> *Symtab);
