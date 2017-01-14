//===- Driver.cpp ---------------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Driver.h"
#include "Config.h"
#include "Error.h"
#include "ICF.h"
#include "InputFiles.h"
#include "InputSection.h"
#include "LinkerScript.h"
#include "Strings.h"
#include "SymbolListFile.h"
#include "SymbolTable.h"
#include "Target.h"
#include "Writer.h"
#include "lld/Driver/Driver.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <utility>

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::object;
using namespace llvm::sys;

using namespace lld;
using namespace lld::elf;

Configuration *elf::Config;
LinkerDriver *elf::Driver;

bool elf::link(ArrayRef<const char *> Args, raw_ostream &Error) {
  HasError = false;
  ErrorOS = &Error;

  Configuration C;
  LinkerDriver D;
  ScriptConfiguration SC;
  Config = &C;
  Driver = &D;
  ScriptConfig = &SC;

  Driver->main(Args);
  return !HasError;
}

// Parses a linker -m option.
static std::pair<ELFKind, uint16_t> parseEmulation(StringRef S) {
  if (S.endswith("_fbsd"))
    S = S.drop_back(5);

  std::pair<ELFKind, uint16_t> Ret =
      StringSwitch<std::pair<ELFKind, uint16_t>>(S)
          .Case("aarch64linux", {ELF64LEKind, EM_AARCH64})
          .Case("armelf_linux_eabi", {ELF32LEKind, EM_ARM})
          .Case("elf32_x86_64", {ELF32LEKind, EM_X86_64})
          .Case("elf32btsmip", {ELF32BEKind, EM_MIPS})
          .Case("elf32ltsmip", {ELF32LEKind, EM_MIPS})
          .Case("elf32ppc", {ELF32BEKind, EM_PPC})
          .Case("elf64btsmip", {ELF64BEKind, EM_MIPS})
          .Case("elf64ltsmip", {ELF64LEKind, EM_MIPS})
          .Case("elf64ppc", {ELF64BEKind, EM_PPC64})
          .Case("elf_i386", {ELF32LEKind, EM_386})
          .Case("elf_x86_64", {ELF64LEKind, EM_X86_64})
          .Default({ELFNoneKind, EM_NONE});

  if (Ret.first == ELFNoneKind) {
    if (S == "i386pe" || S == "i386pep" || S == "thumb2pe")
      error("Windows targets are not supported on the ELF frontend: " + S);
    else
      error("unknown emulation: " + S);
  }
  return Ret;
}

// Returns slices of MB by parsing MB as an archive file.
// Each slice consists of a member file in the archive.
std::vector<MemoryBufferRef>
LinkerDriver::getArchiveMembers(MemoryBufferRef MB) {
  std::unique_ptr<Archive> File =
      check(Archive::create(MB), "failed to parse archive");

  std::vector<MemoryBufferRef> V;
  Error Err;
  for (const ErrorOr<Archive::Child> &COrErr : File->children(Err)) {
    Archive::Child C = check(COrErr, "could not get the child of the archive " +
                                         File->getFileName());
    MemoryBufferRef MBRef =
        check(C.getMemoryBufferRef(),
              "could not get the buffer for a child of the archive " +
                  File->getFileName());
    V.push_back(MBRef);
  }
  if (Err)
    Error(Err);

  // Take ownership of memory buffers created for members of thin archives.
  for (std::unique_ptr<MemoryBuffer> &MB : File->takeThinBuffers())
    OwningMBs.push_back(std::move(MB));

  return V;
}

// Opens and parses a file. Path has to be resolved already.
// Newly created memory buffers are owned by this driver.
void LinkerDriver::addFile(StringRef Path) {
  using namespace sys::fs;
  if (Config->Verbose)
    outs() << Path << "\n";

  Optional<MemoryBufferRef> Buffer = readFile(Path);
  if (!Buffer.hasValue())
    return;
  MemoryBufferRef MBRef = *Buffer;

  switch (identify_magic(MBRef.getBuffer())) {
  case file_magic::unknown:
    readLinkerScript(MBRef);
    return;
  case file_magic::archive:
    if (WholeArchive) {
      for (MemoryBufferRef MB : getArchiveMembers(MBRef))
        Files.push_back(createObjectFile(MB, Path));
      return;
    }
    Files.push_back(make_unique<ArchiveFile>(MBRef));
    return;
  case file_magic::elf_shared_object:
    if (Config->Relocatable) {
      error("attempted static link of dynamic object " + Path);
      return;
    }
    Files.push_back(createSharedFile(MBRef));
    return;
  default:
    if (InLib)
      Files.push_back(make_unique<LazyObjectFile>(MBRef));
    else
      Files.push_back(createObjectFile(MBRef));
  }
}

