/* $OpenBSD: openbsd.h,v 1.10 1999/02/06 16:31:14 espie Exp $	*/

/* common OpenBSD configuration. 
   All OpenBSD architectures include this file, which is intended as
   a repository for common defines. 

   Some defines are common to all architectures, a few of them are
   triggered by OBSD_* guards, so that we won't override architecture
   defaults by mistakes.

   OBSD_HAS_CORRECT_SPECS: 
      another mechanism provides correct specs already.
   OBSD_NO_DYNAMIC_LIBRARIES: 
      no implementation of dynamic libraries.
   OBSD_OLD_GAS: 
      older flavor of gas which needs help for PIC.
   OBSD_HAS_DECLARE_FUNCTION_NAME, OBSD_HAS_DECLARE_FUNCTION_SIZE,
   OBSD_HAS_DECLARE_OBJECT: 
      PIC support, FUNCTION_NAME/FUNCTION_SIZE are independent, whereas
      the corresponding logic for OBJECTS is necessarily coupled.

   There are also a few `default' defines such as ASM_WEAKEN_LABEL,
   intended as common ground for arch that don't provide anything suitable.
 */

/* OPENBSD_NATIVE is defined only when gcc is configured as part of
   the OpenBSD source tree, specifically through Makefile.bsd-wrapper.

   In such a case the include path can be trimmed as there is no
   distinction between system includes and gcc includes
 */
/* This configuration method, namely Makefile.bsd-wrapper and
   OPENBSD_NATIVE is NOT recommended for building cross-compilers
 */
#ifdef OPENBSD_NATIVE

#undef GCC_INCLUDE_DIR
#define GCC_INCLUDE_DIR "/usr/include"

/* The compiler is configured with ONLY the gcc/g++ standard headers */
#undef INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS			\
  {						\
    { GPLUSPLUS_INCLUDE_DIR, "G++", 1, 1 },	\
    { GCC_INCLUDE_DIR, "GCC", 0, 0 },		\
    { 0, 0, 0, 0 }				\
  }

/* Under OpenBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */
#define STANDARD_STARTFILE_PREFIX	"/usr/lib/"

#endif


/* Controlling the compilation driver 
 * ---------------------------------- */
#ifndef OBSD_HAS_CORRECT_SPECS

#ifndef OBSD_NO_DYNAMIC_LIBRARIES
#undef SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR) \
  (DEFAULT_SWITCH_TAKES_ARG(CHAR) \
   || (CHAR) == 'R')
#endif

/* CPP_SPEC appropriate for OpenBSD. We deal with -posix and -pthread.
   XXX the way threads are handling currently is not very satisfying,
   since all code must be compiled with -pthread to work.
 */
#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE} %{pthread:-D_POSIX_THREADS}"

#ifdef OBSD_OLD_GAS
/* ASM_SPEC appropriate for OpenBSD.  For some architectures, OpenBSD 
   still uses a special flavor of gas that needs to be told when generating 
   pic code. */
#undef ASM_SPEC
#define ASM_SPEC "%{fpic:-k} %{fPIC:-k -K} %|"
#else
/* Since we use gas, stdin -> - is a good idea, but we don't want to
   override native specs just for that. */
#ifndef ASM_SPEC
#define ASM_SPEC "%|"
#endif
#endif

/* LINK_SPEC appropriate for OpenBSD.  Support for GCC options 
   -static, -assert, and -nostdlib.  */
#undef LINK_SPEC
#ifdef OBSD_NO_DYNAMIC_LIBRARIES
#define LINK_SPEC \
  "%{!nostdlib:%{!r*:%{!e*:-e start}}} -dc -dp %{assert*}"
#else
#define LINK_SPEC \
  "%{!nostdlib:%{!r*:%{!e*:-e start}}} -dc -dp %{R*} %{static:-Bstatic} %{assert*}"
#endif

/* LIB_SPEC appropriate for OpenBSD.  Select the appropriate libc, 
   depending on profiling and threads.
   Basically, -lc(_r)?(_p)?, select _r for threads, and _p for p or pg.  */
#undef LIB_SPEC
#define LIB_SPEC "-lc%{pthread:_r}%{p:_p}%{!p:%{pg:_p}}"
#endif


/* Runtime target specification 
 * ---------------------------- */
/* You must redefine CPP_PREDEFINES in any arch specific file. */
#undef CPP_PREDEFINES

/* Implicit calls to library routines
 * ---------------------------------- */
/* Use memcpy and memset instead of bcopy and bzero. */
#define TARGET_MEM_FUNCTIONS

/* Miscellaneous parameters
 * ------------------------ */
/* tell libgcc2.c that OpenBSD targets support atexit. */
#define HAVE_ATEXIT

/* Controlling debugging info: dbx options 
 * --------------------------------------- */
/* Don't use the `xsTAG;' construct in DBX output; OpenBSD systems that
 * use DBX don't support it. */
#define DBX_NO_XREFS


/* Support of shared libraries, mostly imported from svr4.h through netbsd. */
/* Two differences from svr4.h:
   - we use . - _func instead of a local label,
   - we put extra spaces in expressions such as 
     .type _func , @function
     This is more readable for a human being and confuses c++filt less.  */

/* Assembler format: output and generation of labels
 * ------------------------------------------------- */
