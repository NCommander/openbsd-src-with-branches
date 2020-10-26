ENTRY=_start
SCRIPT_NAME=elf
ELFSIZE=64
TEMPLATE_NAME=elf32
EXTRA_EM_FILE=alphaelf
OUTPUT_FORMAT="elf64-alpha"
TEXT_START_ADDR="0x120000000"
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x2000
NONPAGED_TEXT_START_ADDR="0x120000000"
ARCH=alpha
MACHINE=
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes

# Yes, we want duplicate .plt sections.  The linker chooses the
# appropriate one magically in alpha_after_open.
PLT=".plt          ${RELOCATING-0} : SPECIAL { *(.plt) }"
DATA_PLT=yes
TEXT_PLT=yes

#NOP=0x0000fe2f 1f04ff47		# unop; nop
NOP=0x0000fe2f		# unop
TRAP=0x00000000		# illegal?

OTHER_READONLY_SECTIONS="
  .reginfo      ${RELOCATING-0} : { *(.reginfo) }"