Optional<MemoryBufferRef> LinkerDriver::readFile(StringRef Path) {
  auto MBOrErr = MemoryBuffer::getFile(Path);
  if (auto EC = MBOrErr.getError()) {
    error(EC, "cannot open " + Path);
    return None;
  }
  std::unique_ptr<MemoryBuffer> &MB = *MBOrErr;
  MemoryBufferRef MBRef = MB->getMemBufferRef();
  OwningMBs.push_back(std::move(MB)); // take MB ownership

  if (Cpio)
    Cpio->append(relativeToRoot(Path), MBRef.getBuffer());

  return MBRef;
}

// Add a given library by searching it from input search paths.
void LinkerDriver::addLibrary(StringRef Name) {
  std::string Path = searchLibrary(Name);
  if (Path.empty())
    error("unable to find library -l" + Name);
  else
    addFile(Path);
}

// This function is called on startup. We need this for LTO since
// LTO calls LLVM functions to compile bitcode files to native code.
// Technically this can be delayed until we read bitcode files, but
// we don't bother to do lazily because the initialization is fast.
static void initLLVM(opt::InputArgList &Args) {
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  // This is a flag to discard all but GlobalValue names.
  // We want to enable it by default because it saves memory.
  // Disable it only when a developer option (-save-temps) is given.
  Driver->Context.setDiscardValueNames(!Config->SaveTemps);
  Driver->Context.enableDebugTypeODRUniquing();

  // Parse and evaluate -mllvm options.
  std::vector<const char *> V;
  V.push_back("lld (LLVM option parsing)");
  for (auto *Arg : Args.filtered(OPT_mllvm))
    V.push_back(Arg->getValue());
  cl::ParseCommandLineOptions(V.size(), V.data());
}

// Some command line options or some combinations of them are not allowed.
// This function checks for such errors.
static void checkOptions(opt::InputArgList &Args) {
  // The MIPS ABI as of 2016 does not support the GNU-style symbol lookup
  // table which is a relatively new feature.
  if (Config->EMachine == EM_MIPS && Config->GnuHash)
    error("the .gnu.hash section is not compatible with the MIPS target.");

  if (Config->EMachine == EM_AMDGPU && !Config->Entry.empty())
    error("-e option is not valid for AMDGPU.");

  if (Config->Pie && Config->Shared)
    error("-shared and -pie may not be used together");

  if (Config->Relocatable) {
    if (Config->Shared)
      error("-r and -shared may not be used together");
    if (Config->GcSections)
      error("-r and --gc-sections may not be used together");
    if (Config->ICF)
      error("-r and --icf may not be used together");
    if (Config->Pie)
      error("-r and -pie may not be used together");
  }
}

static StringRef
getString(opt::InputArgList &Args, unsigned Key, StringRef Default = "") {
  if (auto *Arg = Args.getLastArg(Key))
    return Arg->getValue();
  return Default;
}

static int getInteger(opt::InputArgList &Args, unsigned Key, int Default) {
  int V = Default;
  if (auto *Arg = Args.getLastArg(Key)) {
    StringRef S = Arg->getValue();
    if (S.getAsInteger(10, V))
      error(Arg->getSpelling() + ": number expected, but got " + S);
  }
  return V;
}

static const char *getReproduceOption(opt::InputArgList &Args) {
  if (auto *Arg = Args.getLastArg(OPT_reproduce))
    return Arg->getValue();
  return getenv("LLD_REPRODUCE");
}

static bool hasZOption(opt::InputArgList &Args, StringRef Key) {
  for (auto *Arg : Args.filtered(OPT_z))
    if (Key == Arg->getValue())
      return true;
  return false;
}

