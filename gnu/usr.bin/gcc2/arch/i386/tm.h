/* Configuration for an i386 running NetBSD as the target machine. 

	$Id: tm.h,v 1.9 1995/06/05 01:59:14 pk Exp $
*/

/* This is tested by i386gas.h.  */
#define YES_UNDERSCORES

#include "i386/gstabs.h"

/* Get perform_* macros to build libgcc.a.  */
#include "i386/perform.h"

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Di386 -Dunix -D__NetBSD__ -D__i386__"

/* Like the default, except no -lg.  */
#define LIB_SPEC "%{!p:%{!pg:-lc}}%{p:-lc_p}%{pg:-lc_p}"

#define STARTFILE_SPEC  \
  "%{pg:gcrt0.o%s}\
   %{!pg:%{p:mcrt0.o%s}\
         %{!p:%{static:scrt0.o%s}%{!static:crt0.o%s}}}"

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "short unsigned int"

#define WCHAR_UNSIGNED 1

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 16

/* NetBSD does have atexit.  */

#define HAVE_ATEXIT

/* Redefine this to use %eax instead of %edx.  */
#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)  \
{									\
  if (flag_pic)								\
    {									\
      fprintf (FILE, "\tcall mcount@PLT\n");				\
    }									\
  else									\
    {									\
      fprintf (FILE, "\tcall mcount\n");				\
    }									\
}

/* There are conflicting reports about whether this system uses
   a different assembler syntax.  wilson@cygnus.com says # is right.  */
#undef COMMENT_BEGIN
#define COMMENT_BEGIN "#"

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* The following macros are stolen from i386v4.h */
/* These have to be defined to get PIC code correct */

/* This is how to output an element of a case-vector that is relative.
   This is only used for PIC code.  See comments by the `casesi' insn in
   i386.md for an explanation of the expression this outputs. */

#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL) \
  fprintf (FILE, "\t.long _GLOBAL_OFFSET_TABLE_+[.-%s%d]\n", LPREFIX, VALUE)

/* Indicate that jump tables go in the text section.  This is
   necessary when compiling PIC code.  */

#define JUMP_TABLES_IN_TEXT_SECTION

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  */
#define DEFAULT_PCC_STRUCT_RETURN 0

/*
 * Some imports from svr4.h in support of shared libraries.
 */

#define HANDLE_SYSV_PRAGMA

/* Define the strings used for the special svr4 .type and .size directives.
   These strings generally do not vary from one system running svr4 to
   another, but if a given system (e.g. m88k running svr) needs to use
   different pseudo-op names for these, they may be overridden in the
   file which includes this one.  */

#define TYPE_ASM_OP	".type"
#define SIZE_ASM_OP	".size"
#define WEAK_ASM_OP	".weak"
#define SET_ASM_OP	".set"

/* The following macro defines the format used to output the second
   operand of the .type assembler directive.  Different svr4 assemblers
   expect various different forms for this operand.  The one given here
   is just a default.  You may need to override it in your machine-
   specific tm.h file (depending upon the particulars of your assembler).  */

#define TYPE_OPERAND_FMT	"@%s"

/* Write the extra assembler code needed to declare a function's result.
   Most svr4 assemblers don't require any special declaration of the
   result value, but there are exceptions.  */

#ifndef ASM_DECLARE_RESULT
#define ASM_DECLARE_RESULT(FILE, RESULT)
#endif

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries in an ELF object file under SVR4.  These macros also output
   the starting labels for the relevant functions/objects.  */

/* Write the extra assembler code needed to declare a function properly.
   Some svr4 assemblers need to also have something extra said about the
   function's return value.  We allow for that here.  */

#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "function");			\
    putc ('\n', FILE);							\
    ASM_DECLARE_RESULT (FILE, DECL_RESULT (DECL));			\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Write the extra assembler code needed to declare an object properly.  */

#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "object");				\
    putc ('\n', FILE);							\
    if (!flag_inhibit_size_directive)					\
      {									\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, NAME);					\
	fprintf (FILE, ",%d\n",  int_size_in_bytes (TREE_TYPE (decl)));	\
      }									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* This is how to declare the size of a function.  */

#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do {									\
    if (!flag_inhibit_size_directive)					\
      {									\
        char label[256];						\
	static int labelno;						\
	labelno++;							\
	ASM_GENERATE_INTERNAL_LABEL (label, "Lfe", labelno);		\
	ASM_OUTPUT_INTERNAL_LABEL (FILE, "Lfe", labelno);		\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, (FNAME));					\
        fprintf (FILE, ",");						\
	assemble_name (FILE, label);					\
        fprintf (FILE, "-");						\
	assemble_name (FILE, (FNAME));					\
	putc ('\n', FILE);						\
      }									\
  } while (0)