/* Define the strings used for the .type and .size directives.
   These strings generally do not vary from one system running OpenBSD
   to another, but if a given system needs to use different pseudo-op
   names for these, they may be overridden in the arch specific file. */ 

/* OpenBSD assembler is hacked to have .type & .size support even in a.out
   format object files.  Functions size are supported but not activated 
   yet (look for GRACE_PERIOD_EXPIRED in gas/config/obj-aout.c). */

#undef TYPE_ASM_OP
#undef SIZE_ASM_OP

#define TYPE_ASM_OP	".type"
#define SIZE_ASM_OP	".size"

/* The following macro defines the format used to output the second
   operand of the .type assembler directive.  */
#undef TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT	"@%s"

/* Provision if extra assembler code is needed to declare a function's result
   (taken from svr4, not needed yet actually).  */
#ifndef ASM_DECLARE_RESULT
#define ASM_DECLARE_RESULT(FILE, RESULT)
#endif

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries under OpenBSD.  These macros also have to output the starting 
   labels for the relevant functions/objects.  */

#ifndef OBSD_HAS_DECLARE_FUNCTION_NAME
/* Extra assembler code needed to declare a function properly.
   Some assemblers may also need to also have something extra said 
   about the function's return value.  We allow for that here.  */
#undef ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    fputs (" , ", FILE);						\
    fprintf (FILE, TYPE_OPERAND_FMT, "function");			\
    putc ('\n', FILE);							\
    ASM_DECLARE_RESULT (FILE, DECL_RESULT (DECL));			\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)
#endif

#ifndef OBSD_HAS_DECLARE_FUNCTION_SIZE
/* Declare the size of a function.  */
#undef ASM_DECLARE_FUNCTION_SIZE
#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do {									\
    if (!flag_inhibit_size_directive)					\
      {									\
	fprintf (FILE, "\t%s\t", SIZE_ASM_OP);				\
	assemble_name (FILE, (FNAME));					\
	fputs(" , . - ", FILE);						\
	assemble_name (FILE, (FNAME));					\
	putc ('\n', FILE);						\
      }									\
  } while (0)
#endif

#ifndef OBSD_HAS_DECLARE_OBJECT
/* Extra assembler code needed to declare an object properly.  */
#undef ASM_DECLARE_OBJECT_NAME
#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    fputs (" , ", FILE);						\
    fprintf (FILE, TYPE_OPERAND_FMT, "object");				\
    putc ('\n', FILE);							\
    size_directive_output = 0;						\
    if (!flag_inhibit_size_directive && DECL_SIZE (DECL))		\
      {									\
	size_directive_output = 1;					\
	fprintf (FILE, "\t%s\t", SIZE_ASM_OP);				\
	assemble_name (FILE, NAME);					\
	fprintf (FILE, " , %d\n", int_size_in_bytes (TREE_TYPE (DECL)));\
      }									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Output the size directive for a decl in rest_of_decl_compilation
   in the case where we did not do so before the initializer.
   Once we find the error_mark_node, we know that the value of
   size_directive_output was set
   by ASM_DECLARE_OBJECT_NAME when it was run for the same decl.  */
#undef ASM_FINISH_DECLARE_OBJECT
#define ASM_FINISH_DECLARE_OBJECT(FILE, DECL, TOP_LEVEL, AT_END)	 \
do {									 \
     char *name = XSTR (XEXP (DECL_RTL (DECL), 0), 0);			 \
     if (!flag_inhibit_size_directive && DECL_SIZE (DECL)		 \
         && ! AT_END && TOP_LEVEL					 \
	 && DECL_INITIAL (DECL) == error_mark_node			 \
	 && !size_directive_output)					 \
       {								 \
	 size_directive_output = 1;					 \
	 fprintf (FILE, "\t%s\t", SIZE_ASM_OP);			 \
	 assemble_name (FILE, name);					 \
	 fprintf (FILE, " , %d\n", int_size_in_bytes (TREE_TYPE (DECL)));\
       }								 \
   } while (0)
#endif


/* Those are `generic' ways to weaken/globalize a label. We shouldn't need
   to override a processor specific definition. Hence, #ifndef ASM_*
   In case overriding turns out to be needed, one can always #undef ASM_* 
   before including this file.  */

/* Tell the assembler that a symbol is weak.  */
/* Note: netbsd arm32 assembler needs a .globl here. An override may 
   be needed when/if we go for arm32 support. */
#ifndef ASM_WEAKEN_LABEL
#define ASM_WEAKEN_LABEL(FILE,NAME) \
  do { fputs ("\t.weak\t", FILE); assemble_name (FILE, NAME); \
       fputc ('\n', FILE); } while (0)
#endif

/* Tell the assembler that a symbol is global. */
#ifndef ASM_GLOBALIZE_LABEL
#define ASM_GLOBALIZE_LABEL(FILE,NAME) \
  do { fputs ("\t.globl\t", FILE); assemble_name (FILE, NAME); \
       fputc ('\n', FILE); } while(0)
#endif


/* Storage layout 
 * -------------- */
/* Use VTABLE_THUNKS always: we don't have to worry about binary
   compatibility with older C++ code. */
#define DEFAULT_VTABLE_THUNKS 1