void LinkerDriver::main(ArrayRef<const char *> ArgsArr) {
  ELFOptTable Parser;
  opt::InputArgList Args = Parser.parse(ArgsArr.slice(1));
  if (Args.hasArg(OPT_help)) {
    printHelp(ArgsArr[0]);
    return;
  }
  if (Args.hasArg(OPT_version)) {
    outs() << getVersionString();
    return;
  }

  if (const char *Path = getReproduceOption(Args)) {
    // Note that --reproduce is a debug option so you can ignore it
    // if you are trying to understand the whole picture of the code.
    Cpio.reset(CpioFile::create(Path));
    if (Cpio) {
      Cpio->append("response.txt", createResponseFile(Args));
      Cpio->append("version.txt", getVersionString());
    }
  }

  readConfigs(Args);
  initLLVM(Args);
  createFiles(Args);
  checkOptions(Args);
  if (HasError)
    return;

  switch (Config->EKind) {
  case ELF32LEKind:
    link<ELF32LE>(Args);
    return;
  case ELF32BEKind:
    link<ELF32BE>(Args);
    return;
  case ELF64LEKind:
    link<ELF64LE>(Args);
    return;
  case ELF64BEKind:
    link<ELF64BE>(Args);
    return;
  default:
    error("-m or at least a .o file required");
  }
}

static UnresolvedPolicy getUnresolvedSymbolOption(opt::InputArgList &Args) {
  if (Args.hasArg(OPT_noinhibit_exec))
    return UnresolvedPolicy::Warn;
  if (Args.hasArg(OPT_no_undefined) || hasZOption(Args, "defs"))
    return UnresolvedPolicy::NoUndef;
  if (Config->Relocatable)
    return UnresolvedPolicy::Ignore;

  if (auto *Arg = Args.getLastArg(OPT_unresolved_symbols)) {
    StringRef S = Arg->getValue();
    if (S == "ignore-all" || S == "ignore-in-object-files")
      return UnresolvedPolicy::Ignore;
    if (S == "ignore-in-shared-libs" || S == "report-all")
      return UnresolvedPolicy::Error;
    error("unknown --unresolved-symbols value: " + S);
  }
  return UnresolvedPolicy::Error;
}

// Initializes Config members by the command line options.
void LinkerDriver::readConfigs(opt::InputArgList &Args) {
  for (auto *Arg : Args.filtered(OPT_L))
    Config->SearchPaths.push_back(Arg->getValue());

  std::vector<StringRef> RPaths;
  for (auto *Arg : Args.filtered(OPT_rpath))
    RPaths.push_back(Arg->getValue());
  if (!RPaths.empty())
    Config->RPath = llvm::join(RPaths.begin(), RPaths.end(), ":");

  if (auto *Arg = Args.getLastArg(OPT_m)) {
    // Parse ELF{32,64}{LE,BE} and CPU type.
    StringRef S = Arg->getValue();
    std::tie(Config->EKind, Config->EMachine) = parseEmulation(S);
    Config->Emulation = S;
  }

  Config->AllowMultipleDefinition = Args.hasArg(OPT_allow_multiple_definition);
  Config->Bsymbolic = Args.hasArg(OPT_Bsymbolic);
  Config->BsymbolicFunctions = Args.hasArg(OPT_Bsymbolic_functions);
  Config->Demangle = !Args.hasArg(OPT_no_demangle);
  Config->DisableVerify = Args.hasArg(OPT_disable_verify);
  Config->DiscardAll = Args.hasArg(OPT_discard_all);
  Config->DiscardLocals = Args.hasArg(OPT_discard_locals);
  Config->DiscardNone = Args.hasArg(OPT_discard_none);
  Config->EhFrameHdr = Args.hasArg(OPT_eh_frame_hdr);
  Config->EnableNewDtags = !Args.hasArg(OPT_disable_new_dtags);
  Config->ExportDynamic = Args.hasArg(OPT_export_dynamic);
  Config->FatalWarnings = Args.hasArg(OPT_fatal_warnings);
  Config->GcSections = Args.hasArg(OPT_gc_sections);
  Config->ICF = Args.hasArg(OPT_icf);
  Config->NoGnuUnique = Args.hasArg(OPT_no_gnu_unique);
  Config->NoUndefinedVersion = Args.hasArg(OPT_no_undefined_version);
  Config->Pie = Args.hasArg(OPT_pie);
  Config->PrintGcSections = Args.hasArg(OPT_print_gc_sections);
  Config->Relocatable = Args.hasArg(OPT_relocatable);
  Config->SaveTemps = Args.hasArg(OPT_save_temps);
  Config->Shared = Args.hasArg(OPT_shared);
  Config->StripAll = Args.hasArg(OPT_strip_all);
  Config->StripDebug = Args.hasArg(OPT_strip_debug);
  Config->Threads = Args.hasArg(OPT_threads);
  Config->Trace = Args.hasArg(OPT_trace);
  Config->Verbose = Args.hasArg(OPT_verbose);
  Config->WarnCommon = Args.hasArg(OPT_warn_common);

  Config->DynamicLinker = getString(Args, OPT_dynamic_linker);
  Config->Entry = getString(Args, OPT_entry);
  Config->Fini = getString(Args, OPT_fini, "_fini");
  Config->Init = getString(Args, OPT_init, "_init");
  Config->LtoAAPipeline = getString(Args, OPT_lto_aa_pipeline);
  Config->LtoNewPmPasses = getString(Args, OPT_lto_newpm_passes);
  Config->OutputFile = getString(Args, OPT_o);
  Config->SoName = getString(Args, OPT_soname);
  Config->Sysroot = getString(Args, OPT_sysroot);

  Config->Optimize = getInteger(Args, OPT_O, 1);
  Config->LtoO = getInteger(Args, OPT_lto_O, 2);
  if (Config->LtoO > 3)
    error("invalid optimization level for LTO: " + getString(Args, OPT_lto_O));
  Config->LtoJobs = getInteger(Args, OPT_lto_jobs, 1);
  if (Config->LtoJobs == 0)
    error("number of threads must be > 0");

  Config->ZCombreloc = !hasZOption(Args, "nocombreloc");
  Config->ZExecStack = hasZOption(Args, "execstack");
  Config->ZNodelete = hasZOption(Args, "nodelete");
  Config->ZNow = hasZOption(Args, "now");
  Config->ZOrigin = hasZOption(Args, "origin");
  Config->ZRelro = !hasZOption(Args, "norelro");

  if (Config->Relocatable)
    Config->StripAll = false;

  // --strip-all implies --strip-debug.
  if (Config->StripAll)
    Config->StripDebug = true;

  // Config->Pic is true if we are generating position-independent code.
  Config->Pic = Config->Pie || Config->Shared;

  if (auto *Arg = Args.getLastArg(OPT_hash_style)) {
    StringRef S = Arg->getValue();
    if (S == "gnu") {
      Config->GnuHash = true;
      Config->SysvHash = false;
    } else if (S == "both") {
      Config->GnuHash = true;
    } else if (S != "sysv")
      error("unknown hash style: " + S);
  }

  // Parse --build-id or --build-id=<style>.
  if (Args.hasArg(OPT_build_id))
    Config->BuildId = BuildIdKind::Fnv1;
  if (auto *Arg = Args.getLastArg(OPT_build_id_eq)) {
    StringRef S = Arg->getValue();
    if (S == "md5") {
      Config->BuildId = BuildIdKind::Md5;
    } else if (S == "sha1") {
      Config->BuildId = BuildIdKind::Sha1;
    } else if (S == "none") {
      Config->BuildId = BuildIdKind::None;
    } else if (S.startswith("0x")) {
      Config->BuildId = BuildIdKind::Hexstring;
      Config->BuildIdVector = parseHex(S.substr(2));
    } else {
      error("unknown --build-id style: " + S);
    }
  }

  for (auto *Arg : Args.filtered(OPT_undefined))
    Config->Undefined.push_back(Arg->getValue());

  Config->UnresolvedSymbols = getUnresolvedSymbolOption(Args);

  if (auto *Arg = Args.getLastArg(OPT_dynamic_list))
    if (Optional<MemoryBufferRef> Buffer = readFile(Arg->getValue()))
      parseDynamicList(*Buffer);

  for (auto *Arg : Args.filtered(OPT_export_dynamic_symbol))
    Config->DynamicList.push_back(Arg->getValue());

  if (auto *Arg = Args.getLastArg(OPT_version_script))
    if (Optional<MemoryBufferRef> Buffer = readFile(Arg->getValue()))
      parseVersionScript(*Buffer);
}

void LinkerDriver::createFiles(opt::InputArgList &Args) {
  for (auto *Arg : Args) {
    switch (Arg->getOption().getID()) {
    case OPT_l:
      addLibrary(Arg->getValue());
      break;
    case OPT_alias_script_T:
    case OPT_INPUT:
    case OPT_script:
      addFile(Arg->getValue());
      break;
    case OPT_as_needed:
      Config->AsNeeded = true;
      break;
    case OPT_no_as_needed:
      Config->AsNeeded = false;
      break;
    case OPT_Bstatic:
      Config->Static = true;
      break;
    case OPT_Bdynamic:
      Config->Static = false;
      break;
    case OPT_whole_archive:
      WholeArchive = true;
      break;
    case OPT_no_whole_archive:
      WholeArchive = false;
      break;
    case OPT_start_lib:
      InLib = true;
      break;
    case OPT_end_lib:
      InLib = false;
      break;
    }
  }

  if (Files.empty() && !HasError)
    error("no input files.");

  // If -m <machine_type> was not given, infer it from object files.
  if (Config->EKind == ELFNoneKind) {
    for (std::unique_ptr<InputFile> &F : Files) {
      if (F->EKind == ELFNoneKind)
        continue;
      Config->EKind = F->EKind;
      Config->EMachine = F->EMachine;
      break;
    }
  }
}

// Do actual linking. Note that when this function is called,
// all linker scripts have already been parsed.
template <class ELFT> void LinkerDriver::link(opt::InputArgList &Args) {
  SymbolTable<ELFT> Symtab;
  elf::Symtab<ELFT>::X = &Symtab;

  std::unique_ptr<TargetInfo> TI(createTarget());
  Target = TI.get();
  LinkerScript<ELFT> LS;
  Script<ELFT>::X = &LS;

  Config->Rela = ELFT::Is64Bits || Config->EMachine == EM_X86_64;
  Config->Mips64EL =
      (Config->EMachine == EM_MIPS && Config->EKind == ELF64LEKind);

  // Add entry symbol. Note that AMDGPU binaries have no entry points.
  if (Config->Entry.empty() && !Config->Shared && !Config->Relocatable &&
      Config->EMachine != EM_AMDGPU)
    Config->Entry = (Config->EMachine == EM_MIPS) ? "__start" : "_start";

  // Default output filename is "a.out" by the Unix tradition.
  if (Config->OutputFile.empty())
    Config->OutputFile = "a.out";

  // Handle --trace-symbol.
  for (auto *Arg : Args.filtered(OPT_trace_symbol))
    Symtab.trace(Arg->getValue());

  // Set either EntryAddr (if S is a number) or EntrySym (otherwise).
  if (!Config->Entry.empty()) {
    StringRef S = Config->Entry;
    if (S.getAsInteger(0, Config->EntryAddr))
      Config->EntrySym = Symtab.addUndefined(S);
  }

  // Initialize Config->ImageBase.
  if (auto *Arg = Args.getLastArg(OPT_image_base)) {
    StringRef S = Arg->getValue();
    if (S.getAsInteger(0, Config->ImageBase))
      error(Arg->getSpelling() + ": number expected, but got " + S);
    else if ((Config->ImageBase % Target->PageSize) != 0)
      warning(Arg->getSpelling() + ": address isn't multiple of page size");
  } else {
    Config->ImageBase = Config->Pic ? 0 : Target->DefaultImageBase;
  }

  for (std::unique_ptr<InputFile> &F : Files)
    Symtab.addFile(std::move(F));
  if (HasError)
    return; // There were duplicate symbols or incompatible files

  Symtab.scanUndefinedFlags();
  Symtab.scanShlibUndefined();
  Symtab.scanDynamicList();
  Symtab.scanVersionScript();
  Symtab.scanSymbolVersions();

  Symtab.addCombinedLtoObject();
  if (HasError)
    return;

  for (auto *Arg : Args.filtered(OPT_wrap))
    Symtab.wrap(Arg->getValue());

  // Write the result to the file.
  if (Config->GcSections)
    markLive<ELFT>();
  if (Config->ICF)
    doIcf<ELFT>();

  // MergeInputSection::splitIntoPieces needs to be called before
  // any call of MergeInputSection::getOffset. Do that.
  for (const std::unique_ptr<elf::ObjectFile<ELFT>> &F :
       Symtab.getObjectFiles())
    for (InputSectionBase<ELFT> *S : F->getSections()) {
      if (!S || S == &InputSection<ELFT>::Discarded || !S->Live)
        continue;
      if (S->Compressed)
        S->uncompress();
      if (auto *MS = dyn_cast<MergeInputSection<ELFT>>(S))
        MS->splitIntoPieces();
    }

  writeResult<ELFT>(&Symtab);
}
