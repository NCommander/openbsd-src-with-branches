
/*  A Bison parser, made from ./parse.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse java_parse
#define yylex java_lex
#define yyerror java_error
#define yylval java_lval
#define yychar java_char
#define yydebug java_debug
#define yynerrs java_nerrs
#define	PLUS_TK	258
#define	MINUS_TK	259
#define	MULT_TK	260
#define	DIV_TK	261
#define	REM_TK	262
#define	LS_TK	263
#define	SRS_TK	264
#define	ZRS_TK	265
#define	AND_TK	266
#define	XOR_TK	267
#define	OR_TK	268
#define	BOOL_AND_TK	269
#define	BOOL_OR_TK	270
#define	EQ_TK	271
#define	NEQ_TK	272
#define	GT_TK	273
#define	GTE_TK	274
#define	LT_TK	275
#define	LTE_TK	276
#define	PLUS_ASSIGN_TK	277
#define	MINUS_ASSIGN_TK	278
#define	MULT_ASSIGN_TK	279
#define	DIV_ASSIGN_TK	280
#define	REM_ASSIGN_TK	281
#define	LS_ASSIGN_TK	282
#define	SRS_ASSIGN_TK	283
#define	ZRS_ASSIGN_TK	284
#define	AND_ASSIGN_TK	285
#define	XOR_ASSIGN_TK	286
#define	OR_ASSIGN_TK	287
#define	PUBLIC_TK	288
#define	PRIVATE_TK	289
#define	PROTECTED_TK	290
#define	STATIC_TK	291
#define	FINAL_TK	292
#define	SYNCHRONIZED_TK	293
#define	VOLATILE_TK	294
#define	TRANSIENT_TK	295
#define	NATIVE_TK	296
#define	PAD_TK	297
#define	ABSTRACT_TK	298
#define	MODIFIER_TK	299
#define	DECR_TK	300
#define	INCR_TK	301
#define	DEFAULT_TK	302
#define	IF_TK	303
#define	THROW_TK	304
#define	BOOLEAN_TK	305
#define	DO_TK	306
#define	IMPLEMENTS_TK	307
#define	THROWS_TK	308
#define	BREAK_TK	309
#define	IMPORT_TK	310
#define	ELSE_TK	311
#define	INSTANCEOF_TK	312
#define	RETURN_TK	313
#define	VOID_TK	314
#define	CATCH_TK	315
#define	INTERFACE_TK	316
#define	CASE_TK	317
#define	EXTENDS_TK	318
#define	FINALLY_TK	319
#define	SUPER_TK	320
#define	WHILE_TK	321
#define	CLASS_TK	322
#define	SWITCH_TK	323
#define	CONST_TK	324
#define	TRY_TK	325
#define	FOR_TK	326
#define	NEW_TK	327
#define	CONTINUE_TK	328
#define	GOTO_TK	329
#define	PACKAGE_TK	330
#define	THIS_TK	331
#define	BYTE_TK	332
#define	SHORT_TK	333
#define	INT_TK	334
#define	LONG_TK	335
#define	CHAR_TK	336
#define	INTEGRAL_TK	337
#define	FLOAT_TK	338
#define	DOUBLE_TK	339
#define	FP_TK	340
#define	ID_TK	341
#define	REL_QM_TK	342
#define	REL_CL_TK	343
#define	NOT_TK	344
#define	NEG_TK	345
#define	ASSIGN_ANY_TK	346
#define	ASSIGN_TK	347
#define	OP_TK	348
#define	CP_TK	349
#define	OCB_TK	350
#define	CCB_TK	351
#define	OSB_TK	352
#define	CSB_TK	353
#define	SC_TK	354
#define	C_TK	355
#define	DOT_TK	356
#define	STRING_LIT_TK	357
#define	CHAR_LIT_TK	358
#define	INT_LIT_TK	359
#define	FP_LIT_TK	360
#define	TRUE_TK	361
#define	FALSE_TK	362
#define	BOOL_LIT_TK	363
#define	NULL_TK	364

#line 48 "./parse.y"

#include "config.h"
#include "system.h"
#include <dirent.h>
#include "tree.h"
#include "rtl.h"
#include "obstack.h"
#include "toplev.h"
#include "flags.h"
#include "java-tree.h"
#include "jcf.h"
#include "lex.h"
#include "parse.h"
#include "zipfile.h"
#include "convert.h"
#include "buffer.h"
#include "xref.h"
#include "except.h"

#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR '/'
#endif

/* Local function prototypes */
static char *java_accstring_lookup PROTO ((int));
static void  classitf_redefinition_error PROTO ((char *,tree, tree, tree));
static void  variable_redefinition_error PROTO ((tree, tree, tree, int));
static void  check_modifiers PROTO ((char *, int, int));
static tree  create_class PROTO ((int, tree, tree, tree));
static tree  create_interface PROTO ((int, tree, tree));
static tree  find_field PROTO ((tree, tree));
static tree lookup_field_wrapper PROTO ((tree, tree));
static int   duplicate_declaration_error_p PROTO ((tree, tree, tree));
static void  register_fields PROTO ((int, tree, tree));
static tree parser_qualified_classname PROTO ((tree));
static int  parser_check_super PROTO ((tree, tree, tree));
static int  parser_check_super_interface PROTO ((tree, tree, tree));
static void check_modifiers_consistency PROTO ((int));
static tree lookup_cl PROTO ((tree));
static tree lookup_java_method2 PROTO ((tree, tree, int));
static tree method_header PROTO ((int, tree, tree, tree));
static void fix_method_argument_names PROTO ((tree ,tree));
static tree method_declarator PROTO ((tree, tree));
static void parse_warning_context PVPROTO ((tree cl, const char *msg, ...))
  ATTRIBUTE_PRINTF_2;
static void issue_warning_error_from_context PROTO ((tree, const char *msg, va_list));
static tree parse_jdk1_1_error PROTO ((char *));
static void complete_class_report_errors PROTO ((jdep *));
static int process_imports PROTO ((void));
static void read_import_dir PROTO ((tree));
static int find_in_imports_on_demand PROTO ((tree));
static int find_in_imports PROTO ((tree));
static int check_pkg_class_access PROTO ((tree, tree));
static tree resolve_package PROTO ((tree, tree *));
static tree lookup_package_type PROTO ((char *, int));
static tree resolve_class PROTO ((tree, tree, tree));
static void declare_local_variables PROTO ((int, tree, tree));
static void source_start_java_method PROTO ((tree));
static void source_end_java_method PROTO ((void));
static void expand_start_java_method PROTO ((tree));
static tree find_name_in_single_imports PROTO ((tree));
static void check_abstract_method_header PROTO ((tree));
static tree lookup_java_interface_method2 PROTO ((tree, tree));
static tree resolve_expression_name PROTO ((tree, tree *));
static tree maybe_create_class_interface_decl PROTO ((tree, tree, tree));
static int check_class_interface_creation PROTO ((int, int, tree, 
						  tree, tree, tree));
static tree patch_method_invocation PROTO ((tree, tree, tree, 
					    int *, tree *));
static int breakdown_qualified PROTO ((tree *, tree *, tree));
static tree resolve_and_layout PROTO ((tree, tree));
static tree resolve_no_layout PROTO ((tree, tree));
static int invocation_mode PROTO ((tree, int));
static tree find_applicable_accessible_methods_list PROTO ((int, tree, 
							    tree, tree));
static void search_applicable_methods_list PROTO ((int, tree, tree, tree, 
						   tree *, tree *));
static tree find_most_specific_methods_list PROTO ((tree));
static int argument_types_convertible PROTO ((tree, tree));
static tree patch_invoke PROTO ((tree, tree, tree));
static tree lookup_method_invoke PROTO ((int, tree, tree, tree, tree));
static tree register_incomplete_type PROTO ((int, tree, tree, tree));
static tree obtain_incomplete_type PROTO ((tree));
static tree java_complete_lhs PROTO ((tree));
static tree java_complete_tree PROTO ((tree));
static void java_complete_expand_method PROTO ((tree));
static int  unresolved_type_p PROTO ((tree, tree *));
static void create_jdep_list PROTO ((struct parser_ctxt *));
static tree build_expr_block PROTO ((tree, tree));
static tree enter_block PROTO ((void));
static tree enter_a_block PROTO ((tree));
static tree exit_block PROTO ((void));
static tree lookup_name_in_blocks PROTO ((tree));
static void maybe_absorb_scoping_blocks PROTO ((void));
static tree build_method_invocation PROTO ((tree, tree));
static tree build_new_invocation PROTO ((tree, tree));
static tree build_assignment PROTO ((int, int, tree, tree));
static tree build_binop PROTO ((enum tree_code, int, tree, tree));
static int check_final_assignment PROTO ((tree ,tree));
static tree patch_assignment PROTO ((tree, tree, tree ));
static tree patch_binop PROTO ((tree, tree, tree));
static tree build_unaryop PROTO ((int, int, tree));
static tree build_incdec PROTO ((int, int, tree, int));
static tree patch_unaryop PROTO ((tree, tree));
static tree build_cast PROTO ((int, tree, tree));
static tree build_null_of_type PROTO ((tree));
static tree patch_cast PROTO ((tree, tree));
static int valid_ref_assignconv_cast_p PROTO ((tree, tree, int));
static int valid_builtin_assignconv_identity_widening_p PROTO ((tree, tree));
static int valid_cast_to_p PROTO ((tree, tree));
static int valid_method_invocation_conversion_p PROTO ((tree, tree));
static tree try_builtin_assignconv PROTO ((tree, tree, tree));
static tree try_reference_assignconv PROTO ((tree, tree));
static tree build_unresolved_array_type PROTO ((tree));
static tree build_array_from_name PROTO ((tree, tree, tree, tree *));
static tree build_array_ref PROTO ((int, tree, tree));
static tree patch_array_ref PROTO ((tree));
static tree make_qualified_name PROTO ((tree, tree, int));
static tree merge_qualified_name PROTO ((tree, tree));
static tree make_qualified_primary PROTO ((tree, tree, int));
static int resolve_qualified_expression_name PROTO ((tree, tree *, 
						     tree *, tree *));
static void qualify_ambiguous_name PROTO ((tree));
static void maybe_generate_clinit PROTO ((void));
static tree resolve_field_access PROTO ((tree, tree *, tree *));
static tree build_newarray_node PROTO ((tree, tree, int));
static tree patch_newarray PROTO ((tree));
static tree resolve_type_during_patch PROTO ((tree));
static tree build_this PROTO ((int));
static tree build_return PROTO ((int, tree));
static tree patch_return PROTO ((tree));
static tree maybe_access_field PROTO ((tree, tree, tree));
static int complete_function_arguments PROTO ((tree));
static int check_for_static_method_reference PROTO ((tree, tree, tree, tree, tree));
static int not_accessible_p PROTO ((tree, tree, int));
static void check_deprecation PROTO ((tree, tree));
static int class_in_current_package PROTO ((tree));
static tree build_if_else_statement PROTO ((int, tree, tree, tree));
static tree patch_if_else_statement PROTO ((tree));
static tree add_stmt_to_compound PROTO ((tree, tree, tree));
static tree add_stmt_to_block PROTO ((tree, tree, tree));
static tree patch_exit_expr PROTO ((tree));
static tree build_labeled_block PROTO ((int, tree));
static tree finish_labeled_statement PROTO ((tree, tree));
static tree build_bc_statement PROTO ((int, int, tree));
static tree patch_bc_statement PROTO ((tree));
static tree patch_loop_statement PROTO ((tree));
static tree build_new_loop PROTO ((tree));
static tree build_loop_body PROTO ((int, tree, int));
static tree finish_loop_body PROTO ((int, tree, tree, int));
static tree build_debugable_stmt PROTO ((int, tree));
static tree finish_for_loop PROTO ((int, tree, tree, tree));
static tree patch_switch_statement PROTO ((tree));
static tree string_constant_concatenation PROTO ((tree, tree));
static tree build_string_concatenation PROTO ((tree, tree));
static tree patch_string_cst PROTO ((tree));
static tree patch_string PROTO ((tree));
static tree build_try_statement PROTO ((int, tree, tree));
static tree build_try_finally_statement PROTO ((int, tree, tree));
static tree patch_try_statement PROTO ((tree));
static tree patch_synchronized_statement PROTO ((tree, tree));
static tree patch_throw_statement PROTO ((tree, tree));
static void check_thrown_exceptions PROTO ((int, tree));
static int check_thrown_exceptions_do PROTO ((tree));
static void purge_unchecked_exceptions PROTO ((tree));
static void check_throws_clauses PROTO ((tree, tree, tree));
static void finish_method_declaration PROTO ((tree));
static tree build_super_invocation PROTO (());
static int verify_constructor_circularity PROTO ((tree, tree));
static char *constructor_circularity_msg PROTO ((tree, tree));
static tree build_this_super_qualified_invocation PROTO ((int, tree, tree,
							  int, int));
static char *get_printable_method_name PROTO ((tree));
static tree patch_conditional_expr PROTO ((tree, tree, tree));
static void maybe_generate_finit PROTO (());
static void fix_constructors PROTO ((tree));
static int verify_constructor_super PROTO (());
static tree create_artificial_method PROTO ((tree, int, tree, tree, tree));
static void start_artificial_method_body PROTO ((tree));
static void end_artificial_method_body PROTO ((tree));
static int check_method_redefinition PROTO ((tree, tree));
static int reset_method_name PROTO ((tree));
static void java_check_regular_methods PROTO ((tree));
static void java_check_abstract_methods PROTO ((tree));
static tree maybe_build_primttype_type_ref PROTO ((tree, tree));
static void unreachable_stmt_error PROTO ((tree));
static tree find_expr_with_wfl PROTO ((tree));
static void missing_return_error PROTO ((tree));
static tree build_new_array_init PROTO ((int, tree));
static tree patch_new_array_init PROTO ((tree, tree));
static tree maybe_build_array_element_wfl PROTO ((tree));
static int array_constructor_check_entry PROTO ((tree, tree));
static char *purify_type_name PROTO ((char *));
static tree fold_constant_for_init PROTO ((tree, tree));
static tree strip_out_static_field_access_decl PROTO ((tree));
static jdeplist *reverse_jdep_list PROTO ((struct parser_ctxt *));
static void static_ref_err PROTO ((tree, tree, tree));

/* Number of error found so far. */
int java_error_count; 
/* Number of warning found so far. */
int java_warning_count;
/* Tell when not to fold, when doing xrefs */
int do_not_fold;

/* The current parser context */
struct parser_ctxt *ctxp;

/* List of things that were analyzed for which code will be generated */
static struct parser_ctxt *ctxp_for_generation = NULL;

/* binop_lookup maps token to tree_code. It is used where binary
   operations are involved and required by the parser. RDIV_EXPR
   covers both integral/floating point division. The code is changed
   once the type of both operator is worked out.  */

static enum tree_code binop_lookup[19] = 
  { 
    PLUS_EXPR, MINUS_EXPR, MULT_EXPR, RDIV_EXPR, TRUNC_MOD_EXPR,
    LSHIFT_EXPR, RSHIFT_EXPR, URSHIFT_EXPR, 
    BIT_AND_EXPR, BIT_XOR_EXPR, BIT_IOR_EXPR,
    TRUTH_ANDIF_EXPR, TRUTH_ORIF_EXPR,
    EQ_EXPR, NE_EXPR, GT_EXPR, GE_EXPR, LT_EXPR, LE_EXPR,
   };
#define BINOP_LOOKUP(VALUE) 						\
  binop_lookup [((VALUE) - PLUS_TK)%					\
		(sizeof (binop_lookup) / sizeof (binop_lookup[0]))]

/* Fake WFL used to report error message. It is initialized once if
   needed and reused with it's location information is overriden.  */
tree wfl_operator = NULL_TREE;

/* The "$L" identifier we use to create labels.  */
static tree label_id = NULL_TREE;

/* The "StringBuffer" identifier used for the String `+' operator. */
static tree wfl_string_buffer = NULL_TREE; 

/* The "append" identifier used for String `+' operator.  */
static tree wfl_append = NULL_TREE;

/* The "toString" identifier used for String `+' operator. */
static tree wfl_to_string = NULL_TREE;

/* The "java.lang" import qualified name.  */
static tree java_lang_id = NULL_TREE;

/* The "java.lang.Cloneable" qualified name.  */
static tree java_lang_cloneable = NULL_TREE;

/* Context and flag for static blocks */
static tree current_static_block = NULL_TREE;


#line 303 "./parse.y"
typedef union {
  tree node;
  int sub_token;
  struct {
    int token;
    int location;
  } operator;
  int value;
} YYSTYPE;
#line 313 "./parse.y"

#include "lex.c"
#ifndef YYDEBUG
#define YYDEBUG 1
#endif

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		777
#define	YYFLAG		-32768
#define	YYNTBASE	110

#define YYTRANSLATE(x) ((unsigned)(x) <= 364 ? yytranslate[x] : 266)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
    86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
    96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
   106,   107,   108,   109
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,     8,    10,    12,    14,    16,    18,
    20,    22,    24,    26,    28,    30,    32,    34,    38,    42,
    46,    50,    54,    56,    58,    60,    64,    66,    67,    69,
    71,    73,    76,    79,    82,    86,    88,    91,    93,    96,
   100,   103,   107,   109,   111,   115,   118,   122,   128,   133,
   139,   141,   143,   145,   147,   149,   152,   153,   161,   162,
   169,   173,   176,   180,   185,   186,   189,   193,   196,   197,
   200,   203,   205,   209,   213,   216,   220,   222,   225,   227,
   229,   231,   233,   235,   238,   240,   242,   244,   248,   253,
   255,   259,   263,   265,   269,   273,   278,   280,   284,   287,
   291,   295,   297,   299,   300,   304,   307,   311,   315,   320,
   325,   328,   332,   335,   339,   342,   346,   351,   355,   359,
   363,   365,   369,   373,   376,   380,   383,   387,   388,   391,
   394,   396,   400,   404,   406,   409,   411,   414,   418,   420,
   421,   425,   428,   432,   436,   441,   444,   448,   452,   457,
   459,   462,   467,   473,   481,   488,   490,   492,   493,   498,
   499,   505,   506,   512,   513,   520,   524,   529,   532,   536,
   539,   543,   546,   550,   552,   555,   557,   559,   561,   563,
   565,   568,   571,   574,   578,   583,   585,   589,   593,   596,
   600,   602,   604,   606,   609,   611,   613,   615,   618,   621,
   625,   627,   629,   631,   633,   635,   637,   639,   641,   643,
   645,   647,   649,   651,   653,   655,   657,   659,   661,   663,
   665,   667,   669,   671,   674,   677,   680,   683,   686,   689,
   692,   695,   699,   704,   709,   715,   720,   726,   733,   741,
   748,   750,   752,   754,   756,   758,   760,   762,   768,   771,
   775,   780,   788,   796,   797,   801,   806,   809,   813,   819,
   822,   826,   830,   835,   837,   840,   843,   845,   848,   852,
   855,   858,   862,   865,   870,   873,   876,   880,   885,   888,
   890,   898,   906,   913,   917,   923,   928,   936,   943,   946,
   949,   953,   956,   957,   959,   961,   964,   965,   967,   969,
   973,   977,   980,   984,   987,   991,   994,   998,  1001,  1005,
  1008,  1012,  1015,  1019,  1023,  1026,  1030,  1036,  1042,  1045,
  1050,  1054,  1056,  1060,  1064,  1069,  1072,  1074,  1077,  1080,
  1085,  1088,  1092,  1097,  1100,  1103,  1105,  1107,  1109,  1111,
  1115,  1117,  1119,  1121,  1123,  1127,  1131,  1135,  1139,  1143,
  1147,  1151,  1155,  1161,  1166,  1173,  1179,  1184,  1190,  1196,
  1203,  1207,  1211,  1216,  1222,  1225,  1229,  1233,  1237,  1239,
  1243,  1247,  1251,  1255,  1260,  1265,  1270,  1275,  1279,  1283,
  1285,  1288,  1292,  1296,  1299,  1302,  1306,  1310,  1314,  1318,
  1321,  1325,  1330,  1336,  1343,  1349,  1356,  1361,  1366,  1371,
  1376,  1380,  1385,  1389,  1394,  1396,  1398,  1400,  1402,  1405,
  1408,  1410,  1412,  1415,  1418,  1420,  1423,  1426,  1429,  1432,
  1435,  1438,  1440,  1443,  1446,  1448,  1451,  1454,  1460,  1465,
  1470,  1476,  1481,  1484,  1490,  1495,  1501,  1503,  1507,  1511,
  1515,  1519,  1523,  1527,  1529,  1533,  1537,  1541,  1545,  1547,
  1551,  1555,  1559,  1563,  1567,  1571,  1573,  1577,  1581,  1585,
  1589,  1593,  1597,  1601,  1605,  1609,  1613,  1615,  1619,  1623,
  1627,  1631,  1633,  1637,  1641,  1643,  1647,  1651,  1653,  1657,
  1661,  1663,  1667,  1671,  1673,  1677,  1681,  1683,  1689,  1694,
  1698,  1704,  1706,  1708,  1712,  1716,  1718,  1720,  1722,  1724,
  1726,  1728
};

static const short yyrhs[] = {   123,
     0,   104,     0,   105,     0,   108,     0,   103,     0,   102,
     0,   109,     0,   113,     0,   114,     0,    82,     0,    85,
     0,    50,     0,   115,     0,   118,     0,   119,     0,   115,
     0,   115,     0,   113,    97,    98,     0,   119,    97,    98,
     0,   118,    97,    98,     0,   113,    97,     1,     0,   118,
    97,     1,     0,   120,     0,   121,     0,   122,     0,   119,
   101,   122,     0,    86,     0,     0,   126,     0,   124,     0,
   125,     0,   126,   124,     0,   126,   125,     0,   124,   125,
     0,   126,   124,   125,     0,   127,     0,   124,   127,     0,
   130,     0,   125,   130,     0,    75,   119,    99,     0,    75,
     1,     0,    75,   119,     1,     0,   128,     0,   129,     0,
    55,   119,    99,     0,    55,     1,     0,    55,   119,     1,
     0,    55,   119,   101,     5,    99,     0,    55,   119,   101,
     1,     0,    55,   119,   101,     5,     1,     0,   132,     0,
   166,     0,    99,     0,     1,     0,    44,     0,   131,    44,
     0,     0,   131,    67,   122,   135,   136,   133,   138,     0,
     0,    67,   122,   135,   136,   134,   138,     0,   131,    67,
     1,     0,    67,     1,     0,    67,   122,     1,     0,   131,
    67,   122,     1,     0,     0,    63,   116,     0,    63,   116,
     1,     0,    63,     1,     0,     0,    52,   137,     0,    52,
     1,     0,   117,     0,   137,   100,   117,     0,   137,   100,
     1,     0,    95,    96,     0,    95,   139,    96,     0,   140,
     0,   139,   140,     0,   141,     0,   156,     0,   158,     0,
   179,     0,   142,     0,   142,    99,     0,   147,     0,   132,
     0,   166,     0,   112,   143,    99,     0,   131,   112,   143,
    99,     0,   144,     0,   143,   100,   144,     0,   143,   100,
     1,     0,   145,     0,   145,    92,   146,     0,   145,    92,
     1,     0,   145,    92,   146,     1,     0,   122,     0,   145,
    97,    98,     0,   122,     1,     0,   145,    97,     1,     0,
   145,    98,     1,     0,   264,     0,   177,     0,     0,   149,
   148,   155,     0,   149,     1,     0,   112,   150,   153,     0,
    59,   150,   153,     0,   131,   112,   150,   153,     0,   131,
    59,   150,   153,     0,   112,     1,     0,   131,   112,     1,
     0,    59,     1,     0,   131,    59,     1,     0,   131,     1,
     0,   122,    93,    94,     0,   122,    93,   151,    94,     0,
   150,    97,    98,     0,   122,    93,     1,     0,   150,    97,
     1,     0,   152,     0,   151,   100,   152,     0,   151,   100,
     1,     0,   112,   145,     0,   131,   112,   145,     0,   112,
     1,     0,   131,   112,     1,     0,     0,    53,   154,     0,
    53,     1,     0,   116,     0,   154,   100,   116,     0,   154,
   100,     1,     0,   179,     0,   179,    99,     0,    99,     0,
   157,   179,     0,   157,   179,    99,     0,    44,     0,     0,
   160,   159,   162,     0,   161,   153,     0,   131,   161,   153,
     0,   120,    93,    94,     0,   120,    93,   151,    94,     0,
   180,   163,     0,   180,   164,   163,     0,   180,   182,   163,
     0,   180,   164,   182,   163,     0,   181,     0,   181,    99,
     0,   165,    93,    94,    99,     0,   165,    93,   233,    94,
    99,     0,   119,   101,    65,    93,   233,    94,    99,     0,
   119,   101,    65,    93,    94,    99,     0,    76,     0,    65,
     0,     0,    61,   122,   167,   172,     0,     0,   131,    61,
   122,   168,   172,     0,     0,    61,   122,   171,   169,   172,
     0,     0,   131,    61,   122,   171,   170,   172,     0,    61,
   122,     1,     0,   131,    61,   122,     1,     0,    63,   117,
     0,   171,   100,   117,     0,    63,     1,     0,   171,   100,
     1,     0,    95,    96,     0,    95,   173,    96,     0,   174,
     0,   173,   174,     0,   175,     0,   176,     0,   132,     0,
   166,     0,   142,     0,   149,    99,     0,   149,     1,     0,
    95,    96,     0,    95,   178,    96,     0,    95,   178,   100,
    96,     0,   146,     0,   178,   100,   146,     0,   178,   100,
     1,     0,    95,    96,     0,   180,   182,   181,     0,    95,
     0,    96,     0,   183,     0,   182,   183,     0,   184,     0,
   186,     0,   132,     0,   185,    99,     0,   112,   143,     0,
   131,   112,   143,     0,   188,     0,   191,     0,   195,     0,
   196,     0,   207,     0,   211,     0,   188,     0,   192,     0,
   197,     0,   208,     0,   212,     0,   179,     0,   189,     0,
   193,     0,   198,     0,   210,     0,   218,     0,   219,     0,
   220,     0,   222,     0,   221,     0,   224,     0,    99,     0,
   122,    88,     0,   190,   186,     0,   122,     1,     0,   190,
   187,     0,   194,    99,     0,     1,    99,     0,     1,    95,
     0,     1,    96,     0,   165,    93,     1,     0,   165,    93,
    94,     1,     0,   165,    93,   233,     1,     0,   165,    93,
   233,    94,     1,     0,   119,   101,    65,     1,     0,   119,
   101,    65,    93,     1,     0,   119,   101,    65,    93,   233,
     1,     0,   119,   101,    65,    93,   233,    94,     1,     0,
   119,   101,    65,    93,    94,     1,     0,   261,     0,   245,
     0,   246,     0,   242,     0,   243,     0,   239,     0,   231,
     0,    48,    93,   264,    94,   186,     0,    48,     1,     0,
    48,    93,     1,     0,    48,    93,   264,     1,     0,    48,
    93,   264,    94,   187,    56,   186,     0,    48,    93,   264,
    94,   187,    56,   187,     0,     0,   200,   199,   201,     0,
    68,    93,   264,    94,     0,    68,     1,     0,    68,    93,
     1,     0,    68,    93,   264,    94,     1,     0,    95,    96,
     0,    95,   204,    96,     0,    95,   202,    96,     0,    95,
   202,   204,    96,     0,   203,     0,   202,   203,     0,   204,
   182,     0,   205,     0,   204,   205,     0,    62,   265,    88,
     0,    47,    88,     0,    62,     1,     0,    62,   265,     1,
     0,    47,     1,     0,    66,    93,   264,    94,     0,   206,
   186,     0,    66,     1,     0,    66,    93,     1,     0,    66,
    93,   264,     1,     0,   206,   187,     0,    51,     0,   209,
   186,    66,    93,   264,    94,    99,     0,   214,    99,   264,
    99,   216,    94,   186,     0,   214,    99,    99,   216,    94,
   186,     0,   214,    99,     1,     0,   214,    99,   264,    99,
     1,     0,   214,    99,    99,     1,     0,   214,    99,   264,
    99,   216,    94,   187,     0,   214,    99,    99,   216,    94,
   187,     0,    71,    93,     0,    71,     1,     0,    71,    93,
     1,     0,   213,   215,     0,     0,   217,     0,   185,     0,
   217,     1,     0,     0,   217,     0,   194,     0,   217,   100,
   194,     0,   217,   100,     1,     0,    54,    99,     0,    54,
   122,    99,     0,    54,     1,     0,    54,   122,     1,     0,
    73,    99,     0,    73,   122,    99,     0,    73,     1,     0,
    73,   122,     1,     0,    58,    99,     0,    58,   264,    99,
     0,    58,     1,     0,    58,   264,     1,     0,    49,   264,
    99,     0,    49,     1,     0,    49,   264,     1,     0,   223,
    93,   264,    94,   179,     0,   223,    93,   264,    94,     1,
     0,   223,     1,     0,   223,    93,     1,    94,     0,   223,
    93,     1,     0,    44,     0,    70,   179,   225,     0,    70,
   179,   228,     0,    70,   179,   225,   228,     0,    70,     1,
     0,   226,     0,   225,   226,     0,   227,   179,     0,    60,
    93,   152,    94,     0,    60,     1,     0,    60,    93,     1,
     0,    60,    93,     1,    94,     0,    64,   179,     0,    64,
     1,     0,   230,     0,   234,     0,   111,     0,    76,     0,
    93,   264,    94,     0,   231,     0,   238,     0,   239,     0,
   240,     0,   119,   101,    67,     0,   113,   101,    67,     0,
    59,   101,    67,     0,   119,   101,    76,     0,    93,   264,
     1,     0,   119,   101,     1,     0,   113,   101,     1,     0,
    59,   101,     1,     0,    72,   116,    93,   233,    94,     0,
    72,   116,    93,    94,     0,    72,   116,    93,   233,    94,
   138,     0,    72,   116,    93,    94,   138,     0,   232,   122,
    93,    94,     0,   232,   122,    93,    94,   138,     0,   232,
   122,    93,   233,    94,     0,   232,   122,    93,   233,    94,
   138,     0,    72,     1,    99,     0,    72,   116,     1,     0,
    72,   116,    93,     1,     0,    72,   116,    93,   233,     1,
     0,   232,     1,     0,   232,   122,     1,     0,   119,   101,
    72,     0,   229,   101,    72,     0,   264,     0,   233,   100,
   264,     0,   233,   100,     1,     0,    72,   113,   235,     0,
    72,   115,   235,     0,    72,   113,   235,   237,     0,    72,
   115,   235,   237,     0,    72,   115,   237,   177,     0,    72,
   113,   237,   177,     0,    72,     1,    98,     0,    72,     1,
    97,     0,   236,     0,   235,   236,     0,    97,   264,    98,
     0,    97,   264,     1,     0,    97,     1,     0,    97,    98,
     0,   237,    97,    98,     0,   237,    97,     1,     0,   229,
   101,   122,     0,    65,   101,   122,     0,    65,     1,     0,
   119,    93,    94,     0,   119,    93,   233,    94,     0,   229,
   101,   122,    93,    94,     0,   229,   101,   122,    93,   233,
    94,     0,    65,   101,   122,    93,    94,     0,    65,   101,
   122,    93,   233,    94,     0,    65,   101,     1,    94,     0,
    65,   101,     1,   101,     0,   119,    97,   264,    98,     0,
   230,    97,   264,    98,     0,   119,    97,     1,     0,   119,
    97,   264,     1,     0,   230,    97,     1,     0,   230,    97,
   264,     1,     0,   229,     0,   119,     0,   242,     0,   243,
     0,   241,    46,     0,   241,    45,     0,   245,     0,   246,
     0,     3,   244,     0,     4,   244,     0,   247,     0,     3,
     1,     0,     4,     1,     0,    46,   244,     0,    46,     1,
     0,    45,   244,     0,    45,     1,     0,   241,     0,    89,
   244,     0,    90,   244,     0,   248,     0,    89,     1,     0,
    90,     1,     0,    93,   113,   237,    94,   244,     0,    93,
   113,    94,   244,     0,    93,   264,    94,   247,     0,    93,
   119,   237,    94,   247,     0,    93,   113,    97,     1,     0,
    93,     1,     0,    93,   113,   237,    94,     1,     0,    93,
   113,    94,     1,     0,    93,   119,   237,    94,     1,     0,
   244,     0,   249,     5,   244,     0,   249,     6,   244,     0,
   249,     7,   244,     0,   249,     5,     1,     0,   249,     6,
     1,     0,   249,     7,     1,     0,   249,     0,   250,     3,
   249,     0,   250,     4,   249,     0,   250,     3,     1,     0,
   250,     4,     1,     0,   250,     0,   251,     8,   250,     0,
   251,     9,   250,     0,   251,    10,   250,     0,   251,     8,
     1,     0,   251,     9,     1,     0,   251,    10,     1,     0,
   251,     0,   252,    20,   251,     0,   252,    18,   251,     0,
   252,    21,   251,     0,   252,    19,   251,     0,   252,    57,
   114,     0,   252,    20,     1,     0,   252,    18,     1,     0,
   252,    21,     1,     0,   252,    19,     1,     0,   252,    57,
     1,     0,   252,     0,   253,    16,   252,     0,   253,    17,
   252,     0,   253,    16,     1,     0,   253,    17,     1,     0,
   253,     0,   254,    11,   253,     0,   254,    11,     1,     0,
   254,     0,   255,    12,   254,     0,   255,    12,     1,     0,
   255,     0,   256,    13,   255,     0,   256,    13,     1,     0,
   256,     0,   257,    14,   256,     0,   257,    14,     1,     0,
   257,     0,   258,    15,   257,     0,   258,    15,     1,     0,
   258,     0,   258,    87,   264,    88,   259,     0,   258,    87,
    88,     1,     0,   258,    87,     1,     0,   258,    87,   264,
    88,     1,     0,   259,     0,   261,     0,   262,   263,   260,
     0,   262,   263,     1,     0,   119,     0,   238,     0,   240,
     0,    91,     0,    92,     0,   260,     0,   264,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   457,   463,   465,   466,   467,   468,   469,   473,   475,   478,
   480,   481,   484,   486,   489,   493,   497,   501,   507,   509,
   511,   513,   518,   520,   523,   527,   532,   537,   539,   540,
   541,   542,   543,   544,   545,   548,   553,   559,   561,   564,
   567,   569,   573,   575,   578,   605,   607,   611,   624,   626,
   630,   637,   642,   644,   654,   659,   674,   678,   681,   684,
   687,   689,   691,   696,   700,   702,   704,   706,   710,   712,
   714,   721,   727,   732,   736,   745,   755,   757,   760,   762,
   763,   764,   768,   770,   772,   773,   775,   780,   783,   793,
   796,   798,   802,   805,   812,   818,   826,   828,   830,   832,
   834,   838,   840,   844,   851,   852,   856,   859,   861,   863,
   865,   867,   869,   871,   873,   880,   883,   885,   894,   896,
   900,   905,   910,   914,   919,   924,   926,   933,   935,   937,
   941,   944,   946,   950,   952,   953,   958,   964,   971,   979,
   986,   989,   992,   996,   999,  1003,  1012,  1014,  1016,  1020,
  1022,  1025,  1032,  1040,  1042,  1046,  1053,  1063,  1067,  1070,
  1073,  1076,  1079,  1082,  1085,  1088,  1090,  1094,  1100,  1105,
  1107,  1111,  1114,  1118,  1120,  1123,  1125,  1126,  1128,  1132,
  1136,  1142,  1147,  1150,  1152,  1156,  1162,  1166,  1171,  1180,
  1184,  1189,  1201,  1203,  1206,  1208,  1210,  1214,  1218,  1221,
  1225,  1227,  1228,  1229,  1230,  1231,  1235,  1237,  1238,  1239,
  1240,  1244,  1246,  1247,  1248,  1249,  1250,  1251,  1252,  1253,
  1254,  1255,  1258,  1263,  1274,  1277,  1281,  1288,  1298,  1304,
  1310,  1316,  1318,  1324,  1326,  1332,  1334,  1336,  1338,  1340,
  1344,  1346,  1347,  1348,  1349,  1350,  1351,  1354,  1360,  1362,
  1364,  1368,  1373,  1378,  1384,  1394,  1400,  1402,  1404,  1411,
  1414,  1416,  1418,  1422,  1424,  1427,  1431,  1433,  1436,  1443,
  1449,  1451,  1453,  1457,  1465,  1468,  1470,  1472,  1476,  1481,
  1490,  1495,  1498,  1505,  1507,  1509,  1513,  1516,  1525,  1532,
  1534,  1538,  1551,  1553,  1559,  1565,  1569,  1571,  1575,  1578,
  1580,  1584,  1587,  1589,  1591,  1595,  1598,  1600,  1602,  1606,
  1609,  1611,  1613,  1617,  1623,  1625,  1629,  1636,  1638,  1640,
  1642,  1646,  1654,  1657,  1659,  1664,  1668,  1670,  1677,  1685,
  1702,  1704,  1706,  1710,  1713,  1718,  1720,  1723,  1725,  1727,
  1729,  1730,  1731,  1732,  1736,  1738,  1740,  1745,  1747,  1749,
  1751,  1753,  1757,  1760,  1765,  1767,  1772,  1773,  1774,  1775,
  1776,  1778,  1780,  1782,  1784,  1786,  1790,  1792,  1795,  1801,
  1806,  1810,  1813,  1815,  1817,  1821,  1823,  1825,  1827,  1831,
  1834,  1838,  1844,  1846,  1854,  1881,  1883,  1887,  1892,  1899,
  1903,  1906,  1908,  1919,  1930,  1935,  1944,  1946,  1950,  1953,
  1955,  1960,  1965,  1970,  1977,  1979,  1980,  1981,  1984,  1989,
  1994,  1996,  1997,  1999,  2001,  2002,  2004,  2008,  2011,  2015,
  2018,  2022,  2024,  2026,  2028,  2029,  2031,  2035,  2044,  2046,
  2048,  2061,  2063,  2069,  2071,  2073,  2077,  2079,  2084,  2089,
  2094,  2096,  2098,  2102,  2104,  2109,  2114,  2116,  2120,  2122,
  2127,  2132,  2137,  2139,  2141,  2145,  2147,  2152,  2157,  2162,
  2167,  2169,  2171,  2173,  2175,  2177,  2181,  2183,  2188,  2193,
  2195,  2199,  2201,  2206,  2210,  2212,  2217,  2221,  2223,  2228,
  2232,  2234,  2239,  2243,  2245,  2250,  2254,  2256,  2261,  2267,
  2269,  2273,  2275,  2278,  2281,  2289,  2291,  2292,  2295,  2297,
  2300,  2304
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","PLUS_TK",
"MINUS_TK","MULT_TK","DIV_TK","REM_TK","LS_TK","SRS_TK","ZRS_TK","AND_TK","XOR_TK",
"OR_TK","BOOL_AND_TK","BOOL_OR_TK","EQ_TK","NEQ_TK","GT_TK","GTE_TK","LT_TK",
"LTE_TK","PLUS_ASSIGN_TK","MINUS_ASSIGN_TK","MULT_ASSIGN_TK","DIV_ASSIGN_TK",
"REM_ASSIGN_TK","LS_ASSIGN_TK","SRS_ASSIGN_TK","ZRS_ASSIGN_TK","AND_ASSIGN_TK",
"XOR_ASSIGN_TK","OR_ASSIGN_TK","PUBLIC_TK","PRIVATE_TK","PROTECTED_TK","STATIC_TK",
"FINAL_TK","SYNCHRONIZED_TK","VOLATILE_TK","TRANSIENT_TK","NATIVE_TK","PAD_TK",
"ABSTRACT_TK","MODIFIER_TK","DECR_TK","INCR_TK","DEFAULT_TK","IF_TK","THROW_TK",
"BOOLEAN_TK","DO_TK","IMPLEMENTS_TK","THROWS_TK","BREAK_TK","IMPORT_TK","ELSE_TK",
"INSTANCEOF_TK","RETURN_TK","VOID_TK","CATCH_TK","INTERFACE_TK","CASE_TK","EXTENDS_TK",
"FINALLY_TK","SUPER_TK","WHILE_TK","CLASS_TK","SWITCH_TK","CONST_TK","TRY_TK",
"FOR_TK","NEW_TK","CONTINUE_TK","GOTO_TK","PACKAGE_TK","THIS_TK","BYTE_TK","SHORT_TK",
"INT_TK","LONG_TK","CHAR_TK","INTEGRAL_TK","FLOAT_TK","DOUBLE_TK","FP_TK","ID_TK",
"REL_QM_TK","REL_CL_TK","NOT_TK","NEG_TK","ASSIGN_ANY_TK","ASSIGN_TK","OP_TK",
"CP_TK","OCB_TK","CCB_TK","OSB_TK","CSB_TK","SC_TK","C_TK","DOT_TK","STRING_LIT_TK",
"CHAR_LIT_TK","INT_LIT_TK","FP_LIT_TK","TRUE_TK","FALSE_TK","BOOL_LIT_TK","NULL_TK",
"goal","literal","type","primitive_type","reference_type","class_or_interface_type",
"class_type","interface_type","array_type","name","simple_name","qualified_name",
"identifier","compilation_unit","import_declarations","type_declarations","package_declaration",
"import_declaration","single_type_import_declaration","type_import_on_demand_declaration",
"type_declaration","modifiers","class_declaration","@1","@2","super","interfaces",
"interface_type_list","class_body","class_body_declarations","class_body_declaration",
"class_member_declaration","field_declaration","variable_declarators","variable_declarator",
"variable_declarator_id","variable_initializer","method_declaration","@3","method_header",
"method_declarator","formal_parameter_list","formal_parameter","throws","class_type_list",
"method_body","static_initializer","static","constructor_declaration","@4","constructor_header",
"constructor_declarator","constructor_body","constructor_block_end","explicit_constructor_invocation",
"this_or_super","interface_declaration","@5","@6","@7","@8","extends_interfaces",
"interface_body","interface_member_declarations","interface_member_declaration",
"constant_declaration","abstract_method_declaration","array_initializer","variable_initializers",
"block","block_begin","block_end","block_statements","block_statement","local_variable_declaration_statement",
"local_variable_declaration","statement","statement_nsi","statement_without_trailing_substatement",
"empty_statement","label_decl","labeled_statement","labeled_statement_nsi","expression_statement",
"statement_expression","if_then_statement","if_then_else_statement","if_then_else_statement_nsi",
"switch_statement","@9","switch_expression","switch_block","switch_block_statement_groups",
"switch_block_statement_group","switch_labels","switch_label","while_expression",
"while_statement","while_statement_nsi","do_statement_begin","do_statement",
"for_statement","for_statement_nsi","for_header","for_begin","for_init","for_update",
"statement_expression_list","break_statement","continue_statement","return_statement",
"throw_statement","synchronized_statement","synchronized","try_statement","catches",
"catch_clause","catch_clause_parameter","finally","primary","primary_no_new_array",
"class_instance_creation_expression","something_dot_new","argument_list","array_creation_expression",
"dim_exprs","dim_expr","dims","field_access","method_invocation","array_access",
"postfix_expression","post_increment_expression","post_decrement_expression",
"unary_expression","pre_increment_expression","pre_decrement_expression","unary_expression_not_plus_minus",
"cast_expression","multiplicative_expression","additive_expression","shift_expression",
"relational_expression","equality_expression","and_expression","exclusive_or_expression",
"inclusive_or_expression","conditional_and_expression","conditional_or_expression",
"conditional_expression","assignment_expression","assignment","left_hand_side",
"assignment_operator","expression","constant_expression", NULL
};
#endif

static const short yyr1[] = {     0,
   110,   111,   111,   111,   111,   111,   111,   112,   112,   113,
   113,   113,   114,   114,   115,   116,   117,   118,   118,   118,
   118,   118,   119,   119,   120,   121,   122,   123,   123,   123,
   123,   123,   123,   123,   123,   124,   124,   125,   125,   126,
   126,   126,   127,   127,   128,   128,   128,   129,   129,   129,
   130,   130,   130,   130,   131,   131,   133,   132,   134,   132,
   132,   132,   132,   132,   135,   135,   135,   135,   136,   136,
   136,   137,   137,   137,   138,   138,   139,   139,   140,   140,
   140,   140,   141,   141,   141,   141,   141,   142,   142,   143,
   143,   143,   144,   144,   144,   144,   145,   145,   145,   145,
   145,   146,   146,   148,   147,   147,   149,   149,   149,   149,
   149,   149,   149,   149,   149,   150,   150,   150,   150,   150,
   151,   151,   151,   152,   152,   152,   152,   153,   153,   153,
   154,   154,   154,   155,   155,   155,   156,   156,   157,   159,
   158,   160,   160,   161,   161,   162,   162,   162,   162,   163,
   163,   164,   164,   164,   164,   165,   165,   167,   166,   168,
   166,   169,   166,   170,   166,   166,   166,   171,   171,   171,
   171,   172,   172,   173,   173,   174,   174,   174,   174,   175,
   176,   176,   177,   177,   177,   178,   178,   178,   179,   179,
   180,   181,   182,   182,   183,   183,   183,   184,   185,   185,
   186,   186,   186,   186,   186,   186,   187,   187,   187,   187,
   187,   188,   188,   188,   188,   188,   188,   188,   188,   188,
   188,   188,   189,   190,   191,   191,   192,   193,   193,   193,
   193,   193,   193,   193,   193,   193,   193,   193,   193,   193,
   194,   194,   194,   194,   194,   194,   194,   195,   195,   195,
   195,   196,   197,   199,   198,   200,   200,   200,   200,   201,
   201,   201,   201,   202,   202,   203,   204,   204,   205,   205,
   205,   205,   205,   206,   207,   207,   207,   207,   208,   209,
   210,   211,   211,   211,   211,   211,   212,   212,   213,   213,
   213,   214,   215,   215,   215,   215,   216,   216,   217,   217,
   217,   218,   218,   218,   218,   219,   219,   219,   219,   220,
   220,   220,   220,   221,   221,   221,   222,   222,   222,   222,
   222,   223,   224,   224,   224,   224,   225,   225,   226,   227,
   227,   227,   227,   228,   228,   229,   229,   230,   230,   230,
   230,   230,   230,   230,   230,   230,   230,   230,   230,   230,
   230,   230,   231,   231,   231,   231,   231,   231,   231,   231,
   231,   231,   231,   231,   231,   231,   232,   232,   233,   233,
   233,   234,   234,   234,   234,   234,   234,   234,   234,   235,
   235,   236,   236,   236,   237,   237,   237,   238,   238,   238,
   239,   239,   239,   239,   239,   239,   239,   239,   240,   240,
   240,   240,   240,   240,   241,   241,   241,   241,   242,   243,
   244,   244,   244,   244,   244,   244,   244,   245,   245,   246,
   246,   247,   247,   247,   247,   247,   247,   248,   248,   248,
   248,   248,   248,   248,   248,   248,   249,   249,   249,   249,
   249,   249,   249,   250,   250,   250,   250,   250,   251,   251,
   251,   251,   251,   251,   251,   252,   252,   252,   252,   252,
   252,   252,   252,   252,   252,   252,   253,   253,   253,   253,
   253,   254,   254,   254,   255,   255,   255,   256,   256,   256,
   257,   257,   257,   258,   258,   258,   259,   259,   259,   259,
   259,   260,   260,   261,   261,   262,   262,   262,   263,   263,
   264,   265
};

static const short yyr2[] = {     0,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     3,     3,     3,
     3,     3,     1,     1,     1,     3,     1,     0,     1,     1,
     1,     2,     2,     2,     3,     1,     2,     1,     2,     3,
     2,     3,     1,     1,     3,     2,     3,     5,     4,     5,
     1,     1,     1,     1,     1,     2,     0,     7,     0,     6,
     3,     2,     3,     4,     0,     2,     3,     2,     0,     2,
     2,     1,     3,     3,     2,     3,     1,     2,     1,     1,
     1,     1,     1,     2,     1,     1,     1,     3,     4,     1,
     3,     3,     1,     3,     3,     4,     1,     3,     2,     3,
     3,     1,     1,     0,     3,     2,     3,     3,     4,     4,
     2,     3,     2,     3,     2,     3,     4,     3,     3,     3,
     1,     3,     3,     2,     3,     2,     3,     0,     2,     2,
     1,     3,     3,     1,     2,     1,     2,     3,     1,     0,
     3,     2,     3,     3,     4,     2,     3,     3,     4,     1,
     2,     4,     5,     7,     6,     1,     1,     0,     4,     0,
     5,     0,     5,     0,     6,     3,     4,     2,     3,     2,
     3,     2,     3,     1,     2,     1,     1,     1,     1,     1,
     2,     2,     2,     3,     4,     1,     3,     3,     2,     3,
     1,     1,     1,     2,     1,     1,     1,     2,     2,     3,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     2,     2,     2,     2,     2,     2,     2,
     2,     3,     4,     4,     5,     4,     5,     6,     7,     6,
     1,     1,     1,     1,     1,     1,     1,     5,     2,     3,
     4,     7,     7,     0,     3,     4,     2,     3,     5,     2,
     3,     3,     4,     1,     2,     2,     1,     2,     3,     2,
     2,     3,     2,     4,     2,     2,     3,     4,     2,     1,
     7,     7,     6,     3,     5,     4,     7,     6,     2,     2,
     3,     2,     0,     1,     1,     2,     0,     1,     1,     3,
     3,     2,     3,     2,     3,     2,     3,     2,     3,     2,
     3,     2,     3,     3,     2,     3,     5,     5,     2,     4,
     3,     1,     3,     3,     4,     2,     1,     2,     2,     4,
     2,     3,     4,     2,     2,     1,     1,     1,     1,     3,
     1,     1,     1,     1,     3,     3,     3,     3,     3,     3,
     3,     3,     5,     4,     6,     5,     4,     5,     5,     6,
     3,     3,     4,     5,     2,     3,     3,     3,     1,     3,
     3,     3,     3,     4,     4,     4,     4,     3,     3,     1,
     2,     3,     3,     2,     2,     3,     3,     3,     3,     2,
     3,     4,     5,     6,     5,     6,     4,     4,     4,     4,
     3,     4,     3,     4,     1,     1,     1,     1,     2,     2,
     1,     1,     2,     2,     1,     2,     2,     2,     2,     2,
     2,     1,     2,     2,     1,     2,     2,     5,     4,     4,
     5,     4,     2,     5,     4,     5,     1,     3,     3,     3,
     3,     3,     3,     1,     3,     3,     3,     3,     1,     3,
     3,     3,     3,     3,     3,     1,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     1,     3,     3,     3,
     3,     1,     3,     3,     1,     3,     3,     1,     3,     3,
     1,     3,     3,     1,     3,     3,     1,     5,     4,     3,
     5,     1,     1,     3,     3,     1,     1,     1,     1,     1,
     1,     1
};

static const short yydefact[] = {     0,
    54,    55,     0,     0,     0,     0,    53,     1,     0,     0,
     0,    36,    43,    44,    38,     0,    51,    52,    46,    27,
     0,    23,    24,    25,     0,    62,     0,    41,     0,     0,
    37,    39,     0,     0,    56,     0,     0,    47,    45,     0,
   166,     0,     0,   162,    63,     0,    69,    42,    40,     0,
     0,     0,    61,     0,    49,     0,    26,   170,    17,   168,
    15,     0,   159,     0,     0,    68,    16,     0,     0,    59,
   167,     0,   164,    64,    69,    50,    48,    12,     0,    10,
    11,   172,     0,     8,     9,    13,    14,    15,     0,   178,
   180,     0,   179,     0,   174,   176,   177,   171,   169,   163,
    67,    71,    72,    70,     0,   161,     0,    57,   113,     0,
   128,   111,     0,     0,    90,    93,   128,     0,     0,     0,
   115,     0,     0,   182,   181,   173,   175,     0,     0,    60,
   165,     0,     0,     0,     0,   108,    99,    88,     0,     0,
     0,     0,   107,    21,    18,    22,    20,    19,   114,   128,
   112,     0,   128,    74,    73,    55,   191,    75,    23,     0,
    86,     0,    77,    79,    83,    85,     0,    80,     0,    81,
   140,   128,    87,    82,     0,    58,   119,   116,     0,     0,
     0,   121,   130,   131,   129,   120,   118,    92,     0,    91,
    95,     0,     0,     0,     0,     0,     0,     0,   339,     0,
     0,     0,     0,     6,     5,     2,     3,     4,     7,   338,
     0,   406,     0,   103,   405,   336,   341,     0,   337,   342,
   343,   344,   422,   407,   408,   437,   411,   412,   415,   425,
   444,   449,   456,   467,   472,   475,   478,   481,   484,   487,
   492,   501,   493,     0,   102,   100,    98,   101,   110,    89,
   109,   189,     0,   128,    76,    78,    84,   106,     0,   137,
     0,   142,     0,    55,     0,     0,   280,     0,     0,     0,
     0,     0,     0,     0,     0,   339,     0,   223,     0,     8,
   406,     0,     0,   197,     0,   212,     0,   193,   195,     0,
   196,   201,   213,     0,   202,   214,     0,   203,   204,   215,
   254,     0,   205,     0,   216,   206,   293,     0,   217,   218,
   219,   221,   220,     0,   222,   247,   246,     0,   244,   245,
   242,   243,   241,   126,   124,     0,   117,     0,     0,   416,
   406,   342,   344,   413,   417,   414,   421,   420,   419,   418,
     0,   390,     0,     0,     0,    16,     0,   426,   423,   427,
   424,   433,     0,   406,     0,   183,   186,     0,     0,     0,
     0,     0,    96,     0,     0,   365,     0,   410,   409,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   499,   500,     0,   144,     0,   143,   136,   105,   134,   138,
   191,   141,     0,   230,   231,   229,   249,     0,   315,     0,
   304,   302,     0,   312,   310,     0,   276,     0,   257,     0,
   326,     0,   290,     0,   308,   306,     0,     0,   199,     0,
     0,   226,   224,     0,     0,   192,   190,   194,   198,   322,
   406,   225,   228,     0,   275,     0,   406,     0,   295,   299,
   292,     0,     0,   319,     0,   127,   125,   123,   122,   133,
   132,   352,   347,     0,   389,   379,   378,   361,     0,   372,
   380,     0,   373,     0,   362,     0,     0,     0,     0,     0,
     0,   349,   340,   184,     0,   351,   346,   391,     0,   369,
   401,     0,   350,   345,   367,   348,   368,   388,   403,     0,
   366,     0,   441,   438,   442,   439,   443,   440,   447,   445,
   448,   446,   453,   450,   454,   451,   455,   452,   463,   458,
   465,   460,   462,   457,   464,   459,   466,     0,   461,   470,
   468,   471,   469,   474,   473,   477,   476,   480,   479,   483,
   482,   486,   485,   490,     0,     0,   495,   494,   145,   135,
   406,   146,     0,     0,   150,     0,   250,     0,   316,   314,
   305,   303,   313,   311,   277,     0,   258,     0,     0,     0,
   323,   327,     0,   324,   291,   309,   307,   340,     0,   200,
   232,     0,     0,     0,   255,     0,   296,     0,   284,     0,
     0,   321,     0,   397,   398,     0,   384,   385,     0,   381,
   374,     0,   377,   375,   376,   363,   354,     0,   435,   429,
   432,     0,     0,   430,   188,   185,   187,   392,     0,   402,
   399,     0,   404,   400,   357,     0,   489,     0,     0,   147,
     0,     0,   151,   148,   251,     0,   278,   274,     0,   331,
     0,   335,   334,   328,   325,   329,   236,     0,   233,   234,
     0,     0,     0,   260,     0,   264,     0,   267,     0,   301,
   300,   286,     0,   298,     0,   320,     0,   395,     0,   383,
   382,   387,   386,   356,   364,   353,   434,   428,   436,   431,
   371,   370,   393,     0,   358,   359,   491,   488,     0,   149,
     0,     0,     0,   248,     0,   201,     0,   208,   209,     0,
   210,   211,     0,   259,   332,     0,   237,     0,     0,   235,
   273,   270,   271,   502,     0,   262,   265,     0,   261,     0,
   268,     0,     0,   285,     0,   318,   317,   396,   355,   394,
   360,     0,   152,     0,     0,     0,   227,   279,     0,   333,
   330,   240,   238,     0,   272,   269,   263,     0,   283,     0,
     0,     0,   153,     0,   252,     0,     0,   239,   281,   282,
   155,     0,     0,     0,     0,   154,     0,     0,     0,     0,
   288,     0,   253,   287,     0,     0,     0
};

static const short yydefgoto[] = {   775,
   210,   279,   211,    85,    86,    68,    60,    87,   212,    22,
    23,    24,     8,     9,    10,    11,    12,    13,    14,    15,
   283,   284,   132,   105,    47,    70,   104,   130,   162,   163,
   164,    91,   114,   115,   116,   213,   166,   259,    92,   111,
   181,   182,   136,   185,   398,   168,   169,   170,   261,   171,
   172,   402,   552,   553,   285,    18,    43,    72,    65,   107,
    44,    63,    94,    95,    96,    97,   214,   358,   286,   175,
   555,   720,   288,   289,   290,   291,   695,   292,   293,   294,
   295,   698,   296,   297,   298,   299,   699,   300,   444,   301,
   585,   655,   656,   657,   658,   302,   303,   701,   304,   305,
   306,   702,   307,   308,   451,   663,   664,   309,   310,   311,
   312,   313,   314,   315,   571,   572,   573,   574,   215,   216,
   217,   218,   489,   219,   470,   471,   472,   220,   221,   222,
   223,   224,   225,   226,   227,   228,   229,   230,   231,   232,
   233,   234,   235,   236,   237,   238,   239,   240,   241,   242,
   243,   244,   393,   490,   715
};

static const short yypact[] = {   507,
-32768,-32768,    15,   -30,    18,   244,-32768,-32768,   414,   443,
   556,-32768,-32768,-32768,-32768,   714,-32768,-32768,-32768,-32768,
    23,-32768,-32768,-32768,   359,-32768,   340,-32768,    49,   601,
-32768,-32768,   585,   603,-32768,   -30,   417,-32768,-32768,    92,
-32768,   489,   -24,   107,-32768,   529,   295,-32768,-32768,   -30,
   710,   452,-32768,   344,-32768,    40,-32768,-32768,-32768,-32768,
   192,  1047,-32768,   530,   -24,-32768,-32768,   388,   534,-32768,
-32768,   -24,   107,-32768,   295,-32768,-32768,-32768,   540,-32768,
-32768,-32768,   548,   258,-32768,-32768,   307,   442,   736,-32768,
-32768,    77,-32768,  1638,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   312,   406,-32768,   -24,-32768,-32768,   379,
   -18,-32768,   426,   331,-32768,   693,   -18,   120,   210,   429,
-32768,   564,   587,-32768,-32768,-32768,-32768,   593,   951,-32768,
-32768,   406,   607,   617,   214,-32768,-32768,-32768,   618,  1077,
   219,   531,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   -18,
-32768,   361,   -18,-32768,-32768,   460,   471,-32768,   494,   736,
-32768,  1204,-32768,-32768,   500,-32768,   180,-32768,   535,-32768,
-32768,   591,-32768,-32768,  1759,-32768,-32768,-32768,   620,   728,
   377,-32768,-32768,-32768,   613,-32768,-32768,-32768,   157,-32768,
-32768,  2663,  2728,  2779,  2844,   655,    32,   707,-32768,  2895,
  2960,  3011,  5025,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   673,   881,    56,-32768,   678,   746,-32768,   640,-32768,   671,
-32768,   715,   866,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   899,   921,  1006,   838,   920,   654,   814,   820,   847,   263,
-32768,-32768,-32768,   850,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   963,   591,-32768,-32768,-32768,-32768,   484,   769,
   775,-32768,    -1,   306,   353,  3076,-32768,   232,  1357,    35,
   392,   419,   228,   424,   234,   779,  5324,-32768,   -30,   538,
   974,   265,   872,-32768,   784,-32768,  1691,-32768,-32768,   789,
-32768,-32768,-32768,  1827,-32768,-32768,   797,-32768,-32768,-32768,
-32768,  1827,-32768,  1827,-32768,-32768,  5375,   802,-32768,-32768,
-32768,-32768,-32768,   427,-32768,   737,   806,   866,   902,   916,
-32768,-32768,-32768,-32768,   891,   642,-32768,   592,   660,-32768,
   189,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   409,-32768,   666,   964,   793,   793,   428,-32768,-32768,-32768,
-32768,-32768,   777,  1121,    66,-32768,-32768,   622,   432,  5092,
  3127,   729,-32768,   -28,  3192,-32768,   435,-32768,-32768,  3243,
  3308,  3359,  3424,  3475,  3540,  3591,  3656,  3707,  3772,  3823,
  3888,   864,  3939,  4004,  4055,  4120,  4171,  4236,  4287,  2032,
-32768,-32768,  4352,-32768,   605,-32768,-32768,-32768,   810,-32768,
-32768,-32768,  1691,-32768,-32768,-32768,-32768,  4403,-32768,    95,
-32768,-32768,   126,-32768,-32768,   174,-32768,  4468,-32768,  4519,
-32768,   785,-32768,  4933,-32768,-32768,   182,   269,   815,  2083,
   604,-32768,-32768,   -30,  2148,-32768,-32768,-32768,-32768,-32768,
  1158,-32768,-32768,   823,-32768,   861,  1018,   728,-32768,-32768,
-32768,    86,  2199,-32768,  4584,-32768,   891,-32768,-32768,-32768,
-32768,-32768,-32768,   565,   836,-32768,-32768,-32768,  2264,   793,
-32768,   590,   793,   590,-32768,  2315,  4635,   223,   -43,  2380,
   191,-32768,   894,-32768,  1967,-32768,-32768,-32768,   649,-32768,
-32768,   237,-32768,-32768,-32768,-32768,-32768,   839,-32768,   239,
-32768,  5143,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   899,
-32768,   899,-32768,   921,-32768,   921,-32768,   921,-32768,  1006,
-32768,  1006,-32768,  1006,-32768,  1006,-32768,   258,-32768,-32768,
   838,-32768,   838,-32768,   920,-32768,   654,-32768,   814,-32768,
   820,-32768,   847,-32768,   933,   857,-32768,-32768,-32768,-32768,
  1254,-32768,  1691,   859,   856,  1691,-32768,   314,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   348,-32768,   870,   461,   229,
   785,-32768,   535,-32768,-32768,-32768,-32768,-32768,   467,   815,
-32768,   967,    14,   444,-32768,   878,-32768,  5067,-32768,  4958,
   876,   887,   892,-32768,-32768,  5208,-32768,-32768,   241,-32768,
   895,   299,-32768,   895,-32768,-32768,   406,    17,-32768,-32768,
-32768,  4700,   487,-32768,-32768,-32768,-32768,-32768,  4751,-32768,
-32768,  5259,-32768,-32768,   406,   704,-32768,  4816,   662,-32768,
  1691,  2431,-32768,-32768,-32768,  1893,-32768,-32768,   343,-32768,
   835,-32768,-32768,-32768,-32768,-32768,-32768,  2496,-32768,-32768,
   999,    91,  4867,-32768,   674,-32768,  1467,-32768,  5324,-32768,
-32768,-32768,   928,   923,  5000,-32768,   410,-32768,   723,-32768,
-32768,-32768,-32768,-32768,-32768,   406,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   731,-32768,   406,-32768,-32768,   477,-32768,
   243,   108,   485,-32768,   978,   979,  1893,-32768,-32768,  1893,
-32768,-32768,   942,-32768,   948,   950,-32768,  1053,   128,-32768,
-32768,-32768,-32768,-32768,   328,-32768,-32768,  1540,-32768,  1608,
-32768,   961,  1827,-32768,   970,-32768,-32768,-32768,-32768,-32768,
-32768,  2547,-32768,   262,  4403,  1827,-32768,-32768,  2612,-32768,
-32768,-32768,-32768,  1071,-32768,-32768,-32768,   980,-32768,  1827,
   296,   202,-32768,   358,-32768,  4958,   983,-32768,-32768,-32768,
-32768,   302,  1893,   990,  5000,-32768,  1021,  1893,   992,  1893,
-32768,  1893,-32768,-32768,  1087,  1089,-32768
};

static const short yypgoto[] = {-32768,
-32768,   -57,   -60,   711,   -16,  -123,   568,-32768,    -3,   552,
-32768,   130,-32768,  1085,   791,-32768,    22,-32768,-32768,   778,
    12,   154,-32768,-32768,  1044,  1030,-32768,  -128,-32768,   945,
-32768,   382,  -106,   973,  -165,  -193,-32768,-32768,   469,   -58,
   865,  -327,   -40,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   957,-32768,   142,-32768,   718,   -10,-32768,-32768,-32768,-32768,
  1072,   584,-32768,  1031,-32768,-32768,   372,-32768,  -122,   867,
   843,  -166,  -279,-32768,   819,  -234,   197,  -551,-32768,  -529,
-32768,-32768,-32768,  -301,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   476,   486,  -619,  -496,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,  -436,-32768,  -603,   832,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   569,-32768,   573,-32768,-32768,
   119,-32768,  -386,-32768,   799,   362,  -326,   858,   163,  1120,
   320,   437,   438,  -112,   560,   636,  -470,-32768,   632,   724,
   651,   625,   761,   765,   760,   764,   776,-32768,   526,   771,
   717,-32768,-32768,    29,-32768
};


#define	YYLAST		5484


static const short yytable[] = {    21,
   459,    84,    29,   176,    83,   450,   174,   438,   287,   357,
   184,    16,   614,   325,   650,    19,   152,   675,    26,   474,
    16,    16,    16,    38,   117,    59,   479,   481,    84,    67,
    31,   123,   342,    84,   134,   342,    83,   721,    61,   174,
    76,    16,    61,   497,    16,    16,   260,    59,   583,    48,
   612,    93,    59,   602,    31,    20,   363,    20,    88,   442,
    61,   725,    16,   150,   153,    61,   482,   445,    84,   446,
    62,    83,    84,    89,   347,   179,   143,   124,   135,   334,
   336,   338,   340,    93,   696,    88,   587,   349,   351,   608,
    88,   711,    55,   404,   405,   559,    56,   406,   721,    84,
    20,    84,   123,    20,    83,    89,   697,   651,   650,   249,
   676,    59,   251,   619,   280,   626,   619,    67,   173,    84,
   144,    39,   326,    40,    61,    88,   561,  -157,   743,    88,
    61,   262,   343,    25,    27,   343,   399,   345,    77,   700,
   160,   353,   680,   601,   180,   696,   604,    49,   696,    50,
   422,   173,   764,    17,   -94,   -94,    88,   137,    88,   483,
   457,   769,    17,    17,    17,    52,    54,   697,   245,    57,
   697,   281,   429,   160,   563,   125,    88,    20,   712,    57,
   258,   346,   576,    17,  -294,   588,    17,    17,   331,   331,
   331,   331,    84,   560,    61,   179,   331,   331,   354,   703,
   700,   734,   743,   700,    17,   461,    64,   619,   110,   669,
   146,   696,   113,   396,   186,    90,   696,   145,   696,   246,
   696,   744,    84,   611,   562,   434,   280,   619,   421,   642,
   355,   245,   411,   697,   425,   684,   556,   620,   697,   623,
   697,   670,   697,   649,    28,   692,   280,    90,   -97,    88,
   -97,   110,   113,   -97,   -97,   -97,   -97,   504,   506,   508,
   703,   709,   710,   703,   180,   432,   700,    84,   189,   482,
   179,   700,   564,   700,  -104,   700,   438,   389,  -104,    88,
   577,   360,   161,   281,   613,   361,   661,   602,   450,   362,
   441,   617,    50,   316,   410,   762,   742,   416,   441,   672,
   441,   619,   758,   447,   282,   428,  -322,   147,   189,   -25,
   -25,   187,    67,   706,   635,   161,   247,    20,   448,    20,
   598,   528,   157,   157,    88,    61,   703,   580,   745,    20,
   412,   703,   426,   703,   621,   703,   624,   317,   671,   180,
    45,   733,   280,   704,    74,   752,    69,   367,   637,   390,
   -25,   438,   433,   407,   118,   -25,   -25,   -25,   635,    41,
   753,   -25,   578,   450,   610,   -25,   331,   331,   331,   331,
   331,   331,   331,   331,   331,   331,   331,   331,    88,   331,
   331,   331,   331,   331,   331,   331,   631,    84,   101,   492,
   434,   -65,   417,   500,   761,   -65,   673,   413,  -322,   551,
   766,   694,    46,   119,   427,   316,    46,   636,   189,   462,
   726,   128,   316,   -30,     1,   746,   282,    53,   546,   419,
   316,    42,   316,   282,   423,   316,   137,   454,   475,   138,
   139,   282,   486,   282,   -65,   501,   558,  -256,   -65,   -66,
   438,   638,   -31,     1,    88,   408,   566,   643,   568,   317,
   646,   763,    71,  -158,   450,   189,   317,     2,   492,   250,
   139,   640,   442,   450,   317,   445,   317,   647,     3,   317,
   327,   133,   465,   331,     4,   463,   328,   647,   674,   331,
     5,   591,   -66,   593,   418,   407,     2,   679,   749,    58,
   652,    57,   280,   498,   318,   280,   685,   599,   487,   678,
   129,   755,    20,     4,   157,   653,   -28,     1,   492,     5,
   165,   420,     7,   245,    42,   760,   424,   -97,   133,   455,
   476,   316,   -97,   -97,   -97,   -97,   148,   502,   694,    66,
    98,   248,   282,   749,   102,   755,    78,   760,   120,   654,
   109,     7,    50,   165,   727,   196,  -160,   729,   112,   281,
     2,   197,   281,   641,  -139,   -29,     1,   731,   198,   648,
    57,     3,   199,   189,   149,   317,   252,     4,    80,   732,
   280,    81,    20,     5,    20,   200,   201,   735,   157,   202,
    84,     6,   397,   179,   -32,     1,   253,   151,   204,   205,
   206,   207,   458,   154,   208,   209,   280,   167,   257,     2,
   -34,     1,   -33,     1,   493,     7,   318,   177,   331,   331,
     3,   319,   320,   318,    20,    20,     4,   183,   188,    20,
   324,   318,     5,   318,   331,    20,   318,   281,     2,   157,
   167,    99,   441,    20,   118,     2,   103,    88,   359,     3,
   366,    78,   456,   134,     2,     4,     2,   682,   100,    20,
     2,     5,   180,   281,     7,   106,    78,   280,   594,   280,
   460,     4,   493,     4,   385,   595,   464,     5,   579,     5,
   494,   316,    20,    80,   316,   495,    81,    20,    20,   496,
   159,   714,   282,     7,   203,   282,   602,   722,    80,    20,
   131,    81,    20,   441,   630,   155,   441,   634,   549,     7,
   178,     7,    20,    20,   328,    20,   316,   344,   316,   -35,
     1,   159,   329,   159,   281,   317,   281,   484,   317,   441,
   652,   485,   318,   319,   320,    20,   689,    20,   494,   493,
   319,   320,   441,   495,   321,   653,   121,   496,   319,   320,
   319,   320,   618,   319,   320,    20,   441,    20,   619,   316,
   317,    20,   317,     2,   316,   341,    78,    35,    57,   441,
   282,  -497,  -497,   754,   441,   282,   441,   757,   441,   716,
     4,    35,   690,   359,    36,   316,     5,    78,   364,    35,
    37,  -341,  -341,   316,   140,    78,   282,    32,    80,   141,
   142,    81,    20,   317,   122,   494,    36,   686,   317,    30,
   495,    34,    37,   619,   496,  -498,  -498,    32,     7,    80,
   322,    32,    81,    20,    20,   316,   728,    80,   316,   317,
    81,    20,   619,    51,   730,   386,   282,   317,    32,   282,
   619,   600,   387,  -341,   600,   705,   316,  -341,   316,   319,
   320,   316,   365,   603,   569,   605,   321,   282,   570,   282,
  -343,  -343,   282,   321,   316,   378,   379,   380,   381,   317,
   388,   321,   317,   321,   527,   282,   321,   400,   316,   401,
   477,  -156,   318,   478,   316,   318,   435,   359,     2,   282,
   317,   316,   317,   316,    78,   317,   316,   439,   316,   469,
   316,   323,   282,   737,   382,   443,   738,   282,   317,   282,
   453,   282,  -343,   370,   371,   372,  -343,   318,   550,   318,
   368,   369,   317,    78,   139,    35,    80,   584,   317,    81,
    20,    78,   322,   373,   374,   317,   586,   317,   596,   322,
   317,   622,   317,   627,   317,   383,   384,   322,    37,   322,
   391,   392,   322,    78,   628,    80,  -407,  -407,    81,    20,
   318,   632,   196,    80,   633,   318,    81,    20,   197,   767,
  -408,  -408,   321,   639,   771,   198,   773,   649,   774,   199,
   659,  -496,  -496,   360,   665,    80,   318,   361,    81,    20,
   666,   362,   200,   201,   318,   667,   202,   141,   142,   319,
   320,   602,   319,   320,   156,   204,   205,   206,   207,   710,
    78,   208,   209,   323,   510,   512,     2,   531,   533,    79,
   323,     4,    78,   375,   376,   377,   318,     5,   323,   318,
   323,   723,   588,   323,   319,   320,   319,   320,   520,   522,
   524,   526,    80,   736,  -207,    81,    20,   318,   322,   318,
   739,   740,   318,   741,    80,   157,   158,    81,    20,   332,
   332,   332,   332,   742,   748,   318,   394,   332,   332,   -15,
   466,   467,   468,   750,  -496,  -496,   360,   319,   320,   318,
   430,   758,   319,   320,   431,   318,   770,   191,   759,   192,
   193,   765,   318,   768,   318,   772,   776,   318,   777,   318,
     2,   318,   529,   319,   320,    33,    78,    75,   514,   516,
   518,   319,   320,   -15,   108,    79,   256,     4,  -496,  -496,
   360,   190,   321,     5,   430,   321,   254,   395,   362,   323,
   554,   194,   195,    73,   127,   449,    78,   403,    80,   437,
   717,    81,    20,   319,   320,   196,   319,   320,   452,   644,
   718,   197,    82,   645,   473,   535,   539,   321,   198,   321,
   537,   541,   199,   688,   319,   320,   319,   320,    80,   319,
   320,    81,    20,   548,   543,   200,   201,     0,     0,   202,
     0,   203,   319,   320,     0,     0,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   319,   320,   322,     0,
   321,   322,   319,   320,     0,   321,     0,     0,     0,   319,
   320,   319,   320,     0,   319,   320,   319,   320,   319,   320,
     0,  -496,  -496,   360,     0,     0,   321,   480,     0,     0,
     0,   362,     0,   322,   321,   322,     0,   332,   332,   332,
   332,   332,   332,   332,   332,   332,   332,   332,   332,     0,
   332,   332,   332,   332,   332,   332,   332,   156,  -496,  -496,
   360,     0,     0,    78,   361,     0,   321,     0,   431,   321,
     0,     0,    79,     0,     4,     0,   322,     0,     0,   323,
     5,   322,   323,     0,     0,     0,     0,   321,     0,   321,
     0,     0,   321,     0,     0,    80,     0,     0,    81,    20,
     0,     0,   322,     0,     0,   321,     0,     0,   157,   255,
   322,     0,     0,     0,   323,     0,   323,     0,     0,   321,
     0,   333,   333,   333,   333,   321,     0,     0,     0,   333,
   333,     0,   321,     0,   321,     0,     0,   321,     0,   321,
     0,   321,   322,     0,   332,   322,     0,     0,     0,   -15,
   332,     0,     0,     0,  -496,  -496,   360,   323,     0,     0,
   430,     0,   323,   322,   629,   322,     0,   414,   322,   192,
   193,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,   322,     0,   323,     0,     0,     0,     0,     0,     0,
     0,   323,     0,     0,     0,   322,     0,     0,     0,     0,
     0,   322,     0,     0,     0,     0,     0,     0,   322,     0,
   322,   194,   195,   322,     0,   322,    78,   322,     0,     0,
     0,     0,     0,   323,     0,   196,   323,     0,     0,     0,
     0,   197,     0,     0,     0,     0,     0,     0,   198,     0,
     0,     0,   199,     0,   323,     0,   323,     0,    80,   323,
     0,    81,    20,     0,     0,   200,   201,     0,     0,   202,
     0,     0,   323,     0,     0,   415,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   323,   263,     0,   332,
   332,     0,   323,     0,     0,     0,     0,     0,     0,   323,
     0,   323,     0,     0,   323,   332,   323,     0,   323,   333,
   333,   333,   333,   333,   333,   333,   333,   333,   333,   333,
   333,     0,   333,   333,   333,   333,   333,   333,   333,     0,
   264,   194,   195,   652,   265,   266,    78,   267,     0,     0,
   268,     0,     0,     0,   269,   196,     0,     0,   653,     0,
     0,   270,   271,     5,   272,     0,   273,   274,   198,   275,
   263,     0,   276,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,     0,     0,     0,     0,   277,
     0,   157,   719,     0,     0,   278,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,     0,     0,     0,     0,
     0,     0,     0,   264,   194,   195,   652,   265,   266,    78,
   267,     0,     0,   268,     0,     0,   333,   269,   196,     0,
     0,   653,   333,     0,   270,   271,     5,   272,   263,   273,
   274,   198,   275,     0,     0,   276,     0,     0,     0,     0,
     0,    80,     0,     0,    81,    20,     0,     0,     0,     0,
     0,     0,   277,     0,   157,   747,     0,     0,   278,     0,
     0,   204,   205,   206,   207,     0,     0,   208,   209,     0,
     0,   264,   194,   195,  -266,   265,   266,    78,   267,     0,
     0,   268,     0,     0,     0,   269,   196,     0,     0,  -266,
     0,     0,   270,   271,     5,   272,     0,   273,   274,   198,
   275,     2,     0,   276,     0,     0,     0,    78,     0,    80,
     0,   263,    81,    20,     0,     0,    79,     0,     4,     0,
   277,     0,   157,  -266,     5,     0,   278,     0,     0,   204,
   205,   206,   207,     0,     0,   208,   209,     0,     0,    80,
     0,     0,    81,    20,     0,     0,     0,     0,     0,     0,
     0,   333,   333,   126,   264,   194,   195,     0,   265,   266,
    78,   267,     0,     0,   268,     0,     0,   333,   269,   196,
     0,     0,     0,     0,     0,   270,   271,     5,   272,   263,
   273,   274,   198,   275,     0,     0,   276,     0,     0,     0,
     0,     0,    80,     0,     0,    81,    20,     0,     0,     0,
     0,     0,     0,   277,     0,   157,   436,     0,     0,   278,
     0,     0,   204,   205,   206,   207,     0,     0,   208,   209,
     0,     0,   264,   194,   195,     0,   265,   266,    78,   267,
     0,     0,   268,     0,     0,     0,   269,   196,     0,     0,
     0,     0,     0,   270,   271,     5,   272,   263,   273,   274,
   198,   275,     0,     0,   276,     0,     0,     0,     0,     0,
    80,     0,     0,    81,    20,     0,     0,     0,     0,     0,
     0,   277,     0,   157,     0,     0,     0,   278,     0,     0,
   204,   205,   206,   207,     0,     0,   208,   209,     0,     0,
   440,   194,   195,     0,   265,   266,    78,   267,     0,     0,
   268,     0,     0,     0,   269,   196,     0,     0,     0,     0,
     0,   270,   271,   263,   272,     0,   273,   274,   198,   275,
     0,     0,   276,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,     0,     0,     0,     0,   277,
     0,   157,     0,     0,     0,   278,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   440,   194,   195,     0,
   693,   266,    78,   267,     0,     0,   268,     0,     0,     0,
   269,   196,     0,     0,     0,     0,     0,   270,   271,     0,
   272,     0,   273,   274,   198,   275,     0,   615,   276,   192,
   193,     0,     0,     0,    80,     0,     0,    81,    20,     0,
     0,     0,     0,     0,     0,   277,     0,   157,     0,     0,
     0,   278,     0,     0,   204,   205,   206,   207,     0,     0,
   208,   209,     0,     0,     0,     0,     0,     0,     0,     0,
     0,   194,   195,     0,     0,     0,    78,     0,     0,     0,
     0,     0,     0,     0,     0,   196,     0,     0,     0,     0,
     0,   197,   544,     0,   192,   193,     0,     0,   198,     0,
     0,     0,   199,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,   200,   201,     0,     0,   202,
     0,   203,   616,     0,     0,     0,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   194,   195,     0,     0,
     0,    78,     0,   491,     0,   192,   193,     0,     0,     0,
   196,     0,     0,     0,     0,     0,   197,     0,     0,     0,
     0,     0,     0,   198,     0,     0,     0,   199,     0,     0,
     0,     0,     0,    80,     0,     0,    81,    20,     0,   545,
   200,   201,     0,     0,   202,     0,     0,   194,   195,     0,
     0,     0,    78,   204,   205,   206,   207,     0,     0,   208,
   209,   196,     0,     0,     0,     0,     0,   197,   581,     0,
   192,   193,     0,     0,   198,     0,     0,     0,   199,     0,
     0,     0,     0,     0,    80,     0,     0,    81,    20,     0,
     0,   200,   201,     0,     0,   202,     0,     0,     0,     0,
   148,     0,     0,     0,   204,   205,   206,   207,     0,     0,
   208,   209,   194,   195,     0,     0,     0,    78,     0,   589,
     0,   192,   193,     0,     0,     0,   196,     0,     0,     0,
     0,     0,   197,     0,     0,     0,     0,     0,     0,   198,
     0,     0,     0,   199,     0,     0,     0,     0,     0,    80,
     0,     0,    81,    20,     0,     0,   200,   201,     0,     0,
   202,   582,     0,   194,   195,     0,     0,     0,    78,   204,
   205,   206,   207,     0,     0,   208,   209,   196,     0,     0,
     0,     0,     0,   197,   597,     0,   192,   193,     0,     0,
   198,     0,     0,     0,   199,     0,     0,     0,     0,     0,
    80,     0,     0,    81,    20,     0,     0,   200,   201,     0,
     0,   202,     0,     0,     0,     0,     0,   590,     0,     0,
   204,   205,   206,   207,     0,     0,   208,   209,   194,   195,
     0,     0,     0,    78,     0,   606,     0,   192,   193,     0,
     0,     0,   196,     0,     0,     0,     0,     0,   197,     0,
     0,     0,     0,     0,     0,   198,     0,     0,     0,   199,
     0,     0,     0,     0,     0,    80,     0,     0,    81,    20,
     0,     0,   200,   201,     0,     0,   202,     0,     0,   194,
   195,   598,     0,     0,    78,   204,   205,   206,   207,     0,
     0,   208,   209,   196,     0,     0,     0,     0,     0,   197,
   491,     0,   192,   193,     0,     0,   198,     0,     0,     0,
   199,     0,     0,     0,     0,     0,    80,     0,     0,    81,
    20,     0,     0,   200,   201,     0,     0,   202,   607,     0,
     0,     0,     0,     0,     0,     0,   204,   205,   206,   207,
     0,     0,   208,   209,   194,   195,     0,     0,     0,    78,
     0,   581,     0,   192,   193,     0,     0,     0,   196,     0,
     0,     0,     0,     0,   197,     0,     0,     0,     0,     0,
     0,   198,     0,     0,     0,   199,     0,     0,     0,     0,
     0,    80,     0,     0,    81,    20,     0,     0,   200,   201,
     0,     0,   202,     0,     0,   194,   195,   598,     0,     0,
    78,   204,   205,   206,   207,     0,     0,   208,   209,   196,
     0,     0,     0,     0,     0,   197,   707,     0,   192,   193,
     0,     0,   198,     0,     0,     0,   199,     0,     0,     0,
     0,     0,    80,     0,     0,    81,    20,     0,     0,   200,
   201,     0,     0,   202,   691,     0,     0,     0,     0,     0,
     0,     0,   204,   205,   206,   207,     0,     0,   208,   209,
   194,   195,     0,     0,     0,    78,     0,   707,     0,   192,
   193,     0,     0,     0,   196,     0,     0,     0,     0,     0,
   197,     0,     0,     0,     0,     0,     0,   198,     0,     0,
     0,   199,     0,     0,     0,     0,     0,    80,     0,     0,
    81,    20,     0,     0,   200,   201,     0,     0,   202,   708,
     0,   194,   195,     0,     0,     0,    78,   204,   205,   206,
   207,     0,     0,   208,   209,   196,     0,     0,     0,     0,
     0,   197,   589,     0,   192,   193,     0,     0,   198,     0,
     0,     0,   199,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,   200,   201,     0,     0,   202,
   751,     0,     0,     0,     0,     0,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   194,   195,     0,     0,
     0,    78,     0,   330,     0,   192,   193,     0,     0,     0,
   196,     0,     0,     0,     0,     0,   197,     0,     0,     0,
     0,     0,     0,   198,     0,     0,     0,   199,     0,     0,
     0,     0,     0,    80,     0,     0,    81,    20,     0,     0,
   200,   201,     0,     0,   202,     0,     0,   194,   195,     0,
   756,     0,    78,   204,   205,   206,   207,     0,     0,   208,
   209,   196,     0,     0,     0,     0,     0,   197,   335,     0,
   192,   193,     0,     0,   198,     0,     0,     0,   199,     0,
     0,     0,     0,     0,    80,     0,     0,    81,    20,     0,
     0,   200,   201,     0,     0,   202,     0,     0,     0,     0,
     0,     0,     0,     0,   204,   205,   206,   207,     0,     0,
   208,   209,   194,   195,     0,     0,     0,    78,     0,   337,
     0,   192,   193,     0,     0,     0,   196,     0,     0,     0,
     0,     0,   197,     0,     0,     0,     0,     0,     0,   198,
     0,     0,     0,   199,     0,     0,     0,     0,     0,    80,
     0,     0,    81,    20,     0,     0,   200,   201,     0,     0,
   202,     0,     0,   194,   195,     0,     0,     0,    78,   204,
   205,   206,   207,     0,     0,   208,   209,   196,     0,     0,
     0,     0,     0,   197,   339,     0,   192,   193,     0,     0,
   198,     0,     0,     0,   199,     0,     0,     0,     0,     0,
    80,     0,     0,    81,    20,     0,     0,   200,   201,     0,
     0,   202,     0,     0,     0,     0,     0,     0,     0,     0,
   204,   205,   206,   207,     0,     0,   208,   209,   194,   195,
     0,     0,     0,    78,     0,   348,     0,   192,   193,     0,
     0,     0,   196,     0,     0,     0,     0,     0,   197,     0,
     0,     0,     0,     0,     0,   198,     0,     0,     0,   199,
     0,     0,     0,     0,     0,    80,     0,     0,    81,    20,
     0,     0,   200,   201,     0,     0,   202,     0,     0,   194,
   195,     0,     0,     0,    78,   204,   205,   206,   207,     0,
     0,   208,   209,   196,     0,     0,     0,     0,     0,   197,
   350,     0,   192,   193,     0,     0,   198,     0,     0,     0,
   199,     0,     0,     0,     0,     0,    80,     0,     0,    81,
    20,     0,     0,   200,   201,     0,     0,   202,     0,     0,
     0,     0,     0,     0,     0,     0,   204,   205,   206,   207,
     0,     0,   208,   209,   194,   195,     0,     0,     0,    78,
     0,   352,     0,   192,   193,     0,     0,     0,   196,     0,
     0,     0,     0,     0,   197,     0,     0,     0,     0,     0,
     0,   198,     0,     0,     0,   199,     0,     0,     0,     0,
     0,    80,     0,     0,    81,    20,     0,     0,   200,   201,
     0,     0,   202,     0,     0,   194,   195,     0,     0,     0,
    78,   204,   205,   206,   207,     0,     0,   208,   209,   196,
     0,     0,     0,     0,     0,   197,   409,     0,   192,   193,
     0,     0,   198,     0,     0,     0,   199,     0,     0,     0,
     0,     0,    80,     0,     0,    81,    20,     0,     0,   200,
   201,     0,     0,   202,     0,     0,     0,     0,     0,     0,
     0,     0,   204,   205,   206,   207,     0,     0,   208,   209,
   194,   195,     0,     0,     0,    78,     0,   491,     0,   192,
   193,     0,     0,     0,   196,     0,     0,     0,     0,     0,
   197,     0,     0,     0,     0,     0,     0,   198,     0,     0,
     0,   199,     0,     0,     0,     0,     0,    80,     0,     0,
    81,    20,     0,     0,   200,   201,     0,     0,   202,     0,
     0,   194,   195,     0,     0,     0,    78,   204,   205,   206,
   207,     0,     0,   208,   209,   196,     0,     0,     0,     0,
     0,   197,   499,     0,   192,   193,     0,     0,   198,     0,
     0,     0,   199,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,   200,   201,     0,     0,   202,
     0,     0,     0,     0,     0,     0,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   194,   195,     0,     0,
     0,    78,     0,   503,     0,   192,   193,     0,     0,     0,
   196,     0,     0,     0,     0,     0,   197,     0,     0,     0,
     0,     0,     0,   198,     0,     0,     0,   199,     0,     0,
     0,     0,     0,    80,     0,     0,    81,    20,     0,     0,
   200,   201,     0,     0,   202,     0,     0,   194,   195,     0,
     0,     0,    78,   204,   205,   206,   207,     0,     0,   208,
   209,   196,     0,     0,     0,     0,     0,   197,   505,     0,
   192,   193,     0,     0,   198,     0,     0,     0,   199,     0,
     0,     0,     0,     0,    80,     0,     0,    81,    20,     0,
     0,   200,   201,     0,     0,   202,     0,     0,     0,     0,
     0,     0,     0,     0,   204,   205,   206,   207,     0,     0,
   208,   209,   194,   195,     0,     0,     0,    78,     0,   507,
     0,   192,   193,     0,     0,     0,   196,     0,     0,     0,
     0,     0,   197,     0,     0,     0,     0,     0,     0,   198,
     0,     0,     0,   199,     0,     0,     0,     0,     0,    80,
     0,     0,    81,    20,     0,     0,   200,   201,     0,     0,
   202,     0,     0,   194,   195,     0,     0,     0,    78,   204,
   205,   206,   207,     0,     0,   208,   209,   196,     0,     0,
     0,     0,     0,   197,   509,     0,   192,   193,     0,     0,
   198,     0,     0,     0,   199,     0,     0,     0,     0,     0,
    80,     0,     0,    81,    20,     0,     0,   200,   201,     0,
     0,   202,     0,     0,     0,     0,     0,     0,     0,     0,
   204,   205,   206,   207,     0,     0,   208,   209,   194,   195,
     0,     0,     0,    78,     0,   511,     0,   192,   193,     0,
     0,     0,   196,     0,     0,     0,     0,     0,   197,     0,
     0,     0,     0,     0,     0,   198,     0,     0,     0,   199,
     0,     0,     0,     0,     0,    80,     0,     0,    81,    20,
     0,     0,   200,   201,     0,     0,   202,     0,     0,   194,
   195,     0,     0,     0,    78,   204,   205,   206,   207,     0,
     0,   208,   209,   196,     0,     0,     0,     0,     0,   197,
   513,     0,   192,   193,     0,     0,   198,     0,     0,     0,
   199,     0,     0,     0,     0,     0,    80,     0,     0,    81,
    20,     0,     0,   200,   201,     0,     0,   202,     0,     0,
     0,     0,     0,     0,     0,     0,   204,   205,   206,   207,
     0,     0,   208,   209,   194,   195,     0,     0,     0,    78,
     0,   515,     0,   192,   193,     0,     0,     0,   196,     0,
     0,     0,     0,     0,   197,     0,     0,     0,     0,     0,
     0,   198,     0,     0,     0,   199,     0,     0,     0,     0,
     0,    80,     0,     0,    81,    20,     0,     0,   200,   201,
     0,     0,   202,     0,     0,   194,   195,     0,     0,     0,
    78,   204,   205,   206,   207,     0,     0,   208,   209,   196,
     0,     0,     0,     0,     0,   197,   517,     0,   192,   193,
     0,     0,   198,     0,     0,     0,   199,     0,     0,     0,
     0,     0,    80,     0,     0,    81,    20,     0,     0,   200,
   201,     0,     0,   202,     0,     0,     0,     0,     0,     0,
     0,     0,   204,   205,   206,   207,     0,     0,   208,   209,
   194,   195,     0,     0,     0,    78,     0,   519,     0,   192,
   193,     0,     0,     0,   196,     0,     0,     0,     0,     0,
   197,     0,     0,     0,     0,     0,     0,   198,     0,     0,
     0,   199,     0,     0,     0,     0,     0,    80,     0,     0,
    81,    20,     0,     0,   200,   201,     0,     0,   202,     0,
     0,   194,   195,     0,     0,     0,    78,   204,   205,   206,
   207,     0,     0,   208,   209,   196,     0,     0,     0,     0,
     0,   197,   521,     0,   192,   193,     0,     0,   198,     0,
     0,     0,   199,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,   200,   201,     0,     0,   202,
     0,     0,     0,     0,     0,     0,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   194,   195,     0,     0,
     0,    78,     0,   523,     0,   192,   193,     0,     0,     0,
   196,     0,     0,     0,     0,     0,   197,     0,     0,     0,
     0,     0,     0,   198,     0,     0,     0,   199,     0,     0,
     0,     0,     0,    80,     0,     0,    81,    20,     0,     0,
   200,   201,     0,     0,   202,     0,     0,   194,   195,     0,
     0,     0,    78,   204,   205,   206,   207,     0,     0,   208,
   209,   196,     0,     0,     0,     0,     0,   197,   525,     0,
   192,   193,     0,     0,   198,     0,     0,     0,   199,     0,
     0,     0,     0,     0,    80,     0,     0,    81,    20,     0,
     0,   200,   201,     0,     0,   202,     0,     0,     0,     0,
     0,     0,     0,     0,   204,   205,   206,   207,     0,     0,
   208,   209,   194,   195,     0,     0,     0,    78,     0,   530,
     0,   192,   193,     0,     0,     0,   196,     0,     0,     0,
     0,     0,   197,     0,     0,     0,     0,     0,     0,   198,
     0,     0,     0,   199,     0,     0,     0,     0,     0,    80,
     0,     0,    81,    20,     0,     0,   200,   201,     0,     0,
   202,     0,     0,   194,   195,     0,     0,     0,    78,   204,
   205,   206,   207,     0,     0,   208,   209,   196,     0,     0,
     0,     0,     0,   197,   532,     0,   192,   193,     0,     0,
   198,     0,     0,     0,   199,     0,     0,     0,     0,     0,
    80,     0,     0,    81,    20,     0,     0,   200,   201,     0,
     0,   202,     0,     0,     0,     0,     0,     0,     0,     0,
   204,   205,   206,   207,     0,     0,   208,   209,   194,   195,
     0,     0,     0,    78,     0,   534,     0,   192,   193,     0,
     0,     0,   196,     0,     0,     0,     0,     0,   197,     0,
     0,     0,     0,     0,     0,   198,     0,     0,     0,   199,
     0,     0,     0,     0,     0,    80,     0,     0,    81,    20,
     0,     0,   200,   201,     0,     0,   202,     0,     0,   194,
   195,     0,     0,     0,    78,   204,   205,   206,   207,     0,
     0,   208,   209,   196,     0,     0,     0,     0,     0,   197,
   536,     0,   192,   193,     0,     0,   198,     0,     0,     0,
   199,     0,     0,     0,     0,     0,    80,     0,     0,    81,
    20,     0,     0,   200,   201,     0,     0,   202,     0,     0,
     0,     0,     0,     0,     0,     0,   204,   205,   206,   207,
     0,     0,   208,   209,   194,   195,     0,     0,     0,    78,
     0,   538,     0,   192,   193,     0,     0,     0,   196,     0,
     0,     0,     0,     0,   197,     0,     0,     0,     0,     0,
     0,   198,     0,     0,     0,   199,     0,     0,     0,     0,
     0,    80,     0,     0,    81,    20,     0,     0,   200,   201,
     0,     0,   202,     0,     0,   194,   195,     0,     0,     0,
    78,   204,   205,   206,   207,     0,     0,   208,   209,   196,
     0,     0,     0,     0,     0,   197,   540,     0,   192,   193,
     0,     0,   198,     0,     0,     0,   199,     0,     0,     0,
     0,     0,    80,     0,     0,    81,    20,     0,     0,   200,
   201,     0,     0,   202,     0,     0,     0,     0,     0,     0,
     0,     0,   204,   205,   206,   207,     0,     0,   208,   209,
   194,   195,     0,     0,     0,    78,     0,   542,     0,   192,
   193,     0,     0,     0,   196,     0,     0,     0,     0,     0,
   197,     0,     0,     0,     0,     0,     0,   198,     0,     0,
     0,   199,     0,     0,     0,     0,     0,    80,     0,     0,
    81,    20,     0,     0,   200,   201,     0,     0,   202,     0,
     0,   194,   195,     0,     0,     0,    78,   204,   205,   206,
   207,     0,     0,   208,   209,   196,     0,     0,     0,     0,
     0,   197,   547,     0,   192,   193,     0,     0,   198,     0,
     0,     0,   199,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,   200,   201,     0,     0,   202,
     0,     0,     0,     0,     0,     0,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,   194,   195,     0,     0,
     0,    78,     0,   557,     0,   192,   193,     0,     0,     0,
   196,     0,     0,     0,     0,     0,   197,     0,     0,     0,
     0,     0,     0,   198,     0,     0,     0,   199,     0,     0,
     0,     0,     0,    80,     0,     0,    81,    20,     0,     0,
   200,   201,     0,     0,   202,     0,     0,   194,   195,     0,
     0,     0,    78,   204,   205,   206,   207,     0,     0,   208,
   209,   196,     0,     0,     0,     0,     0,   197,   565,     0,
   192,   193,     0,     0,   198,     0,     0,     0,   199,     0,
     0,     0,     0,     0,    80,     0,     0,    81,    20,     0,
     0,   200,   201,     0,     0,   202,     0,     0,     0,     0,
     0,     0,     0,     0,   204,   205,   206,   207,     0,     0,
   208,   209,   194,   195,     0,     0,     0,    78,     0,   567,
     0,   192,   193,     0,     0,     0,   196,     0,     0,     0,
     0,     0,   197,     0,     0,     0,     0,     0,     0,   198,
     0,     0,     0,   199,     0,     0,     0,     0,     0,    80,
     0,     0,    81,    20,     0,     0,   200,   201,     0,     0,
   202,     0,     0,   194,   195,     0,     0,     0,    78,   204,
   205,   206,   207,     0,     0,   208,   209,   196,     0,     0,
     0,     0,     0,   197,   592,     0,   192,   193,     0,     0,
   198,     0,     0,     0,   199,     0,     0,     0,     0,     0,
    80,     0,     0,    81,    20,     0,     0,   200,   201,     0,
     0,   202,     0,     0,     0,     0,     0,     0,     0,     0,
   204,   205,   206,   207,     0,     0,   208,   209,   194,   195,
     0,     0,     0,    78,     0,   609,     0,   192,   193,     0,
     0,     0,   196,     0,     0,     0,     0,     0,   197,     0,
     0,     0,     0,     0,     0,   198,     0,     0,     0,   199,
     0,     0,     0,     0,     0,    80,     0,     0,    81,    20,
     0,     0,   200,   201,     0,     0,   202,     0,     0,   194,
   195,     0,     0,     0,    78,   204,   205,   206,   207,     0,
     0,   208,   209,   196,     0,     0,     0,     0,     0,   197,
   677,     0,   192,   193,     0,     0,   198,     0,     0,     0,
   199,     0,     0,     0,     0,     0,    80,     0,     0,    81,
    20,     0,     0,   200,   201,     0,     0,   202,     0,     0,
     0,     0,     0,     0,     0,     0,   204,   205,   206,   207,
     0,     0,   208,   209,   194,   195,     0,     0,     0,    78,
     0,   681,     0,   192,   193,     0,     0,     0,   196,     0,
     0,     0,     0,     0,   197,     0,     0,     0,     0,     0,
     0,   198,     0,     0,     0,   199,     0,     0,     0,     0,
     0,    80,     0,     0,    81,    20,     0,     0,   200,   201,
     0,     0,   202,     0,     0,   194,   195,     0,     0,     0,
    78,   204,   205,   206,   207,     0,     0,   208,   209,   196,
     0,     0,     0,     0,     0,   197,   687,     0,   192,   193,
     0,     0,   198,     0,     0,     0,   199,     0,     0,     0,
     0,     0,    80,     0,     0,    81,    20,     0,     0,   200,
   201,     0,     0,   202,     0,     0,     0,     0,     0,     0,
     0,     0,   204,   205,   206,   207,     0,     0,   208,   209,
   194,   195,     0,     0,     0,    78,     0,   713,     0,   192,
   193,     0,     0,     0,   196,     0,     0,     0,     0,     0,
   197,     0,     0,     0,     0,     0,     0,   198,     0,     0,
     0,   199,     0,     0,     0,     0,     0,    80,     0,     0,
    81,    20,     0,     0,   200,   201,     0,     0,   202,     0,
     0,   194,   195,     0,     0,     0,    78,   204,   205,   206,
   207,     0,     0,   208,   209,   196,     0,     0,     0,     0,
     0,   197,     0,   575,     0,     0,     0,     0,   198,     0,
     0,     0,   199,     0,     0,     0,     0,     0,    80,     0,
     0,    81,    20,     0,     0,   200,   201,     0,   662,   202,
     0,     0,     0,     0,     0,     0,     0,     0,   204,   205,
   206,   207,     0,     0,   208,   209,  -289,  -289,  -289,     0,
     0,     0,  -289,     0,     0,     0,     0,     0,     0,     0,
     0,  -289,     0,     0,     0,     0,     0,  -289,     0,     0,
   724,     0,   194,   195,  -289,     0,     0,    78,  -289,     0,
     0,     0,     0,     0,  -289,     0,   196,  -289,  -289,     0,
     0,     0,   197,     0,     0,  -289,     0,   192,   193,   198,
     0,  -289,     0,   199,  -289,  -289,  -289,  -289,     0,    80,
  -289,  -289,    81,    20,   194,   195,     0,     0,     0,    78,
   277,  -297,     0,     0,     0,     0,     0,     0,   196,   204,
   205,   206,   207,     0,   197,   208,   209,   660,     0,   194,
   195,   198,     0,     0,    78,   199,     0,     0,     0,     0,
     0,    80,     0,   196,    81,    20,     0,     0,     0,   197,
     0,     0,   277,  -297,   192,   193,   198,     0,     0,     0,
   199,   204,   205,   206,   207,     0,    80,   208,   209,    81,
    20,   194,   195,   200,   201,     0,    78,   202,     0,   203,
   356,     0,     0,     0,     0,   196,   204,   205,   206,   207,
     0,   197,   208,   209,     0,     0,   194,   195,   198,     0,
     0,    78,   199,     0,     0,   192,   193,     0,    80,     0,
   196,    81,    20,     0,     0,     0,   197,     0,     0,   277,
     0,     0,     0,   198,     0,     0,     0,   199,   204,   205,
   206,   207,     0,    80,   208,   209,    81,    20,     0,     0,
   200,   201,     0,     0,   202,   488,     0,   194,   195,     0,
     0,     0,    78,   204,   205,   206,   207,     0,     0,   208,
   209,   196,     0,     0,     0,     0,     0,   197,     0,     0,
   192,   193,     0,     0,   198,     0,     0,     0,   199,     0,
     0,     0,     0,     0,    80,     0,     0,    81,    20,     0,
     0,   200,   201,     0,     0,   202,   625,     0,     0,     0,
     0,     0,     0,     0,   204,   205,   206,   207,     0,     0,
   208,   209,   194,   195,     0,     0,     0,    78,     0,     0,
     0,   192,   193,     0,     0,     0,   196,     0,     0,     0,
     0,     0,   197,     0,     0,     0,     0,     0,     0,   198,
     0,     0,     0,   199,     0,     0,     0,     0,     0,    80,
     0,     0,    81,    20,     0,     0,   200,   201,     0,     0,
   202,   668,     0,   194,   195,     0,     0,     0,    78,   204,
   205,   206,   207,     0,     0,   208,   209,   196,     0,     0,
     0,     0,     0,   197,     0,     0,   192,   193,     0,     0,
   198,     0,     0,     0,   199,     0,     0,     0,     0,     0,
    80,     0,     0,    81,    20,     0,     0,   200,   201,     0,
     0,   202,   683,     0,     0,     0,     0,     0,     0,     0,
   204,   205,   206,   207,     0,     0,   208,   209,   194,   195,
     0,     0,     0,    78,     0,     0,     0,     0,     0,     0,
     0,     0,   196,     0,     0,     0,     0,     0,   197,     0,
     0,     0,     0,     0,     0,   198,     0,     0,     0,   199,
     0,     0,     0,     0,     0,    80,     0,     0,    81,    20,
     0,     0,   200,   201,     0,     0,   202,     0,     2,   194,
   195,     0,     0,     0,    78,   204,   205,   206,   207,     0,
     0,   208,   209,   196,     0,     0,     0,     0,     0,   197,
     0,     0,     0,     0,     0,     0,   198,     0,     0,     0,
   199,     0,     0,     0,     0,     0,    80,     0,     0,    81,
    20,     0,     0,     0,     0,     0,     0,   277,     0,     0,
     0,     0,     0,     0,     0,     0,   204,   205,   206,   207,
     0,     0,   208,   209
};

static const short yycheck[] = {     3,
   328,    62,     6,   132,    62,   307,   129,   287,   175,   203,
   134,     0,   483,   179,     1,     1,   123,     1,     1,   346,
     9,    10,    11,     1,    83,    42,   353,   354,    89,    46,
     9,    89,     1,    94,    53,     1,    94,   657,    42,   162,
     1,    30,    46,    72,    33,    34,   169,    64,   435,     1,
    94,    62,    69,    97,    33,    86,     1,    86,    62,   294,
    64,   665,    51,   122,   123,    69,     1,   302,   129,   304,
    95,   129,   133,    62,   198,   133,   117,     1,    97,   192,
   193,   194,   195,    94,   636,    89,     1,   200,   201,   476,
    94,     1,     1,    95,    96,     1,     5,    99,   718,   160,
    86,   162,   160,    86,   162,    94,   636,    94,     1,   150,
    94,   128,   153,   100,   175,   502,   100,   134,   129,   180,
     1,    99,   180,   101,   128,   129,     1,    93,     1,   133,
   134,   172,   101,     4,     5,   101,   259,   198,    99,   636,
   129,   202,   613,   470,   133,   697,   473,    99,   700,   101,
   273,   162,   756,     0,    99,   100,   160,     1,   162,    94,
   326,   765,     9,    10,    11,    36,    37,   697,   140,    40,
   700,   175,   279,   162,     1,    99,   180,    86,    88,    50,
     1,   198,     1,    30,    99,   100,    33,    34,   192,   193,
   194,   195,   253,    99,   198,   253,   200,   201,   202,   636,
   697,    94,     1,   700,    51,   329,   100,   100,    79,   596,
     1,   763,    83,   254,     1,    62,   768,    98,   770,     1,
   772,    94,   283,     1,    99,   283,   287,   100,     1,     1,
   202,   203,     1,   763,     1,   622,   403,     1,   768,     1,
   770,     1,   772,     1,     1,   632,   307,    94,    92,   253,
    94,   122,   123,    97,    98,    99,   100,   370,   371,   372,
   697,   648,     1,   700,   253,     1,   763,   328,   139,     1,
   328,   768,    99,   770,    95,   772,   556,    15,    99,   283,
    99,    93,   129,   287,    94,    97,   588,    97,   590,   101,
   294,   485,   101,   175,   266,    94,     1,   269,   302,     1,
   304,   100,     1,   307,   175,   277,     1,    98,   179,    45,
    46,    98,   329,   641,     1,   162,    98,    86,   307,    86,
    98,   382,    95,    95,   328,   329,   763,   434,     1,    86,
    99,   768,    99,   770,    98,   772,    98,   175,    98,   328,
     1,    99,   403,     1,     1,   732,    52,   218,     1,    87,
    86,   631,    88,     1,    97,    91,    92,    93,     1,     1,
    99,    97,    94,   665,   477,   101,   370,   371,   372,   373,
   374,   375,   376,   377,   378,   379,   380,   381,   382,   383,
   384,   385,   386,   387,   388,   389,   553,   448,     1,   361,
   448,    52,     1,   365,    99,    52,    98,   268,    93,   403,
    99,   636,    63,    97,   275,   287,    63,    94,   279,     1,
     1,   100,   294,     0,     1,    88,   287,     1,   390,     1,
   302,    63,   304,   294,     1,   307,     1,     1,     1,    99,
   100,   302,     1,   304,    95,     1,   408,    95,    95,    52,
   720,    94,     0,     1,   448,    93,   418,   570,   420,   287,
   573,    94,     1,    95,   756,   326,   294,    44,   430,    99,
   100,     1,   697,   765,   302,   700,   304,     1,    55,   307,
    94,    93,   343,   477,    61,    67,   100,     1,   607,   483,
    67,   453,    95,   455,    93,     1,    44,     1,   723,     1,
    47,   362,   553,   364,   175,   556,   625,   469,    67,   612,
    95,   736,    86,    61,    95,    62,     0,     1,   480,    67,
   129,    93,    99,   485,    63,   750,    93,    92,    93,    93,
    93,   403,    97,    98,    99,   100,    98,    93,   763,     1,
     1,     1,   403,   768,     1,   770,    50,   772,    97,    96,
     1,    99,   101,   162,   667,    59,    95,   676,     1,   553,
    44,    65,   556,    93,    95,     0,     1,   686,    72,    93,
   431,    55,    76,   434,     1,   403,    96,    61,    82,    93,
   631,    85,    86,    67,    86,    89,    90,    93,    95,    93,
   641,    75,    99,   641,     0,     1,    93,     1,   102,   103,
   104,   105,     1,     1,   108,   109,   657,   129,    99,    44,
     0,     1,     0,     1,     1,    99,   287,     1,   612,   613,
    55,   175,   175,   294,    86,    86,    61,     1,     1,    86,
     1,   302,    67,   304,   628,    86,   307,   631,    44,    95,
   162,    64,   636,    86,    97,    44,    69,   641,   101,    55,
     1,    50,     1,    53,    44,    61,    44,   619,    65,    86,
    44,    67,   641,   657,    99,    72,    50,   718,    94,   720,
     1,    61,     1,    61,    11,   101,     1,    67,    65,    67,
    67,   553,    86,    82,   556,    72,    85,    86,    86,    76,
   129,   653,   553,    99,    95,   556,    97,   659,    82,    86,
   107,    85,    86,   697,   553,   128,   700,   556,    94,    99,
    94,    99,    86,    86,   100,    86,   588,     1,   590,     0,
     1,   160,   100,   162,   718,   553,   720,    96,   556,   723,
    47,   100,   403,   287,   287,    86,    65,    86,    67,     1,
   294,   294,   736,    72,   175,    62,     1,    76,   302,   302,
   304,   304,    94,   307,   307,    86,   750,    86,   100,   631,
   588,    86,   590,    44,   636,   101,    50,    44,   629,   763,
   631,    91,    92,   735,   768,   636,   770,   739,   772,    96,
    61,    44,   631,   101,    61,   657,    67,    50,   101,    44,
    67,    45,    46,   665,    92,    50,   657,    10,    82,    97,
    98,    85,    86,   631,    59,    67,    61,    94,   636,     9,
    72,    11,    67,   100,    76,    91,    92,    30,    99,    82,
   175,    34,    85,    86,    86,   697,    94,    82,   700,   657,
    85,    86,   100,    33,    94,    12,   697,   665,    51,   700,
   100,   470,    13,    97,   473,     1,   718,   101,   720,   403,
   403,   723,    97,   472,    60,   474,   287,   718,    64,   720,
    45,    46,   723,   294,   736,    18,    19,    20,    21,   697,
    14,   302,   700,   304,     1,   736,   307,    99,   750,    95,
    94,    93,   553,    97,   756,   556,    93,   101,    44,   750,
   718,   763,   720,   765,    50,   723,   768,    99,   770,    97,
   772,   175,   763,   697,    57,    99,   700,   768,   736,   770,
    99,   772,    97,     5,     6,     7,   101,   588,    99,   590,
    45,    46,   750,    50,   100,    44,    82,    95,   756,    85,
    86,    50,   287,     3,     4,   763,    66,   765,    93,   294,
   768,    93,   770,     1,   772,    16,    17,   302,    67,   304,
    91,    92,   307,    50,    88,    82,    45,    46,    85,    86,
   631,    93,    59,    82,    99,   636,    85,    86,    65,   763,
    45,    46,   403,    94,   768,    72,   770,     1,   772,    76,
    93,    91,    92,    93,    99,    82,   657,    97,    85,    86,
    94,   101,    89,    90,   665,    94,    93,    97,    98,   553,
   553,    97,   556,   556,    44,   102,   103,   104,   105,     1,
    50,   108,   109,   287,   373,   374,    44,   383,   384,    59,
   294,    61,    50,     8,     9,    10,   697,    67,   302,   700,
   304,    94,   100,   307,   588,   588,   590,   590,   378,   379,
   380,   381,    82,    56,    56,    85,    86,   718,   403,   720,
    99,    94,   723,    94,    82,    95,    96,    85,    86,   192,
   193,   194,   195,     1,    94,   736,    94,   200,   201,    86,
    97,    98,    99,    94,    91,    92,    93,   631,   631,   750,
    97,     1,   636,   636,   101,   756,    56,     1,    99,     3,
     4,    99,   763,    94,   765,    94,     0,   768,     0,   770,
    44,   772,   382,   657,   657,    11,    50,    54,   375,   376,
   377,   665,   665,    86,    75,    59,   162,    61,    91,    92,
    93,   139,   553,    67,    97,   556,   160,   253,   101,   403,
   403,    45,    46,    52,    94,   307,    50,   261,    82,   287,
   655,    85,    86,   697,   697,    59,   700,   700,   307,   571,
   655,    65,    96,   571,   346,   385,   387,   588,    72,   590,
   386,   388,    76,   628,   718,   718,   720,   720,    82,   723,
   723,    85,    86,   393,   389,    89,    90,    -1,    -1,    93,
    -1,    95,   736,   736,    -1,    -1,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,   750,   750,   553,    -1,
   631,   556,   756,   756,    -1,   636,    -1,    -1,    -1,   763,
   763,   765,   765,    -1,   768,   768,   770,   770,   772,   772,
    -1,    91,    92,    93,    -1,    -1,   657,    97,    -1,    -1,
    -1,   101,    -1,   588,   665,   590,    -1,   370,   371,   372,
   373,   374,   375,   376,   377,   378,   379,   380,   381,    -1,
   383,   384,   385,   386,   387,   388,   389,    44,    91,    92,
    93,    -1,    -1,    50,    97,    -1,   697,    -1,   101,   700,
    -1,    -1,    59,    -1,    61,    -1,   631,    -1,    -1,   553,
    67,   636,   556,    -1,    -1,    -1,    -1,   718,    -1,   720,
    -1,    -1,   723,    -1,    -1,    82,    -1,    -1,    85,    86,
    -1,    -1,   657,    -1,    -1,   736,    -1,    -1,    95,    96,
   665,    -1,    -1,    -1,   588,    -1,   590,    -1,    -1,   750,
    -1,   192,   193,   194,   195,   756,    -1,    -1,    -1,   200,
   201,    -1,   763,    -1,   765,    -1,    -1,   768,    -1,   770,
    -1,   772,   697,    -1,   477,   700,    -1,    -1,    -1,    86,
   483,    -1,    -1,    -1,    91,    92,    93,   631,    -1,    -1,
    97,    -1,   636,   718,   101,   720,    -1,     1,   723,     3,
     4,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,   736,    -1,   657,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,   665,    -1,    -1,    -1,   750,    -1,    -1,    -1,    -1,
    -1,   756,    -1,    -1,    -1,    -1,    -1,    -1,   763,    -1,
   765,    45,    46,   768,    -1,   770,    50,   772,    -1,    -1,
    -1,    -1,    -1,   697,    -1,    59,   700,    -1,    -1,    -1,
    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,
    -1,    -1,    76,    -1,   718,    -1,   720,    -1,    82,   723,
    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,
    -1,    -1,   736,    -1,    -1,    99,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,   750,     1,    -1,   612,
   613,    -1,   756,    -1,    -1,    -1,    -1,    -1,    -1,   763,
    -1,   765,    -1,    -1,   768,   628,   770,    -1,   772,   370,
   371,   372,   373,   374,   375,   376,   377,   378,   379,   380,
   381,    -1,   383,   384,   385,   386,   387,   388,   389,    -1,
    44,    45,    46,    47,    48,    49,    50,    51,    -1,    -1,
    54,    -1,    -1,    -1,    58,    59,    -1,    -1,    62,    -1,
    -1,    65,    66,    67,    68,    -1,    70,    71,    72,    73,
     1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    -1,    -1,    -1,    -1,    93,
    -1,    95,    96,    -1,    -1,    99,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    44,    45,    46,    47,    48,    49,    50,
    51,    -1,    -1,    54,    -1,    -1,   477,    58,    59,    -1,
    -1,    62,   483,    -1,    65,    66,    67,    68,     1,    70,
    71,    72,    73,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    -1,    -1,
    -1,    -1,    93,    -1,    95,    96,    -1,    -1,    99,    -1,
    -1,   102,   103,   104,   105,    -1,    -1,   108,   109,    -1,
    -1,    44,    45,    46,    47,    48,    49,    50,    51,    -1,
    -1,    54,    -1,    -1,    -1,    58,    59,    -1,    -1,    62,
    -1,    -1,    65,    66,    67,    68,    -1,    70,    71,    72,
    73,    44,    -1,    76,    -1,    -1,    -1,    50,    -1,    82,
    -1,     1,    85,    86,    -1,    -1,    59,    -1,    61,    -1,
    93,    -1,    95,    96,    67,    -1,    99,    -1,    -1,   102,
   103,   104,   105,    -1,    -1,   108,   109,    -1,    -1,    82,
    -1,    -1,    85,    86,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,   612,   613,    96,    44,    45,    46,    -1,    48,    49,
    50,    51,    -1,    -1,    54,    -1,    -1,   628,    58,    59,
    -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,     1,
    70,    71,    72,    73,    -1,    -1,    76,    -1,    -1,    -1,
    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    -1,
    -1,    -1,    -1,    93,    -1,    95,    96,    -1,    -1,    99,
    -1,    -1,   102,   103,   104,   105,    -1,    -1,   108,   109,
    -1,    -1,    44,    45,    46,    -1,    48,    49,    50,    51,
    -1,    -1,    54,    -1,    -1,    -1,    58,    59,    -1,    -1,
    -1,    -1,    -1,    65,    66,    67,    68,     1,    70,    71,
    72,    73,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
    82,    -1,    -1,    85,    86,    -1,    -1,    -1,    -1,    -1,
    -1,    93,    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,
   102,   103,   104,   105,    -1,    -1,   108,   109,    -1,    -1,
    44,    45,    46,    -1,    48,    49,    50,    51,    -1,    -1,
    54,    -1,    -1,    -1,    58,    59,    -1,    -1,    -1,    -1,
    -1,    65,    66,     1,    68,    -1,    70,    71,    72,    73,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    -1,    -1,    -1,    -1,    93,
    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    44,    45,    46,    -1,
    48,    49,    50,    51,    -1,    -1,    54,    -1,    -1,    -1,
    58,    59,    -1,    -1,    -1,    -1,    -1,    65,    66,    -1,
    68,    -1,    70,    71,    72,    73,    -1,     1,    76,     3,
     4,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
    -1,    -1,    -1,    -1,    -1,    93,    -1,    95,    -1,    -1,
    -1,    99,    -1,    -1,   102,   103,   104,   105,    -1,    -1,
   108,   109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    45,    46,    -1,    -1,    -1,    50,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,
    -1,    65,     1,    -1,     3,     4,    -1,    -1,    72,    -1,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,
    -1,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    45,    46,    -1,    -1,
    -1,    50,    -1,     1,    -1,     3,     4,    -1,    -1,    -1,
    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,
    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,
    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
    89,    90,    -1,    -1,    93,    -1,    -1,    45,    46,    -1,
    -1,    -1,    50,   102,   103,   104,   105,    -1,    -1,   108,
   109,    59,    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,
     3,     4,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,
    98,    -1,    -1,    -1,   102,   103,   104,   105,    -1,    -1,
   108,   109,    45,    46,    -1,    -1,    -1,    50,    -1,     1,
    -1,     3,     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,
    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,
    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,
    93,    94,    -1,    45,    46,    -1,    -1,    -1,    50,   102,
   103,   104,   105,    -1,    -1,   108,   109,    59,    -1,    -1,
    -1,    -1,    -1,    65,     1,    -1,     3,     4,    -1,    -1,
    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,
    -1,    93,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,
   102,   103,   104,   105,    -1,    -1,   108,   109,    45,    46,
    -1,    -1,    -1,    50,    -1,     1,    -1,     3,     4,    -1,
    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,
    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    45,
    46,    98,    -1,    -1,    50,   102,   103,   104,   105,    -1,
    -1,   108,   109,    59,    -1,    -1,    -1,    -1,    -1,    65,
     1,    -1,     3,     4,    -1,    -1,    72,    -1,    -1,    -1,
    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
    86,    -1,    -1,    89,    90,    -1,    -1,    93,    94,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
    -1,    -1,   108,   109,    45,    46,    -1,    -1,    -1,    50,
    -1,     1,    -1,     3,     4,    -1,    -1,    -1,    59,    -1,
    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,
    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,
    -1,    -1,    93,    -1,    -1,    45,    46,    98,    -1,    -1,
    50,   102,   103,   104,   105,    -1,    -1,   108,   109,    59,
    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,     3,     4,
    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,
    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,
    90,    -1,    -1,    93,    94,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,   102,   103,   104,   105,    -1,    -1,   108,   109,
    45,    46,    -1,    -1,    -1,    50,    -1,     1,    -1,     3,
     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,
    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,
    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,    94,
    -1,    45,    46,    -1,    -1,    -1,    50,   102,   103,   104,
   105,    -1,    -1,   108,   109,    59,    -1,    -1,    -1,    -1,
    -1,    65,     1,    -1,     3,     4,    -1,    -1,    72,    -1,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,
    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    45,    46,    -1,    -1,
    -1,    50,    -1,     1,    -1,     3,     4,    -1,    -1,    -1,
    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,
    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,
    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,
    89,    90,    -1,    -1,    93,    -1,    -1,    45,    46,    -1,
    99,    -1,    50,   102,   103,   104,   105,    -1,    -1,   108,
   109,    59,    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,
     3,     4,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,   102,   103,   104,   105,    -1,    -1,
   108,   109,    45,    46,    -1,    -1,    -1,    50,    -1,     1,
    -1,     3,     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,
    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,
    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,
    93,    -1,    -1,    45,    46,    -1,    -1,    -1,    50,   102,
   103,   104,   105,    -1,    -1,   108,   109,    59,    -1,    -1,
    -1,    -1,    -1,    65,     1,    -1,     3,     4,    -1,    -1,
    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,
    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
   102,   103,   104,   105,    -1,    -1,   108,   109,    45,    46,
    -1,    -1,    -1,    50,    -1,     1,    -1,     3,     4,    -1,
    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,
    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    45,
    46,    -1,    -1,    -1,    50,   102,   103,   104,   105,    -1,
    -1,   108,   109,    59,    -1,    -1,    -1,    -1,    -1,    65,
     1,    -1,     3,     4,    -1,    -1,    72,    -1,    -1,    -1,
    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
    -1,    -1,   108,   109,    45,    46,    -1,    -1,    -1,    50,
    -1,     1,    -1,     3,     4,    -1,    -1,    -1,    59,    -1,
    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,
    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,
    -1,    -1,    93,    -1,    -1,    45,    46,    -1,    -1,    -1,
    50,   102,   103,   104,   105,    -1,    -1,   108,   109,    59,
    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,     3,     4,
    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,
    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,
    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,   102,   103,   104,   105,    -1,    -1,   108,   109,
    45,    46,    -1,    -1,    -1,    50,    -1,     1,    -1,     3,
     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,
    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,
    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,
    -1,    45,    46,    -1,    -1,    -1,    50,   102,   103,   104,
   105,    -1,    -1,   108,   109,    59,    -1,    -1,    -1,    -1,
    -1,    65,     1,    -1,     3,     4,    -1,    -1,    72,    -1,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    45,    46,    -1,    -1,
    -1,    50,    -1,     1,    -1,     3,     4,    -1,    -1,    -1,
    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,
    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,
    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,
    89,    90,    -1,    -1,    93,    -1,    -1,    45,    46,    -1,
    -1,    -1,    50,   102,   103,   104,   105,    -1,    -1,   108,
   109,    59,    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,
     3,     4,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,   102,   103,   104,   105,    -1,    -1,
   108,   109,    45,    46,    -1,    -1,    -1,    50,    -1,     1,
    -1,     3,     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,
    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,
    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,
    93,    -1,    -1,    45,    46,    -1,    -1,    -1,    50,   102,
   103,   104,   105,    -1,    -1,   108,   109,    59,    -1,    -1,
    -1,    -1,    -1,    65,     1,    -1,     3,     4,    -1,    -1,
    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,
    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
   102,   103,   104,   105,    -1,    -1,   108,   109,    45,    46,
    -1,    -1,    -1,    50,    -1,     1,    -1,     3,     4,    -1,
    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,
    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    45,
    46,    -1,    -1,    -1,    50,   102,   103,   104,   105,    -1,
    -1,   108,   109,    59,    -1,    -1,    -1,    -1,    -1,    65,
     1,    -1,     3,     4,    -1,    -1,    72,    -1,    -1,    -1,
    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
    -1,    -1,   108,   109,    45,    46,    -1,    -1,    -1,    50,
    -1,     1,    -1,     3,     4,    -1,    -1,    -1,    59,    -1,
    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,
    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,
    -1,    -1,    93,    -1,    -1,    45,    46,    -1,    -1,    -1,
    50,   102,   103,   104,   105,    -1,    -1,   108,   109,    59,
    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,     3,     4,
    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,
    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,
    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,   102,   103,   104,   105,    -1,    -1,   108,   109,
    45,    46,    -1,    -1,    -1,    50,    -1,     1,    -1,     3,
     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,
    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,
    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,
    -1,    45,    46,    -1,    -1,    -1,    50,   102,   103,   104,
   105,    -1,    -1,   108,   109,    59,    -1,    -1,    -1,    -1,
    -1,    65,     1,    -1,     3,     4,    -1,    -1,    72,    -1,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    45,    46,    -1,    -1,
    -1,    50,    -1,     1,    -1,     3,     4,    -1,    -1,    -1,
    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,
    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,
    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,
    89,    90,    -1,    -1,    93,    -1,    -1,    45,    46,    -1,
    -1,    -1,    50,   102,   103,   104,   105,    -1,    -1,   108,
   109,    59,    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,
     3,     4,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,   102,   103,   104,   105,    -1,    -1,
   108,   109,    45,    46,    -1,    -1,    -1,    50,    -1,     1,
    -1,     3,     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,
    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,
    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,
    93,    -1,    -1,    45,    46,    -1,    -1,    -1,    50,   102,
   103,   104,   105,    -1,    -1,   108,   109,    59,    -1,    -1,
    -1,    -1,    -1,    65,     1,    -1,     3,     4,    -1,    -1,
    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,
    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
   102,   103,   104,   105,    -1,    -1,   108,   109,    45,    46,
    -1,    -1,    -1,    50,    -1,     1,    -1,     3,     4,    -1,
    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,
    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    45,
    46,    -1,    -1,    -1,    50,   102,   103,   104,   105,    -1,
    -1,   108,   109,    59,    -1,    -1,    -1,    -1,    -1,    65,
     1,    -1,     3,     4,    -1,    -1,    72,    -1,    -1,    -1,
    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
    -1,    -1,   108,   109,    45,    46,    -1,    -1,    -1,    50,
    -1,     1,    -1,     3,     4,    -1,    -1,    -1,    59,    -1,
    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,
    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,
    -1,    -1,    93,    -1,    -1,    45,    46,    -1,    -1,    -1,
    50,   102,   103,   104,   105,    -1,    -1,   108,   109,    59,
    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,     3,     4,
    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,
    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,
    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,   102,   103,   104,   105,    -1,    -1,   108,   109,
    45,    46,    -1,    -1,    -1,    50,    -1,     1,    -1,     3,
     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,
    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,
    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,
    -1,    45,    46,    -1,    -1,    -1,    50,   102,   103,   104,
   105,    -1,    -1,   108,   109,    59,    -1,    -1,    -1,    -1,
    -1,    65,     1,    -1,     3,     4,    -1,    -1,    72,    -1,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    45,    46,    -1,    -1,
    -1,    50,    -1,     1,    -1,     3,     4,    -1,    -1,    -1,
    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,
    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,
    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,
    89,    90,    -1,    -1,    93,    -1,    -1,    45,    46,    -1,
    -1,    -1,    50,   102,   103,   104,   105,    -1,    -1,   108,
   109,    59,    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,
     3,     4,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,   102,   103,   104,   105,    -1,    -1,
   108,   109,    45,    46,    -1,    -1,    -1,    50,    -1,     1,
    -1,     3,     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,
    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,
    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,
    93,    -1,    -1,    45,    46,    -1,    -1,    -1,    50,   102,
   103,   104,   105,    -1,    -1,   108,   109,    59,    -1,    -1,
    -1,    -1,    -1,    65,     1,    -1,     3,     4,    -1,    -1,
    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,
    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
   102,   103,   104,   105,    -1,    -1,   108,   109,    45,    46,
    -1,    -1,    -1,    50,    -1,     1,    -1,     3,     4,    -1,
    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,
    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,    45,
    46,    -1,    -1,    -1,    50,   102,   103,   104,   105,    -1,
    -1,   108,   109,    59,    -1,    -1,    -1,    -1,    -1,    65,
     1,    -1,     3,     4,    -1,    -1,    72,    -1,    -1,    -1,
    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
    -1,    -1,   108,   109,    45,    46,    -1,    -1,    -1,    50,
    -1,     1,    -1,     3,     4,    -1,    -1,    -1,    59,    -1,
    -1,    -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,
    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,
    -1,    -1,    93,    -1,    -1,    45,    46,    -1,    -1,    -1,
    50,   102,   103,   104,   105,    -1,    -1,   108,   109,    59,
    -1,    -1,    -1,    -1,    -1,    65,     1,    -1,     3,     4,
    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,
    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    -1,    89,
    90,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,   102,   103,   104,   105,    -1,    -1,   108,   109,
    45,    46,    -1,    -1,    -1,    50,    -1,     1,    -1,     3,
     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,
    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,
    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
    85,    86,    -1,    -1,    89,    90,    -1,    -1,    93,    -1,
    -1,    45,    46,    -1,    -1,    -1,    50,   102,   103,   104,
   105,    -1,    -1,   108,   109,    59,    -1,    -1,    -1,    -1,
    -1,    65,    -1,     1,    -1,    -1,    -1,    -1,    72,    -1,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,
    -1,    85,    86,    -1,    -1,    89,    90,    -1,     1,    93,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,
   104,   105,    -1,    -1,   108,   109,    44,    45,    46,    -1,
    -1,    -1,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,    -1,
     1,    -1,    45,    46,    72,    -1,    -1,    50,    76,    -1,
    -1,    -1,    -1,    -1,    82,    -1,    59,    85,    86,    -1,
    -1,    -1,    65,    -1,    -1,    93,    -1,     3,     4,    72,
    -1,    99,    -1,    76,   102,   103,   104,   105,    -1,    82,
   108,   109,    85,    86,    45,    46,    -1,    -1,    -1,    50,
    93,    94,    -1,    -1,    -1,    -1,    -1,    -1,    59,   102,
   103,   104,   105,    -1,    65,   108,   109,     1,    -1,    45,
    46,    72,    -1,    -1,    50,    76,    -1,    -1,    -1,    -1,
    -1,    82,    -1,    59,    85,    86,    -1,    -1,    -1,    65,
    -1,    -1,    93,    94,     3,     4,    72,    -1,    -1,    -1,
    76,   102,   103,   104,   105,    -1,    82,   108,   109,    85,
    86,    45,    46,    89,    90,    -1,    50,    93,    -1,    95,
    96,    -1,    -1,    -1,    -1,    59,   102,   103,   104,   105,
    -1,    65,   108,   109,    -1,    -1,    45,    46,    72,    -1,
    -1,    50,    76,    -1,    -1,     3,     4,    -1,    82,    -1,
    59,    85,    86,    -1,    -1,    -1,    65,    -1,    -1,    93,
    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,   102,   103,
   104,   105,    -1,    82,   108,   109,    85,    86,    -1,    -1,
    89,    90,    -1,    -1,    93,    94,    -1,    45,    46,    -1,
    -1,    -1,    50,   102,   103,   104,   105,    -1,    -1,   108,
   109,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,    -1,
     3,     4,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
    -1,    89,    90,    -1,    -1,    93,    94,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,   102,   103,   104,   105,    -1,    -1,
   108,   109,    45,    46,    -1,    -1,    -1,    50,    -1,    -1,
    -1,     3,     4,    -1,    -1,    -1,    59,    -1,    -1,    -1,
    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    72,
    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,    -1,
    93,    94,    -1,    45,    46,    -1,    -1,    -1,    50,   102,
   103,   104,   105,    -1,    -1,   108,   109,    59,    -1,    -1,
    -1,    -1,    -1,    65,    -1,    -1,     3,     4,    -1,    -1,
    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
    82,    -1,    -1,    85,    86,    -1,    -1,    89,    90,    -1,
    -1,    93,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
   102,   103,   104,   105,    -1,    -1,   108,   109,    45,    46,
    -1,    -1,    -1,    50,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,    65,    -1,
    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
    -1,    -1,    89,    90,    -1,    -1,    93,    -1,    44,    45,
    46,    -1,    -1,    -1,    50,   102,   103,   104,   105,    -1,
    -1,   108,   109,    59,    -1,    -1,    -1,    -1,    -1,    65,
    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,
    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
    86,    -1,    -1,    -1,    -1,    -1,    -1,    93,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
    -1,    -1,   108,   109
};
#define YYPURE 1

/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/cygnus/gnupro-98r2/share/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "/usr/cygnus/gnupro-98r2/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 459 "./parse.y"
{;
    break;}
case 18:
#line 503 "./parse.y"
{ 
		  yyval.node = build_java_array_type (yyvsp[-2].node, -1);
		  CLASS_LOADED_P (yyval.node) = 1;
		;
    break;}
case 19:
#line 508 "./parse.y"
{ yyval.node = build_unresolved_array_type (yyvsp[-2].node); ;
    break;}
case 20:
#line 510 "./parse.y"
{ yyval.node = build_unresolved_array_type (yyvsp[-2].node); ;
    break;}
case 21:
#line 512 "./parse.y"
{RULE ("']' expected"); RECOVER;;
    break;}
case 22:
#line 514 "./parse.y"
{RULE ("']' expected"); RECOVER;;
    break;}
case 26:
#line 529 "./parse.y"
{ yyval.node = make_qualified_name (yyvsp[-2].node, yyvsp[0].node, yyvsp[-1].operator.location); ;
    break;}
case 28:
#line 538 "./parse.y"
{yyval.node = NULL;;
    break;}
case 36:
#line 550 "./parse.y"
{
		  yyval.node = NULL;
		;
    break;}
case 37:
#line 554 "./parse.y"
{
		  yyval.node = NULL;
		;
    break;}
case 40:
#line 566 "./parse.y"
{ ctxp->package = EXPR_WFL_NODE (yyvsp[-1].node); ;
    break;}
case 41:
#line 568 "./parse.y"
{yyerror ("Missing name"); RECOVER;;
    break;}
case 42:
#line 570 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 45:
#line 580 "./parse.y"
{
		  tree name = EXPR_WFL_NODE (yyvsp[-1].node), node, last_name;
		  int   i = IDENTIFIER_LENGTH (name)-1;
		  char *last = &IDENTIFIER_POINTER (name)[i];
		  while (last != IDENTIFIER_POINTER (name))
		    {
		      if (last [0] == '.')
			break;
		      last--;
		    }
		  last_name = get_identifier (++last);
		  if (IS_A_SINGLE_IMPORT_CLASSFILE_NAME_P (last_name))
		    {
		      tree err = find_name_in_single_imports (last_name);
		      if (err && err != name)
			parse_error_context
			  (yyvsp[-1].node, "Ambiguous class: `%s' and `%s'",
			   IDENTIFIER_POINTER (name), 
			   IDENTIFIER_POINTER (err));
		      else
			REGISTER_IMPORT (yyvsp[-1].node, last_name)
		    }
		  else
		    REGISTER_IMPORT (yyvsp[-1].node, last_name);
		;
    break;}
case 46:
#line 606 "./parse.y"
{yyerror ("Missing name"); RECOVER;;
    break;}
case 47:
#line 608 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 48:
#line 613 "./parse.y"
{
		  tree name = EXPR_WFL_NODE (yyvsp[-3].node);
		  /* Don't import java.lang.* twice. */
		  if (name != java_lang_id)
		    {
		      tree node = build_tree_list (yyvsp[-3].node, NULL_TREE);
		      read_import_dir (yyvsp[-3].node);
		      TREE_CHAIN (node) = ctxp->import_demand_list;
		      ctxp->import_demand_list = node;
		    }
		;
    break;}
case 49:
#line 625 "./parse.y"
{yyerror ("'*' expected"); RECOVER;;
    break;}
case 50:
#line 627 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 51:
#line 632 "./parse.y"
{
		  maybe_generate_finit ();
		  maybe_generate_clinit ();
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 52:
#line 638 "./parse.y"
{
		  maybe_generate_clinit ();
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 53:
#line 643 "./parse.y"
{ yyval.node = NULL; ;
    break;}
case 54:
#line 645 "./parse.y"
{
		  YYERROR_NOW;
		  yyerror ("Class or interface declaration expected");
		;
    break;}
case 55:
#line 656 "./parse.y"
{
		  yyval.value = (1 << yyvsp[0].value);
		;
    break;}
case 56:
#line 660 "./parse.y"
{
		  int acc = (1 << yyvsp[0].value);
		  if (yyval.value & acc)
		    parse_error_context 
		      (ctxp->modifier_ctx [yyvsp[0].value], "Modifier `%s' declared twice",
		       java_accstring_lookup (acc));
		  else
		    {
		      yyval.value |= acc;
		    }
		;
    break;}
case 57:
#line 676 "./parse.y"
{ create_class (yyvsp[-4].value, yyvsp[-2].node, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 58:
#line 678 "./parse.y"
{ 
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 59:
#line 682 "./parse.y"
{ create_class (0, yyvsp[-2].node, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 60:
#line 684 "./parse.y"
{ 	
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 61:
#line 688 "./parse.y"
{yyerror ("Missing class name"); RECOVER;;
    break;}
case 62:
#line 690 "./parse.y"
{yyerror ("Missing class name"); RECOVER;;
    break;}
case 63:
#line 692 "./parse.y"
{
		  if (!ctxp->class_err) yyerror ("'{' expected"); 
		  DRECOVER(class1);
		;
    break;}
case 64:
#line 697 "./parse.y"
{if (!ctxp->class_err) yyerror ("'{' expected"); RECOVER;;
    break;}
case 65:
#line 701 "./parse.y"
{ yyval.node = NULL; ;
    break;}
case 66:
#line 703 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 67:
#line 705 "./parse.y"
{yyerror ("'{' expected"); ctxp->class_err=1;;
    break;}
case 68:
#line 707 "./parse.y"
{yyerror ("Missing super class name"); ctxp->class_err=1;;
    break;}
case 69:
#line 711 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 70:
#line 713 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 71:
#line 715 "./parse.y"
{
		  ctxp->class_err=1;
		  yyerror ("Missing interface name"); 
		;
    break;}
case 72:
#line 723 "./parse.y"
{ 
		  ctxp->interface_number = 1;
		  yyval.node = build_tree_list (yyvsp[0].node, NULL_TREE);
		;
    break;}
case 73:
#line 728 "./parse.y"
{ 
		  ctxp->interface_number++;
		  yyval.node = chainon (yyvsp[-2].node, build_tree_list (yyvsp[0].node, NULL_TREE));
		;
    break;}
case 74:
#line 733 "./parse.y"
{yyerror ("Missing interface name"); RECOVER;;
    break;}
case 75:
#line 738 "./parse.y"
{ 
		  /* Store the location of the `}' when doing xrefs */
		  if (flag_emit_xref)
		    DECL_END_SOURCE_LINE (ctxp->current_parsed_class) = 
		      EXPR_WFL_ADD_COL (yyvsp[0].operator.location, 1);
		  yyval.node = ctxp->current_parsed_class;
		;
    break;}
case 76:
#line 746 "./parse.y"
{ 
		  /* Store the location of the `}' when doing xrefs */
		  if (flag_emit_xref)
		    DECL_END_SOURCE_LINE (ctxp->current_parsed_class) = 
		      EXPR_WFL_ADD_COL (yyvsp[0].operator.location, 1);
		  yyval.node = ctxp->current_parsed_class;
		;
    break;}
case 82:
#line 765 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("instance initializer"); ;
    break;}
case 84:
#line 771 "./parse.y"
{ yyval.node = yyvsp[-1].node; ;
    break;}
case 86:
#line 774 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("inner classe declaration"); ;
    break;}
case 87:
#line 776 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("inner interface declaration"); ;
    break;}
case 88:
#line 782 "./parse.y"
{ register_fields (0, yyvsp[-2].node, yyvsp[-1].node); ;
    break;}
case 89:
#line 784 "./parse.y"
{
		  check_modifiers 
		    ("Illegal modifier `%s' for field declaration",
		     yyvsp[-3].value, FIELD_MODIFIERS);
		  check_modifiers_consistency (yyvsp[-3].value);
		  register_fields (yyvsp[-3].value, yyvsp[-2].node, yyvsp[-1].node);
		;
    break;}
case 91:
#line 797 "./parse.y"
{ yyval.node = chainon (yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 92:
#line 799 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 93:
#line 804 "./parse.y"
{ yyval.node = build_tree_list (yyvsp[0].node, NULL_TREE); ;
    break;}
case 94:
#line 806 "./parse.y"
{ 
		  if (java_error_count)
		    yyvsp[0].node = NULL_TREE;
		  yyval.node = build_tree_list 
		    (yyvsp[-2].node, build_assignment (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[-2].node, yyvsp[0].node));
		;
    break;}
case 95:
#line 813 "./parse.y"
{
		  yyerror ("Missing variable initializer");
		  yyval.node = build_tree_list (yyvsp[-2].node, NULL_TREE);
		  RECOVER;
		;
    break;}
case 96:
#line 819 "./parse.y"
{
		  yyerror ("';' expected");
		  yyval.node = build_tree_list (yyvsp[-3].node, NULL_TREE);
		  RECOVER;
		;
    break;}
case 98:
#line 829 "./parse.y"
{ yyval.node = build_unresolved_array_type (yyvsp[-2].node); ;
    break;}
case 99:
#line 831 "./parse.y"
{yyerror ("Invalid declaration"); DRECOVER(vdi);;
    break;}
case 100:
#line 833 "./parse.y"
{yyerror ("']' expected"); DRECOVER(vdi);;
    break;}
case 101:
#line 835 "./parse.y"
{yyerror ("Unbalanced ']'"); DRECOVER(vdi);;
    break;}
case 104:
#line 846 "./parse.y"
{
		  current_function_decl = yyvsp[0].node;
		  source_start_java_method (current_function_decl);
		;
    break;}
case 105:
#line 851 "./parse.y"
{ finish_method_declaration (yyvsp[0].node); ;
    break;}
case 106:
#line 853 "./parse.y"
{YYNOT_TWICE yyerror ("'{' expected"); RECOVER;;
    break;}
case 107:
#line 858 "./parse.y"
{ yyval.node = method_header (0, yyvsp[-2].node, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 108:
#line 860 "./parse.y"
{ yyval.node = method_header (0, void_type_node, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 109:
#line 862 "./parse.y"
{ yyval.node = method_header (yyvsp[-3].value, yyvsp[-2].node, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 110:
#line 864 "./parse.y"
{ yyval.node = method_header (yyvsp[-3].value, void_type_node, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 111:
#line 866 "./parse.y"
{RECOVER;;
    break;}
case 112:
#line 868 "./parse.y"
{RECOVER;;
    break;}
case 113:
#line 870 "./parse.y"
{yyerror ("Identifier expected"); RECOVER;;
    break;}
case 114:
#line 872 "./parse.y"
{yyerror ("Identifier expected"); RECOVER;;
    break;}
case 115:
#line 874 "./parse.y"
{
		  yyerror ("Invalid method declaration, return type required");
		  RECOVER;
		;
    break;}
case 116:
#line 882 "./parse.y"
{ yyval.node = method_declarator (yyvsp[-2].node, NULL_TREE); ;
    break;}
case 117:
#line 884 "./parse.y"
{ yyval.node = method_declarator (yyvsp[-3].node, yyvsp[-1].node); ;
    break;}
case 118:
#line 886 "./parse.y"
{
		  EXPR_WFL_LINECOL (wfl_operator) = yyvsp[-1].operator.location;
		  TREE_PURPOSE (yyvsp[-2].node) = 
		    build_unresolved_array_type (TREE_PURPOSE (yyvsp[-2].node));
		  parse_warning_context 
		    (wfl_operator, 
		     "Discouraged form of returned type specification");
		;
    break;}
case 119:
#line 895 "./parse.y"
{yyerror ("')' expected"); DRECOVER(method_declarator);;
    break;}
case 120:
#line 897 "./parse.y"
{yyerror ("']' expected"); RECOVER;;
    break;}
case 121:
#line 902 "./parse.y"
{
		  ctxp->formal_parameter_number = 1;
		;
    break;}
case 122:
#line 906 "./parse.y"
{
		  ctxp->formal_parameter_number += 1;
		  yyval.node = chainon (yyvsp[-2].node, yyvsp[0].node);
		;
    break;}
case 123:
#line 911 "./parse.y"
{yyerror ("Missing formal parameter term"); RECOVER;;
    break;}
case 124:
#line 916 "./parse.y"
{
		  yyval.node = build_tree_list (yyvsp[0].node, yyvsp[-1].node);
		;
    break;}
case 125:
#line 920 "./parse.y"
{ 
		  parse_jdk1_1_error ("final parameters");
		  yyval.node = build_tree_list (yyvsp[0].node, yyvsp[-1].node);
		;
    break;}
case 126:
#line 925 "./parse.y"
{yyerror ("Missing identifier"); RECOVER;;
    break;}
case 127:
#line 927 "./parse.y"
{
		  SOURCE_FRONTEND_DEBUG (("Modifiers: %d", yyvsp[-2].value));
		  yyerror ("Missing identifier"); RECOVER;
		;
    break;}
case 128:
#line 934 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 129:
#line 936 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 130:
#line 938 "./parse.y"
{yyerror ("Missing class type term"); RECOVER;;
    break;}
case 131:
#line 943 "./parse.y"
{ yyval.node = build_tree_list (yyvsp[0].node, yyvsp[0].node); ;
    break;}
case 132:
#line 945 "./parse.y"
{ yyval.node = tree_cons (yyvsp[0].node, yyvsp[0].node, yyvsp[-2].node); ;
    break;}
case 133:
#line 947 "./parse.y"
{yyerror ("Missing class type term"); RECOVER;;
    break;}
case 136:
#line 954 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 137:
#line 960 "./parse.y"
{
		  TREE_CHAIN (yyvsp[0].node) = ctxp->static_initialized;
		  ctxp->static_initialized = yyvsp[0].node;
		;
    break;}
case 138:
#line 965 "./parse.y"
{
		  TREE_CHAIN (yyvsp[-1].node) = ctxp->static_initialized;
		  ctxp->static_initialized = yyvsp[-1].node;
		;
    break;}
case 139:
#line 973 "./parse.y"
{
		  SOURCE_FRONTEND_DEBUG (("Modifiers: %d", yyvsp[0].value));
		;
    break;}
case 140:
#line 981 "./parse.y"
{
		  current_function_decl = yyvsp[0].node;
		  source_start_java_method (current_function_decl);
		;
    break;}
case 141:
#line 986 "./parse.y"
{ finish_method_declaration (yyvsp[0].node); ;
    break;}
case 142:
#line 991 "./parse.y"
{ yyval.node = method_header (0, NULL_TREE, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 143:
#line 993 "./parse.y"
{ yyval.node = method_header (yyvsp[-2].value, NULL_TREE, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 144:
#line 998 "./parse.y"
{ yyval.node = method_declarator (yyvsp[-2].node, NULL_TREE); ;
    break;}
case 145:
#line 1000 "./parse.y"
{ yyval.node = method_declarator (yyvsp[-3].node, yyvsp[-1].node); ;
    break;}
case 146:
#line 1008 "./parse.y"
{ 
		  BLOCK_EXPR_BODY (yyvsp[0].node) = empty_stmt_node;
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 147:
#line 1013 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 148:
#line 1015 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 149:
#line 1017 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 152:
#line 1027 "./parse.y"
{ 
		  yyval.node = build_method_invocation (yyvsp[-3].node, NULL_TREE); 
		  yyval.node = build_debugable_stmt (EXPR_WFL_LINECOL (yyvsp[-3].node), yyval.node);
		  yyval.node = java_method_add_stmt (current_function_decl, yyval.node);
		;
    break;}
case 153:
#line 1033 "./parse.y"
{ 
		  yyval.node = build_method_invocation (yyvsp[-4].node, yyvsp[-2].node); 
		  yyval.node = build_debugable_stmt (EXPR_WFL_LINECOL (yyvsp[-4].node), yyval.node);
		  yyval.node = java_method_add_stmt (current_function_decl, yyval.node);
		;
    break;}
case 154:
#line 1041 "./parse.y"
{yyval.node = parse_jdk1_1_error ("explicit constructor invocation"); ;
    break;}
case 155:
#line 1043 "./parse.y"
{yyval.node = parse_jdk1_1_error ("explicit constructor invocation"); ;
    break;}
case 156:
#line 1048 "./parse.y"
{
		  tree wfl = build_wfl_node (this_identifier_node);
		  EXPR_WFL_LINECOL (wfl) = yyvsp[0].operator.location;
		  yyval.node = wfl;
		;
    break;}
case 157:
#line 1054 "./parse.y"
{
		  tree wfl = build_wfl_node (super_identifier_node);
		  EXPR_WFL_LINECOL (wfl) = yyvsp[0].operator.location;
		  yyval.node = wfl;
		;
    break;}
case 158:
#line 1065 "./parse.y"
{ create_interface (0, yyvsp[0].node, NULL_TREE); ;
    break;}
case 159:
#line 1067 "./parse.y"
{
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 160:
#line 1071 "./parse.y"
{ create_interface (yyvsp[-2].value, yyvsp[0].node, NULL_TREE); ;
    break;}
case 161:
#line 1073 "./parse.y"
{
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 162:
#line 1077 "./parse.y"
{ create_interface (0, yyvsp[-1].node, yyvsp[0].node);	;
    break;}
case 163:
#line 1079 "./parse.y"
{
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 164:
#line 1083 "./parse.y"
{ create_interface (yyvsp[-3].value, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 165:
#line 1085 "./parse.y"
{
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 166:
#line 1089 "./parse.y"
{yyerror ("'{' expected"); RECOVER;;
    break;}
case 167:
#line 1091 "./parse.y"
{yyerror ("'{' expected"); RECOVER;;
    break;}
case 168:
#line 1096 "./parse.y"
{ 
		  ctxp->interface_number = 1;
		  yyval.node = build_tree_list (yyvsp[0].node, NULL_TREE);
		;
    break;}
case 169:
#line 1101 "./parse.y"
{ 
		  ctxp->interface_number++;
		  yyval.node = chainon (yyvsp[-2].node, build_tree_list (yyvsp[0].node, NULL_TREE));
		;
    break;}
case 170:
#line 1106 "./parse.y"
{yyerror ("Invalid interface type"); RECOVER;;
    break;}
case 171:
#line 1108 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 172:
#line 1113 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 173:
#line 1115 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 178:
#line 1127 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("inner class declaration"); ;
    break;}
case 179:
#line 1129 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("inner interface declaration"); ;
    break;}
case 181:
#line 1138 "./parse.y"
{ 
		  check_abstract_method_header (yyvsp[-1].node);
		  current_function_decl = NULL_TREE; /* FIXME ? */
		;
    break;}
case 182:
#line 1143 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 183:
#line 1149 "./parse.y"
{ yyval.node = build_new_array_init (yyvsp[-1].operator.location, NULL_TREE); ;
    break;}
case 184:
#line 1151 "./parse.y"
{ yyval.node = build_new_array_init (yyvsp[-2].operator.location, yyvsp[-1].node); ;
    break;}
case 185:
#line 1153 "./parse.y"
{ yyval.node = build_new_array_init (yyvsp[-3].operator.location, yyvsp[-2].node); ;
    break;}
case 186:
#line 1158 "./parse.y"
{ 
		  yyval.node = tree_cons (maybe_build_array_element_wfl (yyvsp[0].node), 
				  yyvsp[0].node, NULL_TREE);
		;
    break;}
case 187:
#line 1163 "./parse.y"
{
		  yyval.node = tree_cons (maybe_build_array_element_wfl (yyvsp[0].node), yyvsp[0].node, yyvsp[-2].node);
		;
    break;}
case 188:
#line 1167 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 189:
#line 1173 "./parse.y"
{ 
		  /* Store the location of the `}' when doing xrefs */
		  if (current_function_decl && flag_emit_xref)
		    DECL_END_SOURCE_LINE (current_function_decl) = 
		      EXPR_WFL_ADD_COL (yyvsp[0].operator.location, 1);
		  yyval.node = empty_stmt_node; 
		;
    break;}
case 190:
#line 1181 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 191:
#line 1186 "./parse.y"
{ enter_block (); ;
    break;}
case 192:
#line 1191 "./parse.y"
{ 
		  maybe_absorb_scoping_blocks ();
		  /* Store the location of the `}' when doing xrefs */
		  if (current_function_decl && flag_emit_xref)
		    DECL_END_SOURCE_LINE (current_function_decl) = 
		      EXPR_WFL_ADD_COL (yyvsp[0].operator.location, 1);		  
		  yyval.node = exit_block ();
		;
    break;}
case 196:
#line 1209 "./parse.y"
{ java_method_add_stmt (current_function_decl, yyvsp[0].node); ;
    break;}
case 197:
#line 1211 "./parse.y"
{ parse_jdk1_1_error ("inner class declaration"); ;
    break;}
case 199:
#line 1220 "./parse.y"
{ declare_local_variables (0, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 200:
#line 1222 "./parse.y"
{ declare_local_variables (yyvsp[-2].value, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 206:
#line 1232 "./parse.y"
{ yyval.node = exit_block (); ;
    break;}
case 211:
#line 1241 "./parse.y"
{ yyval.node = exit_block (); ;
    break;}
case 223:
#line 1260 "./parse.y"
{ yyval.node = empty_stmt_node; ;
    break;}
case 224:
#line 1265 "./parse.y"
{
		  yyval.node = build_labeled_block (EXPR_WFL_LINECOL (yyvsp[-1].node), 
					    EXPR_WFL_NODE (yyvsp[-1].node));
		  pushlevel (2);
		  push_labeled_block (yyval.node);
		  PUSH_LABELED_BLOCK (yyval.node);
		;
    break;}
case 225:
#line 1276 "./parse.y"
{ yyval.node = finish_labeled_statement (yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 226:
#line 1278 "./parse.y"
{yyerror ("':' expected"); RECOVER;;
    break;}
case 227:
#line 1283 "./parse.y"
{ yyval.node = finish_labeled_statement (yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 228:
#line 1290 "./parse.y"
{
		  /* We have a statement. Generate a WFL around it so
		     we can debug it */
		  yyval.node = build_expr_wfl (yyvsp[-1].node, input_filename, lineno, 0);
		  /* We know we have a statement, so set the debug
                     info to be eventually generate here. */
		  yyval.node = JAVA_MAYBE_GENERATE_DEBUG_INFO (yyval.node);
		;
    break;}
case 229:
#line 1299 "./parse.y"
{
		  if (ctxp->prevent_ese != lineno)
		    yyerror ("Invalid expression statement");
		  DRECOVER (expr_stmt);
		;
    break;}
case 230:
#line 1305 "./parse.y"
{
		  if (ctxp->prevent_ese != lineno)
		    yyerror ("Invalid expression statement");
		  DRECOVER (expr_stmt);
		;
    break;}
case 231:
#line 1311 "./parse.y"
{
		  if (ctxp->prevent_ese != lineno)
		    yyerror ("Invalid expression statement");
		  DRECOVER (expr_stmt);
		;
    break;}
case 232:
#line 1317 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 233:
#line 1319 "./parse.y"
{
		  yyerror ("Constructor invocation must be first "
			   "thing in a constructor"); 
		  RECOVER;
		;
    break;}
case 234:
#line 1325 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 235:
#line 1327 "./parse.y"
{
		  yyerror ("Constructor invocation must be first "
			   "thing in a constructor"); 
		  RECOVER;
		;
    break;}
case 236:
#line 1333 "./parse.y"
{yyerror ("'(' expected"); RECOVER;;
    break;}
case 237:
#line 1335 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 238:
#line 1337 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 239:
#line 1339 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 240:
#line 1341 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 248:
#line 1356 "./parse.y"
{ 
		  yyval.node = build_if_else_statement (yyvsp[-3].operator.location, yyvsp[-2].node, 
						yyvsp[0].node, NULL_TREE);
		;
    break;}
case 249:
#line 1361 "./parse.y"
{yyerror ("'(' expected"); RECOVER;;
    break;}
case 250:
#line 1363 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 251:
#line 1365 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 252:
#line 1370 "./parse.y"
{ yyval.node = build_if_else_statement (yyvsp[-5].operator.location, yyvsp[-4].node, yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 253:
#line 1375 "./parse.y"
{ yyval.node = build_if_else_statement (yyvsp[-5].operator.location, yyvsp[-4].node, yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 254:
#line 1380 "./parse.y"
{
		  enter_block ();
		;
    break;}
case 255:
#line 1384 "./parse.y"
{ 
		  /* Make into "proper list" of COMPOUND_EXPRs.
		     I.e. make the last statment also have its own
		     COMPOUND_EXPR. */
		  maybe_absorb_scoping_blocks ();
		  TREE_OPERAND (yyvsp[-2].node, 1) = exit_block ();
		  yyval.node = build_debugable_stmt (EXPR_WFL_LINECOL (yyvsp[-2].node), yyvsp[-2].node);
		;
    break;}
case 256:
#line 1396 "./parse.y"
{ 
		  yyval.node = build (SWITCH_EXPR, NULL_TREE, yyvsp[-1].node, NULL_TREE);
		  EXPR_WFL_LINECOL (yyval.node) = yyvsp[-2].operator.location;
		;
    break;}
case 257:
#line 1401 "./parse.y"
{yyerror ("'(' expected"); RECOVER;;
    break;}
case 258:
#line 1403 "./parse.y"
{yyerror ("Missing term or ')'"); DRECOVER(switch_statement);;
    break;}
case 259:
#line 1405 "./parse.y"
{yyerror ("'{' expected"); RECOVER;;
    break;}
case 260:
#line 1413 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 261:
#line 1415 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 262:
#line 1417 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 263:
#line 1419 "./parse.y"
{ yyval.node = NULL_TREE; ;
    break;}
case 269:
#line 1438 "./parse.y"
{ 
		  tree lab = build1 (CASE_EXPR, NULL_TREE, yyvsp[-1].node);
		  EXPR_WFL_LINECOL (lab) = yyvsp[-2].operator.location;
		  java_method_add_stmt (current_function_decl, lab);
		;
    break;}
case 270:
#line 1444 "./parse.y"
{ 
		  tree lab = build1 (DEFAULT_EXPR, NULL_TREE, NULL_TREE);
		  EXPR_WFL_LINECOL (lab) = yyvsp[-1].operator.location;
		  java_method_add_stmt (current_function_decl, lab);
		;
    break;}
case 271:
#line 1450 "./parse.y"
{yyerror ("Missing or invalid constant expression"); RECOVER;;
    break;}
case 272:
#line 1452 "./parse.y"
{yyerror ("':' expected"); RECOVER;;
    break;}
case 273:
#line 1454 "./parse.y"
{yyerror ("':' expected"); RECOVER;;
    break;}
case 274:
#line 1459 "./parse.y"
{ 
		  tree body = build_loop_body (yyvsp[-2].operator.location, yyvsp[-1].node, 0);
		  yyval.node = build_new_loop (body);
		;
    break;}
case 275:
#line 1467 "./parse.y"
{ yyval.node = finish_loop_body (0, NULL_TREE, yyvsp[0].node, 0); ;
    break;}
case 276:
#line 1469 "./parse.y"
{YYERROR_NOW; yyerror ("'(' expected"); RECOVER;;
    break;}
case 277:
#line 1471 "./parse.y"
{yyerror ("Missing term and ')' expected"); RECOVER;;
    break;}
case 278:
#line 1473 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 279:
#line 1478 "./parse.y"
{ yyval.node = finish_loop_body (0, NULL_TREE, yyvsp[0].node, 0); ;
    break;}
case 280:
#line 1483 "./parse.y"
{ 
		  tree body = build_loop_body (0, NULL_TREE, 1);
		  yyval.node = build_new_loop (body);
		;
    break;}
case 281:
#line 1492 "./parse.y"
{ yyval.node = finish_loop_body (yyvsp[-3].operator.location, yyvsp[-2].node, yyvsp[-5].node, 1); ;
    break;}
case 282:
#line 1497 "./parse.y"
{ yyval.node = finish_for_loop (EXPR_WFL_LINECOL (yyvsp[-4].node), yyvsp[-4].node, yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 283:
#line 1499 "./parse.y"
{ 
		  yyval.node = finish_for_loop (0, NULL_TREE, yyvsp[-2].node, yyvsp[0].node);
		  /* We have not condition, so we get rid of the EXIT_EXPR */
		  LOOP_EXPR_BODY_CONDITION_EXPR (LOOP_EXPR_BODY (yyval.node), 0) = 
		    empty_stmt_node;
		;
    break;}
case 284:
#line 1506 "./parse.y"
{yyerror ("Invalid control expression"); RECOVER;;
    break;}
case 285:
#line 1508 "./parse.y"
{yyerror ("Invalid update expression"); RECOVER;;
    break;}
case 286:
#line 1510 "./parse.y"
{yyerror ("Invalid update expression"); RECOVER;;
    break;}
case 287:
#line 1515 "./parse.y"
{ yyval.node = finish_for_loop (EXPR_WFL_LINECOL (yyvsp[-4].node), yyvsp[-4].node, yyvsp[-2].node, yyvsp[0].node);;
    break;}
case 288:
#line 1517 "./parse.y"
{ 
		  yyval.node = finish_for_loop (0, NULL_TREE, yyvsp[-2].node, yyvsp[0].node);
		  /* We have not condition, so we get rid of the EXIT_EXPR */
		  LOOP_EXPR_BODY_CONDITION_EXPR (LOOP_EXPR_BODY (yyval.node), 0) = 
		    empty_stmt_node;
		;
    break;}
case 289:
#line 1527 "./parse.y"
{ 
		  /* This scope defined for local variable that may be
                     defined within the scope of the for loop */
		  enter_block (); 
		;
    break;}
case 290:
#line 1533 "./parse.y"
{yyerror ("'(' expected"); DRECOVER(for_1);;
    break;}
case 291:
#line 1535 "./parse.y"
{yyerror ("Invalid init statement"); RECOVER;;
    break;}
case 292:
#line 1540 "./parse.y"
{ 
		  /* We now declare the loop body. The loop is
                     declared as a for loop. */
		  tree body = build_loop_body (0, NULL_TREE, 0);
		  yyval.node =  build_new_loop (body);
		  IS_FOR_LOOP_P (yyval.node) = 1;
		  /* The loop is added to the current block the for
                     statement is defined within */
		  java_method_add_stmt (current_function_decl, yyval.node);
		;
    break;}
case 293:
#line 1552 "./parse.y"
{ yyval.node = empty_stmt_node; ;
    break;}
case 294:
#line 1554 "./parse.y"
{ 
		  /* Init statement recorded within the previously
                     defined block scope */
		  yyval.node = java_method_add_stmt (current_function_decl, yyvsp[0].node);
		;
    break;}
case 295:
#line 1560 "./parse.y"
{ 
		  /* Local variable are recorded within the previously
		     defined block scope */
		  yyval.node = NULL_TREE;
		;
    break;}
case 296:
#line 1566 "./parse.y"
{yyerror ("';' expected"); DRECOVER(for_init_1);;
    break;}
case 297:
#line 1570 "./parse.y"
{yyval.node = empty_stmt_node;;
    break;}
case 298:
#line 1572 "./parse.y"
{ yyval.node = build_debugable_stmt (BUILD_LOCATION (), yyvsp[0].node); ;
    break;}
case 299:
#line 1577 "./parse.y"
{ yyval.node = add_stmt_to_compound (NULL_TREE, NULL_TREE, yyvsp[0].node); ;
    break;}
case 300:
#line 1579 "./parse.y"
{ yyval.node = add_stmt_to_compound (yyvsp[-2].node, NULL_TREE, yyvsp[0].node); ;
    break;}
case 301:
#line 1581 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 302:
#line 1586 "./parse.y"
{ yyval.node = build_bc_statement (yyvsp[-1].operator.location, 1, NULL_TREE); ;
    break;}
case 303:
#line 1588 "./parse.y"
{ yyval.node = build_bc_statement (yyvsp[-2].operator.location, 1, yyvsp[-1].node); ;
    break;}
case 304:
#line 1590 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 305:
#line 1592 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 306:
#line 1597 "./parse.y"
{ yyval.node = build_bc_statement (yyvsp[-1].operator.location, 0, NULL_TREE); ;
    break;}
case 307:
#line 1599 "./parse.y"
{ yyval.node = build_bc_statement (yyvsp[-2].operator.location, 0, yyvsp[-1].node); ;
    break;}
case 308:
#line 1601 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 309:
#line 1603 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 310:
#line 1608 "./parse.y"
{ yyval.node = build_return (yyvsp[-1].operator.location, NULL_TREE); ;
    break;}
case 311:
#line 1610 "./parse.y"
{ yyval.node = build_return (yyvsp[-2].operator.location, yyvsp[-1].node); ;
    break;}
case 312:
#line 1612 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 313:
#line 1614 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 314:
#line 1619 "./parse.y"
{ 
		  yyval.node = build1 (THROW_EXPR, NULL_TREE, yyvsp[-1].node);
		  EXPR_WFL_LINECOL (yyval.node) = yyvsp[-2].operator.location;
		;
    break;}
case 315:
#line 1624 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 316:
#line 1626 "./parse.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 317:
#line 1631 "./parse.y"
{ 
		  yyval.node = build (SYNCHRONIZED_EXPR, NULL_TREE, yyvsp[-2].node, yyvsp[0].node);
		  EXPR_WFL_LINECOL (yyval.node) = 
		    EXPR_WFL_LINECOL (MODIFIER_WFL (SYNCHRONIZED_TK));
		;
    break;}
case 318:
#line 1637 "./parse.y"
{yyerror ("'{' expected"); RECOVER;;
    break;}
case 319:
#line 1639 "./parse.y"
{yyerror ("'(' expected"); RECOVER;;
    break;}
case 320:
#line 1641 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 321:
#line 1643 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 322:
#line 1648 "./parse.y"
{
		  if ((1 << yyvsp[0].value) != ACC_SYNCHRONIZED)
		    fatal ("synchronized was '%d' - yyparse", (1 << yyvsp[0].value));
		;
    break;}
case 323:
#line 1656 "./parse.y"
{ yyval.node = build_try_statement (yyvsp[-2].operator.location, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 324:
#line 1658 "./parse.y"
{ yyval.node = build_try_finally_statement (yyvsp[-2].operator.location, yyvsp[-1].node, yyvsp[0].node); ;
    break;}
case 325:
#line 1660 "./parse.y"
{ yyval.node = build_try_finally_statement 
		    (yyvsp[-3].operator.location, build_try_statement (yyvsp[-3].operator.location,
						       yyvsp[-2].node, yyvsp[-1].node), yyvsp[0].node);
		;
    break;}
case 326:
#line 1665 "./parse.y"
{yyerror ("'{' expected"); DRECOVER (try_statement);;
    break;}
case 328:
#line 1671 "./parse.y"
{ 
		  TREE_CHAIN (yyvsp[0].node) = yyvsp[-1].node;
		  yyval.node = yyvsp[0].node;
		;
    break;}
case 329:
#line 1679 "./parse.y"
{ 
		  java_method_add_stmt (current_function_decl, yyvsp[0].node);
		  exit_block ();
		  yyval.node = yyvsp[-1].node;
		;
    break;}
case 330:
#line 1687 "./parse.y"
{ 
		  /* We add a block to define a scope for
		     formal_parameter (CCBP). The formal parameter is
		     declared initialized by the appropriate function
		     call */
		  tree ccpb = enter_block ();
		  tree init = build_assignment (ASSIGN_TK, yyvsp[-2].operator.location, 
						TREE_PURPOSE (yyvsp[-1].node), 
						soft_exceptioninfo_call_node);
		  declare_local_variables (0, TREE_VALUE (yyvsp[-1].node),
					   build_tree_list (TREE_PURPOSE (yyvsp[-1].node),
							    init));
		  yyval.node = build1 (CATCH_EXPR, NULL_TREE, ccpb);
		  EXPR_WFL_LINECOL (yyval.node) = yyvsp[-3].operator.location;
		;
    break;}
case 331:
#line 1703 "./parse.y"
{yyerror ("'(' expected"); RECOVER;;
    break;}
case 332:
#line 1705 "./parse.y"
{yyerror ("Missing term or ')' expected"); DRECOVER (2);;
    break;}
case 333:
#line 1707 "./parse.y"
{yyerror ("')' expected"); DRECOVER (1);;
    break;}
case 334:
#line 1712 "./parse.y"
{ yyval.node = yyvsp[0].node; ;
    break;}
case 335:
#line 1714 "./parse.y"
{yyerror ("'{' expected"); RECOVER; ;
    break;}
case 339:
#line 1726 "./parse.y"
{ yyval.node = build_this (yyvsp[0].operator.location); ;
    break;}
case 340:
#line 1728 "./parse.y"
{yyval.node = yyvsp[-1].node;;
    break;}
case 345:
#line 1737 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("named class literals"); ;
    break;}
case 346:
#line 1739 "./parse.y"
{ yyval.node = build_class_ref (yyvsp[-2].node); ;
    break;}
case 347:
#line 1741 "./parse.y"
{ yyval.node = build_class_ref (void_type_node); ;
    break;}
case 348:
#line 1746 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("class literals"); ;
    break;}
case 349:
#line 1748 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 350:
#line 1750 "./parse.y"
{yyerror ("'class' or 'this' expected" ); RECOVER;;
    break;}
case 351:
#line 1752 "./parse.y"
{yyerror ("'class' expected" ); RECOVER;;
    break;}
case 352:
#line 1754 "./parse.y"
{yyerror ("'class' expected" ); RECOVER;;
    break;}
case 353:
#line 1759 "./parse.y"
{ yyval.node = build_new_invocation (yyvsp[-3].node, yyvsp[-1].node); ;
    break;}
case 354:
#line 1761 "./parse.y"
{ yyval.node = build_new_invocation (yyvsp[-2].node, NULL_TREE); ;
    break;}
case 355:
#line 1766 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("inner class instance creation"); ;
    break;}
case 356:
#line 1768 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("inner class instance creation"); ;
    break;}
case 361:
#line 1777 "./parse.y"
{yyerror ("'(' expected"); DRECOVER(new_1);;
    break;}
case 362:
#line 1779 "./parse.y"
{yyerror ("'(' expected"); RECOVER;;
    break;}
case 363:
#line 1781 "./parse.y"
{yyerror ("')' or term expected"); RECOVER;;
    break;}
case 364:
#line 1783 "./parse.y"
{yyerror ("')' expected"); RECOVER;;
    break;}
case 365:
#line 1785 "./parse.y"
{YYERROR_NOW; yyerror ("Identifier expected"); RECOVER;;
    break;}
case 366:
#line 1787 "./parse.y"
{yyerror ("'(' expected"); RECOVER;;
    break;}
case 369:
#line 1797 "./parse.y"
{ 
		  yyval.node = tree_cons (NULL_TREE, yyvsp[0].node, NULL_TREE);
		  ctxp->formal_parameter_number = 1; 
		;
    break;}
case 370:
#line 1802 "./parse.y"
{
		  ctxp->formal_parameter_number += 1;
		  yyval.node = tree_cons (NULL_TREE, yyvsp[0].node, yyvsp[-2].node);
		;
    break;}
case 371:
#line 1807 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 372:
#line 1812 "./parse.y"
{ yyval.node = build_newarray_node (yyvsp[-1].node, yyvsp[0].node, 0); ;
    break;}
case 373:
#line 1814 "./parse.y"
{ yyval.node = build_newarray_node (yyvsp[-1].node, yyvsp[0].node, 0); ;
    break;}
case 374:
#line 1816 "./parse.y"
{ yyval.node = build_newarray_node (yyvsp[-2].node, yyvsp[-1].node, CURRENT_OSB (ctxp));;
    break;}
case 375:
#line 1818 "./parse.y"
{ yyval.node = build_newarray_node (yyvsp[-2].node, yyvsp[-1].node, CURRENT_OSB (ctxp));;
    break;}
case 376:
#line 1822 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("anonymous array"); ;
    break;}
case 377:
#line 1824 "./parse.y"
{ yyval.node = parse_jdk1_1_error ("anonymous array"); ;
    break;}
case 378:
#line 1826 "./parse.y"
{yyerror ("'[' expected"); DRECOVER ("]");;
    break;}
case 379:
#line 1828 "./parse.y"
{yyerror ("']' expected"); RECOVER;;
    break;}
case 380:
#line 1833 "./parse.y"
{ yyval.node = build_tree_list (NULL_TREE, yyvsp[0].node); ;
    break;}
case 381:
#line 1835 "./parse.y"
{ yyval.node = tree_cons (NULL_TREE, yyvsp[0].node, yyval.node); ;
    break;}
case 382:
#line 1840 "./parse.y"
{ 
		  EXPR_WFL_LINECOL (yyvsp[-1].node) = yyvsp[-2].operator.location;
		  yyval.node = yyvsp[-1].node;
		;
    break;}
case 383:
#line 1845 "./parse.y"
{yyerror ("']' expected"); RECOVER;;
    break;}
case 384:
#line 1847 "./parse.y"
{
		  yyerror ("Missing term");
		  yyerror ("']' expected");
		  RECOVER;
		;
    break;}
case 385:
#line 1856 "./parse.y"
{ 
		  int allocate = 0;
		  /* If not initialized, allocate memory for the osb
                     numbers stack */
		  if (!ctxp->osb_limit)
		    {
		      allocate = ctxp->osb_limit = 32;
		      ctxp->osb_depth = -1;
		    }
		  /* If capacity overflown, reallocate a bigger chuck */
		  else if (ctxp->osb_depth+1 == ctxp->osb_limit)
		    allocate = ctxp->osb_limit << 1;
		  
		  if (allocate)
		    {
		      allocate *= sizeof (int);
		      if (ctxp->osb_number)
			ctxp->osb_number = (int *)xrealloc (ctxp->osb_number,
							    allocate);
		      else
			ctxp->osb_number = (int *)xmalloc (allocate);
		    }
		  ctxp->osb_depth++;
		  CURRENT_OSB (ctxp) = 1;
		;
    break;}
case 386:
#line 1882 "./parse.y"
{ CURRENT_OSB (ctxp)++; ;
    break;}
case 387:
#line 1884 "./parse.y"
{ yyerror ("']' expected"); RECOVER;;
    break;}
case 388:
#line 1889 "./parse.y"
{ yyval.node = make_qualified_primary (yyvsp[-2].node, yyvsp[0].node, yyvsp[-1].operator.location); ;
    break;}
case 389:
#line 1893 "./parse.y"
{
		  tree super_wfl = 
		    build_wfl_node (super_identifier_node);
		  EXPR_WFL_LINECOL (super_wfl) = yyvsp[-2].operator.location;
		  yyval.node = make_qualified_name (super_wfl, yyvsp[0].node, yyvsp[-1].operator.location);
		;
    break;}
case 390:
#line 1900 "./parse.y"
{yyerror ("Field expected"); DRECOVER (super_field_acces);;
    break;}
case 391:
#line 1905 "./parse.y"
{ yyval.node = build_method_invocation (yyvsp[-2].node, NULL_TREE); ;
    break;}
case 392:
#line 1907 "./parse.y"
{ yyval.node = build_method_invocation (yyvsp[-3].node, yyvsp[-1].node); ;
    break;}
case 393:
#line 1909 "./parse.y"
{ 
		  if (TREE_CODE (yyvsp[-4].node) == THIS_EXPR)
		    yyval.node = build_this_super_qualified_invocation 
		      (1, yyvsp[-2].node, NULL_TREE, 0, yyvsp[-3].operator.location);
		  else
		    {
		      tree invok = build_method_invocation (yyvsp[-2].node, NULL_TREE);
		      yyval.node = make_qualified_primary (yyvsp[-4].node, invok, yyvsp[-3].operator.location);
		    }
		;
    break;}
case 394:
#line 1920 "./parse.y"
{ 
		  if (TREE_CODE (yyvsp[-5].node) == THIS_EXPR)
		    yyval.node = build_this_super_qualified_invocation 
		      (1, yyvsp[-3].node, yyvsp[-1].node, 0, yyvsp[-4].operator.location);
		  else
		    {
		      tree invok = build_method_invocation (yyvsp[-3].node, yyvsp[-1].node);
		      yyval.node = make_qualified_primary (yyvsp[-5].node, invok, yyvsp[-4].operator.location);
		    }
		;
    break;}
case 395:
#line 1931 "./parse.y"
{ 
		  yyval.node = build_this_super_qualified_invocation 
		    (0, yyvsp[-2].node, NULL_TREE, yyvsp[-4].operator.location, yyvsp[-3].operator.location);
		;
    break;}
case 396:
#line 1936 "./parse.y"
{
		  yyval.node = build_this_super_qualified_invocation 
		    (0, yyvsp[-3].node, yyvsp[-1].node, yyvsp[-5].operator.location, yyvsp[-4].operator.location);
		;
    break;}
case 397:
#line 1945 "./parse.y"
{ yyerror ("'(' expected"); DRECOVER (method_invocation); ;
    break;}
case 398:
#line 1947 "./parse.y"
{ yyerror ("'(' expected"); DRECOVER (method_invocation); ;
    break;}
case 399:
#line 1952 "./parse.y"
{ yyval.node = build_array_ref (yyvsp[-2].operator.location, yyvsp[-3].node, yyvsp[-1].node); ;
    break;}
case 400:
#line 1954 "./parse.y"
{ yyval.node = build_array_ref (yyvsp[-2].operator.location, yyvsp[-3].node, yyvsp[-1].node); ;
    break;}
case 401:
#line 1956 "./parse.y"
{
		  yyerror ("Missing term and ']' expected");
		  DRECOVER(array_access);
		;
    break;}
case 402:
#line 1961 "./parse.y"
{
		  yyerror ("']' expected");
		  DRECOVER(array_access);
		;
    break;}
case 403:
#line 1966 "./parse.y"
{
		  yyerror ("Missing term and ']' expected");
		  DRECOVER(array_access);
		;
    break;}
case 404:
#line 1971 "./parse.y"
{
		  yyerror ("']' expected");
		  DRECOVER(array_access);
		;
    break;}
case 409:
#line 1986 "./parse.y"
{ yyval.node = build_incdec (yyvsp[0].operator.token, yyvsp[0].operator.location, yyvsp[-1].node, 1); ;
    break;}
case 410:
#line 1991 "./parse.y"
{ yyval.node = build_incdec (yyvsp[0].operator.token, yyvsp[0].operator.location, yyvsp[-1].node, 1); ;
    break;}
case 413:
#line 1998 "./parse.y"
{yyval.node = build_unaryop (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[0].node); ;
    break;}
case 414:
#line 2000 "./parse.y"
{yyval.node = build_unaryop (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[0].node); ;
    break;}
case 416:
#line 2003 "./parse.y"
{yyerror ("Missing term"); RECOVER;
    break;}
case 417:
#line 2005 "./parse.y"
{yyerror ("Missing term"); RECOVER;
    break;}
case 418:
#line 2010 "./parse.y"
{yyval.node = build_incdec (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[0].node, 0); ;
    break;}
case 419:
#line 2012 "./parse.y"
{yyerror ("Missing term"); RECOVER;
    break;}
case 420:
#line 2017 "./parse.y"
{yyval.node = build_incdec (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[0].node, 0); ;
    break;}
case 421:
#line 2019 "./parse.y"
{yyerror ("Missing term"); RECOVER;
    break;}
case 423:
#line 2025 "./parse.y"
{yyval.node = build_unaryop (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[0].node); ;
    break;}
case 424:
#line 2027 "./parse.y"
{yyval.node = build_unaryop (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[0].node); ;
    break;}
case 426:
#line 2030 "./parse.y"
{yyerror ("Missing term"); RECOVER;
    break;}
case 427:
#line 2032 "./parse.y"
{yyerror ("Missing term"); RECOVER;
    break;}
case 428:
#line 2037 "./parse.y"
{ 
		  tree type = yyvsp[-3].node;
		  while (CURRENT_OSB (ctxp)--)
		    type = build_java_array_type (type, -1);
		  ctxp->osb_depth--;
		  yyval.node = build_cast (yyvsp[-4].operator.location, type, yyvsp[0].node); 
		;
    break;}
case 429:
#line 2045 "./parse.y"
{ yyval.node = build_cast (yyvsp[-3].operator.location, yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 430:
#line 2047 "./parse.y"
{ yyval.node = build_cast (yyvsp[-3].operator.location, yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 431:
#line 2049 "./parse.y"
{ 
		  char *ptr;
		  while (CURRENT_OSB (ctxp)--)
		    obstack_1grow (&temporary_obstack, '[');
		  ctxp->osb_depth--;
		  obstack_grow0 (&temporary_obstack, 
				 IDENTIFIER_POINTER (EXPR_WFL_NODE (yyvsp[-3].node)),
				 IDENTIFIER_LENGTH (EXPR_WFL_NODE (yyvsp[-3].node)));
		  ptr = obstack_finish (&temporary_obstack);
		  EXPR_WFL_NODE (yyvsp[-3].node) = get_identifier (ptr);
		  yyval.node = build_cast (yyvsp[-4].operator.location, yyvsp[-3].node, yyvsp[0].node);
		;
    break;}
case 432:
#line 2062 "./parse.y"
{yyerror ("']' expected, invalid type expression");;
    break;}
case 433:
#line 2064 "./parse.y"
{
	          if (ctxp->prevent_ese != lineno)
		    yyerror ("Invalid type expression"); RECOVER;
		  RECOVER;
		;
    break;}
case 434:
#line 2070 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 435:
#line 2072 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 436:
#line 2074 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 438:
#line 2080 "./parse.y"
{ 
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), 
				    yyvsp[-1].operator.location, yyvsp[-2].node, yyvsp[0].node);
		;
    break;}
case 439:
#line 2085 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 440:
#line 2090 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 441:
#line 2095 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 442:
#line 2097 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 443:
#line 2099 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 445:
#line 2105 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 446:
#line 2110 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 447:
#line 2115 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 448:
#line 2117 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 450:
#line 2123 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 451:
#line 2128 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 452:
#line 2133 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 453:
#line 2138 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 454:
#line 2140 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 455:
#line 2142 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 457:
#line 2148 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 458:
#line 2153 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 459:
#line 2158 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 460:
#line 2163 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 461:
#line 2168 "./parse.y"
{ yyval.node = build_binop (INSTANCEOF_EXPR, yyvsp[-1].operator.location, yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 462:
#line 2170 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 463:
#line 2172 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 464:
#line 2174 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 465:
#line 2176 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 466:
#line 2178 "./parse.y"
{yyerror ("Invalid reference type"); RECOVER;;
    break;}
case 468:
#line 2184 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 469:
#line 2189 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 470:
#line 2194 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 471:
#line 2196 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 473:
#line 2202 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 474:
#line 2207 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 476:
#line 2213 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 477:
#line 2218 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 479:
#line 2224 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 480:
#line 2229 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 482:
#line 2235 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 483:
#line 2240 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 485:
#line 2246 "./parse.y"
{
		  yyval.node = build_binop (BINOP_LOOKUP (yyvsp[-1].operator.token), yyvsp[-1].operator.location,
				    yyvsp[-2].node, yyvsp[0].node); 
		;
    break;}
case 486:
#line 2251 "./parse.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 488:
#line 2257 "./parse.y"
{
		  yyval.node = build (CONDITIONAL_EXPR, NULL_TREE, yyvsp[-4].node, yyvsp[-2].node, yyvsp[0].node);
		  EXPR_WFL_LINECOL (yyval.node) = yyvsp[-3].operator.location;
		;
    break;}
case 489:
#line 2262 "./parse.y"
{
		  YYERROR_NOW;
		  yyerror ("Missing term");
		  DRECOVER (1);
		;
    break;}
case 490:
#line 2268 "./parse.y"
{yyerror ("Missing term"); DRECOVER (2);;
    break;}
case 491:
#line 2270 "./parse.y"
{yyerror ("Missing term"); DRECOVER (3);;
    break;}
case 494:
#line 2280 "./parse.y"
{ yyval.node = build_assignment (yyvsp[-1].operator.token, yyvsp[-1].operator.location, yyvsp[-2].node, yyvsp[0].node); ;
    break;}
case 495:
#line 2282 "./parse.y"
{
		  if (ctxp->prevent_ese != lineno)
		    yyerror ("Missing term");
		  DRECOVER (assign);
		;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/cygnus/gnupro-98r2/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 2308 "./parse.y"



/* Flag for the error report routine to issue the error the first time
   it's called (overriding the default behavior which is to drop the
   first invocation and honor the second one, taking advantage of a
   richer context.  */
static int force_error = 0;

/* Create a new parser context and make it the current one. */

void
java_push_parser_context ()
{
  struct parser_ctxt *new = 
    (struct parser_ctxt *)xmalloc(sizeof (struct parser_ctxt));

  bzero ((PTR) new, sizeof (struct parser_ctxt));
  new->next = ctxp;
  ctxp = new;
  if (ctxp->next)
    {
      ctxp->incomplete_class = ctxp->next->incomplete_class;
      ctxp->gclass_list = ctxp->next->gclass_list;
    }
}  

/* If the first file of a file list was a class file, no context
   exists for a source file to be parsed. This boolean remembers that
   java_parser_context_save_global might have created a dummy one, so
   that java_parser_context_restore_global can pop it.  */
static int extra_ctxp_pushed_p = 0;

void
java_parser_context_save_global ()
{
  if (!ctxp)
    {
      java_push_parser_context ();
      extra_ctxp_pushed_p = 1;
    }
  ctxp->finput = finput;
  ctxp->lineno = lineno;
  ctxp->current_class = current_class;
  ctxp->filename = input_filename;
  ctxp->current_function_decl = current_function_decl;
}

void
java_parser_context_restore_global ()
{
  finput = ctxp->finput;
  lineno = ctxp->lineno;
  current_class = ctxp->current_class;
  input_filename = ctxp->filename;
  current_function_decl = ctxp->current_function_decl;
  if (!ctxp->next && extra_ctxp_pushed_p)
    {
      java_pop_parser_context (0);
      extra_ctxp_pushed_p = 0;
    }
}

void 
java_pop_parser_context (generate)
     int generate;
{
  tree current;
  struct parser_ctxt *toFree, *next;

  if (!ctxp)
    return;

  toFree = ctxp;
  next = ctxp->next;
  if (next)
    {
      next->incomplete_class = ctxp->incomplete_class;
      next->gclass_list = ctxp->gclass_list;
      lineno = ctxp->lineno;
      finput = ctxp->finput;
      current_class = ctxp->current_class;
    }

  /* Set the single import class file flag to 0 for the current list
     of imported things */
  for (current = ctxp->import_list; current; current = TREE_CHAIN (current))
    IS_A_SINGLE_IMPORT_CLASSFILE_NAME_P (TREE_PURPOSE (current)) = 0;

  /* And restore those of the previous context */
  if ((ctxp = next))		/* Assignment is really meant here */
    for (current = ctxp->import_list; current; current = TREE_CHAIN (current))
      IS_A_SINGLE_IMPORT_CLASSFILE_NAME_P (TREE_PURPOSE (current)) = 1;

  if (generate)
    {
      toFree->next = ctxp_for_generation;
      ctxp_for_generation = toFree;
    }
  else
    free (toFree);
}

/* Reporting JDK1.1 features not implemented */

static tree
parse_jdk1_1_error (msg)
    char *msg;
{
  sorry (": `%s' JDK1.1(TM) feature", msg);
  java_error_count++;
  return empty_stmt_node;
}

static int do_warning = 0;

void
yyerror (msg)
     char *msg;
{
  static java_lc elc;
  static int  prev_lineno;
  static char *prev_msg;

  int save_lineno;
  char *remainder, *code_from_source;
  extern struct obstack temporary_obstack;
  
  if (!force_error && prev_lineno == lineno)
    return;

  /* Save current error location but report latter, when the context is
     richer.  */
  if (ctxp->java_error_flag == 0)
    {
      ctxp->java_error_flag = 1;
      elc = ctxp->elc;
      /* Do something to use the previous line if we're reaching the
	 end of the file... */
#ifdef VERBOSE_SKELETON
      printf ("* Error detected (%s)\n", (msg ? msg : "(null)"));
#endif
      return;
    }

  /* Ignore duplicate message on the same line. BTW, this is dubious. FIXME */
  if (!force_error && msg == prev_msg && prev_lineno == elc.line)
    return;

  ctxp->java_error_flag = 0;
  if (do_warning)
    java_warning_count++;
  else
    java_error_count++;
  
  if (elc.col == 0 && msg[1] == ';')
    {
      elc.col  = ctxp->p_line->char_col-1;
      elc.line = ctxp->p_line->lineno;
    }

  save_lineno = lineno;
  prev_lineno = lineno = elc.line;
  prev_msg = msg;

  code_from_source = java_get_line_col (ctxp->filename, elc.line, elc.col);
  obstack_grow0 (&temporary_obstack, 
		 code_from_source, strlen (code_from_source));
  remainder = obstack_finish (&temporary_obstack);
  if (do_warning)
    warning ("%s.\n%s", msg, remainder);
  else
    error ("%s.\n%s", msg, remainder);

  /* This allow us to cheaply avoid an extra 'Invalid expression
     statement' error report when errors have been already reported on
     the same line. This occurs when we report an error but don't have
     a synchronization point other than ';', which
     expression_statement is the only one to take care of.  */
  ctxp->prevent_ese = lineno = save_lineno;
}

static void
issue_warning_error_from_context (cl, msg, ap)
     tree cl;
     const char *msg;
     va_list ap;
{
  char *saved, *saved_input_filename;
  char buffer [4096];
  vsprintf (buffer, msg, ap);
  force_error = 1;

  ctxp->elc.line = EXPR_WFL_LINENO (cl);
  ctxp->elc.col  = (EXPR_WFL_COLNO (cl) == 0xfff ? -1 : 
		    (EXPR_WFL_COLNO (cl) == 0xffe ? -2 : EXPR_WFL_COLNO (cl)));

  /* We have a CL, that's a good reason for using it if it contains data */
  saved = ctxp->filename;
  if (TREE_CODE (cl) == EXPR_WITH_FILE_LOCATION && EXPR_WFL_FILENAME_NODE (cl))
    ctxp->filename = EXPR_WFL_FILENAME (cl);
  saved_input_filename = input_filename;
  input_filename = ctxp->filename;
  java_error (NULL);
  java_error (buffer);
  ctxp->filename = saved;
  input_filename = saved_input_filename;
  force_error = 0;
}

/* Issue an error message at a current source line CL */

void
parse_error_context VPROTO ((tree cl, const char *msg, ...))
{
#ifndef ANSI_PROTOTYPES
  tree cl;
  const char *msg;
#endif
  va_list ap;

  VA_START (ap, msg);
#ifndef ANSI_PROTOTYPES
  cl = va_arg (ap, tree);
  msg = va_arg (ap, const char *);
#endif
  issue_warning_error_from_context (cl, msg, ap);
  va_end (ap);
}

/* Issue a warning at a current source line CL */

static void
parse_warning_context VPROTO ((tree cl, const char *msg, ...))
{
#ifndef ANSI_PROTOTYPES
  tree cl;
  const char *msg;
#endif
  va_list ap;

  VA_START (ap, msg);
#ifndef ANSI_PROTOTYPES
  cl = va_arg (ap, tree);
  msg = va_arg (ap, const char *);
#endif

  force_error = do_warning = 1;
  issue_warning_error_from_context (cl, msg, ap);
  do_warning = force_error = 0;
  va_end (ap);
}

static tree
find_expr_with_wfl (node)
     tree node;
{
  while (node)
    {
      char code;
      tree to_return;

      switch (TREE_CODE (node))
	{
	case BLOCK:
	  node = BLOCK_EXPR_BODY (node);
	  continue;

	case COMPOUND_EXPR:
	  to_return = find_expr_with_wfl (TREE_OPERAND (node, 0));
	  if (to_return)
	    return to_return;
	  node = TREE_OPERAND (node, 1);
	  continue;

	case LOOP_EXPR:
	  node = TREE_OPERAND (node, 0);
	  continue;
	  
	case LABELED_BLOCK_EXPR:
	  node = TREE_OPERAND (node, 1);
	  continue;

	default:
	  code = TREE_CODE_CLASS (TREE_CODE (node));
	  if (((code == '1') || (code == '2') || (code == 'e'))
	      && EXPR_WFL_LINECOL (node))
	    return node;
	  return NULL_TREE;
	}
    }
  return NULL_TREE;
}

/* Issue a missing return statement error. Uses METHOD to figure the
   last line of the method the error occurs in.  */

static void
missing_return_error (method)
     tree method;
{
  EXPR_WFL_SET_LINECOL (wfl_operator, DECL_SOURCE_LINE_LAST (method), -2);
  parse_error_context (wfl_operator, "Missing return statement");
}

/* Issue an unreachable statement error. From NODE, find the next
   statement to report appropriately.  */
static void
unreachable_stmt_error (node)
     tree node;
{
  /* Browse node to find the next expression node that has a WFL. Use
     the location to report the error */
  if (TREE_CODE (node) == COMPOUND_EXPR)
    node = find_expr_with_wfl (TREE_OPERAND (node, 1));
  else
    node = find_expr_with_wfl (node);

  if (node)
    {
      EXPR_WFL_SET_LINECOL (wfl_operator, EXPR_WFL_LINENO (node), -2);
      parse_error_context (wfl_operator, "Unreachable statement");
    }
  else
    fatal ("Can't get valid statement - unreachable_stmt_error");
}

int
java_report_errors ()
{
  if (java_error_count)
    fprintf (stderr, "%d error%s", 
	     java_error_count, (java_error_count == 1 ? "" : "s"));
  if (java_warning_count)
    fprintf (stderr, "%s%d warning%s", (java_error_count ? ", " : ""),
	     java_warning_count, (java_warning_count == 1 ? "" : "s"));
  if (java_error_count || java_warning_count)
    putc ('\n', stderr);
  return java_error_count;
}

static char *
java_accstring_lookup (flags)
     int flags;
{
  static char buffer [80];
#define COPY_RETURN(S) {strcpy (buffer, S); return buffer;}

  /* Access modifier looked-up first for easier report on forbidden
     access. */
  if (flags & ACC_PUBLIC) COPY_RETURN ("public");
  if (flags & ACC_PRIVATE) COPY_RETURN ("private");
  if (flags & ACC_PROTECTED) COPY_RETURN ("protected");
  if (flags & ACC_STATIC) COPY_RETURN ("static");
  if (flags & ACC_FINAL) COPY_RETURN ("final");
  if (flags & ACC_SYNCHRONIZED) COPY_RETURN ("synchronized");
  if (flags & ACC_VOLATILE) COPY_RETURN ("volatile");
  if (flags & ACC_TRANSIENT) COPY_RETURN ("transient");
  if (flags & ACC_NATIVE) COPY_RETURN ("native");
  if (flags & ACC_INTERFACE) COPY_RETURN ("interface");
  if (flags & ACC_ABSTRACT) COPY_RETURN ("abstract");

  buffer [0] = '\0';
  return buffer;
#undef COPY_RETURN
}

/* Issuing error messages upon redefinition of classes, interfaces or
   variables. */

static void
classitf_redefinition_error (context, id, decl, cl)
     char *context;
     tree id, decl, cl;
{
  parse_error_context (cl, "%s `%s' already defined in %s:%d", 
		       context, IDENTIFIER_POINTER (id), 
		       DECL_SOURCE_FILE (decl), DECL_SOURCE_LINE (decl));
  /* Here we should point out where its redefined. It's a unicode. FIXME */
}

static void
variable_redefinition_error (context, name, type, line)
     tree context, name, type;
     int line;
{
  char *type_name;

  /* Figure a proper name for type. We might haven't resolved it */
  if (TREE_CODE (type) == POINTER_TYPE && !TREE_TYPE (type))
    type_name = IDENTIFIER_POINTER (TYPE_NAME (type));
  else
    type_name = lang_printable_name (type, 0);

  parse_error_context (context,
		       "Variable `%s' is already defined in this method and "
		       "was declared `%s %s' at line %d", 
		       IDENTIFIER_POINTER (name),
		       type_name, IDENTIFIER_POINTER (name), line);
}

static tree
build_array_from_name (type, type_wfl, name, ret_name)
     tree type, type_wfl, name, *ret_name;
{
  int more_dims = 0;
  char *string;

  /* Eventually get more dims */
  string = IDENTIFIER_POINTER (name);
  while (string [more_dims] == '[')
    more_dims++;
  
  /* If we have, then craft a new type for this variable */
  if (more_dims)
    {
      name = get_identifier (&string [more_dims]);

      /* If we have a pointer, use its type */
      if (TREE_CODE (type) == POINTER_TYPE)
        type = TREE_TYPE (type);

      /* Building the first dimension of a primitive type uses this
         function */
      if (JPRIMITIVE_TYPE_P (type))
	{
	  type = build_java_array_type (type, -1);
	  CLASS_LOADED_P (type) = 1;
	  more_dims--;
	}
      /* Otherwise, if we have a WFL for this type, use it (the type
         is already an array on an unresolved type, and we just keep
         on adding dimensions) */
      else if (type_wfl)
	type = type_wfl;

      /* Add all the dimensions */
      while (more_dims--)
	type = build_unresolved_array_type (type);

      /* The type may have been incomplete in the first place */
      if (type_wfl)
	type = obtain_incomplete_type (type);
    }

  *ret_name = name;
  return type;
}

/* Build something that the type identifier resolver will identify as
   being an array to an unresolved type. TYPE_WFL is a WFL on a
   identifier. */

static tree
build_unresolved_array_type (type_or_wfl)
     tree type_or_wfl;
{
  char *ptr;

  /* TYPE_OR_WFL might be an array on a resolved type. In this case,
     just create a array type */
  if (TREE_CODE (type_or_wfl) == RECORD_TYPE)
    {
      tree type = build_java_array_type (type_or_wfl, -1);
      CLASS_LOADED_P (type) = CLASS_LOADED_P (type_or_wfl);
      return type;
    }

  obstack_1grow (&temporary_obstack, '[');
  obstack_grow0 (&temporary_obstack,
		 IDENTIFIER_POINTER (EXPR_WFL_NODE (type_or_wfl)),
		 IDENTIFIER_LENGTH (EXPR_WFL_NODE (type_or_wfl)));
  ptr = obstack_finish (&temporary_obstack);
  return build_expr_wfl (get_identifier (ptr),
			 EXPR_WFL_FILENAME (type_or_wfl),
			 EXPR_WFL_LINENO (type_or_wfl),
			 EXPR_WFL_COLNO (type_or_wfl));
}

/* Check modifiers. If one doesn't fit, retrieve it in its declaration line
  and point it out.  */

static void
check_modifiers (message, value, mask)
     char *message;
     int value;
     int mask;
{
  /* Should point out the one that don't fit. ASCII/unicode,
     going backward. FIXME */
  if (value & ~mask)
    {
      int i, remainder = value & ~mask;
      for (i = 0; i <= 10; i++)
        if ((1 << i) & remainder)
	  parse_error_context (ctxp->modifier_ctx [i], message, 
			       java_accstring_lookup (1 << i));
    }
}

static void
parser_add_interface (class_decl, interface_decl, wfl)
     tree class_decl, interface_decl, wfl;
{
  if (maybe_add_interface (TREE_TYPE (class_decl), TREE_TYPE (interface_decl)))
    parse_error_context (wfl, "Interface `%s' repeated",
			 IDENTIFIER_POINTER (DECL_NAME (interface_decl)));
}

/* Bulk of common class/interface checks. Return 1 if an error was
   encountered. TAG is 0 for a class, 1 for an interface.  */

static int
check_class_interface_creation (is_interface, flags, raw_name, qualified_name, decl, cl)
     int is_interface, flags;
     tree raw_name, qualified_name, decl, cl;
{
  tree node;

  if (!quiet_flag)
    fprintf (stderr, " %s %s", (is_interface ? "interface" : "class"), 
	     IDENTIFIER_POINTER (qualified_name));

  /* Scope of an interface/class type name:
       - Can't be imported by a single type import
       - Can't already exists in the package */
  if (IS_A_SINGLE_IMPORT_CLASSFILE_NAME_P (raw_name)
      && (node = find_name_in_single_imports (raw_name)))
    {
      parse_error_context 
	(cl, "%s name `%s' clashes with imported type `%s'",
	 (is_interface ? "Interface" : "Class"),
	 IDENTIFIER_POINTER (raw_name), IDENTIFIER_POINTER (node));
      return 1;
    }
  if (decl && CLASS_COMPLETE_P (decl))
    {
      classitf_redefinition_error ((is_interface ? "Interface" : "Class"), 
				   qualified_name, decl, cl);
      return 1;
    }

  /* If public, file name should match class/interface name */
  if (flags & ACC_PUBLIC)
    {
      char *f;

      /* Contains OS dependent assumption on path separator. FIXME */
      for (f = &input_filename [strlen (input_filename)]; 
	   f != input_filename && f[0] != '/' && f[0] != DIR_SEPARATOR;
	   f--)
	;
      if (f[0] == '/' || f[0] == DIR_SEPARATOR)
	f++;
      if (strncmp (IDENTIFIER_POINTER (raw_name), 
		   f , IDENTIFIER_LENGTH (raw_name)) ||
	  f [IDENTIFIER_LENGTH (raw_name)] != '.')
	parse_error_context (cl, "Public %s `%s' must be defined in a file "
			     "called `%s.java'", 
			     (is_interface ? "interface" : "class"),
			     IDENTIFIER_POINTER (qualified_name),
			     IDENTIFIER_POINTER (raw_name));
    }

  check_modifiers ((is_interface ? 
		    "Illegal modifier `%s' for interface declaration" :
		    "Illegal modifier `%s' for class declaration"), flags,
		   (is_interface ? INTERFACE_MODIFIERS : CLASS_MODIFIERS));
  return 0;
}

/* If DECL is NULL, create and push a new DECL, record the current
   line CL and do other maintenance things.  */

static tree
maybe_create_class_interface_decl (decl, qualified_name, cl)
     tree decl, qualified_name, cl;
{
  if (!decl)
    decl = push_class (make_class (), qualified_name);
  
  /* Take care of the file and line business */
  DECL_SOURCE_FILE (decl) = EXPR_WFL_FILENAME (cl);
  /* If we're emiting xrefs, store the line/col number information */
  if (flag_emit_xref)
    DECL_SOURCE_LINE (decl) = EXPR_WFL_LINECOL (cl);
  else
    DECL_SOURCE_LINE (decl) = EXPR_WFL_LINENO (cl);
  CLASS_FROM_SOURCE_P (TREE_TYPE (decl)) = 1;
  CLASS_FROM_CURRENTLY_COMPILED_SOURCE_P (TREE_TYPE (decl)) =
    IS_A_COMMAND_LINE_FILENAME_P (EXPR_WFL_FILENAME_NODE (cl));

  ctxp->current_parsed_class = decl;
  
  /* Link the declaration to the already seen ones */
  TREE_CHAIN (decl) = ctxp->class_list;
  ctxp->class_list = decl;

  /* Create a new nodes in the global lists */
  ctxp->gclass_list = tree_cons (NULL_TREE, decl, ctxp->gclass_list);
  all_class_list = tree_cons (NULL_TREE, decl, all_class_list);

  /* Install a new dependency list element */
  create_jdep_list (ctxp);

  SOURCE_FRONTEND_DEBUG (("Defining class/interface %s", 
			  IDENTIFIER_POINTER (qualified_name)));
  return decl;
}

static void
add_superinterfaces (decl, interface_list)
     tree decl, interface_list;
{
  tree node;
  /* Superinterface(s): if present and defined, parser_check_super_interface ()
     takes care of ensuring that:
       - This is an accessible interface type,
       - Circularity detection.
   parser_add_interface is then called. If present but not defined,
   the check operation is delayed until the super interface gets
   defined.  */
  for (node = interface_list; node; node = TREE_CHAIN (node))
    {
      tree current = TREE_PURPOSE (node);
      tree idecl = IDENTIFIER_CLASS_VALUE (EXPR_WFL_NODE (current));
      if (idecl && CLASS_LOADED_P (TREE_TYPE (idecl)))
	{
	  if (!parser_check_super_interface (idecl, decl, current))
	    parser_add_interface (decl, idecl, current);
	}
      else
	register_incomplete_type (JDEP_INTERFACE,
				  current, decl, NULL_TREE);
    }
}

/* Create an interface in pass1 and return its decl. Return the
   interface's decl in pass 2.  */

static tree
create_interface (flags, id, super)
     int flags;
     tree id, super;
{
  tree raw_name = EXPR_WFL_NODE (id);
  tree q_name = parser_qualified_classname (id);
  tree decl = IDENTIFIER_CLASS_VALUE (q_name);

  EXPR_WFL_NODE (id) = q_name;	/* Keep source location, even if refined. */

  /* Basic checks: scope, redefinition, modifiers */ 
  if (check_class_interface_creation (1, flags, raw_name, q_name, decl, id))
    return NULL_TREE;

  /* Interface modifiers check
       - public/abstract allowed (already done at that point)
       - abstract is obsolete (comes first, it's a warning, or should be)
       - Can't use twice the same (checked in the modifier rule) */
  if ((flags & ACC_ABSTRACT) && flag_redundant)
    parse_warning_context 
      (MODIFIER_WFL (ABSTRACT_TK),
       "Redundant use of `abstract' modifier. Interface `%s' is implicitely "
       "abstract", IDENTIFIER_POINTER (raw_name));

  /* Create a new decl if DECL is NULL, otherwise fix it */
  decl = maybe_create_class_interface_decl (decl, q_name, id);

  /* Set super info and mark the class a complete */
  set_super_info (ACC_INTERFACE | flags, TREE_TYPE (decl), 
		  object_type_node, ctxp->interface_number);
  ctxp->interface_number = 0;
  CLASS_COMPLETE_P (decl) = 1;
  add_superinterfaces (decl, super);

  return decl;
}

/* Create an class in pass1 and return its decl. Return class
   interface's decl in pass 2.  */

static tree
create_class (flags, id, super, interfaces)
     int flags;
     tree id, super, interfaces;
{
  tree raw_name = EXPR_WFL_NODE (id);
  tree class_id, decl;
  tree super_decl_type;

  class_id = parser_qualified_classname (id);
  decl = IDENTIFIER_CLASS_VALUE (class_id);
  ctxp->current_parsed_class_un = EXPR_WFL_NODE (id);
  EXPR_WFL_NODE (id) = class_id;

  /* Basic check: scope, redefinition, modifiers */
  if (check_class_interface_creation (0, flags, raw_name, class_id, decl, id))
    return NULL_TREE;

  /* Class modifier check: 
       - Allowed modifier (already done at that point)
       - abstract AND final forbidden 
       - Public classes defined in the correct file */
  if ((flags & ACC_ABSTRACT) && (flags & ACC_FINAL))
    parse_error_context (id, "Class `%s' can't be declared both abstract "
			 "and final", IDENTIFIER_POINTER (raw_name));

  /* Create a new decl if DECL is NULL, otherwise fix it */
  decl = maybe_create_class_interface_decl (decl, class_id, id);

  /* If SUPER exists, use it, otherwise use Object */
  if (super)
    {
      /* Can't extend java.lang.Object */
      if (TREE_TYPE (IDENTIFIER_CLASS_VALUE (class_id)) == object_type_node)
	{
	  parse_error_context (id, "Can't extend `java.lang.Object'");
	  return NULL_TREE;
	}

      super_decl_type = 
	register_incomplete_type (JDEP_SUPER, super, decl, NULL_TREE);
    }
  else if (TREE_TYPE (decl) != object_type_node)
    super_decl_type = object_type_node;
  /* We're defining java.lang.Object */
  else
    super_decl_type = NULL_TREE;

  /* Set super info and mark the class a complete */
  set_super_info (flags, TREE_TYPE (decl), super_decl_type, 
		  ctxp->interface_number);
  ctxp->interface_number = 0;
  CLASS_COMPLETE_P (decl) = 1;
  add_superinterfaces (decl, interfaces);

  /* If doing xref, store the location at which the inherited class
     (if any) was seen. */
  if (flag_emit_xref && super)
    DECL_INHERITED_SOURCE_LINE (decl) = EXPR_WFL_LINECOL (super);

  /* Eventually sets the @deprecated tag flag */
  CHECK_DEPRECATED (decl);

  return decl;
}

/* Can't use lookup_field () since we don't want to load the class and
   can't set the CLASS_LOADED_P flag */

static tree
find_field (class, name)
     tree class;
     tree name;
{
  tree decl;
  for (decl = TYPE_FIELDS (class); decl; decl = TREE_CHAIN (decl))
    {
      if (DECL_NAME (decl) == name)
	return decl;
    }
  return NULL_TREE;
}

/* Wrap around lookup_field that doesn't potentially upset the value
   of CLASS */

static tree
lookup_field_wrapper (class, name)
     tree class, name;
{
  tree type = class;
  tree decl;
  java_parser_context_save_global ();
  decl = lookup_field (&type, name);
  java_parser_context_restore_global ();
  return decl == error_mark_node ? NULL : decl;
}

/* Find duplicate field within the same class declarations and report
   the error. Returns 1 if a duplicated field was found, 0
   otherwise.  */

static int
duplicate_declaration_error_p (new_field_name, new_type, cl)
     tree new_field_name, new_type, cl;
{
  /* This might be modified to work with method decl as well */
  tree decl = find_field (TREE_TYPE (ctxp->current_parsed_class), 
			  new_field_name);
  if (decl)
    {
      char *t1 = strdup (purify_type_name
			 ((TREE_CODE (new_type) == POINTER_TYPE 
			   && TREE_TYPE (new_type) == NULL_TREE) ?
			  IDENTIFIER_POINTER (TYPE_NAME (new_type)) :
			  lang_printable_name (new_type, 1)));
      /* The type may not have been completed by the time we report
	 the error */
      char *t2 = strdup (purify_type_name
			 ((TREE_CODE (TREE_TYPE (decl)) == POINTER_TYPE 
			   && TREE_TYPE (TREE_TYPE (decl)) == NULL_TREE) ?
			  IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (decl))) :
			  lang_printable_name (TREE_TYPE (decl), 1)));
      parse_error_context 
	(cl , "Duplicate variable declaration: `%s %s' was `%s %s' (%s:%d)", 
	 t1, IDENTIFIER_POINTER (new_field_name),
	 t2, IDENTIFIER_POINTER (DECL_NAME (decl)),
	 DECL_SOURCE_FILE (decl), DECL_SOURCE_LINE (decl));
      free (t1);
      free (t2);
      return 1;
    }
  return 0;
}

/* Field registration routine. If TYPE doesn't exist, field
   declarations are linked to the undefined TYPE dependency list, to
   be later resolved in java_complete_class () */

static void
register_fields (flags, type, variable_list)
     int flags;
     tree type, variable_list;
{
  tree current, saved_type;
  tree class_type = TREE_TYPE (ctxp->current_parsed_class);
  int saved_lineno = lineno;
  int must_chain = 0;
  tree wfl = NULL_TREE;

  /* If we're adding fields to interfaces, those fields are public,
     static, final */
  if (CLASS_INTERFACE (TYPE_NAME (class_type)))
    {
      OBSOLETE_MODIFIER_WARNING (MODIFIER_WFL (PUBLIC_TK),
				 flags, ACC_PUBLIC, 
				 "%s", "interface field(s)");
      OBSOLETE_MODIFIER_WARNING (MODIFIER_WFL (STATIC_TK),
				 flags, ACC_STATIC, 
				 "%s", "interface field(s)");
      OBSOLETE_MODIFIER_WARNING (MODIFIER_WFL (FINAL_TK),
				 flags, ACC_FINAL, "%s", "interface field(s)");
      check_modifiers ("Illegal interface member modifier `%s'", flags,
		       INTERFACE_FIELD_MODIFIERS);
      flags |= (ACC_PUBLIC | ACC_STATIC | ACC_FINAL);
    }

  /* Obtain a suitable type for resolution, if necessary */
  SET_TYPE_FOR_RESOLUTION (type, wfl, must_chain);

  /* If TYPE is fully resolved and we don't have a reference, make one */
  PROMOTE_RECORD_IF_COMPLETE (type, must_chain);

  for (current = variable_list, saved_type = type; current; 
       current = TREE_CHAIN (current), type = saved_type)
    {
      tree real_type;
      tree field_decl;
      tree cl = TREE_PURPOSE (current);
      tree init = TREE_VALUE (current);
      tree current_name = EXPR_WFL_NODE (cl);

      /* Process NAME, as it may specify extra dimension(s) for it */
      type = build_array_from_name (type, wfl, current_name, &current_name);

      /* Type adjustment. We may have just readjusted TYPE because
	 the variable specified more dimensions. Make sure we have
	 a reference if we can and don't have one already. Also
	 change the name if we have an init. */
      if (type != saved_type)
	{
	  PROMOTE_RECORD_IF_COMPLETE (type, must_chain);
	  if (init)
	    EXPR_WFL_NODE (TREE_OPERAND (init, 0)) = current_name;
	}

      real_type = GET_REAL_TYPE (type);
      /* Check for redeclarations */
      if (duplicate_declaration_error_p (current_name, real_type, cl))
	continue;

      /* Set lineno to the line the field was found and create a
         declaration for it. Eventually sets the @deprecated tag flag. */
      if (flag_emit_xref)
	lineno = EXPR_WFL_LINECOL (cl);
      else
	lineno = EXPR_WFL_LINENO (cl);
      field_decl = add_field (class_type, current_name, real_type, flags);
      CHECK_DEPRECATED (field_decl);
      
      /* Check if we must chain. */
      if (must_chain)
	register_incomplete_type (JDEP_FIELD, wfl, field_decl, type);
	  
      /* If we have an initialization value tied to the field */
      if (init)
	{
	  /* The field is declared static */
	  if (flags & ACC_STATIC)
	    {
	      /* We include the field and its initialization part into
		 a list used to generate <clinit>. After <clinit> is
		 walked, field initializations will be processed and
		 fields initialized with known constants will be taken
		 out of <clinit> and have their DECL_INITIAL set
		 appropriately. */
	      TREE_CHAIN (init) = ctxp->static_initialized;
	      ctxp->static_initialized = init;
	      DECL_INITIAL (field_decl) = TREE_OPERAND (init, 1);
	      if (TREE_OPERAND (init, 1) 
		  && TREE_CODE (TREE_OPERAND (init, 1)) == NEW_ARRAY_INIT)
		TREE_STATIC (TREE_OPERAND (init, 1)) = 1;
	    }
	  /* A non-static field declared with an immediate initialization is
	     to be initialized in <init>, if any.  This field is remembered
	     to be processed at the time of the generation of <init>. */
	  else
	    {
	      TREE_CHAIN (init) = ctxp->non_static_initialized;
	      ctxp->non_static_initialized = init;
	    }
	  MODIFY_EXPR_FROM_INITIALIZATION_P (init) = 1;
	}
    }
  lineno = saved_lineno;
}

/* Generate the method $finit$ that initializes fields initialized
   upon declaration.  */

static void
maybe_generate_finit ()
{
  tree mdecl, current;
  
  if (!ctxp->non_static_initialized || java_error_count)
    return;

  mdecl = create_artificial_method (TREE_TYPE (ctxp->current_parsed_class),
				    ACC_PRIVATE, void_type_node,
				    finit_identifier_node, end_params_node);
  start_artificial_method_body (mdecl);

  ctxp->non_static_initialized = nreverse (ctxp->non_static_initialized);
  for (current = ctxp->non_static_initialized; current;
       current = TREE_CHAIN (current))
    java_method_add_stmt (mdecl, 
			  build_debugable_stmt (EXPR_WFL_LINECOL (current), 
						current));

  end_artificial_method_body (mdecl);
  CLASS_HAS_FINIT_P (TREE_TYPE (ctxp->current_parsed_class)) = 1;
  ctxp->non_static_initialized = NULL_TREE;
}

/* Check whether it is necessary to generate a <clinit> for the class
   we just parsed. */

static void
maybe_generate_clinit ()
{
  tree mdecl, c;

  if (!ctxp->static_initialized || java_error_count)
    return;

  mdecl = create_artificial_method (TREE_TYPE (ctxp->current_parsed_class),
				    ACC_STATIC, void_type_node,
				    clinit_identifier_node, end_params_node);
  start_artificial_method_body (mdecl);

  /* Keep initialization in order to enforce 8.5 */
  ctxp->static_initialized = nreverse (ctxp->static_initialized);

  /* We process the list of assignment we produced as the result of
     the declaration of initialized static field and add them as
     statement to the <clinit> method. */
  for (c = ctxp->static_initialized; c; c = TREE_CHAIN (c))
    {
      /* We build the assignment expression that will initialize the
	 field to its value. There are strict rules on static
	 initializers (8.5). FIXME */
      java_method_add_stmt (mdecl, 
			    build_debugable_stmt (EXPR_WFL_LINECOL (c), c));
    }

  end_artificial_method_body (mdecl);
  ctxp->static_initialized = NULL_TREE;
}

/* Shared accros method_declarator and method_header to remember the
   patch stage that was reached during the declaration of the method.
   A method DECL is built differently is there is no patch
   (JDEP_NO_PATCH) or a patch (JDEP_METHOD or JDEP_METHOD_RETURN)
   pending on the currently defined method.  */

static int patch_stage;

/* Check the method declaration and add the method to its current
   class.  If the argument list is known to contain incomplete types,
   the method is partially added and the registration will be resume
   once the method arguments resolved. If TYPE is NULL, we're dealing
   with a constructor.  */

static tree
method_header (flags, type, mdecl, throws)
     int flags;
     tree type, mdecl, throws;
{
  tree meth = TREE_VALUE (mdecl);
  tree id = TREE_PURPOSE (mdecl);
  tree this_class = TREE_TYPE (ctxp->current_parsed_class);
  tree type_wfl = NULL_TREE;
  tree meth_name = NULL_TREE, current, orig_arg;
  int saved_lineno;
  int constructor_ok = 0, must_chain;
  
  check_modifiers_consistency (flags);
  
  /* There are some forbidden modifiers for an abstract method and its
     class must be abstract as well.  */
  if (type && (flags & ACC_ABSTRACT))
    {
      ABSTRACT_CHECK (flags, ACC_PRIVATE, id, "Private");
      ABSTRACT_CHECK (flags, ACC_STATIC, id, "Static");
      ABSTRACT_CHECK (flags, ACC_FINAL, id, "Final");
      ABSTRACT_CHECK (flags, ACC_NATIVE, id, "Native");
      ABSTRACT_CHECK (flags, ACC_SYNCHRONIZED,id, "Synchronized");
      if (!CLASS_ABSTRACT (TYPE_NAME (this_class))
	  && !CLASS_INTERFACE (TYPE_NAME (this_class)))
	parse_error_context 
	  (id, "Class `%s' must be declared abstract to define abstract "
	   "method `%s'", 
	   IDENTIFIER_POINTER (DECL_NAME (ctxp->current_parsed_class)),
	   IDENTIFIER_POINTER (EXPR_WFL_NODE (id)));
    }
  /* Things to be checked when declaring a constructor */
  if (!type)
    {
      int ec = java_error_count;
      /* 8.6: Constructor declarations: we might be trying to define a
         method without specifying a return type. */
      if (EXPR_WFL_NODE (id) != ctxp->current_parsed_class_un)
	parse_error_context 
	  (id, "Invalid method declaration, return type required");
      /* 8.6.3: Constructor modifiers */
      else
	{
	  JCONSTRUCTOR_CHECK (flags, ACC_ABSTRACT, id, "abstract");
	  JCONSTRUCTOR_CHECK (flags, ACC_STATIC, id, "static");
	  JCONSTRUCTOR_CHECK (flags, ACC_FINAL, id, "final");
	  JCONSTRUCTOR_CHECK (flags, ACC_NATIVE, id, "native");
	  JCONSTRUCTOR_CHECK (flags, ACC_SYNCHRONIZED, id, "synchronized");
	}
      /* If we found error here, we don't consider it's OK to tread
	 the method definition as a constructor, for the rest of this
	 function */
      if (ec == java_error_count)
	constructor_ok = 1;
    }

  /* Method declared within the scope of an interface are implicitly
     abstract and public. Conflicts with other erroneously provided
     modifiers are checked right after. */

  if (CLASS_INTERFACE (TYPE_NAME (this_class)))
    {
      /* If FLAGS isn't set because of a modifier, turn the
	 corresponding modifier WFL to NULL so we issue a warning on
	 the obsolete use of the modifier */
      if (!(flags & ACC_PUBLIC))
        MODIFIER_WFL (PUBLIC_TK) = NULL;
      if (!(flags & ACC_ABSTRACT))
        MODIFIER_WFL (ABSTRACT_TK) = NULL;
      flags |= ACC_PUBLIC;
      flags |= ACC_ABSTRACT;
    }

  /* Modifiers context reset moved up, so abstract method declaration
     modifiers can be later checked.  */

  /* Set constructor returned type to void and method name to <init>,
     unless we found an error identifier the constructor (in which
     case we retain the original name) */
  if (!type)
    {
      type = void_type_node;
      if (constructor_ok)
	meth_name = init_identifier_node;
    }
  else
    meth_name = EXPR_WFL_NODE (id);

  /* Do the returned type resolution and registration if necessary */
  SET_TYPE_FOR_RESOLUTION (type, type_wfl, must_chain);

  if (meth_name)
    type = build_array_from_name (type, type_wfl, meth_name, &meth_name);
  EXPR_WFL_NODE (id) = meth_name;
  PROMOTE_RECORD_IF_COMPLETE (type, must_chain);

  if (must_chain)
    {
      patch_stage = JDEP_METHOD_RETURN;
      register_incomplete_type (patch_stage, type_wfl, id, type);
      TREE_TYPE (meth) = GET_REAL_TYPE (type);
    }
  else
    TREE_TYPE (meth) = type;

  saved_lineno = lineno;
  /* When defining an abstract or interface method, the curly
     bracket at level 1 doesn't exist because there is no function
     body */
  lineno = (ctxp->first_ccb_indent1 ? ctxp->first_ccb_indent1 : 
	    EXPR_WFL_LINENO (id));

  /* Remember the original argument list */
  orig_arg = TYPE_ARG_TYPES (meth);

  if (patch_stage)		/* includes ret type and/or all args */
    {
      jdep *jdep;
      meth = add_method_1 (this_class, flags, meth_name, meth);
      /* Patch for the return type */
      if (patch_stage == JDEP_METHOD_RETURN)
	{
	  jdep = CLASSD_LAST (ctxp->classd_list);
	  JDEP_GET_PATCH (jdep) = &TREE_TYPE (TREE_TYPE (meth));
	}
      /* This is the stop JDEP. METH allows the function's signature
	 to be computed. */
      register_incomplete_type (JDEP_METHOD_END, NULL_TREE, meth, NULL_TREE);
    }
  else
    meth = add_method (this_class, flags, meth_name, 
		       build_java_signature (meth));

  /* Fix the method argument list so we have the argument name
     information */
  fix_method_argument_names (orig_arg, meth);

  /* Register the parameter number and re-install the current line
     number */
  DECL_MAX_LOCALS (meth) = ctxp->formal_parameter_number+1;
  lineno = saved_lineno;

  /* Register exception specified by the `throws' keyword for
     resolution and set the method decl appropriate field to the list.
     Note: the grammar ensures that what we get here are class
     types. */
  if (throws)
    {
      throws = nreverse (throws);
      for (current = throws; current; current = TREE_CHAIN (current))
	{
	  register_incomplete_type (JDEP_EXCEPTION, TREE_VALUE (current),
				    NULL_TREE, NULL_TREE);
	  JDEP_GET_PATCH (CLASSD_LAST (ctxp->classd_list)) = 
	    &TREE_VALUE (current);
	}
      DECL_FUNCTION_THROWS (meth) = throws;
    }

  /* We set the DECL_NAME to ID so we can track the location where
     the function was declared. This allow us to report
     redefinition error accurately. When method are verified,
     DECL_NAME is reinstalled properly (using the content of the
     WFL node ID) (see check_method_redefinition). We don't do that
     when Object is being defined. Constructor <init> names will be
     reinstalled the same way. */
  if (TREE_TYPE (ctxp->current_parsed_class) != object_type_node)
    DECL_NAME (meth) = id;

  /* Set the flag if we correctly processed a constructor */
  if (constructor_ok)
    DECL_CONSTRUCTOR_P (meth) = 1;

  /* Eventually set the @deprecated tag flag */
  CHECK_DEPRECATED (meth);

  /* If doing xref, store column and line number information instead
     of the line number only. */
  if (flag_emit_xref)
    DECL_SOURCE_LINE (meth) = EXPR_WFL_LINECOL (id);

  return meth;
}

static void
fix_method_argument_names (orig_arg, meth)
    tree orig_arg, meth;
{
  tree arg = TYPE_ARG_TYPES (TREE_TYPE (meth));
  if (TREE_CODE (TREE_TYPE (meth)) == METHOD_TYPE)
    {
      TREE_PURPOSE (arg) = this_identifier_node;
      arg = TREE_CHAIN (arg);
    }
  while (orig_arg != end_params_node)
    {
      TREE_PURPOSE (arg) = TREE_PURPOSE (orig_arg);
      orig_arg = TREE_CHAIN (orig_arg);
      arg = TREE_CHAIN (arg);
    }
}

/* Complete the method declaration with METHOD_BODY.  */

static void
finish_method_declaration (method_body)
     tree method_body;
{
  int flags = get_access_flags_from_decl (current_function_decl);

  /* 8.4.5 Method Body */
  if ((flags & ACC_ABSTRACT || flags & ACC_NATIVE) && method_body)
    {
      tree wfl = DECL_NAME (current_function_decl);
      parse_error_context (wfl, 
			   "%s method `%s' can't have a body defined",
			   (METHOD_NATIVE (current_function_decl) ?
			    "Native" : "Abstract"),
			   IDENTIFIER_POINTER (EXPR_WFL_NODE (wfl)));
      method_body = NULL_TREE;
    }
  else if (!(flags & ACC_ABSTRACT) && !(flags & ACC_NATIVE) && !method_body)
    {
      tree wfl = DECL_NAME (current_function_decl);
      parse_error_context (wfl, 
			   "Non native and non abstract method `%s' must "
			   "have a body defined",
			   IDENTIFIER_POINTER (EXPR_WFL_NODE (wfl)));
      method_body = NULL_TREE;
    }

  BLOCK_EXPR_BODY (DECL_FUNCTION_BODY (current_function_decl)) = method_body;
  maybe_absorb_scoping_blocks ();
  /* Exit function's body */
  exit_block ();
  /* Merge last line of the function with first line, directly in the
     function decl. It will be used to emit correct debug info. */
  if (!flag_emit_xref)
    DECL_SOURCE_LINE_MERGE (current_function_decl, ctxp->last_ccb_indent1);
  /* So we don't have an irrelevant function declaration context for
     the next static block we'll see. */
  current_function_decl = NULL_TREE;
}

/* Build a an error message for constructor circularity errors.  */

static char *
constructor_circularity_msg (from, to)
     tree from, to;
{
  static char string [4096];
  char *t = strdup (lang_printable_name (from, 0));
  sprintf (string, "`%s' invokes `%s'", t, lang_printable_name (to, 0));
  free (t);
  return string;
}

/* Verify a circular call to METH. Return 1 if an error is found, 0
   otherwise.  */

static int
verify_constructor_circularity (meth, current)
     tree meth, current;
{
  static tree list = NULL_TREE;
  tree c;
  for (c = DECL_CONSTRUCTOR_CALLS (current); c; c = TREE_CHAIN (c))
    {
      if (TREE_VALUE (c) == meth)
	{
	  char *t;
	  if (list)
	    {
	      tree liste;
	      list = nreverse (list);
	      for (liste = list; liste; liste = TREE_CHAIN (liste))
		{
		  parse_error_context 
		    (TREE_PURPOSE (TREE_PURPOSE (liste)),
		     constructor_circularity_msg
		      (TREE_VALUE (liste), TREE_VALUE (TREE_PURPOSE (liste)))); 
		  java_error_count--;
		}
	    }
	  t = strdup (lang_printable_name (meth, 0));
	  parse_error_context (TREE_PURPOSE (c), 
			       "%s: recursive invocation of constructor `%s'",
			       constructor_circularity_msg (current, meth), t);
	  free (t);
	  list = NULL_TREE;
	  return 1;
	}
    }
  for (c = DECL_CONSTRUCTOR_CALLS (current); c; c = TREE_CHAIN (c))
    {
      list = tree_cons (c, current, list);
      if (verify_constructor_circularity (meth, TREE_VALUE (c)))
	return 1;
      list = TREE_CHAIN (list);
    }
  return 0;
}

/* Check modifiers that can be declared but exclusively */

static void
check_modifiers_consistency (flags)
     int flags;
{
  int acc_count = 0;
  tree cl = NULL_TREE;

  THIS_MODIFIER_ONLY (flags, ACC_PUBLIC, 0, acc_count, cl);
  THIS_MODIFIER_ONLY (flags, ACC_PRIVATE, 1, acc_count, cl);
  THIS_MODIFIER_ONLY (flags, ACC_PROTECTED, 2, acc_count, cl);
  if (acc_count > 1)
    parse_error_context
      (cl, "Inconsistent member declaration. At most one of `public', "
       "`private', or `protected' may be specified");
}

/* Check the methode header METH for abstract specifics features */

static void
check_abstract_method_header (meth)
     tree meth;
{
  int flags = get_access_flags_from_decl (meth);
  /* DECL_NAME might still be a WFL node */
  tree name = GET_METHOD_NAME (meth);

  OBSOLETE_MODIFIER_WARNING (MODIFIER_WFL (ABSTRACT_TK), flags,
			     ACC_ABSTRACT, "abstract method `%s'",
			     IDENTIFIER_POINTER (name));
  OBSOLETE_MODIFIER_WARNING (MODIFIER_WFL (PUBLIC_TK), flags, 
			     ACC_PUBLIC, "abstract method `%s'",
			     IDENTIFIER_POINTER (name));

  check_modifiers ("Illegal modifier `%s' for interface method",
		  flags, INTERFACE_METHOD_MODIFIERS);
}

/* Create a FUNCTION_TYPE node and start augmenting it with the
   declared function arguments. Arguments type that can't be resolved
   are left as they are, but the returned node is marked as containing
   incomplete types.  */

static tree
method_declarator (id, list)
     tree id, list;
{
  tree arg_types = NULL_TREE, current, node;
  tree meth = make_node (FUNCTION_TYPE);
  jdep *jdep;

  patch_stage = JDEP_NO_PATCH;
  
  for (current = list; current; current = TREE_CHAIN (current))
    {
      int must_chain = 0;
      tree wfl_name = TREE_PURPOSE (current);
      tree type = TREE_VALUE (current);
      tree name = EXPR_WFL_NODE (wfl_name);
      tree already, arg_node;
      tree type_wfl = NULL_TREE;
      tree real_type;

      /* Obtain a suitable type for resolution, if necessary */
      SET_TYPE_FOR_RESOLUTION (type, type_wfl, must_chain);

      /* Process NAME, as it may specify extra dimension(s) for it */
      type = build_array_from_name (type, type_wfl, name, &name);
      EXPR_WFL_NODE (wfl_name) = name;

      real_type = GET_REAL_TYPE (type);
      if (TREE_CODE (real_type) == RECORD_TYPE)
	{
	  real_type = promote_type (real_type);
	  if (TREE_CODE (type) == TREE_LIST)
	    TREE_PURPOSE (type) = real_type;
	}

      /* Check redefinition */
      for (already = arg_types; already; already = TREE_CHAIN (already))
	if (TREE_PURPOSE (already) == name)
	  {
	    parse_error_context 
	      (wfl_name, "Variable `%s' is used more than once in the "
	       "argument list of method `%s'", IDENTIFIER_POINTER (name),
	       IDENTIFIER_POINTER (EXPR_WFL_NODE (id)));
	    break;
	  }

      /* If we've an incomplete argument type, we know there is a location
	 to patch when the type get resolved, later.  */
      jdep = NULL;
      if (must_chain)
	{
	  patch_stage = JDEP_METHOD;
	  type = register_incomplete_type (patch_stage, 
					   type_wfl, wfl_name, type);
	  jdep = CLASSD_LAST (ctxp->classd_list);
	  JDEP_MISC (jdep) = id;
	}

      /* The argument node: a name and a (possibly) incomplete type */
      arg_node = build_tree_list (name, real_type);
      if (jdep)
	JDEP_GET_PATCH (jdep) = &TREE_VALUE (arg_node);
      TREE_CHAIN (arg_node) = arg_types;
      arg_types = arg_node;
    }
  TYPE_ARG_TYPES (meth) = chainon (nreverse (arg_types), end_params_node);
  node = build_tree_list (id, meth);
  return node;
}

static int
unresolved_type_p (wfl, returned)
     tree wfl;
     tree *returned;
     
{
  if (TREE_CODE (wfl) == EXPR_WITH_FILE_LOCATION)
    {
      tree decl = IDENTIFIER_CLASS_VALUE (EXPR_WFL_NODE (wfl));
      if (returned)
	*returned = (decl ? TREE_TYPE (decl) : NULL_TREE);
      return 1;
    }
  if (returned)
    *returned = wfl;
  return 0;
}

/* From NAME, build a qualified identifier node using the
   qualification from the current package definition. */

static tree
parser_qualified_classname (name)
     tree name;
{
  if (ctxp->package)
    return merge_qualified_name (ctxp->package, EXPR_WFL_NODE (name));
  else 
    return EXPR_WFL_NODE (name);
}

/* Called once the type a interface extends is resolved. Returns 0 if
   everything is OK.  */

static int
parser_check_super_interface (super_decl, this_decl, this_wfl)
     tree super_decl, this_decl, this_wfl;
{
  tree super_type = TREE_TYPE (super_decl);

  /* Has to be an interface */
  if (!CLASS_INTERFACE (TYPE_NAME (TREE_TYPE (super_decl))))
    {
      parse_error_context 
	(this_wfl, "Can't use %s `%s' to implement/extend %s `%s'",
	 (TYPE_ARRAY_P (super_type) ? "array" : "class"),
	 IDENTIFIER_POINTER (DECL_NAME (super_decl)),
	 (CLASS_INTERFACE (TYPE_NAME (TREE_TYPE (this_decl))) ? 
	  "interface" : "class"),
	 IDENTIFIER_POINTER (DECL_NAME (this_decl)));
      return 1;
    }

  /* Check scope: same package OK, other package: OK if public */
  if (check_pkg_class_access (DECL_NAME (super_decl), lookup_cl (this_decl)))
    return 1;

  SOURCE_FRONTEND_DEBUG (("Completing interface %s with %s",
			  IDENTIFIER_POINTER (DECL_NAME (this_decl)),
			  IDENTIFIER_POINTER (DECL_NAME (super_decl))));
  return 0;
}

/* Makes sure that SUPER_DECL is suitable to extend THIS_DECL. Returns
   0 if everthing is OK.  */

static int
parser_check_super (super_decl, this_decl, wfl)
     tree super_decl, this_decl, wfl;
{
  tree super_type = TREE_TYPE (super_decl);

  /* SUPER should be a CLASS (neither an array nor an interface) */
  if (TYPE_ARRAY_P (super_type) || CLASS_INTERFACE (TYPE_NAME (super_type)))
    {
      parse_error_context 
	(wfl, "Class `%s' can't subclass %s `%s'",
	 IDENTIFIER_POINTER (DECL_NAME (this_decl)),
	 (CLASS_INTERFACE (TYPE_NAME (super_type)) ? "interface" : "array"),
	 IDENTIFIER_POINTER (DECL_NAME (super_decl)));
      return 1;
    }

  if (CLASS_FINAL (TYPE_NAME (super_type)))
    {
      parse_error_context (wfl, "Can't subclass final classes: %s",
			   IDENTIFIER_POINTER (DECL_NAME (super_decl)));
      return 1;
    }

  /* Check scope: same package OK, other package: OK if public */
  if (check_pkg_class_access (DECL_NAME (super_decl), wfl))
    return 1;
  
  SOURCE_FRONTEND_DEBUG (("Completing class %s with %s",
			  IDENTIFIER_POINTER (DECL_NAME (this_decl)),
			  IDENTIFIER_POINTER (DECL_NAME (super_decl))));
  return 0;
}

/* Create a new dependency list and link it (in a LIFO manner) to the
   CTXP list of type dependency list.  */

static void
create_jdep_list (ctxp)
     struct parser_ctxt *ctxp;
{
  jdeplist *new = (jdeplist *)xmalloc (sizeof (jdeplist));	
  new->first = new->last = NULL;
  new->next = ctxp->classd_list;
  ctxp->classd_list = new;
}

static jdeplist *
reverse_jdep_list (ctxp)
     struct parser_ctxt *ctxp;
{
  register jdeplist *prev = NULL, *current, *next;
  for (current = ctxp->classd_list; current; current = next)
    {
      next = current->next;
      current->next = prev;
      prev = current;
    }
  return prev;
}

/* Create a fake pointer based on the ID stored in
   TYPE_NAME. TYPE_NAME can be a WFL or a incomplete type asking to be
   registered again. */

static tree
obtain_incomplete_type (type_name)
     tree type_name;
{
  tree ptr, name;

  if (TREE_CODE (type_name) == EXPR_WITH_FILE_LOCATION)
    name = EXPR_WFL_NODE (type_name);
  else if (INCOMPLETE_TYPE_P (type_name))
    name = TYPE_NAME (type_name);
  else
    fatal ("invalid type name - obtain_incomplete_type");

  for (ptr = ctxp->incomplete_class; ptr; ptr = TREE_CHAIN (ptr))
    if (TYPE_NAME (ptr) == name)
      break;

  if (!ptr)
    {
      push_obstacks (&permanent_obstack, &permanent_obstack);
      BUILD_PTR_FROM_NAME (ptr, name);
      layout_type (ptr);
      pop_obstacks ();
      TREE_CHAIN (ptr) = ctxp->incomplete_class;
      ctxp->incomplete_class = ptr;
    }

  return ptr;
}

/* Register a incomplete type whose name is WFL. Reuse PTR if PTR is
   non NULL instead of computing a new fake type based on WFL. The new
   dependency is inserted in the current type dependency list, in FIFO
   manner.  */

static tree
register_incomplete_type (kind, wfl, decl, ptr)
     int kind;
     tree wfl, decl, ptr;
{
  jdep *new = (jdep *)xmalloc (sizeof (jdep));

  if (!ptr && kind != JDEP_METHOD_END) /* JDEP_METHOD_END is a mere marker */
    ptr = obtain_incomplete_type (wfl);

  JDEP_KIND (new) = kind;
  JDEP_DECL (new) = decl;
  JDEP_SOLV (new) = ptr;
  JDEP_WFL (new) = wfl;
  JDEP_CHAIN (new) = NULL;
  JDEP_MISC (new) = NULL_TREE;
  JDEP_GET_PATCH (new) = (tree *)NULL;

  JDEP_INSERT (ctxp->classd_list, new);

  return ptr;
}

void
java_check_circular_reference ()
{
  tree current;
  for (current = ctxp->class_list; current; current = TREE_CHAIN (current))
    {
      tree type = TREE_TYPE (current);
      if (CLASS_INTERFACE (TYPE_NAME (type)))
	{
	  /* Check all interfaces this class extends */
	  tree basetype_vec = TYPE_BINFO_BASETYPES (type);
	  int n, i;

	  if (!basetype_vec)
	    return;
	  n = TREE_VEC_LENGTH (basetype_vec);
	  for (i = 0; i < n; i++)
	    {
	      tree vec_elt = TREE_VEC_ELT (basetype_vec, i);
	      if (vec_elt && BINFO_TYPE (vec_elt) != object_type_node 
		  && interface_of_p (type, BINFO_TYPE (vec_elt)))
		parse_error_context (lookup_cl (current),
				     "Cyclic interface inheritance");
	    }
	}
      else
	if (inherits_from_p (CLASSTYPE_SUPER (type), type))
	  parse_error_context (lookup_cl (current), 
			       "Cyclic class inheritance");
    }
}

/* safe_layout_class just makes sure that we can load a class without
   disrupting the current_class, input_file, lineno, etc, information
   about the class processed currently.  */

void
safe_layout_class (class)
     tree class;
{
  tree save_current_class = current_class;
  char *save_input_filename = input_filename;
  int save_lineno = lineno;

  push_obstacks (&permanent_obstack, &permanent_obstack);

  layout_class (class);
  pop_obstacks ();

  current_class = save_current_class;
  input_filename = save_input_filename;
  lineno = save_lineno;
  CLASS_LOADED_P (class) = 1;
}

static tree
jdep_resolve_class (dep)
     jdep *dep;
{
  tree decl;

  if (JDEP_RESOLVED_P (dep))
    decl = JDEP_RESOLVED_DECL (dep);
  else
    {
      decl = resolve_class (JDEP_TO_RESOLVE (dep), 
			    JDEP_DECL (dep), JDEP_WFL (dep));
      JDEP_RESOLVED (dep, decl);
    }
    
  if (!decl)
    complete_class_report_errors (dep);

  return decl;
}

/* Complete unsatisfied class declaration and their dependencies */

void
java_complete_class ()
{
  tree cclass;
  jdeplist *cclassd;
  int error_found;
  tree type;

  push_obstacks (&permanent_obstack, &permanent_obstack);

  /* Process imports and reverse the import on demand list */
  process_imports ();
  if (ctxp->import_demand_list)
    ctxp->import_demand_list = nreverse (ctxp->import_demand_list);

  /* Rever things so we have the right order */
  ctxp->class_list = nreverse (ctxp->class_list);
  ctxp->classd_list = reverse_jdep_list (ctxp);

  for (cclassd = ctxp->classd_list, cclass = ctxp->class_list; 
       cclass && cclassd; 
       cclass = TREE_CHAIN (cclass), cclassd = CLASSD_CHAIN (cclassd))
    {
      jdep *dep;
      for (dep = CLASSD_FIRST (cclassd); dep; dep = JDEP_CHAIN (dep))
	{
	  tree decl;
	  if (!(decl = jdep_resolve_class (dep)))
	    continue;

	  /* Now it's time to patch */
	  switch (JDEP_KIND (dep))
	    {
	    case JDEP_SUPER:
	      /* Simply patch super */
	      if (parser_check_super (decl, JDEP_DECL (dep), JDEP_WFL (dep)))
		continue;
	      BINFO_TYPE (TREE_VEC_ELT (BINFO_BASETYPES (TYPE_BINFO 
	        (TREE_TYPE (JDEP_DECL (dep)))), 0)) = TREE_TYPE (decl);
	      break;

	    case JDEP_FIELD:
	      {
		/* We do part of the job done in add_field */
		tree field_decl = JDEP_DECL (dep);
		tree field_type = TREE_TYPE (decl);
		push_obstacks (&permanent_obstack, &permanent_obstack);
		if (TREE_CODE (field_type) == RECORD_TYPE)
		  field_type = promote_type (field_type);
		pop_obstacks ();
		TREE_TYPE (field_decl) = field_type;
		DECL_ALIGN (field_decl) = 0;
		layout_decl (field_decl, 0);
		SOURCE_FRONTEND_DEBUG 
		  (("Completed field/var decl `%s' with `%s'",
		    IDENTIFIER_POINTER (DECL_NAME (field_decl)),
		    IDENTIFIER_POINTER (DECL_NAME (decl))));
		break;
	      }
	    case JDEP_METHOD:	/* We start patching a method */
	    case JDEP_METHOD_RETURN:
	      error_found = 0;
	      while (1)
		{
		  if (decl)
		    {
		      type = TREE_TYPE(decl);
		      if (TREE_CODE (type) == RECORD_TYPE)
			type = promote_type (type);
		      JDEP_APPLY_PATCH (dep, type);
		      SOURCE_FRONTEND_DEBUG 
			(((JDEP_KIND (dep) == JDEP_METHOD_RETURN ?
			   "Completing fct `%s' with ret type `%s'":
			   "Completing arg `%s' with type `%s'"),
			  IDENTIFIER_POINTER (EXPR_WFL_NODE 
					      (JDEP_DECL_WFL (dep))),
			  IDENTIFIER_POINTER (DECL_NAME (decl))));
		    }
		  else
		    error_found = 1;
		  dep = JDEP_CHAIN (dep);
		  if (JDEP_KIND (dep) == JDEP_METHOD_END)
		    break;
		  else
		    decl = jdep_resolve_class (dep);
		}
	      if (!error_found)
		{
		  tree mdecl = JDEP_DECL (dep), signature;
		  push_obstacks (&permanent_obstack, &permanent_obstack);
		  /* Recompute and reset the signature */
		  signature = build_java_signature (TREE_TYPE (mdecl));
		  set_java_signature (TREE_TYPE (mdecl), signature);
		  pop_obstacks ();
		}
	      else
		continue;
	      break;

	    case JDEP_INTERFACE:
	      if (parser_check_super_interface (decl, JDEP_DECL (dep),
						JDEP_WFL (dep)))
		continue;
	      parser_add_interface (JDEP_DECL (dep), decl, JDEP_WFL (dep));
	      break;

	    case JDEP_PARM:
	    case JDEP_VARIABLE:
	      type = TREE_TYPE(decl);
	      if (TREE_CODE (type) == RECORD_TYPE)
		type = promote_type (type);
	      JDEP_APPLY_PATCH (dep, type);
	      break;

	    case JDEP_TYPE:
	      JDEP_APPLY_PATCH (dep, TREE_TYPE (decl));
	      SOURCE_FRONTEND_DEBUG 
		(("Completing a random type dependency on a '%s' node",
		  tree_code_name [TREE_CODE (JDEP_DECL (dep))]));
	      break;

	    case JDEP_EXCEPTION:
	      JDEP_APPLY_PATCH (dep, TREE_TYPE (decl));
	      SOURCE_FRONTEND_DEBUG 
		(("Completing `%s' `throws' argument node",
		  IDENTIFIER_POINTER (EXPR_WFL_NODE (JDEP_WFL (dep)))));
	      break;

	    default:
	      fatal ("Can't handle patch code %d - java_complete_class",
		     JDEP_KIND (dep));
	    }
	}
    }
  pop_obstacks ();
  return;
}

/* Resolve class CLASS_TYPE. Handle the case of trying to resolve an
   array.  */

static tree
resolve_class (class_type, decl, cl)
     tree class_type, decl, cl;
{
  char *name = IDENTIFIER_POINTER (TYPE_NAME (class_type));
  char *base = name;
  tree resolved_type = TREE_TYPE (class_type);
  tree resolved_type_decl;
  
  if (resolved_type != NULL_TREE)
    {
      tree resolved_type_decl = TYPE_NAME (resolved_type);
      if (resolved_type_decl == NULL_TREE
	  || TREE_CODE (resolved_type_decl) == IDENTIFIER_NODE)
	{
	  resolved_type_decl = build_decl (TYPE_DECL,
					   TYPE_NAME (class_type),
					   resolved_type);
	}
      return resolved_type_decl;
    }

  /* 1- Check to see if we have an array. If true, find what we really
     want to resolve  */
  while (name[0] == '[')
    name++;
  if (base != name)
    TYPE_NAME (class_type) = get_identifier (name);

  /* 2- Resolve the bare type */
  if (!(resolved_type_decl = do_resolve_class (class_type, decl, cl)))
    return NULL_TREE;
  resolved_type = TREE_TYPE (resolved_type_decl);

  /* 3- If we have and array, reconstruct the array down to its nesting */
  if (base != name)
    {
      while (base != name)
	{
	  if (TREE_CODE (resolved_type) == RECORD_TYPE)
	    resolved_type  = promote_type (resolved_type);
	  resolved_type = build_java_array_type (resolved_type, -1);
	  CLASS_LOADED_P (resolved_type) = 1;
	  name--;
	}
      /* Build a fake decl for this, since this is what is expected to
         be returned.  */
      resolved_type_decl =
	build_decl (TYPE_DECL, TYPE_NAME (resolved_type), resolved_type);
      /* Figure how those two things are important for error report. FIXME */
      DECL_SOURCE_LINE (resolved_type_decl) = 0;
      DECL_SOURCE_FILE (resolved_type_decl) = input_filename;
      TYPE_NAME (class_type) = TYPE_NAME (resolved_type);
    }
  TREE_TYPE (class_type) = resolved_type;
  return resolved_type_decl;
}

/* Effectively perform the resolution of class CLASS_TYPE. DECL or CL
   are used to report error messages.  */

tree
do_resolve_class (class_type, decl, cl)
     tree class_type;
     tree decl;
     tree cl;
{
  tree new_class_decl;
  tree original_name = NULL_TREE;

  /* Do not try to replace TYPE_NAME (class_type) by a variable, since
     its is changed by find_in_imports{_on_demand} */

  /* 1- Check for the type in single imports */
  if (find_in_imports (class_type))
    return NULL_TREE;

  /* 2- And check for the type in the current compilation unit. If it fails,
     try with a name qualified with the package name if appropriate. */
  if ((new_class_decl = IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type))))
    {
      if (!CLASS_LOADED_P (TREE_TYPE (new_class_decl)) &&
	  !CLASS_FROM_SOURCE_P (TREE_TYPE (new_class_decl)))
	load_class (TYPE_NAME (class_type), 0);
      return IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type));
    }

  original_name = TYPE_NAME (class_type);
  if (!QUALIFIED_P (TYPE_NAME (class_type)) && ctxp->package)
    TYPE_NAME (class_type) = merge_qualified_name (ctxp->package, 
						   TYPE_NAME (class_type));
#if 1
  if (!(new_class_decl = IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type))))
    load_class (TYPE_NAME (class_type), 0);
  if ((new_class_decl = IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type))))
    {
      if (!CLASS_LOADED_P (TREE_TYPE (new_class_decl)) &&
	  !CLASS_FROM_SOURCE_P (TREE_TYPE (new_class_decl)))
	load_class (TYPE_NAME (class_type), 0);
      return IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type));
    }
#else
  new_name = TYPE_NAME (class_type);
  if ((new_class_decl = IDENTIFIER_CLASS_VALUE (new_name)) != NULL_TREE)
    {
      if (!CLASS_LOADED_P (TREE_TYPE (new_class_decl)) &&
          !CLASS_FROM_SOURCE_P (TREE_TYPE (new_class_decl)))
        load_class (new_name, 0);
      return IDENTIFIER_CLASS_VALUE (new_name);
    }
  else
    {
      tree class = read_class (new_name);
      if (class != NULL_TREE)
	{
	  tree decl = IDENTIFIER_CLASS_VALUE (new_name);
	  if (decl == NULL_TREE)
	    decl = push_class (class, new_name);
	  return decl;
	}
    }
#endif
  TYPE_NAME (class_type) = original_name;

  /* 3- Check an other compilation unit that bears the name of type */
  load_class (TYPE_NAME (class_type), 0);
  if (check_pkg_class_access (TYPE_NAME (class_type), 
			      (cl ? cl : lookup_cl (decl))))
    return NULL_TREE;

  if ((new_class_decl = IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type))))
    return new_class_decl;

  /* 4- Check the import on demands. Don't allow bar.baz to be
     imported from foo.* */
  if (!QUALIFIED_P (TYPE_NAME (class_type)))
    if (find_in_imports_on_demand (class_type))
      return NULL_TREE;

  /* 5- Last call for a resolution */
  return IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type));
}

/* Resolve NAME and lay it out (if not done and if not the current
   parsed class). Return a decl node. This function is meant to be
   called when type resolution is necessary during the walk pass.  */

static tree
resolve_and_layout (something, cl)
     tree something;
     tree cl;
{
  tree decl;

  /* Don't do that on the current class */
  if (something == current_class)
    return TYPE_NAME (current_class);

  /* Don't do anything for void and other primitive types */
  if (JPRIMITIVE_TYPE_P (something) || something == void_type_node)
    return NULL_TREE;

  /* Pointer types can be reall pointer types or fake pointers. When
     finding a real pointer, recheck for primitive types */
  if (TREE_CODE (something) == POINTER_TYPE)
    {
      if (TREE_TYPE (something))
	{
	  something = TREE_TYPE (something);
	  if (JPRIMITIVE_TYPE_P (something) || something == void_type_node)
	    return NULL_TREE;
	}
      else
	something = TYPE_NAME (something);
    }

  /* Don't do anything for arrays of primitive types */
  if (TREE_CODE (something) == RECORD_TYPE && TYPE_ARRAY_P (something)
      && JPRIMITIVE_TYPE_P (TYPE_ARRAY_ELEMENT (something)))
    return NULL_TREE;

  /* If something is not and IDENTIFIER_NODE, it can be a a TYPE_DECL
     or a real TYPE */
  if (TREE_CODE (something) != IDENTIFIER_NODE)
    something = (TREE_CODE (TYPE_NAME (something)) == TYPE_DECL ?
	    DECL_NAME (TYPE_NAME (something)) : TYPE_NAME (something));

  if (!(decl = resolve_no_layout (something, cl)))
    return NULL_TREE;

  /* Resolve and layout if necessary */
  layout_class_methods (TREE_TYPE (decl));
  if (CLASS_FROM_SOURCE_P (TREE_TYPE (decl)))
    CHECK_METHODS (decl);
  if (TREE_TYPE (decl) != current_class && !CLASS_LOADED_P (TREE_TYPE (decl)))
    safe_layout_class (TREE_TYPE (decl));

  return decl;
}

/* Resolve a class, returns its decl but doesn't perform any
   layout. The current parsing context is saved and restored */

static tree
resolve_no_layout (name, cl)
     tree name, cl;
{
  tree ptr, decl;
  BUILD_PTR_FROM_NAME (ptr, name);
  java_parser_context_save_global ();
  decl = resolve_class (ptr, NULL_TREE, cl);
  java_parser_context_restore_global ();
  
  return decl;
}

/* Called when reporting errors. Skip leader '[' in a complex array
   type description that failed to be resolved.  */

static char *
purify_type_name (name)
     char *name;
{
  while (*name && *name == '[')
    name++;
  return name;
}

/* The type CURRENT refers to can't be found. We print error messages.  */

static void
complete_class_report_errors (dep)
     jdep *dep;
{
  char *name;

  if (!JDEP_WFL (dep))
    return;

  name = IDENTIFIER_POINTER (EXPR_WFL_NODE (JDEP_WFL (dep)));
  switch (JDEP_KIND (dep))
    {
    case JDEP_SUPER:
      parse_error_context  
	(JDEP_WFL (dep), "Superclass `%s' of class `%s' not found",
	 purify_type_name (name),
	 IDENTIFIER_POINTER (DECL_NAME (JDEP_DECL (dep))));
      break;
    case JDEP_FIELD:
      parse_error_context
	(JDEP_WFL (dep), "Type `%s' not found in declaration of field `%s'",
	 purify_type_name (name),
	 IDENTIFIER_POINTER (DECL_NAME (JDEP_DECL (dep))));
      break;
    case JDEP_METHOD:		/* Covers arguments */
      parse_error_context
	(JDEP_WFL (dep), "Type `%s' not found in the declaration of the "
	 "argument `%s' of method `%s'",
	 purify_type_name (name),
	 IDENTIFIER_POINTER (EXPR_WFL_NODE (JDEP_DECL_WFL (dep))),
	 IDENTIFIER_POINTER (EXPR_WFL_NODE (JDEP_MISC (dep))));
      break;
    case JDEP_METHOD_RETURN:	/* Covers return type */
      parse_error_context
	(JDEP_WFL (dep), "Type `%s' not found in the declaration of the "
	 "return type of method `%s'", 
	 purify_type_name (name),
	 IDENTIFIER_POINTER (EXPR_WFL_NODE (JDEP_DECL_WFL (dep))));
      break;
    case JDEP_INTERFACE:
      parse_error_context
	(JDEP_WFL (dep), "Superinterface `%s' of %s `%s' not found",
	 IDENTIFIER_POINTER (EXPR_WFL_NODE (JDEP_WFL (dep))),
	 (CLASS_OR_INTERFACE (JDEP_DECL (dep), "class", "interface")),
	 IDENTIFIER_POINTER (DECL_NAME (JDEP_DECL (dep))));
      break;
    case JDEP_VARIABLE:
      parse_error_context
	(JDEP_WFL (dep), "Type `%s' not found in the declaration of the "
	 "local variable `%s'", 
	 purify_type_name (IDENTIFIER_POINTER 
			   (EXPR_WFL_NODE (JDEP_WFL (dep)))),
	 IDENTIFIER_POINTER (DECL_NAME (JDEP_DECL (dep))));
      break;
    case JDEP_EXCEPTION:	/* As specified by `throws' */
      parse_error_context 
	  (JDEP_WFL (dep), "Class `%s' not found in `throws'",
	 IDENTIFIER_POINTER (EXPR_WFL_NODE (JDEP_WFL (dep))));
      break;
    default:
      /* Fix for -Wall. Just break doing nothing. The error will be
         caught later */
      break;
    }
}

/* Check uninitialized final.  */

void
java_check_final ()
{
}

/* Return a static string containing the DECL prototype string. If
   DECL is a constructor, use the class name instead of the form
   <init> */

static char *
get_printable_method_name (decl)
     tree decl;
{
  char *to_return;
  tree name = NULL_TREE;

  if (DECL_CONSTRUCTOR_P (decl))
    {
      name = DECL_NAME (decl);
      DECL_NAME (decl) = DECL_NAME (TYPE_NAME (DECL_CONTEXT (decl)));
    }
      
  to_return = lang_printable_name (decl, 0);
  if (DECL_CONSTRUCTOR_P (decl))
    DECL_NAME (decl) = name;
  
  return to_return;
}

/* Reinstall the proper DECL_NAME on METHOD. Return 0 if the method
   nevertheless needs to be verfied, 1 otherwise.  */

static int
reset_method_name (method)
     tree method;
{
  if (!IS_CLINIT (method) && DECL_NAME (method) != finit_identifier_node)
    {
      /* NAME is just the plain name when Object is being defined */
      if (DECL_CONTEXT (method) != object_type_node)
	DECL_NAME (method) = (DECL_CONSTRUCTOR_P (method) ? 
			      init_identifier_node : GET_METHOD_NAME (method));
      return 0;
    }
  else 
    return 1;
}

/* Return the name of METHOD_DECL, when DECL_NAME is a WFL */

tree
java_get_real_method_name (method_decl)
     tree method_decl;
{
  tree method_name = DECL_NAME (method_decl);
  if (DECL_CONSTRUCTOR_P (method_decl))
    return init_identifier_node;

  /* Explain here why METHOD_DECL doesn't have the DECL_CONSTRUCTUR_P
     and still can be a constructor. FIXME */

  /* Don't confuse method only bearing the name of their class as
     constructors */
  else if (!CLASS_FROM_SOURCE_P (DECL_CONTEXT (method_decl))
	   && ctxp
	   && ctxp->current_parsed_class_un == EXPR_WFL_NODE (method_name)
	   && get_access_flags_from_decl (method_decl) <= ACC_PROTECTED
	   && TREE_TYPE (TREE_TYPE (method_decl)) == void_type_node)
    return init_identifier_node;
  else
    return EXPR_WFL_NODE (method_name);
}

/* Track method being redefined inside the same class. As a side
   effect, set DECL_NAME to an IDENTIFIER (prior entering this
   function it's a FWL, so we can track errors more accurately */

static int
check_method_redefinition (class, method)
     tree class, method;
{
  tree redef, name;
  tree cl = DECL_NAME (method);
  tree sig = TYPE_ARGUMENT_SIGNATURE (TREE_TYPE (method));
  /* decl name of artificial <clinit> and $finit$ doesn't need to be
     fixed and checked */

  /* Reset the method name before running the check. If it returns 1,
     the method doesn't need to be verified with respect to method
     redeclaration and we return 0 */
  if (reset_method_name (method))
    return 0;

  name = DECL_NAME (method);
  for (redef = TYPE_METHODS (class); redef; redef = TREE_CHAIN (redef))
    {
      if (redef == method)
	break;
      if (DECL_NAME (redef) == name 
	  && sig == TYPE_ARGUMENT_SIGNATURE (TREE_TYPE (redef)))
	{
	  parse_error_context 
	    (cl, "Duplicate %s declaration `%s'",
	     (DECL_CONSTRUCTOR_P (redef) ? "constructor" : "method"),
	     get_printable_method_name (redef));
	  return 1;
	}
    }
  return 0;
}

/* Check all the methods of CLASS. Methods are first completed then
   checked according to regular method existance rules.
   If no constructor were encountered, then build its declaration. */

static void
java_check_regular_methods (class_decl)
     tree class_decl;
{
  int saw_constructor = 0;
  tree method;
  tree class = CLASS_TO_HANDLE_TYPE (TREE_TYPE (class_decl));
  tree super_class = CLASSTYPE_SUPER (class);
  tree saved_found_wfl = NULL_TREE, found = NULL_TREE;
  tree mthrows;

  /* It is not necessary to check methods defined in java.lang.Object */
  if (class == object_type_node)
    return;

  if (!TYPE_NVIRTUALS (class))
    TYPE_METHODS (class) = nreverse (TYPE_METHODS (class));

  /* Should take interfaces into account. FIXME */
  for (method = TYPE_METHODS (class); method; method = TREE_CHAIN (method))
    {
      tree sig;
      tree method_wfl = DECL_NAME (method);
      int aflags;

      /* If we previously found something and its name was saved,
         reinstall it now */
      if (found && saved_found_wfl)
	{
	  DECL_NAME (found) = saved_found_wfl;
	  saved_found_wfl = NULL_TREE;
	}

      /* Check for redefinitions */
      if (check_method_redefinition (class, method))
	continue;

      /* If we see one constructor a mark so we don't generate the
	 default one. Also skip other verifications: constructors
	 can't be inherited hence hiden or overriden */
     if (DECL_CONSTRUCTOR_P (method))
       {
	 saw_constructor = 1;
	 continue;
       }

      /* We verify things thrown by the method. They must inherits from
	 java.lang.Throwable */
      for (mthrows = DECL_FUNCTION_THROWS (method);
	   mthrows; mthrows = TREE_CHAIN (mthrows))
	{
	  if (!inherits_from_p (TREE_VALUE (mthrows), throwable_type_node))
	    parse_error_context 
	      (TREE_PURPOSE (mthrows), "Class `%s' in `throws' clause must be "
	       "a subclass of class `java.lang.Throwable'",
	       IDENTIFIER_POINTER 
	         (DECL_NAME (TYPE_NAME (TREE_VALUE (mthrows)))));
	}

      sig = build_java_argument_signature (TREE_TYPE (method));
      found = lookup_argument_method (super_class, DECL_NAME (method), sig);

      /* Nothing overrides or it's a private method. */
      if (!found)
	continue;
      if (METHOD_PRIVATE (found))
	{
	  found = NULL_TREE;
	  continue;
	}

      /* If found wasn't verified, it's DECL_NAME won't be set properly. 
	 We set it temporarily for the sake of the error report. */
      saved_found_wfl = DECL_NAME (found);
      reset_method_name (found);

      /* Can't override a method with the same name and different return
	 types. */
      if (TREE_TYPE (TREE_TYPE (found)) != TREE_TYPE (TREE_TYPE (method)))
	{
	  char *t = strdup (lang_printable_name (TREE_TYPE (TREE_TYPE (found)),
						 0));
	  parse_error_context 
	    (method_wfl,
	     "Method `%s' was defined with return type `%s' in class `%s'", 
	     lang_printable_name (found, 0), t,
	     IDENTIFIER_POINTER 
	       (DECL_NAME (TYPE_NAME (DECL_CONTEXT (found)))));
	  free (t);
	}

      aflags = get_access_flags_from_decl (found);
      /* If the method has default, access in an other package, then
	 issue a warning that the current method doesn't override the
	 one that was found elsewhere. Do not issue this warning when
	 the match was found in java.lang.Object.  */
      if (DECL_CONTEXT (found) != object_type_node
	  && ((aflags & 0x7) == 0)
	  && !class_in_current_package (DECL_CONTEXT (found))
	  && DECL_NAME (found) != clinit_identifier_node
	  && flag_not_overriding)
        {
	  parse_warning_context 
	    (method_wfl, "Method `%s' in class `%s' does not "
	     "override the corresponding method in class `%s', which is "
	     "private to a different package",
	     lang_printable_name (found, 0),
	     IDENTIFIER_POINTER (DECL_NAME (class_decl)),
	     IDENTIFIER_POINTER (DECL_NAME 
				 (TYPE_NAME (DECL_CONTEXT (found)))));
	  continue;
	}

      /* Can't override final. Can't override static. */
      if (METHOD_FINAL (found) || METHOD_STATIC (found))
	{
	  /* Static *can* override static */
	  if (METHOD_STATIC (found) && METHOD_STATIC (method))
	    continue;
	  parse_error_context 
	    (method_wfl,
	     "%s methods can't be overriden. Method `%s' is %s in class `%s'",
	     (METHOD_FINAL (found) ? "Final" : "Static"),
	     lang_printable_name (found, 0),
	     (METHOD_FINAL (found) ? "final" : "static"),
	     IDENTIFIER_POINTER
	       (DECL_NAME (TYPE_NAME (DECL_CONTEXT (found)))));
	  continue;
	}

      /* Static method can't override instance method. */
      if (METHOD_STATIC (method))
	{
	  parse_error_context 
	    (method_wfl,
	     "Instance methods can't be overriden by a static method. Method "
	     "`%s' is an instance method in class `%s'",
	     lang_printable_name (found, 0),
	     IDENTIFIER_POINTER
	       (DECL_NAME (TYPE_NAME (DECL_CONTEXT (found)))));
	  continue;
	}

      /* - Overriding/hiding public must be public
	 - Overriding/hiding protected must be protected or public
         - If the overriden or hidden method has default (package)
           access, then the overriding or hiding method must not be
           private; otherwise, a compile-time error occurs */
      if ((METHOD_PUBLIC (found) && !METHOD_PUBLIC (method)) 
	  || (METHOD_PROTECTED (found) 
	      && !(METHOD_PUBLIC (method) || METHOD_PROTECTED (method)))
	  || (!(aflags & (ACC_PUBLIC | ACC_PRIVATE | ACC_STATIC))
	      && METHOD_PRIVATE (method)))
	{
	  parse_error_context 
	    (method_wfl,
	     "Methods can't be overridden to be more private. Method `%s' is "
	     "not %s in class `%s'", lang_printable_name (method, 0),
	     (METHOD_PUBLIC (method) ? "public" : 
	      (METHOD_PRIVATE (method) ? "private" : "protected")),
	     IDENTIFIER_POINTER (DECL_NAME 
				 (TYPE_NAME (DECL_CONTEXT (found)))));
	  continue;
	}

      /* Overriding methods must have compatible `throws' clauses on checked
	 exceptions, if any */
      check_throws_clauses (method, method_wfl, found);

      /* Inheriting multiple methods with the same signature. FIXME */
    }
  
  /* Don't forget eventual pending found and saved_found_wfl. Take
     into account that we might have exited because we saw an
     aritifical method as the last entry. */

  if (found && !DECL_ARTIFICIAL (found) && saved_found_wfl)
    DECL_NAME (found) = saved_found_wfl;

  if (!TYPE_NVIRTUALS (class))
    TYPE_METHODS (class) = nreverse (TYPE_METHODS (class));

  if (!saw_constructor)
    {
      /* No constructor seen, we craft one, at line 0. Since this
       operation takes place after we laid methods out
       (layout_class_methods), we prepare the its DECL
       appropriately. */
      int flags;
      tree decl;

      /* If the class is declared PUBLIC, the default constructor is
         PUBLIC otherwise it has default access implied by no access
         modifiers. */
      flags = (get_access_flags_from_decl (class_decl) & ACC_PUBLIC ?
	       ACC_PUBLIC : 0);
      decl = create_artificial_method (class, flags, void_type_node, 
				       init_identifier_node, end_params_node);
      DECL_CONSTRUCTOR_P (decl) = 1;
      layout_class_method (TREE_TYPE (class_decl), NULL_TREE, decl, NULL_TREE);
    }
}

/* Return a non zero value if the `throws' clause of METHOD (if any)
   is incompatible with the `throws' clause of FOUND (if any).  */

static void
check_throws_clauses (method, method_wfl, found)
     tree method, method_wfl, found;
{
  tree mthrows, fthrows;

  /* Can't check these things with class loaded from bytecode. FIXME */
  if (!CLASS_FROM_SOURCE_P (DECL_CONTEXT (found)))
    return;

  for (mthrows = DECL_FUNCTION_THROWS (method);
       mthrows; mthrows = TREE_CHAIN (mthrows))
    {
      /* We don't verify unchecked expressions */
      if (IS_UNCHECKED_EXCEPTION_P (TREE_VALUE (mthrows)))
	continue;
      /* Checked expression must be compatible */
      for (fthrows = DECL_FUNCTION_THROWS (found); 
	   fthrows; fthrows = TREE_CHAIN (fthrows))
	if (inherits_from_p (TREE_VALUE (mthrows), TREE_VALUE (fthrows)))
	  break;
      if (!fthrows)
	{
	  parse_error_context 
	    (method_wfl, "Invalid checked exception class `%s' in "
	     "`throws' clause. The exception must be a subclass of an "
	     "exception thrown by `%s' from class `%s'",
	     IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (TREE_VALUE (mthrows)))),
	     lang_printable_name (found, 0),
	     IDENTIFIER_POINTER 
	       (DECL_NAME (TYPE_NAME (DECL_CONTEXT (found)))));
	}
    }
}

/* Check abstract method of interface INTERFACE */

static void
java_check_abstract_methods (interface_decl)
     tree interface_decl;
{
  int i, n;
  tree method, basetype_vec, found;
  tree interface = TREE_TYPE (interface_decl);

  for (method = TYPE_METHODS (interface); method; method = TREE_CHAIN (method))
    {
      tree method_wfl = DECL_NAME (method);

      /* 2- Check for double definition inside the defining interface */
      if (check_method_redefinition (interface, method))
	continue;

      /* 3- Overriding is OK as far as we preserve the return type and
	 the thrown exceptions (FIXME) */
      found = lookup_java_interface_method2 (interface, method);
      if (found)
	{
	  char *t;
	  tree saved_found_wfl = DECL_NAME (found);
	  reset_method_name (found);
	  t = strdup (lang_printable_name (TREE_TYPE (TREE_TYPE (found)), 0));
	  parse_error_context 
	    (method_wfl,
	     "Method `%s' was defined with return type `%s' in class `%s'",
	     lang_printable_name (found, 0), t,
	     IDENTIFIER_POINTER 
	       (DECL_NAME (TYPE_NAME (DECL_CONTEXT (found)))));
	  free (t);
	  continue;
	  
	  DECL_NAME (found) = saved_found_wfl;
	}
    }

  /* 4- Inherited methods can't differ by their returned types */
  if (!(basetype_vec = TYPE_BINFO_BASETYPES (interface)))
    return;
  n = TREE_VEC_LENGTH (basetype_vec);
  for (i = 0; i < n; i++)
    {
      tree sub_interface_method, sub_interface;
      tree vec_elt = TREE_VEC_ELT (basetype_vec, i);
      if (!vec_elt)
	continue;
      sub_interface = BINFO_TYPE (vec_elt);
      for (sub_interface_method = TYPE_METHODS (sub_interface); 
	   sub_interface_method;
	   sub_interface_method = TREE_CHAIN (sub_interface_method))
	{
	  found = lookup_java_interface_method2 (interface, 
						 sub_interface_method);
	  if (found && (found != sub_interface_method))
	    {
	      tree saved_found_wfl = DECL_NAME (found);
	      reset_method_name (found);
	      parse_error_context 
		(lookup_cl (sub_interface_method),
		 "Interface `%s' inherits method `%s' from interface `%s'. "
		 "This method is redefined with a different return type in "
		 "interface `%s'",
		 IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (interface))),
		 lang_printable_name (found, 0),
		 IDENTIFIER_POINTER 
		   (DECL_NAME (TYPE_NAME 
			       (DECL_CONTEXT (sub_interface_method)))),
		 IDENTIFIER_POINTER 
	           (DECL_NAME (TYPE_NAME (DECL_CONTEXT (found)))));
	      DECL_NAME (found) = saved_found_wfl;
	    }
	}
    }
}

/* Lookup methods in interfaces using their name and partial
   signature. Return a matching method only if their types differ.  */

static tree
lookup_java_interface_method2 (class, method_decl)
     tree class, method_decl;
{
  int i, n;
  tree basetype_vec = TYPE_BINFO_BASETYPES (class), to_return;

  if (!basetype_vec)
    return NULL_TREE;

  n = TREE_VEC_LENGTH (basetype_vec);
  for (i = 0; i < n; i++)
    {
      tree vec_elt = TREE_VEC_ELT (basetype_vec, i), to_return;
      if ((BINFO_TYPE (vec_elt) != object_type_node)
	  && (to_return = 
	      lookup_java_method2 (BINFO_TYPE (vec_elt), method_decl, 1)))
	return to_return;
    }
  for (i = 0; i < n; i++)
    {
      to_return = lookup_java_interface_method2 
	(BINFO_TYPE (TREE_VEC_ELT (basetype_vec, i)), method_decl);
      if (to_return)
	return to_return;
    }

  return NULL_TREE;
}

/* Lookup method using their name and partial signature. Return a
   matching method only if their types differ.  */

static tree
lookup_java_method2 (clas, method_decl, do_interface)
     tree clas, method_decl;
     int do_interface;
{
  tree method, method_signature, method_name, method_type, name;

  method_signature = build_java_argument_signature (TREE_TYPE (method_decl));
  name = DECL_NAME (method_decl);
  method_name = (TREE_CODE (name) == EXPR_WITH_FILE_LOCATION ? 
		 EXPR_WFL_NODE (name) : name);
  method_type = TREE_TYPE (TREE_TYPE (method_decl));

  while (clas != NULL_TREE)
    {
      for (method = TYPE_METHODS (clas);
	   method != NULL_TREE;  method = TREE_CHAIN (method))
	{
	  tree method_sig = build_java_argument_signature (TREE_TYPE (method));
	  tree name = DECL_NAME (method);
	  if ((TREE_CODE (name) == EXPR_WITH_FILE_LOCATION ?
	       EXPR_WFL_NODE (name) : name) == method_name
	      && method_sig == method_signature 
	      && TREE_TYPE (TREE_TYPE (method)) != method_type)
	    return method;
	}
      clas = (do_interface ? NULL_TREE : CLASSTYPE_SUPER (clas));
    }
  return NULL_TREE;
}

/* Return the line that matches DECL line number. Used during error
   report */

static tree
lookup_cl (decl)
     tree decl;
{
  static tree cl = NULL_TREE;
  
  if (!decl)
    return NULL_TREE;

  if (cl == NULL_TREE)
    cl = build_expr_wfl (NULL_TREE, NULL, 0, 0);

  EXPR_WFL_FILENAME_NODE (cl) = get_identifier (DECL_SOURCE_FILE (decl));
  EXPR_WFL_SET_LINECOL (cl, DECL_SOURCE_LINE_FIRST (decl), -1);

  return cl;
}

/* Look for a simple name in the single-type import list */

static tree
find_name_in_single_imports (name)
     tree name;
{
  tree node;

  for (node = ctxp->import_list; node; node = TREE_CHAIN (node))
    if (TREE_VALUE (node) == name)
      return (EXPR_WFL_NODE (TREE_PURPOSE (node)));

  return NULL_TREE;
}

/* Process all single-type import. */

static int
process_imports ()
{
  tree import;
  int error_found;

  for (import = ctxp->import_list; import; import = TREE_CHAIN (import))
    {
      tree to_be_found = EXPR_WFL_NODE (TREE_PURPOSE (import));

      /* Don't load twice something already defined. */
      if (IDENTIFIER_CLASS_VALUE (to_be_found))
	continue;
      QUALIFIED_P (to_be_found) = 1;
      load_class (to_be_found, 0);
      error_found =
	check_pkg_class_access (to_be_found, TREE_PURPOSE (import));
      if (!IDENTIFIER_CLASS_VALUE (to_be_found))
	{
	  parse_error_context (TREE_PURPOSE (import),
			       "Class or interface `%s' not found in import",
			       IDENTIFIER_POINTER (to_be_found));
	  return 1;
	}
      if (error_found)
	return 1;
    }
  return 0;
}

/* Possibly find a class imported by a single-type import statement. Return
   1 if an error occured, 0 otherwise. */

static int
find_in_imports (class_type)
     tree class_type;
{
  tree import;

  for (import = ctxp->import_list; import; import = TREE_CHAIN (import))
    if (TREE_VALUE (import) == TYPE_NAME (class_type))
      {
	TYPE_NAME (class_type) = EXPR_WFL_NODE (TREE_PURPOSE (import));
	QUALIFIED_P (TYPE_NAME (class_type)) = 1;
      }
  return 0;
}

static int
note_possible_classname (name, len)
     char *name;
     int len;
{
  tree node;
  if (len > 5 && strncmp (&name [len-5], ".java", 5) == 0)
    len = len - 5;
  else if (len > 6 && strncmp (&name [len-6], ".class", 6) == 0)
    len = len - 6;
  else
    return 0;
  node = ident_subst (name, len, "", '/', '.', "");
  IS_A_CLASSFILE_NAME (node) = 1; /* Or soon to be */
  QUALIFIED_P (node) = 1; /* As soon as we turn / into . */
  return 1;
}

/* Read a import directory, gathering potential match for further type
   references. Indifferently reads a filesystem or a ZIP archive
   directory.  */

static void
read_import_dir (wfl)
     tree wfl;
{
  tree package_id = EXPR_WFL_NODE (wfl);
  char *package_name = IDENTIFIER_POINTER (package_id);
  int package_length = IDENTIFIER_LENGTH (package_id);
  DIR *dirp = NULL;
  JCF *saved_jcf = current_jcf;

  int found = 0;
  int k;
  void *entry;
  struct buffer filename[1];


  if (IS_AN_IMPORT_ON_DEMAND_P (package_id))
    return;
  IS_AN_IMPORT_ON_DEMAND_P (package_id) = 1;

  BUFFER_INIT (filename);
  buffer_grow (filename, package_length + 100);

  for (entry = jcf_path_start (); entry != NULL; entry = jcf_path_next (entry))
    {
      char *entry_name = jcf_path_name (entry);
      int entry_length = strlen (entry_name);
      if (jcf_path_is_zipfile (entry))
	{
	  ZipFile *zipf;
	  buffer_grow (filename, entry_length);
	  memcpy (filename->data, entry_name, entry_length - 1);
	  filename->data[entry_length-1] = '\0';
	  zipf = opendir_in_zip (filename->data, jcf_path_is_system (entry));
	  if (zipf == NULL)
	    error ("malformed .zip archive in CLASSPATH: %s", entry_name);
	  else
	    {
	      ZipDirectory *zipd = (ZipDirectory *) zipf->central_directory;
	      BUFFER_RESET (filename);
	      for (k = 0; k < package_length; k++)
		{
		  char ch = package_name[k];
		  *filename->ptr++ = ch == '.' ? '/' : ch;
		}
	      *filename->ptr++ = '/';

	      for (k = 0; k < zipf->count;  k++, zipd = ZIPDIR_NEXT (zipd))
		{
		  char *current_entry = ZIPDIR_FILENAME (zipd);
		  int current_entry_len = zipd->filename_length;

		  if (current_entry_len >= BUFFER_LENGTH (filename)
		      && strncmp (filename->data, current_entry, 
				  BUFFER_LENGTH (filename)) != 0)
		    continue;
		  found |= note_possible_classname (current_entry,
						    current_entry_len);
		}
	    }
	}
      else
	{
	  BUFFER_RESET (filename);
	  buffer_grow (filename, entry_length + package_length + 4);
	  strcpy (filename->data, entry_name);
	  filename->ptr = filename->data + entry_length;
	  for (k = 0; k < package_length; k++)
	    {
	      char ch = package_name[k];
	      *filename->ptr++ = ch == '.' ? '/' : ch;
	    }
	  *filename->ptr = '\0';

	  dirp = opendir (filename->data);
	  if (dirp == NULL)
	    continue;
	  *filename->ptr++ = '/';
	  for (;;)
	    {
	      int len; 
	      char *d_name;
	      struct dirent *direntp = readdir (dirp);
	      if (!direntp)
		break;
	      d_name = direntp->d_name;
	      len = strlen (direntp->d_name);
	      buffer_grow (filename, len+1);
	      strcpy (filename->ptr, d_name);
	      found |= note_possible_classname (filename->data + entry_length,
						package_length+len+1);
	    }
	  if (dirp)
	    closedir (dirp);
	}
    }

  free (filename->data);

  /* Here we should have a unified way of retrieving an entry, to be
     indexed. */
  if (!found)
    {
      static int first = 1;
      if (first)
	{
	  char buffer [256];
	  sprintf (buffer, "Can't find default package `%s'. Check "
		   "the CLASSPATH environment variable and the access to the "
		   "archives.", package_name);
	  error (buffer);
	  java_error_count++;
	  first = 0;
	}
      else
	parse_error_context (wfl, "Package `%s' not found in import",
			     package_name);
      current_jcf = saved_jcf;
      return;
    }
  current_jcf = saved_jcf;
}

/* Possibly find a type in the import on demands specified
   types. Returns 1 if an error occured, 0 otherwise. Run throught the
   entire list, to detected potential double definitions.  */
		 
static int
find_in_imports_on_demand (class_type)
     tree class_type;
{
  tree node, import, node_to_use = NULL_TREE;
  int seen_once = -1;
  tree cl = NULL_TREE;

  for (import = ctxp->import_demand_list; import; import = TREE_CHAIN (import))
    {
      char *id_name;
      obstack_grow (&temporary_obstack, 
		    IDENTIFIER_POINTER (EXPR_WFL_NODE (TREE_PURPOSE (import))),
		    IDENTIFIER_LENGTH (EXPR_WFL_NODE (TREE_PURPOSE (import))));
      obstack_1grow (&temporary_obstack, '.');
      obstack_grow0 (&temporary_obstack, 
		     IDENTIFIER_POINTER (TYPE_NAME (class_type)),
		     IDENTIFIER_LENGTH (TYPE_NAME (class_type)));
      id_name = obstack_finish (&temporary_obstack);
	      
      node = maybe_get_identifier (id_name);
      if (node && IS_A_CLASSFILE_NAME (node))
	{
	  if (seen_once < 0)
	    {
	      cl = TREE_PURPOSE (import);
	      seen_once = 1;
	      node_to_use = node;
	    }
	  else
	    {
	      seen_once++;
	      parse_error_context 
		(import, "Type `%s' also potentially defined in package `%s'",
		 IDENTIFIER_POINTER (TYPE_NAME (class_type)),
		 IDENTIFIER_POINTER (EXPR_WFL_NODE (TREE_PURPOSE (import))));
	    }
	}
    }

  if (seen_once == 1)
    {
      /* Setup lineno so that it refers to the line of the import (in
	 case we parse a class file and encounter errors */
      tree decl;
      int saved_lineno = lineno;
      lineno = EXPR_WFL_LINENO (cl);
      TYPE_NAME (class_type) = node_to_use;
      QUALIFIED_P (TYPE_NAME (class_type)) = 1;
      decl = IDENTIFIER_CLASS_VALUE (TYPE_NAME (class_type));
      /* If there is no DECL set for the class or if the class isn't
	 loaded and not seen in source yet, the load */
      if (!decl || (!CLASS_LOADED_P (TREE_TYPE (decl))
		    && !CLASS_FROM_SOURCE_P (TREE_TYPE (decl))))
	load_class (node_to_use, 0);
      lineno = saved_lineno;
      return check_pkg_class_access (TYPE_NAME (class_type), cl);
    }
  else
    return (seen_once < 0 ? 0 : seen_once); /* It's ok not to have found */
}

static tree
resolve_package (pkg, next)
     tree pkg, *next;
{
  tree type_name = NULL_TREE;
  char *name = IDENTIFIER_POINTER (EXPR_WFL_NODE (pkg));

  /* The trick is to determine when the package name stops and were
     the name of something contained in the package starts. Then we
     return a fully qualified name of what we want to get. */

  /* Do a quick search on well known package names */
  if (!strncmp (name, "java.lang.reflect", 17))
    {
      *next = 
	TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (EXPR_WFL_QUALIFICATION (pkg))));
      type_name = lookup_package_type (name, 17);
    }
  else if (!strncmp (name, "java.lang", 9))
    {
      *next = TREE_CHAIN (TREE_CHAIN (EXPR_WFL_QUALIFICATION (pkg)));
      type_name = lookup_package_type (name, 9);
    }
  else
    return NULL_TREE;		/* FIXME, search all imported packages. */

  return type_name;
}

static tree
lookup_package_type (name, from)
     char *name;
     int from;
{
  char subname [128];
  char *sub = &name[from+1];
  while (*sub != '.' && *sub)
    sub++;
  strncpy (subname, name, sub-name);
  subname [sub-name] = '\0';
  return get_identifier (subname);
}

/* Check that CLASS_NAME refers to a PUBLIC class. Return 0 if no
   access violations were found, 1 otherwise.  */

static int
check_pkg_class_access (class_name, cl)
     tree class_name;
     tree cl;
{
  tree type;

  if (!QUALIFIED_P (class_name) || !IDENTIFIER_CLASS_VALUE (class_name))
    return 0;

  if (!(type = TREE_TYPE (IDENTIFIER_CLASS_VALUE (class_name))))
    return 0;

  if (!CLASS_PUBLIC (TYPE_NAME (type)))
    {
      /* Access to a private class within the same package is
         allowed. */
      tree l, r;
      breakdown_qualified (&l, &r, class_name);
      if (l == ctxp->package)
	return 0;

      parse_error_context 
	(cl, "Can't access %s `%s'. Only public classes and interfaces in "
	 "other packages can be accessed",
	 (CLASS_INTERFACE (TYPE_NAME (type)) ? "interface" : "class"),
	 IDENTIFIER_POINTER (class_name));
      return 1;
    }
  return 0;
}

/* Local variable declaration. */

static void
declare_local_variables (modifier, type, vlist)
     int modifier;
     tree type;
     tree vlist;
{
  tree decl, current, saved_type;
  tree type_wfl = NULL_TREE;
  int must_chain = 0;

  /* Push a new block if statements were seen between the last time we
     pushed a block and now. Keep a cound of block to close */
  if (BLOCK_EXPR_BODY (GET_CURRENT_BLOCK (current_function_decl)))
    {
      tree body = GET_CURRENT_BLOCK (current_function_decl);
      tree b = enter_block ();
      BLOCK_EXPR_ORIGIN (b) = body;
    }

  if (modifier)
    {
      int i;
      for (i = 0; i <= 10; i++) if (1 << i & modifier) break;
      if (modifier == ACC_FINAL)
	{
	  if (flag_static_local_jdk1_1)
	    parse_warning_context (ctxp->modifier_ctx [i], 
				   "Unsupported JDK1.1 `final' local variable "
				   "(treated as non final)");
	}
      else 
	{
	  parse_error_context 
	    (ctxp->modifier_ctx [i], 
	     "Only `final' is allowed as a local variables modifier");
	  return;
	}
    }

  /* Obtain an incomplete type if TYPE is not complete. TYPE_WFL will
     hold the TYPE value if a new incomplete has to be created (as
     opposed to being found already existing and reused). */
  SET_TYPE_FOR_RESOLUTION (type, type_wfl, must_chain);

  /* If TYPE is fully resolved and we don't have a reference, make one */
  PROMOTE_RECORD_IF_COMPLETE (type, must_chain);

  /* Go through all the declared variables */
  for (current = vlist, saved_type = type; current;
       current = TREE_CHAIN (current), type = saved_type)
    {
      tree other, real_type;
      tree wfl  = TREE_PURPOSE (current);
      tree name = EXPR_WFL_NODE (wfl);
      tree init = TREE_VALUE (current);

      /* Process NAME, as it may specify extra dimension(s) for it */
      type = build_array_from_name (type, type_wfl, name, &name);

      /* Variable redefinition check */
      if ((other = lookup_name_in_blocks (name)))
	{
	  variable_redefinition_error (wfl, name, TREE_TYPE (other),
				       DECL_SOURCE_LINE (other));
	  continue;
	}

      /* Type adjustment. We may have just readjusted TYPE because
	 the variable specified more dimensions. Make sure we have
	 a reference if we can and don't have one already. */
      PROMOTE_RECORD_IF_COMPLETE (type, must_chain);

      real_type = GET_REAL_TYPE (type);
      /* Never layout this decl. This will be done when its scope
	 will be entered */
      decl = build_decl (VAR_DECL, name, real_type);
      BLOCK_CHAIN_DECL (decl);
      
      /* If doing xreferencing, replace the line number with the WFL
         compound value */
      if (flag_emit_xref)
	DECL_SOURCE_LINE (decl) = EXPR_WFL_LINECOL (wfl);
      
      /* Don't try to use an INIT statement when an error was found */
      if (init && java_error_count)
	init = NULL_TREE;
      
      /* Add the initialization function to the current function's code */
      if (init)
	{
	  /* Name might have been readjusted */
	  EXPR_WFL_NODE (TREE_OPERAND (init, 0)) = name;
	  MODIFY_EXPR_FROM_INITIALIZATION_P (init) = 1;
	  java_method_add_stmt (current_function_decl,
				build_debugable_stmt (EXPR_WFL_LINECOL (init),
						      init));
	}
    
      /* Setup dependency the type of the decl */
      if (must_chain)
	{
	  jdep *dep;
	  register_incomplete_type (JDEP_VARIABLE, type_wfl, decl, type);
	  dep = CLASSD_LAST (ctxp->classd_list);
	  JDEP_GET_PATCH (dep) = &TREE_TYPE (decl);
	}
    }
  SOURCE_FRONTEND_DEBUG (("Defined locals"));
}

/* Called during parsing. Build decls from argument list.  */

static void
source_start_java_method (fndecl)
     tree fndecl;
{
  tree tem;
  tree parm_decl;
  int i;

  current_function_decl = fndecl;

  /* New scope for the function */
  enter_block ();
  for (tem = TYPE_ARG_TYPES (TREE_TYPE (fndecl)), i = 0;
       tem != end_params_node; tem = TREE_CHAIN (tem), i++)
    {
      tree type = TREE_VALUE (tem);
      tree name = TREE_PURPOSE (tem);
      
      /* If type is incomplete. Create an incomplete decl and ask for
	 the decl to be patched later */
      if (INCOMPLETE_TYPE_P (type))
	{
	  jdep *jdep;
	  tree real_type = GET_REAL_TYPE (type);
	  parm_decl = build_decl (PARM_DECL, name, real_type);
	  type = obtain_incomplete_type (type);
	  register_incomplete_type (JDEP_PARM, NULL_TREE, NULL_TREE, type);
	  jdep = CLASSD_LAST (ctxp->classd_list);
	  JDEP_MISC (jdep) = name;
	  JDEP_GET_PATCH (jdep) = &TREE_TYPE (parm_decl);
	}
      else
	parm_decl = build_decl (PARM_DECL, name, type);

      BLOCK_CHAIN_DECL (parm_decl);
    }
  tem = BLOCK_EXPR_DECLS (DECL_FUNCTION_BODY (current_function_decl));
  BLOCK_EXPR_DECLS (DECL_FUNCTION_BODY (current_function_decl)) =
    nreverse (tem);
  DECL_ARG_SLOT_COUNT (current_function_decl) = i;
}

/* Called during parsing. Creates an artificial method declaration.  */

static tree
create_artificial_method (class, flags, type, name, args)
     tree class;
     int flags;
     tree type, name, args;
{
  int saved_lineno = lineno;					    
  tree mdecl;

  lineno = 0;								    
  mdecl = make_node (FUNCTION_TYPE);				    
  TREE_TYPE (mdecl) = type;
  TYPE_ARG_TYPES (mdecl) = args;
  mdecl = add_method (class, flags, name, build_java_signature (mdecl)); 
  lineno = saved_lineno;						    
  DECL_ARTIFICIAL (mdecl) = 1;					    
  return mdecl;
}

/* Starts the body if an artifical method.  */

static void
start_artificial_method_body (mdecl)
     tree mdecl;
{
  DECL_SOURCE_LINE (mdecl) = 1;
  DECL_SOURCE_LINE_MERGE (mdecl, 1);
  source_start_java_method (mdecl);
  enter_block ();
}

static void
end_artificial_method_body (mdecl)
     tree mdecl;
{
  BLOCK_EXPR_BODY (DECL_FUNCTION_BODY (mdecl)) = exit_block ();
  exit_block ();
}

/* Called during expansion. Push decls formerly built from argument
   list so they're usable during expansion. */

static void
expand_start_java_method (fndecl)
     tree fndecl;
{
  tree tem, *ptr;

  current_function_decl = fndecl;

  announce_function (fndecl);
  pushlevel (1);		/* Push parameters */
  ptr = &DECL_ARGUMENTS (fndecl);
  tem  = BLOCK_EXPR_DECLS (DECL_FUNCTION_BODY (current_function_decl));
  while (tem)
    {
      tree next = TREE_CHAIN (tem);
      tree type = TREE_TYPE (tem);
#ifdef PROMOTE_PROTOTYPES
      if (TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node)
	  && INTEGRAL_TYPE_P (type))
	type = integer_type_node;
#endif
      DECL_ARG_TYPE (tem) = type;
      layout_decl (tem, 0);
      pushdecl (tem);
      *ptr = tem;
      ptr = &TREE_CHAIN (tem);
      tem = next;
    }
  *ptr = NULL_TREE;
  pushdecl_force_head (DECL_ARGUMENTS (fndecl));
  lineno = DECL_SOURCE_LINE_FIRST (fndecl);
}

/* Terminate a function and expand its body.  */

static void
source_end_java_method ()
{
  tree fndecl = current_function_decl;
  int flag_asynchronous_exceptions = asynchronous_exceptions;

  java_parser_context_save_global ();
  lineno = ctxp->last_ccb_indent1;

  /* Set EH language codes */
  java_set_exception_lang_code ();

  /* Turn function bodies with only a NOP expr null, so they don't get
     generated at all and we won't get warnings when using the -W
     -Wall flags. */
  if (BLOCK_EXPR_BODY (DECL_FUNCTION_BODY (fndecl)) == empty_stmt_node)
    BLOCK_EXPR_BODY (DECL_FUNCTION_BODY (fndecl)) = NULL_TREE;

  /* Generate function's code */
  if (BLOCK_EXPR_BODY (DECL_FUNCTION_BODY (fndecl))
      && ! flag_emit_class_files
      && ! flag_emit_xref)
    expand_expr_stmt (BLOCK_EXPR_BODY (DECL_FUNCTION_BODY (fndecl)));

  /* pop out of its parameters */
  pushdecl_force_head (DECL_ARGUMENTS (fndecl));
  poplevel (1, 0, 1);
  BLOCK_SUPERCONTEXT (DECL_INITIAL (fndecl)) = fndecl;

  /* Generate rtl for function exit.  */
  if (! flag_emit_class_files && ! flag_emit_xref)
    {
      lineno = DECL_SOURCE_LINE_LAST (fndecl);
      /* Emit catch-finally clauses */
      emit_handlers ();
      expand_function_end (input_filename, lineno, 0);

      /* FIXME: If the current method contains any exception handlers,
	 force asynchronous_exceptions: this is necessary because signal
	 handlers in libjava may throw exceptions.  This is far from being
	 a perfect solution, but it's better than doing nothing at all.*/
      if (catch_clauses)
	asynchronous_exceptions = 1;

      /* Run the optimizers and output assembler code for this function. */
      rest_of_compilation (fndecl);
    }

  current_function_decl = NULL_TREE;
  /*  permanent_allocation (1); */
  java_parser_context_restore_global ();
  asynchronous_exceptions = flag_asynchronous_exceptions;
}

/* Record EXPR in the current function block. Complements compound
   expression second operand if necessary.  */

tree
java_method_add_stmt (fndecl, expr)
     tree fndecl, expr;
{
  return add_stmt_to_block (GET_CURRENT_BLOCK (fndecl), NULL_TREE, expr);
}

static tree
add_stmt_to_block (b, type, stmt)
     tree b, type, stmt;
{
  tree body = BLOCK_EXPR_BODY (b), c;
  
  if (java_error_count)
    return body;
    
  if ((c = add_stmt_to_compound (body, type, stmt)) == body)
    return body;

  BLOCK_EXPR_BODY (b) = c;
  TREE_SIDE_EFFECTS (c) = 1;
  return c;
}

/* Add STMT to EXISTING if possible, otherwise create a new
   COMPOUND_EXPR and add STMT to it. */

static tree
add_stmt_to_compound (existing, type, stmt)
     tree existing, type, stmt;
{
  if (existing)
    return build (COMPOUND_EXPR, type, existing, stmt);
  else
    return stmt;
}

/* Hold THIS for the scope of the current public method decl.  */
static tree current_this;

void java_layout_seen_class_methods ()
{
  tree previous_list = all_class_list;
  tree end = NULL_TREE;
  tree current;

  while (1)
    {
      for (current = previous_list; 
	   current != end; current = TREE_CHAIN (current))
	layout_class_methods (TREE_TYPE (TREE_VALUE (current)));
      
      if (previous_list != all_class_list)
	{
	  end = previous_list;
	  previous_list = all_class_list;
	}
      else
	break;
    }
}

/* Layout the methods of all classes loaded in one way on an
   other. Check methods of source parsed classes. Then reorder the
   fields and layout the classes or the type of all source parsed
   classes */

void
java_layout_classes ()
{
  tree current;
  int save_error_count = java_error_count;

  /* Layout the methods of all classes seen so far */
  java_layout_seen_class_methods ();
  java_parse_abort_on_error ();
  all_class_list = NULL_TREE;

  /* Then check the methods of all parsed classes */
  for (current = ctxp->gclass_list; current; current = TREE_CHAIN (current))
    if (CLASS_FROM_SOURCE_P (TREE_TYPE (TREE_VALUE (current))))
      CHECK_METHODS (TREE_VALUE (current));
  java_parse_abort_on_error ();

  for (current = ctxp->gclass_list; current; current = TREE_CHAIN (current))
    {
      current_class = TREE_TYPE (TREE_VALUE (current));

      /* Reverse the fields, but leave the dummy field in front.
	 Fields are already ordered for Object and Class */
      if (TYPE_FIELDS (current_class) && current_class != object_type_node
	  && current_class != class_type_node)
      {
	/* If the dummy field is there, reverse the right fields and
	   just layout the type for proper fields offset */
	if (!DECL_NAME (TYPE_FIELDS (current_class)))
	  {
	    tree fields = TYPE_FIELDS (current_class);
	    TREE_CHAIN (fields) = nreverse (TREE_CHAIN (fields));
	    TYPE_SIZE (current_class) = NULL_TREE;
	    layout_type (current_class);
	  }
	/* We don't have a dummy field, we need to layout the class,
           after having reversed the fields */
	else
	  {
	    TYPE_FIELDS (current_class) = 
	      nreverse (TYPE_FIELDS (current_class));
	    TYPE_SIZE (current_class) = NULL_TREE;
	    layout_class (current_class);
	  }
      }
      else
	layout_class (current_class);

      /* From now on, the class is considered completely loaded */
      CLASS_LOADED_P (current_class) = 1;

      /* Error reported by the caller */
      if (java_error_count)
	return;
    }

  /* We might have reloaded classes durign the process of laying out
     classes for code generation. We must layout the methods of those
     late additions, as constructor checks might use them */
  java_layout_seen_class_methods ();
  java_parse_abort_on_error ();
}

/* Expand all methods in all registered classes.  */

void
java_complete_expand_methods ()
{
  tree current;

  do_not_fold = flag_emit_xref;
  
  for (current = ctxp->class_list; current; current = TREE_CHAIN (current))
    {
      int is_interface;
      tree class_type = CLASS_TO_HANDLE_TYPE (TREE_TYPE (current));
      tree decl;

      current_class = TREE_TYPE (current);
      is_interface = CLASS_INTERFACE (TYPE_NAME (current_class));

      /* Initialize a new constant pool */
      init_outgoing_cpool ();

      /* We want <clinit> (if any) to be processed first. */
      decl = tree_last (TYPE_METHODS (class_type));
      if (IS_CLINIT (decl))
	{
	  tree fbody = DECL_FUNCTION_BODY (decl);
	  tree list;
	  if (fbody != NULL_TREE)
	    {
	      /* First check if we can ignore empty <clinit> */
	      tree block_body = BLOCK_EXPR_BODY (fbody);

	      current_this = NULL_TREE;
	      current_function_decl = decl;
	      if (block_body != NULL_TREE)
		{
		  /* Prevent the use of `this' inside <clinit> */
		  ctxp->explicit_constructor_p = 1;

		  block_body = java_complete_tree (block_body);
		  ctxp->explicit_constructor_p = 0;
		  BLOCK_EXPR_BODY (fbody) = block_body;
		  if (block_body != NULL_TREE
		      && TREE_CODE (block_body) == BLOCK
		      && BLOCK_EXPR_BODY (block_body) == empty_stmt_node)
		    decl = NULL_TREE;
		}
	    }
	  list = nreverse (TREE_CHAIN (nreverse (TYPE_METHODS (class_type))));
	  if (decl != NULL_TREE)
	    {
	      TREE_CHAIN (decl) = list;
	      TYPE_METHODS (class_type) = decl;
	    }
	    else
	      TYPE_METHODS (class_type) = list;
	}
      
      for (decl = TYPE_METHODS (class_type); decl; decl = TREE_CHAIN (decl))
	{
	  current_function_decl = decl;
	  /* Don't generate debug info on line zero when expanding a
	     generated constructor. */
	  if (DECL_CONSTRUCTOR_P (decl) && !DECL_FUNCTION_BODY (decl))
	    {
	      /* If we found errors, it's too dangerous to try to
		 generate and expand a constructor */
	      if (!java_error_count)
		{
		  restore_line_number_status (1);
		  java_complete_expand_method (decl);
		  restore_line_number_status (0);
		  }
	    }
	  else if (METHOD_ABSTRACT (decl) || METHOD_NATIVE (decl))
	    continue;
	  else 
	    java_complete_expand_method (decl);
	}

      /* Now verify constructor circularity (stop after the first one
         we find) */
      if (!is_interface)
	for (decl = TYPE_METHODS (class_type); decl; decl = TREE_CHAIN (decl))
	  if (DECL_CONSTRUCTOR_P (decl) && 
	      verify_constructor_circularity (decl, decl))
	    break;

      /* Make the class data, register it and run the rest of decl
         compilation on it */
      if (!java_error_count)
	{
	  if (flag_emit_class_files)
	    write_classfile (current_class);
	  if (flag_emit_xref)
	    expand_xref (current_class);
	  else if (! flag_syntax_only)
	    finish_class (current_class);
	}
    }
}

/* Hold a list of catch clauses list. The first element of this list is
   the list of the catch clauses of the currently analysed try block. */
static tree currently_caught_type_list;

/* Complete and expand a method.  */

static void
java_complete_expand_method (mdecl)
     tree mdecl;
{
  /* Fix constructors before expanding them */
  if (DECL_CONSTRUCTOR_P (mdecl))
    fix_constructors (mdecl);
  
  /* Expand functions that have a body */
  if (DECL_FUNCTION_BODY (mdecl))
    {
      tree fbody = DECL_FUNCTION_BODY (mdecl);
      tree block_body = BLOCK_EXPR_BODY (fbody);
      tree exception_copy;
      expand_start_java_method (mdecl);
      build_result_decl (mdecl);

      current_this 
	= (!METHOD_STATIC (mdecl) ? 
	   BLOCK_EXPR_DECLS (DECL_FUNCTION_BODY (mdecl)) : NULL_TREE);

      /* Purge the `throws' list of unchecked exceptions. If we're
	 doing xref, save a copy of the list and re-install it
	 later. */
      if (flag_emit_xref)
	exception_copy = copy_list (DECL_FUNCTION_THROWS (mdecl));

      purge_unchecked_exceptions (mdecl);

      /* Install exceptions thrown with `throws' */
      PUSH_EXCEPTIONS (DECL_FUNCTION_THROWS (mdecl));

      if (block_body != NULL_TREE)
	{
	  block_body = java_complete_tree (block_body);
	  if (!flag_emit_xref)
	    check_for_initialization (block_body);
	  ctxp->explicit_constructor_p = 0;
	}
      BLOCK_EXPR_BODY (fbody) = block_body;

      if ((block_body == NULL_TREE || CAN_COMPLETE_NORMALLY (block_body))
	  && TREE_CODE (TREE_TYPE (TREE_TYPE (mdecl))) != VOID_TYPE
	  && !flag_emit_xref)
	missing_return_error (current_function_decl);

      complete_start_java_method (mdecl); 

      /* Don't go any further if we've found error(s) during the
         expansion */
      if (!java_error_count)
	source_end_java_method ();
      else
	{
	  pushdecl_force_head (DECL_ARGUMENTS (mdecl));
	  poplevel (1, 0, 1);
	}

      /* Pop the exceptions and sanity check */
      POP_EXCEPTIONS();
      if (currently_caught_type_list)
	fatal ("Exception list non empty - java_complete_expand_method");

      if (flag_emit_xref)
	DECL_FUNCTION_THROWS (mdecl) = exception_copy;
    }
}

/* Craft a body for default constructor. Patch existing constructor
   bodies with call to super() and field initialization statements if
   necessary.  */

static void
fix_constructors (mdecl)
     tree mdecl;
{
  tree body = DECL_FUNCTION_BODY (mdecl);

  if (!body)
    {
      /* The constructor body must be crafted by hand. It's the
	 constructor we defined when we realize we didn't have the
	 CLASSNAME() constructor */

      tree compound;

      /* It is an error for the compiler to generate a default
	 constructor if the superclass doesn't have a constructor that
	 takes no argument */
      if (verify_constructor_super ())
	{
	  tree sclass_decl = TYPE_NAME (CLASSTYPE_SUPER (current_class));
	  char *n = IDENTIFIER_POINTER (DECL_NAME (sclass_decl));
	  parse_error_context (lookup_cl (TYPE_NAME (current_class)), 
			       "No constructor matching `%s()' found in "
			       "class `%s'", n, n);
	}
      
      start_artificial_method_body (mdecl);
      
      /* We don't generate a super constructor invocation if we're
	 compiling java.lang.Object. build_super_invocation takes care
	 of that. */
      compound = java_method_add_stmt (mdecl, build_super_invocation ());

      end_artificial_method_body (mdecl);
    }
  /* Search for an explicit constructor invocation */
  else 
    {
      int found = 0;
      tree main_block = BLOCK_EXPR_BODY (body);
      tree compound = NULL_TREE;
      
      while (body)
	switch (TREE_CODE (body))
	  {
	  case CALL_EXPR:
	    found = CALL_EXPLICIT_CONSTRUCTOR_P (body);
	    body = NULL_TREE;
	    break;
	  case COMPOUND_EXPR:
	  case EXPR_WITH_FILE_LOCATION:
	    body = TREE_OPERAND (body, 0);
	    break;
	  case BLOCK:
	    body = BLOCK_EXPR_BODY (body);
	    break;
	  default:
	    found = 0;
	    body = NULL_TREE;
	  }
      /* The constructor is missing an invocation of super() */
      if (!found)
	compound = add_stmt_to_compound (compound, NULL_TREE,
					 build_super_invocation ());
      
      /* Fix the constructor main block if we're adding extra stmts */
      if (compound)
	{
	  compound = add_stmt_to_compound (compound, NULL_TREE,
					   BLOCK_EXPR_BODY (main_block));
	  BLOCK_EXPR_BODY (main_block) = compound;
	}
    }
}

/* Browse constructors in the super class, searching for a constructor
   that doesn't take any argument. Return 0 if one is found, 1
   otherwise. */

static int
verify_constructor_super ()
{
  tree class = CLASSTYPE_SUPER (current_class);
  if (!class)
    return 0;

  if (class)
    {
      tree mdecl;
      for (mdecl = TYPE_METHODS (class); mdecl; mdecl = TREE_CHAIN (mdecl))
	{
	  if (DECL_CONSTRUCTOR_P (mdecl)
	      && TREE_CHAIN (TYPE_ARG_TYPES (TREE_TYPE (mdecl))) == end_params_node)
	    return 0;
	}
    }
  return 1;
}

/* Expand finals.  */

void
java_expand_finals ()
{
}

/* Generate code for all context remembered for code generation.  */

void
java_expand_classes ()
{
  int save_error_count = 0;
  java_parse_abort_on_error ();
  if (!(ctxp = ctxp_for_generation))
    return;
  java_layout_classes ();
  java_parse_abort_on_error ();

  for (; ctxp_for_generation; ctxp_for_generation = ctxp_for_generation->next)
    {
      ctxp = ctxp_for_generation;
      lang_init_source (2);	       /* Error msgs have method prototypes */
      java_complete_expand_methods (); /* Complete and expand method bodies */
      java_parse_abort_on_error ();
      java_expand_finals ();	      /* Expand and check the finals */
      java_parse_abort_on_error ();
      java_check_final ();            /* Check unitialized final  */
      java_parse_abort_on_error ();
    }
}

/* Wrap non WFL PRIMARY around a WFL and set EXPR_WFL_QUALIFICATION to
   a tree list node containing RIGHT. Fore coming RIGHTs will be
   chained to this hook. LOCATION contains the location of the
   separating `.' operator.  */

static tree
make_qualified_primary (primary, right, location)
     tree primary, right;
     int location;
{
  tree wfl;

  /* We want to process THIS . xxx symbolicaly, to keep it consistent
     with the way we're processing SUPER. A THIS from a primary as a
     different form than a SUPER. Turn THIS into something symbolic */
  if (TREE_CODE (primary) == THIS_EXPR)
    {
      wfl = build_wfl_node (this_identifier_node);
      EXPR_WFL_LINECOL (wfl) = EXPR_WFL_LINECOL (primary);
      wfl = make_qualified_name (wfl, right, location);
      PRIMARY_P (wfl) = 1;
      return wfl;
    }
  /* Other non WFL node are wrapped around a WFL */
  else if (TREE_CODE (primary) != EXPR_WITH_FILE_LOCATION)
    {
      wfl = build_expr_wfl (NULL_TREE, ctxp->filename, 0, 0);
      EXPR_WFL_LINECOL (wfl) = EXPR_WFL_LINECOL (primary);
      EXPR_WFL_QUALIFICATION (wfl) = build_tree_list (primary, NULL_TREE);
    }
  else
    {
      wfl = primary;
      if (!EXPR_WFL_QUALIFICATION (primary))
	EXPR_WFL_QUALIFICATION (primary) = 
	  build_tree_list (primary, NULL_TREE);
    }

  EXPR_WFL_LINECOL (right) = location;
  chainon (EXPR_WFL_QUALIFICATION (wfl), build_tree_list (right, NULL_TREE));
  PRIMARY_P (wfl) =  1;
  return wfl;
}

/* Simple merge of two name separated by a `.' */

static tree
merge_qualified_name (left, right)
     tree left, right;
{
  tree node;
  obstack_grow (&temporary_obstack, IDENTIFIER_POINTER (left),
		IDENTIFIER_LENGTH (left));
  obstack_1grow (&temporary_obstack, '.');
  obstack_grow0 (&temporary_obstack, IDENTIFIER_POINTER (right),
		 IDENTIFIER_LENGTH (right));
  node =  get_identifier (obstack_base (&temporary_obstack));
  obstack_free (&temporary_obstack, obstack_base (&temporary_obstack));
  QUALIFIED_P (node) = 1;
  return node;
}

/* Merge the two parts of a qualified name into LEFT.  Set the
   location information of the resulting node to LOCATION, usually
   inherited from the location information of the `.' operator. */

static tree
make_qualified_name (left, right, location)
     tree left, right;
     int location;
{
#ifdef USE_COMPONENT_REF
  tree node = build (COMPONENT_REF, NULL_TREE, left, right);
  EXPR_WFL_LINECOL (node) = location;
  return node;
#else
  tree left_id = EXPR_WFL_NODE (left);
  tree right_id = EXPR_WFL_NODE (right);
  tree wfl, merge;

  merge = merge_qualified_name (left_id, right_id);

  /* Left wasn't qualified and is now qualified */
  if (!QUALIFIED_P (left_id))
    {
      tree wfl = build_expr_wfl (left_id, ctxp->filename, 0, 0);
      EXPR_WFL_LINECOL (wfl) = EXPR_WFL_LINECOL (left);
      EXPR_WFL_QUALIFICATION (left) = build_tree_list (wfl, NULL_TREE);
    }
  
  wfl = build_expr_wfl (right_id, ctxp->filename, 0, 0);
  EXPR_WFL_LINECOL (wfl) = location;
  chainon (EXPR_WFL_QUALIFICATION (left), build_tree_list (wfl, NULL_TREE));

  EXPR_WFL_NODE (left) = merge;
  return left;
#endif
}

/* Extract the last identifier component of the qualified in WFL. The
   last identifier is removed from the linked list */

static tree
cut_identifier_in_qualified (wfl)
     tree wfl;
{
  tree q;
  tree previous = NULL_TREE;
  for (q = EXPR_WFL_QUALIFICATION (wfl); ; previous = q, q = TREE_CHAIN (q))
    if (!TREE_CHAIN (q))
      {
	if (!previous)
	  fatal ("Operating on a non qualified qualified WFL - "
		 "cut_identifier_in_qualified");
	TREE_CHAIN (previous) = NULL_TREE;
	return TREE_PURPOSE (q);
      }
}

/* Resolve the expression name NAME. Return its decl.  */

static tree
resolve_expression_name (id, orig)
     tree id;
     tree *orig;
{
  tree name = EXPR_WFL_NODE (id);
  tree decl;

  /* 6.5.5.1: Simple expression names */
  if (!PRIMARY_P (id) && !QUALIFIED_P (name))
    {
      /* 15.13.1: NAME can appear within the scope of a local variable
         declaration */
      if ((decl = IDENTIFIER_LOCAL_VALUE (name)))
        return decl;

      /* 15.13.1: NAME can appear within a class declaration */
      else 
        {
	  decl = lookup_field_wrapper (current_class, name);
	  if (decl)
	    {
	      int fs = FIELD_STATIC (decl);
	      /* Instance variable (8.3.1.1) can't appear within
		 static method, static initializer or initializer for
		 a static variable. */
	      if (!fs && METHOD_STATIC (current_function_decl))
	        {
		  static_ref_err (id, name, current_class);
		  return error_mark_node;
		}
	      /* Instance variables can't appear as an argument of
		 an explicit constructor invocation */
	      if (!fs && ctxp->explicit_constructor_p)
		{
		  parse_error_context
		    (id, "Can't reference `%s' before the superclass "
		     "constructor has been called", IDENTIFIER_POINTER (name));
		  return error_mark_node;
		}

	      /* Otherwise build what it takes to access the field */
	      decl = build_field_ref ((fs ? NULL_TREE : current_this),
				      DECL_CONTEXT (decl), name);
	      if (fs && !flag_emit_class_files && !flag_emit_xref)
		decl = build_class_init (DECL_CONTEXT (decl), decl);
	      /* We may be asked to save the real field access node */
	      if (orig)
		*orig = decl;
	      /* And we return what we got */
	      return decl;
	    }
	  /* Fall down to error report on undefined variable */
	}
    }
  /* 6.5.5.2 Qualified Expression Names */
  else
    {
      if (orig)
	*orig = NULL_TREE;
      qualify_ambiguous_name (id);
      /* 15.10.1 Field Access Using a Primary and/or Expression Name */
      /* 15.10.2: Accessing Superclass Members using super */
      return resolve_field_access (id, NULL, NULL);
    }

  /* We've got an error here */
  parse_error_context (id, "Undefined variable `%s'", 
		       IDENTIFIER_POINTER (name));

  return error_mark_node;
}

static void
static_ref_err (wfl, field_id, class_type)
    tree wfl, field_id, class_type;
{
  parse_error_context 
    (wfl, 
     "Can't make a static reference to nonstatic variable `%s' in class `%s'",
     IDENTIFIER_POINTER (field_id), 
     IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (class_type))));
}

/* 15.10.1 Field Acess Using a Primary and/or Expression Name.
   We return something suitable to generate the field access. We also
   return the field decl in FIELD_DECL and its type in FIELD_TYPE.  If
   recipient's address can be null. */

static tree
resolve_field_access (qual_wfl, field_decl, field_type)
     tree qual_wfl;
     tree *field_decl, *field_type;
{
  int is_static = 0;
  tree field_ref;
  tree decl, where_found, type_found;

  if (resolve_qualified_expression_name (qual_wfl, &decl,
					 &where_found, &type_found))
    return error_mark_node;

  /* Resolve the LENGTH field of an array here */
  if (DECL_NAME (decl) == length_identifier_node && TYPE_ARRAY_P (type_found)
      && ! flag_emit_class_files && ! flag_emit_xref)
    {
      tree length = build_java_array_length_access (where_found);
      field_ref =
	build_java_arraynull_check (type_found, length, int_type_node);
    }
  /* We might have been trying to resolve field.method(). In which
     case, the resolution is over and decl is the answer */
  else if (JDECL_P (decl) && IDENTIFIER_LOCAL_VALUE (DECL_NAME (decl)) == decl)
    field_ref = decl;
  else if (JDECL_P (decl))
    {
      int static_final_found = 0;
      if (!type_found)
	type_found = DECL_CONTEXT (decl);
      is_static = JDECL_P (decl) && FIELD_STATIC (decl);
      if (FIELD_FINAL (decl) 
	  && JPRIMITIVE_TYPE_P (TREE_TYPE (decl))
	  && DECL_LANG_SPECIFIC (decl)
	  && DECL_INITIAL (decl))
	{
	  field_ref = DECL_INITIAL (decl);
	  static_final_found = 1;
	}
      else
	field_ref = build_field_ref ((is_static && !flag_emit_xref? 
				      NULL_TREE : where_found), 
				     type_found, DECL_NAME (decl));
      if (field_ref == error_mark_node)
	return error_mark_node;
      if (is_static && !static_final_found 
	  && !flag_emit_class_files && !flag_emit_xref)
	{
	  field_ref = build_class_init (type_found, field_ref);
	  /* If the static field was identified by an expression that
	     needs to be generated, make the field access a compound
	     expression whose first part is the evaluation of the
	     field selector part. */
	  if (where_found && TREE_CODE (where_found) != TYPE_DECL 
	      && TREE_CODE (where_found) != RECORD_TYPE)
	    {
	      tree type = QUAL_DECL_TYPE (field_ref);
	      field_ref = build (COMPOUND_EXPR, type, where_found, field_ref);
	    }
	}
    }
  else
    field_ref = decl;

  if (field_decl)
    *field_decl = decl;
  if (field_type)
    *field_type = (QUAL_DECL_TYPE (decl) ? 
		   QUAL_DECL_TYPE (decl) : TREE_TYPE (decl));
  return field_ref;
}

/* If NODE is an access to f static field, strip out the class
   initialization part and return the field decl, otherwise, return
   NODE. */

static tree
strip_out_static_field_access_decl (node)
    tree node;
{
  if (TREE_CODE (node) == COMPOUND_EXPR)
    {
      tree op1 = TREE_OPERAND (node, 1);
      if (TREE_CODE (op1) == COMPOUND_EXPR)
	 {
	   tree call = TREE_OPERAND (op1, 0);
	   if (TREE_CODE (call) == CALL_EXPR
	       && TREE_CODE (TREE_OPERAND (call, 0)) == ADDR_EXPR
	       && TREE_OPERAND (TREE_OPERAND (call, 0), 0)
	       == soft_initclass_node)
	     return TREE_OPERAND (op1, 1);
	 }
    }
  return node;
}

/* 6.5.5.2: Qualified Expression Names */

static int
resolve_qualified_expression_name (wfl, found_decl, where_found, type_found)
     tree wfl;
     tree *found_decl, *type_found, *where_found;
{
  int from_type = 0;		/* Field search initiated from a type */
  int from_super = 0, from_cast = 0;
  int previous_call_static = 0;
  int is_static;
  tree decl = NULL_TREE, type = NULL_TREE, q;
  *type_found = *where_found = NULL_TREE;

  for (q = EXPR_WFL_QUALIFICATION (wfl); q; q = TREE_CHAIN (q))
    {
      tree qual_wfl = QUAL_WFL (q);

      /* 15.10.1 Field Access Using a Primary */
      switch (TREE_CODE (qual_wfl))
	{
	case CALL_EXPR:
	case NEW_CLASS_EXPR:
	  /* If the access to the function call is a non static field,
	     build the code to access it. */
	  if (JDECL_P (decl) && !FIELD_STATIC (decl))
	    {
	      decl = maybe_access_field (decl, *where_found, 
					 DECL_CONTEXT (decl));
	      if (decl == error_mark_node)
		return 1;
	    }
	  /* And code for the function call */
	  if (complete_function_arguments (qual_wfl))
	    return 1;
	  if (from_super && TREE_CODE (qual_wfl) == CALL_EXPR)
	    CALL_USING_SUPER (qual_wfl) = 1;
	  *where_found = 
	    patch_method_invocation (qual_wfl, decl, type, &is_static, NULL);
	  if (*where_found == error_mark_node)
	    return 1;
	  *type_found = type = QUAL_DECL_TYPE (*where_found);

	  /* If the previous call was static and this one is too,
	     build a compound expression to hold the two (because in
	     that case, previous function calls aren't transported as
	     forcoming function's argument. */
	  if (previous_call_static && is_static)
	    {
	      decl = build (COMPOUND_EXPR, type, decl, *where_found);
	      TREE_SIDE_EFFECTS (decl) = 1;
	    }
	  else
	    {
	      previous_call_static = is_static;
	      decl = *where_found;
	    }
	  continue;

	case NEW_ARRAY_EXPR:
	  *where_found = decl = java_complete_tree (qual_wfl);
	  if (decl == error_mark_node)
	    return 1;
	  *type_found = type = QUAL_DECL_TYPE (decl);
	  CLASS_LOADED_P (type) = 1;
	  continue;

	case CONVERT_EXPR:
	  *where_found = decl = java_complete_tree (qual_wfl);
	  if (decl == error_mark_node)
	    return 1;
	  *type_found = type = QUAL_DECL_TYPE (decl);
	  from_cast = 1;
	  continue;

	case CONDITIONAL_EXPR:
	case STRING_CST:
	  *where_found = decl = java_complete_tree (qual_wfl);
	  if (decl == error_mark_node)
	    return 1;
	  *type_found = type = QUAL_DECL_TYPE (decl);
	  continue;

	case ARRAY_REF:
	  /* If the access to the function call is a non static field,
	     build the code to access it. */
	  if (JDECL_P (decl) && !FIELD_STATIC (decl))
	    {
	      decl = maybe_access_field (decl, *where_found, type);
	      if (decl == error_mark_node)
		return 1;
	    }
	  /* And code for the array reference expression */
	  decl = java_complete_tree (qual_wfl);
	  if (decl == error_mark_node)
	    return 1;
	  type = QUAL_DECL_TYPE (decl);
	  continue;

	default:
	  /* Fix for -Wall Just go to the next statement. Don't
             continue */
	  break;
	}

      /* If we fall here, we weren't processing a (static) function call. */
      previous_call_static = 0;

      /* It can be the keyword THIS */
      if (EXPR_WFL_NODE (qual_wfl) == this_identifier_node)
	{
	  if (!current_this)
	    {
	      parse_error_context 
		(wfl, "Keyword `this' used outside allowed context");
	      return 1;
	    }
	  /* We have to generate code for intermediate acess */
	  *where_found = decl = current_this;
	  *type_found = type = QUAL_DECL_TYPE (decl);
	  continue;
	}

      /* 15.10.2 Accessing Superclass Members using SUPER */
      if (EXPR_WFL_NODE (qual_wfl) == super_identifier_node)
	{
	  tree node;
	  /* Check on the restricted use of SUPER */
	  if (METHOD_STATIC (current_function_decl)
	      || current_class == object_type_node)
	    {
	      parse_error_context 
		(wfl, "Keyword `super' used outside allowed context");
	      return 1;
	    }
	  /* Otherwise, treat SUPER as (SUPER_CLASS)THIS */
	  node = build_cast (EXPR_WFL_LINECOL (qual_wfl), 
			     CLASSTYPE_SUPER (current_class),
			     build_this (EXPR_WFL_LINECOL (qual_wfl)));
	  *where_found = decl = java_complete_tree (node);
	  if (decl == error_mark_node)
	    return 1;
	  *type_found = type = QUAL_DECL_TYPE (decl);
	  from_super = from_type = 1;
	  continue;
	}

      /* 15.13.1: Can't search for field name in packages, so we
	 assume a variable/class name was meant. */
      if (RESOLVE_PACKAGE_NAME_P (qual_wfl))
	{
	  tree name = resolve_package (wfl, &q);
	  if (name)
	    {
	      *where_found = decl = resolve_no_layout (name, qual_wfl);
	      /* We wan't to be absolutely that the class is laid
                 out. We're going to search something inside it. */
	      *type_found = type = TREE_TYPE (decl);
	      layout_class (type);
	      from_type = 1;
	      /* Should be a list, really. FIXME */
	      RESOLVE_EXPRESSION_NAME_P (QUAL_WFL (TREE_CHAIN (q))) = 1;
	      RESOLVE_PACKAGE_NAME_P (QUAL_WFL (TREE_CHAIN (q))) = 0;
	    }
	  else
	    {
	      if (from_super || from_cast)
		parse_error_context 
		  ((from_cast ? qual_wfl : wfl),
		   "No variable `%s' defined in class `%s'",
		   IDENTIFIER_POINTER (EXPR_WFL_NODE (qual_wfl)),
		   lang_printable_name (type, 0));
	      else
		parse_error_context
		  (qual_wfl, "Undefined variable or class name: `%s'",
		   IDENTIFIER_POINTER (EXPR_WFL_NODE (qual_wfl)));
	      return 1;
	    }
	}

      /* We have a type name. It's been already resolved when the
	 expression was qualified. */
      else if (RESOLVE_TYPE_NAME_P (qual_wfl))
	{
	  if (!(decl = QUAL_RESOLUTION (q)))
	    return 1;		/* Error reported already */

	  if (not_accessible_p (TREE_TYPE (decl), decl, 0))
	    {
	      parse_error_context 
		(qual_wfl, "Can't access %s field `%s.%s' from `%s'",
		 java_accstring_lookup (get_access_flags_from_decl (decl)),
		 GET_TYPE_NAME (type),
		 IDENTIFIER_POINTER (DECL_NAME (decl)),
		 IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (current_class))));
	      return 1;
	    }
	  check_deprecation (qual_wfl, decl);
	  
	  type = TREE_TYPE (decl);
	  from_type = 1;
	}
      /* We resolve and expression name */
      else 
	{
	  tree field_decl;

	  /* If there exists an early resolution, use it. That occurs
	     only once and we know that there are more things to
	     come. Don't do that when processing something after SUPER
	     (we need more thing to be put in place below */
	  if (!from_super && QUAL_RESOLUTION (q))
	    {
	      decl = QUAL_RESOLUTION (q);
	      if (!type)
		{
		  if (TREE_CODE (decl) == FIELD_DECL && !FIELD_STATIC (decl))
		    {
		      if (current_this)
			*where_found = current_this;
		      else
			{
			  static_ref_err (qual_wfl, DECL_NAME (decl),
					  current_class);
			  return 1;
			}
		    }
		  else
		    {
		      *where_found = TREE_TYPE (decl);
		      if (TREE_CODE (*where_found) == POINTER_TYPE)
			*where_found = TREE_TYPE (*where_found);
		    }
		}
	    }

	  /* We have to search for a field, knowing the type of its
             container. The flag FROM_TYPE indicates that we resolved
             the last member of the expression as a type name, which
             means that for the resolution of this field, we'll look
             for other errors than if it was resolved as a member of
             an other field. */
	  else
	    {
	      int is_static;
	      tree field_decl_type; /* For layout */

	      if (!from_type && !JREFERENCE_TYPE_P (type))
		{
		  parse_error_context 
		    (qual_wfl, "Attempt to reference field `%s' in `%s %s'",
		     IDENTIFIER_POINTER (EXPR_WFL_NODE (qual_wfl)),
		     lang_printable_name (type, 0),
		     IDENTIFIER_POINTER (DECL_NAME (field_decl)));
		  return 1;
		}
	      
	      field_decl = lookup_field_wrapper (type,
						 EXPR_WFL_NODE (qual_wfl));
	      if (field_decl == NULL_TREE)
		{
		  parse_error_context 
		    (qual_wfl, "No variable `%s' defined in type `%s'",
		     IDENTIFIER_POINTER (EXPR_WFL_NODE (qual_wfl)), 
		     GET_TYPE_NAME (type));
		  return 1;
		}
	      if (field_decl == error_mark_node)
		return 1;

	      /* Layout the type of field_decl, since we may need
                 it. Don't do primitive types or loaded classes. The
                 situation of non primitive arrays may not handled
                 properly here. FIXME */
	      if (TREE_CODE (TREE_TYPE (field_decl)) == POINTER_TYPE)
		field_decl_type = TREE_TYPE (TREE_TYPE (field_decl));
	      else
		field_decl_type = TREE_TYPE (field_decl);
	      if (!JPRIMITIVE_TYPE_P (field_decl_type) 
		  && !CLASS_LOADED_P (field_decl_type)
		  && !TYPE_ARRAY_P (field_decl_type))
		resolve_and_layout (field_decl_type, NULL_TREE);
	      if (TYPE_ARRAY_P (field_decl_type))
		CLASS_LOADED_P (field_decl_type) = 1;
	      
	      /* Check on accessibility here */
	      if (not_accessible_p (type, field_decl, from_super))
		{
		  parse_error_context 
		    (qual_wfl,
		     "Can't access %s field `%s.%s' from `%s'",
		     java_accstring_lookup 
		       (get_access_flags_from_decl (field_decl)),
		     GET_TYPE_NAME (type),
		     IDENTIFIER_POINTER (DECL_NAME (field_decl)),
		     IDENTIFIER_POINTER 
		       (DECL_NAME (TYPE_NAME (current_class))));
		  return 1;
		}
	      check_deprecation (qual_wfl, field_decl);
	      
	      /* There are things to check when fields are accessed
	         from type. There are no restrictions on a static
	         declaration of the field when it is accessed from an
	         interface */
	      is_static = FIELD_STATIC (field_decl);
	      if (!from_super && from_type 
		  && !TYPE_INTERFACE_P (type) && !is_static)
		{
		  static_ref_err (qual_wfl, EXPR_WFL_NODE (qual_wfl), type);
		  return 1;
		}
	      from_cast = from_super = 0;

	      /* If we need to generate something to get a proper
		 handle on what this field is accessed from, do it
		 now. */
	      if (!is_static)
		{
		  decl = maybe_access_field (decl, *where_found, *type_found);
		  if (decl == error_mark_node)
		    return 1;
		}

	      /* We want to keep the location were found it, and the type
		 we found. */
	      *where_found = decl;
	      *type_found = type;

	      /* This is the decl found and eventually the next one to
		 search from */
	      decl = field_decl;
	    }
	  from_type = 0;
	  type = QUAL_DECL_TYPE (decl);
	}
    }
  *found_decl = decl;
  return 0;
}

/* 6.6 Qualified name and access control. Returns 1 if MEMBER (a decl)
   can't be accessed from REFERENCE (a record type). */

int not_accessible_p (reference, member, from_super)
     tree reference, member;
     int from_super;
{
  int access_flag = get_access_flags_from_decl (member);

  /* Access always granted for members declared public */
  if (access_flag & ACC_PUBLIC)
    return 0;
  
  /* Check access on protected members */
  if (access_flag & ACC_PROTECTED)
    {
      /* Access granted if it occurs from within the package
         containing the class in which the protected member is
         declared */
      if (class_in_current_package (DECL_CONTEXT (member)))
	return 0;

      /* If accessed with the form `super.member', then access is granted */
      if (from_super)
	return 0;

      /* Otherwise, access is granted if occuring from the class where
	 member is declared or a subclass of it */
      if (inherits_from_p (reference, current_class))
	return 0;
      return 1;
    }

  /* Check access on private members. Access is granted only if it
     occurs from within the class in witch it is declared */
  if (access_flag & ACC_PRIVATE)
    return (current_class == DECL_CONTEXT (member) ? 0 : 1);

  /* Default access are permitted only when occuring within the
     package in which the type (REFERENCE) is declared. In other words,
     REFERENCE is defined in the current package */
  if (ctxp->package)
    return !class_in_current_package (reference);
  
  /* Otherwise, access is granted */
  return 0;
}

/* Test deprecated decl access.  */
static void
check_deprecation (wfl, decl)
     tree wfl, decl;
{
  char *file = DECL_SOURCE_FILE (decl);
  /* Complain if the field is deprecated and the file it was defined
     in isn't compiled at the same time the file which contains its
     use is */
  if (DECL_DEPRECATED (decl) 
      && !IS_A_COMMAND_LINE_FILENAME_P (get_identifier (file)))
    {
      char the [20];
      switch (TREE_CODE (decl))
	{
	case FUNCTION_DECL:
	  strcpy (the, "method");
	  break;
	case FIELD_DECL:
	  strcpy (the, "field");
	  break;
	case TYPE_DECL:
	  strcpy (the, "class");
	  break;
	default:
	  fatal ("unexpected DECL code - check_deprecation");
	}
      parse_warning_context 
	(wfl, "The %s `%s' in class `%s' has been deprecated", 
	 the, lang_printable_name (decl, 0),
	 IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (DECL_CONTEXT (decl)))));
    }
}

/* Returns 1 if class was declared in the current package, 0 otherwise */

static int
class_in_current_package (class)
     tree class;
{
  static tree cache = NULL_TREE;
  int qualified_flag;
  tree left;

  if (cache == class)
    return 1;

  qualified_flag = QUALIFIED_P (DECL_NAME (TYPE_NAME (class)));

  /* If the current package is empty and the name of CLASS is
     qualified, class isn't in the current package.  If there is a
     current package and the name of the CLASS is not qualified, class
     isn't in the current package */
  if ((!ctxp->package && qualified_flag) || (ctxp->package && !qualified_flag))
    return 0;

  /* If there is not package and the name of CLASS isn't qualified,
     they belong to the same unnamed package */
  if (!ctxp->package && !qualified_flag)
    return 1;

  /* Compare the left part of the name of CLASS with the package name */
  breakdown_qualified (&left, NULL, DECL_NAME (TYPE_NAME (class)));
  if (ctxp->package == left)
    {
      cache = class;
      return 1;
    }
  return 0;
}

/* This function may generate code to access DECL from WHERE. This is
   done only if certain conditions meet.  */

static tree
maybe_access_field (decl, where, type)
  tree decl, where, type;
{
  if (TREE_CODE (decl) == FIELD_DECL && decl != current_this
      && !FIELD_STATIC (decl))
    decl = build_field_ref (where ? where : current_this, 
			    (type ? type : DECL_CONTEXT (decl)),
			    DECL_NAME (decl));
  return decl;
}

/* Build a method invocation, by patching PATCH. If non NULL
   and according to the situation, PRIMARY and WHERE may be
   used. IS_STATIC is set to 1 if the invoked function is static. */

static tree
patch_method_invocation (patch, primary, where, is_static, ret_decl)
     tree patch, primary, where;
     int *is_static;
     tree *ret_decl;
{
  tree wfl = TREE_OPERAND (patch, 0);
  tree args = TREE_OPERAND (patch, 1);
  tree name = EXPR_WFL_NODE (wfl);
  tree list;
  int is_static_flag = 0;
  int is_super_init = 0;
  tree this_arg = NULL_TREE;
  
  /* Should be overriden if everything goes well. Otherwise, if
     something fails, it should keep this value. It stop the
     evaluation of a bogus assignment. See java_complete_tree,
     MODIFY_EXPR: for the reasons why we sometimes want to keep on
     evaluating an assignment */
  TREE_TYPE (patch) = error_mark_node;

  /* Since lookup functions are messing with line numbers, save the
     context now.  */
  java_parser_context_save_global ();

  /* 15.11.1: Compile-Time Step 1: Determine Class or Interface to Search */

  /* Resolution of qualified name, excluding constructors */
  if (QUALIFIED_P (name) && !CALL_CONSTRUCTOR_P (patch))
    {
      tree class_decl, identifier, identifier_wfl;
      /* Extract the last IDENTIFIER of the qualified
	 expression. This is a wfl and we will use it's location
	 data during error report. */
      identifier_wfl = cut_identifier_in_qualified (wfl);
      identifier = EXPR_WFL_NODE (identifier_wfl);
      
      /* Given the context, IDENTIFIER is syntactically qualified
	 as a MethodName. We need to qualify what's before */
      qualify_ambiguous_name (wfl);

      /* Package resolution are erroneous */
      if (RESOLVE_PACKAGE_NAME_P (wfl))
	{
	  tree remainder;
	  breakdown_qualified (&remainder, NULL, EXPR_WFL_NODE (wfl));
	  parse_error_context (wfl, "Can't search method `%s' in package "
			       "`%s'",IDENTIFIER_POINTER (identifier),
			       IDENTIFIER_POINTER (remainder));
	  PATCH_METHOD_RETURN_ERROR ();
	}
      /* We're resolving a call from a type */
      else if (RESOLVE_TYPE_NAME_P (wfl))
	{
	  tree decl = QUAL_RESOLUTION (EXPR_WFL_QUALIFICATION (wfl));
	  tree name = DECL_NAME (decl);
	  tree type;

	  class_decl = resolve_and_layout (name, wfl);
	  if (CLASS_INTERFACE (decl))
	    {
	      parse_error_context
		(identifier_wfl, "Can't make static reference to method "
		 "`%s' in interface `%s'", IDENTIFIER_POINTER (identifier), 
		 IDENTIFIER_POINTER (name));
	      PATCH_METHOD_RETURN_ERROR ();
	    }
	  /* Look the method up in the type selector. The method ought
             to be static. */
	  type = TREE_TYPE (class_decl);
	  list = lookup_method_invoke (0, wfl, type, identifier, args);
	  if (list && !METHOD_STATIC (list))
	    {
	      char *fct_name = strdup (lang_printable_name (list, 0));
	      parse_error_context 
		(identifier_wfl,
		 "Can't make static reference to method `%s %s' in class `%s'",
		 lang_printable_name (TREE_TYPE (TREE_TYPE (list)), 0), 
		 fct_name, IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type))));
	      free (fct_name);
	      PATCH_METHOD_RETURN_ERROR ();
	    }
	  args = nreverse (args);
	}
      /* We're resolving an expression name */
      else
	{
	  tree field, type;
	  
	  /* 1- Find the field to which the call applies */
	  field = resolve_field_access (wfl, NULL, &type);
	  if (field == error_mark_node)
	    PATCH_METHOD_RETURN_ERROR ();
	  /* field is used in lieu of a primary. It alows us not to
	   report errors on erroneous use of `this' in
	   constructors. */
	  primary = field;	
	  
	  /* 2- Do the layout of the class where the last field
	     was found, so we can search it. */
	  class_decl = resolve_and_layout (type, NULL_TREE);
	  if (class_decl != NULL_TREE)
	  type = TREE_TYPE (class_decl);

	  /* 3- Retrieve a filtered list of method matches, Refine
	     if necessary. In any cases, point out errors.  */
	  list = lookup_method_invoke (0, identifier_wfl, type, 
				       identifier, args);

	  /* 4- Add the field as an argument */
	  args = nreverse (args);
	  this_arg = field;
	}

      /* IDENTIFIER_WFL will be used to report any problem further */
      wfl = identifier_wfl;
    }
  /* Resolution of simple names, names generated after a primary: or
     constructors */
  else
    {
      tree class_to_search;
      int lc;		/* Looking for Constructor */
      
      /* We search constructor in their target class */
      if (CALL_CONSTRUCTOR_P (patch))
	{
	  if (TREE_CODE (patch) == NEW_CLASS_EXPR)
	    class_to_search = EXPR_WFL_NODE (wfl);
	  else if (EXPR_WFL_NODE (TREE_OPERAND (patch, 0)) == 
		   this_identifier_node)
	    class_to_search = NULL_TREE;
	  else if (EXPR_WFL_NODE (TREE_OPERAND (patch, 0)) ==
		   super_identifier_node)
	    {
	      is_super_init = 1;
	      if (CLASSTYPE_SUPER (current_class))
		class_to_search = 
		  DECL_NAME (TYPE_NAME (CLASSTYPE_SUPER (current_class)));
	      else
		{
		  parse_error_context (wfl, "Can't invoke super constructor "
				       "on java.lang.Object");
		  PATCH_METHOD_RETURN_ERROR ();
		}
	    }

	  /* Class to search is NULL if we're searching the current one */
	  if (class_to_search)
	    {
	      class_to_search = resolve_and_layout (class_to_search, 
						    NULL_TREE);
	      if (!class_to_search)
		{
		  parse_error_context 
		    (wfl, "Class `%s' not found in type declaration",
		     IDENTIFIER_POINTER (EXPR_WFL_NODE (wfl)));
		  PATCH_METHOD_RETURN_ERROR ();
		}
	      
	      /* Can't instantiate an abstract class, but we can
	         invoke it's constructor. It's use within the `new'
	         context is denied here. */
	      if (CLASS_ABSTRACT (class_to_search) 
		  && TREE_CODE (patch) == NEW_CLASS_EXPR)
		{
		  parse_error_context 
		    (wfl, "Class `%s' is an abstract class. It can't be "
		     "instantiated", IDENTIFIER_POINTER (EXPR_WFL_NODE (wfl)));
		  PATCH_METHOD_RETURN_ERROR ();
		}
	      class_to_search = TREE_TYPE (class_to_search);
	    }
	  else
	    class_to_search = current_class;
	  lc = 1;
	}
      /* This is a regular search in the local class, unless an
         alternate class is specified. */
      else
	{
	  class_to_search = (where ? where : current_class);
	  lc = 0;
	}
      
      /* NAME is a simple identifier or comes from a primary. Search
	 in the class whose declaration contain the method being
	 invoked. */
      resolve_and_layout (class_to_search, NULL_TREE);
      list = lookup_method_invoke (lc, wfl, class_to_search, name, args);

      /* Don't continue if no method were found, as the next statement
         can't be executed then. */
      if (!list)
	PATCH_METHOD_RETURN_ERROR ();

      /* Check for static reference if non static methods */
      if (check_for_static_method_reference (wfl, patch, list, 
					     class_to_search, primary))
	PATCH_METHOD_RETURN_ERROR ();

      /* Non static methods are called with the current object extra
	 argument. If patch a `new TYPE()', the argument is the value
	 returned by the object allocator. If method is resolved as a
	 primary, use the primary otherwise use the current THIS. */
      args = nreverse (args);
      if (TREE_CODE (patch) != NEW_CLASS_EXPR)
	this_arg = primary ? primary : current_this;
    }

  /* Merge point of all resolution schemes. If we have nothing, this
     is an error, already signaled */
  if (!list) 
    PATCH_METHOD_RETURN_ERROR ();

  /* Check accessibility, position the is_static flag, build and
     return the call */
  if (not_accessible_p (DECL_CONTEXT (current_function_decl), list, 0))
    {
      char *fct_name = strdup (lang_printable_name (list, 0));
      parse_error_context 
	(wfl, "Can't access %s method `%s %s.%s' from `%s'",
	 java_accstring_lookup (get_access_flags_from_decl (list)),
	 lang_printable_name (TREE_TYPE (TREE_TYPE (list)), 0), 
	 IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (DECL_CONTEXT (list)))), 
	 fct_name, IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (current_class))));
      free (fct_name);
      PATCH_METHOD_RETURN_ERROR ();
    }
  check_deprecation (wfl, list);

  is_static_flag = METHOD_STATIC (list);
  if (! METHOD_STATIC (list) && this_arg != NULL_TREE)
    args = tree_cons (NULL_TREE, this_arg, args);

  /* In the context of an explicit constructor invocation, we can't
     invoke any method relying on `this'. Exceptions are: we're
     invoking a static function, primary exists and is not the current
     this, we're creating a new object. */
  if (ctxp->explicit_constructor_p 
      && !is_static_flag 
      && (!primary || primary == current_this)
      && (TREE_CODE (patch) != NEW_CLASS_EXPR))
    {
      parse_error_context 
	(wfl, "Can't reference `this' before the superclass constructor has "
	 "been called");
      PATCH_METHOD_RETURN_ERROR ();
    }
  java_parser_context_restore_global ();
  if (is_static) 
    *is_static = is_static_flag;
  /* Sometimes, we want the decl of the selected method. Such as for
     EH checking */
  if (ret_decl)
    *ret_decl = list;
  patch = patch_invoke (patch, list, args);
  if (is_super_init && CLASS_HAS_FINIT_P (current_class))
    {
      /* Generate the code used to initialize fields declared with an
	 initialization statement. For now, it returns a call the the
	 artificial function $finit$, if required. */

      tree finit_call =
	build_method_invocation (build_expr_wfl (finit_identifier_node,  
						 input_filename, 0, 0),  
				 NULL_TREE);
      patch = build (COMPOUND_EXPR, void_type_node, patch,
		     java_complete_tree (finit_call));
      CAN_COMPLETE_NORMALLY (patch) = 1;
    }
  return patch;
}

/* Check that we're not trying to do a static reference to a method in
   non static method. Return 1 if it's the case, 0 otherwise. */

static int
check_for_static_method_reference (wfl, node, method, where, primary)
     tree wfl, node, method, where, primary;
{
  if (METHOD_STATIC (current_function_decl) 
      && !METHOD_STATIC (method) && !primary && !CALL_CONSTRUCTOR_P (node))
    {
      char *fct_name = strdup (lang_printable_name (method, 0));
      parse_error_context 
	(wfl, "Can't make static reference to method `%s %s' in class `%s'", 
	 lang_printable_name (TREE_TYPE (TREE_TYPE (method)), 0), fct_name,
	 IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (where))));
      free (fct_name);
      return 1;
    }
  return 0;
}

/* Patch an invoke expression METHOD and ARGS, based on its invocation
   mode.  */

static tree
patch_invoke (patch, method, args)
     tree patch, method, args;
{
  tree dtable, func;
  tree original_call, t, ta;

  /* Last step for args: convert build-in types. If we're dealing with
     a new TYPE() type call, the first argument to the constructor
     isn't found in the incomming argument list, but delivered by
     `new' */
  t = TYPE_ARG_TYPES (TREE_TYPE (method));
  if (TREE_CODE (patch) == NEW_CLASS_EXPR)
    t = TREE_CHAIN (t);
  for (ta = args; t != end_params_node && ta; 
       t = TREE_CHAIN (t), ta = TREE_CHAIN (ta))
    if (JPRIMITIVE_TYPE_P (TREE_TYPE (TREE_VALUE (ta))) &&
	TREE_TYPE (TREE_VALUE (ta)) != TREE_VALUE (t))
      TREE_VALUE (ta) = convert (TREE_VALUE (t), TREE_VALUE (ta));
  
  if (flag_emit_class_files || flag_emit_xref)
    func = method;
  else
    {
      tree signature = build_java_signature (TREE_TYPE (method));
      switch (invocation_mode (method, CALL_USING_SUPER (patch)))
	{
	case INVOKE_VIRTUAL:
	  dtable = invoke_build_dtable (0, args);
	  func = build_invokevirtual (dtable, method);
	  break;

	case INVOKE_SUPER:
	case INVOKE_STATIC:
	  func = build_known_method_ref (method, TREE_TYPE (method),
					 DECL_CONTEXT (method),
					 signature, args);
	  break;

	case INVOKE_INTERFACE:
	  dtable = invoke_build_dtable (1, args);
	  func = build_invokeinterface (dtable, DECL_NAME (method), signature);
	  break;

	default:
	  fatal ("internal error - unknown invocation_mode result");
	}

      /* Ensure self_type is initialized, (invokestatic). FIXME */
      func = build1 (NOP_EXPR, build_pointer_type (TREE_TYPE (method)), func);
    }

  TREE_TYPE (patch) = TREE_TYPE (TREE_TYPE (method));
  TREE_OPERAND (patch, 0) = func;
  TREE_OPERAND (patch, 1) = args;
  original_call = patch;

  /* We're processing a `new TYPE ()' form. New is called an its
     returned value is the first argument to the constructor. We build
     a COMPOUND_EXPR and use saved expression so that the overall NEW
     expression value is a pointer to a newly created and initialized
     class. */
  if (TREE_CODE (original_call) == NEW_CLASS_EXPR)
    {
      tree class = DECL_CONTEXT (method);
      tree c1, saved_new, size, new;
      if (flag_emit_class_files || flag_emit_xref)
	{
	  TREE_TYPE (patch) = build_pointer_type (class);
	  return patch;
	}
      if (!TYPE_SIZE (class))
	safe_layout_class (class);
      size = size_in_bytes (class);
      new = build (CALL_EXPR, promote_type (class),
		   build_address_of (alloc_object_node),
		   tree_cons (NULL_TREE, build_class_ref (class),
			      build_tree_list (NULL_TREE, 
					       size_in_bytes (class))),
		   NULL_TREE);
      saved_new = save_expr (new);
      c1 = build_tree_list (NULL_TREE, saved_new);
      TREE_CHAIN (c1) = TREE_OPERAND (original_call, 1);
      TREE_OPERAND (original_call, 1) = c1;
      TREE_SET_CODE (original_call, CALL_EXPR);
      patch = build (COMPOUND_EXPR, TREE_TYPE (new), patch, saved_new);
    }
  return patch;
}

static int
invocation_mode (method, super)
     tree method;
     int super;
{
  int access = get_access_flags_from_decl (method);

  if (super)
    return INVOKE_SUPER;

  if (access & ACC_STATIC || access & ACC_FINAL || access & ACC_PRIVATE)
    return INVOKE_STATIC;

  if (CLASS_FINAL (TYPE_NAME (DECL_CONTEXT (method))))
    return INVOKE_STATIC;
  
  if (CLASS_INTERFACE (TYPE_NAME (DECL_CONTEXT (method))))
    return INVOKE_INTERFACE;
  
  if (DECL_CONSTRUCTOR_P (method))
    return INVOKE_STATIC;

  return INVOKE_VIRTUAL;
}

/* Retrieve a refined list of matching methods. It covers the step
   15.11.2 (Compile-Time Step 2) */

static tree
lookup_method_invoke (lc, cl, class, name, arg_list)
     int lc;
     tree cl;
     tree class, name, arg_list;
{
  tree atl = end_params_node;		/* Arg Type List */
  tree method, signature, list, node;
  char *candidates;		/* Used for error report */

  /* Fix the arguments */
  for (node = arg_list; node; node = TREE_CHAIN (node))
    {
      tree current_arg = TREE_TYPE (TREE_VALUE (node));
      /* Non primitive type may have to be resolved */
      if (!JPRIMITIVE_TYPE_P (current_arg))
	resolve_and_layout (current_arg, NULL_TREE);
      /* And promoted */
      if (TREE_CODE (current_arg) == RECORD_TYPE)
        current_arg = promote_type (current_arg);
      atl = tree_cons (NULL_TREE, current_arg, atl);
    }

  /* Find all candidates and then refine the list, searching for the
     most specific method. */
  list = find_applicable_accessible_methods_list (lc, class, name, atl);
  list = find_most_specific_methods_list (list);
  if (list && !TREE_CHAIN (list))
    return TREE_VALUE (list);

  /* Issue an error. List candidates if any. Candidates are listed
     only if accessible (non accessible methods may end-up here for
     the sake of a better error report). */
  candidates = NULL;
  if (list)
    {
      tree current;
      obstack_grow (&temporary_obstack, ". Candidates are:\n", 18);
      for (current = list; current; current = TREE_CHAIN (current))
	{
	  tree cm = TREE_VALUE (current);
	  char string [4096];
	  if (!cm || not_accessible_p (class, cm, 0))
	    continue;
	  sprintf 
	    (string, "  `%s' in `%s'%s",
	     get_printable_method_name (cm),
	     IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (DECL_CONTEXT (cm)))),
	     (TREE_CHAIN (current) ? "\n" : ""));
	  obstack_grow (&temporary_obstack, string, strlen (string));
	}
      obstack_1grow (&temporary_obstack, '\0');
      candidates = obstack_finish (&temporary_obstack);
    }
  /* Issue the error message */
  method = make_node (FUNCTION_TYPE);
  TYPE_ARG_TYPES (method) = atl;
  signature = build_java_argument_signature (method);
  parse_error_context (cl, "Can't find %s `%s(%s)' in class `%s'%s",
		       (lc ? "constructor" : "method"),
		       (lc ? 
			IDENTIFIER_POINTER(DECL_NAME (TYPE_NAME (class))) :
			IDENTIFIER_POINTER (name)),
		       IDENTIFIER_POINTER (signature),
		       IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (class))),
		       (candidates ? candidates : ""));
  return NULL_TREE;
}

/* 15.11.2.1: Find Methods that are Applicable and Accessible. LC is 1
   when we're looking for a constructor. */

static tree
find_applicable_accessible_methods_list (lc, class, name, arglist)
     int lc;
     tree class, name, arglist;
{
  tree list = NULL_TREE, all_list = NULL_TREE;

  /* Search interfaces */
  if (CLASS_INTERFACE (TYPE_NAME (class)))
    {
      static tree searched_interfaces = NULL_TREE;
      static int search_not_done = 0;
      int i, n;
      tree basetype_vec = TYPE_BINFO_BASETYPES (class);

      /* Have we searched this interface already? */
      if (searched_interfaces)
	{  
	  tree current;  
	  for (current = searched_interfaces; 
	       current; current = TREE_CHAIN (current))
	    if (TREE_VALUE (current) == class)
	      return NULL;
	}
      searched_interfaces = tree_cons (NULL_TREE, class, searched_interfaces);

      search_applicable_methods_list 
	(lc, TYPE_METHODS (class), name, arglist, &list, &all_list);

      n = TREE_VEC_LENGTH (basetype_vec);
      for (i = 0; i < n; i++)
	{
	  tree t = BINFO_TYPE (TREE_VEC_ELT (basetype_vec, i));
	  tree rlist;

	  /* Skip java.lang.Object (we'll search it once later.) */
	  if (t == object_type_node)
	    continue;

	  search_not_done++;
	  rlist = find_applicable_accessible_methods_list (lc,  t, name, 
							   arglist);
	  all_list = chainon (rlist, (list ? list : all_list)); 
	  search_not_done--;
	}

      /* We're done. Reset the searched interfaces list and finally search
         java.lang.Object */
      if (!search_not_done)
	{  
	  searched_interfaces = NULL_TREE;  
	  search_applicable_methods_list (lc, TYPE_METHODS (object_type_node),
					  name, arglist, &list, &all_list);
	}
    }
  /* Search classes */
  else
    while (class != NULL_TREE)
      {
	search_applicable_methods_list 
	  (lc, TYPE_METHODS (class), name, arglist, &list, &all_list);
	class = (lc ? NULL_TREE : CLASSTYPE_SUPER (class));
      }

  /* Either return the list obtained or all selected (but
     inaccessible) methods for better error report. */
  return (!list ? all_list : list);
}

/* Effectively search for the approriate method in method */

static void 
search_applicable_methods_list(lc, method, name, arglist, list, all_list)
     int lc;
     tree method, name, arglist;
     tree *list, *all_list;
{
  for (; method; method = TREE_CHAIN (method))
    {
      /* When dealing with constructor, stop here, otherwise search
         other classes */
      if (lc && !DECL_CONSTRUCTOR_P (method))
	continue;
      else if (!lc && (DECL_CONSTRUCTOR_P (method) 
		       || (GET_METHOD_NAME (method) != name)))
	continue;
	  
      if (argument_types_convertible (method, arglist))
	{
	  /* Retain accessible methods only */
	  if (!not_accessible_p (DECL_CONTEXT (current_function_decl), 
				 method, 0))
	    *list = tree_cons (NULL_TREE, method, *list);
	  else
	    /* Also retain all selected method here */
	    *all_list = tree_cons (NULL_TREE, method, *list);
	}
    }
}    

/* 15.11.2.2 Choose the Most Specific Method */

static tree
find_most_specific_methods_list (list)
     tree list;
{
  int max = 0;
  tree current, new_list = NULL_TREE;
  for (current = list; current; current = TREE_CHAIN (current))
    {
      tree method;
      DECL_SPECIFIC_COUNT (TREE_VALUE (current)) = 0;

      for (method = list; method; method = TREE_CHAIN (method))
	{
	  /* Don't test a method against itself */
	  if (method == current)
	    continue;

	  /* Compare arguments and location where method where declared */
	  if (argument_types_convertible (TREE_VALUE (method), 
					  TREE_VALUE (current))
	      && valid_method_invocation_conversion_p 
	           (DECL_CONTEXT (TREE_VALUE (method)), 
		    DECL_CONTEXT (TREE_VALUE (current))))
	    {
	      int v = ++DECL_SPECIFIC_COUNT (TREE_VALUE (current));
	      max = (v > max ? v : max);
	    }
	}
    }

  /* Review the list and select the maximally specific methods */
  for (current = list; current; current = TREE_CHAIN (current))
    if (DECL_SPECIFIC_COUNT (TREE_VALUE (current)) == max)
      new_list = tree_cons (NULL_TREE, TREE_VALUE (current), new_list);

  /* If we can't find one, lower expectations and try to gather multiple
     maximally specific methods */
  while (!new_list)
    {
      while (--max > 0)
	{
	  if (DECL_SPECIFIC_COUNT (TREE_VALUE (current)) == max)
	    new_list = tree_cons (NULL_TREE, TREE_VALUE (current), new_list);
	}
      return new_list;
    }

  return new_list;
}

/* Make sure that the type of each M2_OR_ARGLIST arguments can be
   converted by method invocation conversion (5.3) to the type of the
   corresponding parameter of M1. Implementation expects M2_OR_ARGLIST
   to change less often than M1. */

static int
argument_types_convertible (m1, m2_or_arglist)
    tree m1, m2_or_arglist;
{
  static tree m2_arg_value = NULL_TREE;
  static tree m2_arg_cache = NULL_TREE;

  register tree m1_arg, m2_arg;

  m1_arg = TYPE_ARG_TYPES (TREE_TYPE (m1));
  if (!METHOD_STATIC (m1))
    m1_arg = TREE_CHAIN (m1_arg);

  if (m2_arg_value == m2_or_arglist)
    m2_arg = m2_arg_cache;
  else
    {
      /* M2_OR_ARGLIST can be a function DECL or a raw list of
         argument types */
      if (m2_or_arglist && TREE_CODE (m2_or_arglist) == FUNCTION_DECL)
	{
	  m2_arg = TYPE_ARG_TYPES (TREE_TYPE (m2_or_arglist));
	  if (!METHOD_STATIC (m2_or_arglist))
	    m2_arg = TREE_CHAIN (m2_arg);
	}
      else
	m2_arg = m2_or_arglist;

      m2_arg_value = m2_or_arglist;
      m2_arg_cache = m2_arg;
    }

  while (m1_arg != end_params_node && m2_arg != end_params_node)
    {
      resolve_and_layout (TREE_VALUE (m1_arg), NULL_TREE);
      if (!valid_method_invocation_conversion_p (TREE_VALUE (m1_arg),
						 TREE_VALUE (m2_arg)))
	break;
      m1_arg = TREE_CHAIN (m1_arg);
      m2_arg = TREE_CHAIN (m2_arg);
    }
  return m1_arg == end_params_node && m2_arg == end_params_node;
}

/* Qualification routines */

static void
qualify_ambiguous_name (id)
     tree id;
{
  tree qual, qual_wfl, name, decl, ptr_type, saved_current_class;
  int again, super_found = 0, this_found = 0, new_array_found = 0;

  /* We first qualify the first element, then derive qualification of
     others based on the first one. If the first element is qualified
     by a resolution (field or type), this resolution is stored in the
     QUAL_RESOLUTION of the qual element being examined. We need to
     save the current_class since the use of SUPER might change the
     its value. */
  saved_current_class = current_class;
  qual = EXPR_WFL_QUALIFICATION (id);
  do {

    /* Simple qualified expression feature a qual_wfl that is a
       WFL. Expression derived from a primary feature more complicated
       things like a CALL_EXPR. Expression from primary need to be
       worked out to extract the part on which the qualification will
       take place. */
    qual_wfl = QUAL_WFL (qual);
    switch (TREE_CODE (qual_wfl))
      {
      case CALL_EXPR:
	qual_wfl = TREE_OPERAND (qual_wfl, 0);
	if (TREE_CODE (qual_wfl) != EXPR_WITH_FILE_LOCATION)
	  {
	    qual = EXPR_WFL_QUALIFICATION (qual_wfl);
	    qual_wfl = QUAL_WFL (qual);
	  }
	break;
      case NEW_ARRAY_EXPR:
	qual = TREE_CHAIN (qual);
	new_array_found = again = 1;
	continue;
      case NEW_CLASS_EXPR:
      case CONVERT_EXPR:
	qual_wfl = TREE_OPERAND (qual_wfl, 0);
	break;
      case ARRAY_REF:
	while (TREE_CODE (qual_wfl) == ARRAY_REF)
	  qual_wfl = TREE_OPERAND (qual_wfl, 0);
	break;
      default:
	/* Fix for -Wall. Just break doing nothing */
	break;
      }
    name = EXPR_WFL_NODE (qual_wfl);
    ptr_type = current_class;
    again = 0;
    /* If we have a THIS (from a primary), we set the context accordingly */
    if (name == this_identifier_node)
      {
	qual = TREE_CHAIN (qual);
	qual_wfl = QUAL_WFL (qual);
	if (TREE_CODE (qual_wfl) == CALL_EXPR)
	  again = 1;
	else
	  name = EXPR_WFL_NODE (qual_wfl);
	this_found = 1;
      }
    /* If we have a SUPER, we set the context accordingly */
    if (name == super_identifier_node)
      {
	current_class = CLASSTYPE_SUPER (ptr_type);
	/* Check that there is such a thing as a super class. If not,
	   return.  The error will be caught later on, during the
	   resolution */
	if (!current_class)
	  {
	    current_class = saved_current_class;
	    return;
	  }
	qual = TREE_CHAIN (qual);
	/* Do one more interation to set things up */
	super_found = again = 1;
      }
    /* Loop one more time if we're dealing with ?: or a string
       constant, or a convert expression */
    if (TREE_CODE (qual_wfl) == CONDITIONAL_EXPR
	|| TREE_CODE (qual_wfl) == STRING_CST
	|| TREE_CODE (qual_wfl) == CONVERT_EXPR)
      {
	if (TREE_CODE (qual_wfl) == CONVERT_EXPR
	    && TREE_CODE (TREE_TYPE (qual_wfl)) == EXPR_WITH_FILE_LOCATION)
	    name = EXPR_WFL_NODE (TREE_TYPE (qual_wfl));
	else
	  {
	    qual = TREE_CHAIN (qual);
	    qual_wfl = QUAL_WFL (qual);
	    again = 1;
	  }
      }
  } while (again);
  
  /* If name appears within the scope of a location variable
     declaration or parameter declaration, then it is an expression
     name. We don't carry this test out if we're in the context of the
     use of SUPER or THIS */
  if (!this_found && !super_found && (decl = IDENTIFIER_LOCAL_VALUE (name)))
    {
      RESOLVE_EXPRESSION_NAME_P (qual_wfl) = 1;
      QUAL_RESOLUTION (qual) = decl;
    }

  /* If within the class/interface NAME was found to be used there
     exists a (possibly inherited) field named NAME, then this is an
     expression name. If we saw a NEW_ARRAY_EXPR before and want to
     address length, it is OK. */
  else if ((decl = lookup_field_wrapper (ptr_type, name))
	   || (new_array_found && name == length_identifier_node))
    {
      RESOLVE_EXPRESSION_NAME_P (qual_wfl) = 1;
      QUAL_RESOLUTION (qual) = (new_array_found ? NULL_TREE : decl);
    }

  /* We reclassify NAME as a type name if:
     - NAME is a class/interface declared within the compilation
       unit containing NAME,
     - NAME is imported via a single-type-import declaration,
     - NAME is declared in an another compilation unit of the package
       of the compilation unit containing NAME,
     - NAME is declared by exactly on type-import-on-demand declaration
     of the compilation unit containing NAME. */
  else if ((decl = resolve_and_layout (name, NULL_TREE)))
    {
      RESOLVE_TYPE_NAME_P (qual_wfl) = 1;
      QUAL_RESOLUTION (qual) = decl;
    }

  /* Method call are expression name */
  else if (TREE_CODE (QUAL_WFL (qual)) == CALL_EXPR
	   || TREE_CODE (QUAL_WFL (qual)) == ARRAY_REF)
    RESOLVE_EXPRESSION_NAME_P (qual_wfl) = 1;

  /* Check here that NAME isn't declared by more than one
     type-import-on-demand declaration of the compilation unit
     containing NAME. FIXME */

  /* Otherwise, NAME is reclassified as a package name */
  else 
    RESOLVE_PACKAGE_NAME_P (qual_wfl) = 1;

  /* Propagate the qualification accross other components of the
     qualified name */
  for (qual = TREE_CHAIN (qual); qual;
       qual_wfl = QUAL_WFL (qual), qual = TREE_CHAIN (qual))
    {
      if (RESOLVE_PACKAGE_NAME_P (qual_wfl))
	RESOLVE_PACKAGE_NAME_P (QUAL_WFL (qual)) = 1;
      else 
	RESOLVE_EXPRESSION_NAME_P (QUAL_WFL (qual)) = 1;
    }

  /* Store the global qualification for the ambiguous part of ID back
     into ID fields */
  if (RESOLVE_EXPRESSION_NAME_P (qual_wfl))
    RESOLVE_EXPRESSION_NAME_P (id) = 1;
  else if (RESOLVE_TYPE_NAME_P (qual_wfl))
    RESOLVE_TYPE_NAME_P (id) = 1;
  else if (RESOLVE_PACKAGE_NAME_P (qual_wfl))
    RESOLVE_PACKAGE_NAME_P (id) = 1;

  /* Restore the current class */
  current_class = saved_current_class;
}

static int
breakdown_qualified (left, right, source)
    tree *left, *right, source;
{
  char *p = IDENTIFIER_POINTER (source), *base;
  int   l = IDENTIFIER_LENGTH (source);

  /* Breakdown NAME into REMAINDER . IDENTIFIER */
  base = p;
  p += (l-1);
  while (*p != '.' && p != base)
    p--;

  /* We didn't find a '.'. Return an error */
  if (p == base)
    return 1;

  *p = '\0';
  if (right)
    *right = get_identifier (p+1);
  *left = get_identifier (IDENTIFIER_POINTER (source));
  *p = '.';
  
  return 0;
}

/* Patch tree nodes in a function body. When a BLOCK is found, push
   local variable decls if present.
   Same as java_complete_lhs, but does resolve static finals to values. */

static tree
java_complete_tree (node)
     tree node;
{
  node = java_complete_lhs (node);
  if (TREE_CODE (node) == VAR_DECL && FIELD_STATIC (node)
      && FIELD_FINAL (node) && DECL_INITIAL (node) != NULL_TREE
      && !flag_emit_xref)
    {
      tree value = DECL_INITIAL (node);
      DECL_INITIAL (node) = NULL_TREE;
      value = fold_constant_for_init (value, node);
      DECL_INITIAL (node) = value;
      if (value != NULL_TREE)
	return value;
    }
  return node;
}

static tree
java_stabilize_reference (node)
     tree node;
{
  if (TREE_CODE (node) == COMPOUND_EXPR)
    {
      tree op0 = TREE_OPERAND (node, 0);
      tree op1 = TREE_OPERAND (node, 1);
      TREE_OPERAND (node, 0) = save_expr (op0);
      TREE_OPERAND (node, 1) = java_stabilize_reference (op1);
      return node;
    }
  else
    return stabilize_reference (node);
}

/* Patch tree nodes in a function body. When a BLOCK is found, push
   local variable decls if present.
   Same as java_complete_tree, but does not resolve static finals to values. */

static tree
java_complete_lhs (node)
     tree node;
{
  tree nn, cn, wfl_op1, wfl_op2, wfl_op3;
  int flag;

  /* CONVERT_EXPR always has its type set, even though it needs to be
     worked out. */
  if (TREE_TYPE (node) && TREE_CODE (node) != CONVERT_EXPR)
    return node;

  /* The switch block implements cases processing container nodes
     first.  Contained nodes are always written back. Leaves come
     next and return a value. */
  switch (TREE_CODE (node))
    {
    case BLOCK:

      /* 1- Block section.
	 Set the local values on decl names so we can identify them
	 faster when they're referenced. At that stage, identifiers
	 are legal so we don't check for declaration errors. */
      for (cn = BLOCK_EXPR_DECLS (node); cn; cn = TREE_CHAIN (cn))
	{
	  DECL_CONTEXT (cn) = current_function_decl;
	  IDENTIFIER_LOCAL_VALUE (DECL_NAME (cn)) = cn;
	}
      if (BLOCK_EXPR_BODY (node) == NULL_TREE)
	  CAN_COMPLETE_NORMALLY (node) = 1;
      else
	{
	  tree stmt = BLOCK_EXPR_BODY (node);
	  tree *ptr;
	  int error_seen = 0;
	  if (TREE_CODE (stmt) == COMPOUND_EXPR)
	    {
	      /* Re-order from (((A; B); C); ...; Z) to 
		 (A; (B; (C ; (...; Z)))).
		 This makes it easier to scan the statements left-to-right
		 without using recursion (which might overflow the stack
		 if the block has many statements. */
	      for (;;)
		{
		  tree left = TREE_OPERAND (stmt, 0);
		  if (TREE_CODE (left) != COMPOUND_EXPR)
		    break;
		  TREE_OPERAND (stmt, 0) = TREE_OPERAND (left, 1);
		  TREE_OPERAND (left, 1) = stmt;
		  stmt = left;
		}
	      BLOCK_EXPR_BODY (node) = stmt;
	    }

	  /* Now do the actual complete, without deep recursion for
             long blocks. */
	  ptr = &BLOCK_EXPR_BODY (node);
	  while (TREE_CODE (*ptr) == COMPOUND_EXPR
		 && TREE_OPERAND (*ptr, 1) != empty_stmt_node)
	    {
	      tree cur = java_complete_tree (TREE_OPERAND (*ptr, 0));
	      tree *next = &TREE_OPERAND (*ptr, 1);
	      TREE_OPERAND (*ptr, 0) = cur;
	      if (cur == empty_stmt_node)
		{
		  /* Optimization;  makes it easier to detect empty bodies.
		     Most useful for <clinit> with all-constant initializer. */
		  *ptr = *next;
		  continue;
		}
	      if (TREE_CODE (cur) == ERROR_MARK)
		error_seen++;
	      else if (! CAN_COMPLETE_NORMALLY (cur))
		{
		  wfl_op2 = *next;
		  for (;;)
		    {
		      if (TREE_CODE (wfl_op2) == BLOCK)
			wfl_op2 = BLOCK_EXPR_BODY (wfl_op2);
		      else if (TREE_CODE (wfl_op2) == COMPOUND_EXPR)
			wfl_op2 = TREE_OPERAND (wfl_op2, 0);
		      else
			break;
		    }
		  if (TREE_CODE (wfl_op2) != CASE_EXPR
		      && TREE_CODE (wfl_op2) != DEFAULT_EXPR)
		    unreachable_stmt_error (*ptr);
		}
	      ptr = next;
	    }
	  *ptr = java_complete_tree (*ptr);

	  if (TREE_CODE (*ptr) == ERROR_MARK || error_seen > 0)
	    return error_mark_node;
	  CAN_COMPLETE_NORMALLY (node) = CAN_COMPLETE_NORMALLY (*ptr);
	}
      /* Turn local bindings to null */
      for (cn = BLOCK_EXPR_DECLS (node); cn; cn = TREE_CHAIN (cn))
	IDENTIFIER_LOCAL_VALUE (DECL_NAME (cn)) = NULL_TREE;

      TREE_TYPE (node) = void_type_node;
      break;

      /* 2- They are expressions but ultimately deal with statements */

    case THROW_EXPR:
      wfl_op1 = TREE_OPERAND (node, 0);
      COMPLETE_CHECK_OP_0 (node);
      /* CAN_COMPLETE_NORMALLY (node) = 0; */
      return patch_throw_statement (node, wfl_op1);

    case SYNCHRONIZED_EXPR:
      wfl_op1 = TREE_OPERAND (node, 0);
      return patch_synchronized_statement (node, wfl_op1);

    case TRY_EXPR:
      return patch_try_statement (node);

    case TRY_FINALLY_EXPR:
      COMPLETE_CHECK_OP_0 (node);
      COMPLETE_CHECK_OP_1 (node);
      CAN_COMPLETE_NORMALLY (node)
	= (CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 0))
	   && CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 1)));
      TREE_TYPE (node) = TREE_TYPE (TREE_OPERAND (node, 0));
      return node;

    case CLEANUP_POINT_EXPR:
      COMPLETE_CHECK_OP_0 (node);
      TREE_TYPE (node) = void_type_node;
      CAN_COMPLETE_NORMALLY (node) = 
	CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 0));
      return node;

    case WITH_CLEANUP_EXPR:
      COMPLETE_CHECK_OP_0 (node);
      COMPLETE_CHECK_OP_2 (node);
      CAN_COMPLETE_NORMALLY (node) = 
	CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 0));
      TREE_TYPE (node) = void_type_node;
      return node;

    case LABELED_BLOCK_EXPR:
      PUSH_LABELED_BLOCK (node);
      if (LABELED_BLOCK_BODY (node))
	COMPLETE_CHECK_OP_1 (node);
      TREE_TYPE (node) = void_type_node;
      POP_LABELED_BLOCK ();

      if (LABELED_BLOCK_BODY (node) == empty_stmt_node)
	{
	  LABELED_BLOCK_BODY (node) = NULL_TREE;
	  CAN_COMPLETE_NORMALLY (node) = 1;
	}
      else if (CAN_COMPLETE_NORMALLY (LABELED_BLOCK_BODY (node)))
	CAN_COMPLETE_NORMALLY (node) = 1;
      return node;

    case EXIT_BLOCK_EXPR:
      /* We don't complete operand 1, because it's the return value of
         the EXIT_BLOCK_EXPR which doesn't exist it Java */
      return patch_bc_statement (node);

    case CASE_EXPR:
      cn = java_complete_tree (TREE_OPERAND (node, 0));
      if (cn == error_mark_node)
	return cn;

      /* First, the case expression must be constant */
      cn = fold (cn);

      if (!TREE_CONSTANT (cn) && !flag_emit_xref)
	{
	  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);
	  parse_error_context (node, "Constant expression required");
	  return error_mark_node;
	}

      nn = ctxp->current_loop;

      /* It must be assignable to the type of the switch expression. */
      if (!try_builtin_assignconv (NULL_TREE, 
				   TREE_TYPE (TREE_OPERAND (nn, 0)), cn))
	{
	  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);
	  parse_error_context 
	    (wfl_operator,
	     "Incompatible type for case. Can't convert `%s' to `int'",
	     lang_printable_name (TREE_TYPE (cn), 0));
	  return error_mark_node;
	}

      cn = fold (convert (int_type_node, cn));

      /* Multiple instance of a case label bearing the same
	 value is checked during code generation. The case
	 expression is allright so far. */
      TREE_OPERAND (node, 0) = cn;
      TREE_TYPE (node) = void_type_node;
      CAN_COMPLETE_NORMALLY (node) = 1;
      TREE_SIDE_EFFECTS (node) = 1;
      break;

    case DEFAULT_EXPR:
      nn = ctxp->current_loop;
      /* Only one default label is allowed per switch statement */
      if (SWITCH_HAS_DEFAULT (nn))
	{
	  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);
	  parse_error_context (wfl_operator, 
			       "Duplicate case label: `default'");
	  return error_mark_node;
	}
      else
	SWITCH_HAS_DEFAULT (nn) = 1;
      TREE_TYPE (node) = void_type_node;
      TREE_SIDE_EFFECTS (node) = 1;
      CAN_COMPLETE_NORMALLY (node) = 1;
      break;

    case SWITCH_EXPR:
    case LOOP_EXPR:
      PUSH_LOOP (node);
      /* Check whether the loop was enclosed in a labeled
         statement. If not, create one, insert the loop in it and
         return the node */
      nn = patch_loop_statement (node);

      /* Anyways, walk the body of the loop */
      if (TREE_CODE (node) == LOOP_EXPR)
	TREE_OPERAND (node, 0) = java_complete_tree (TREE_OPERAND (node, 0));
      /* Switch statement: walk the switch expression and the cases */
      else
	node = patch_switch_statement (node);

      if (TREE_OPERAND (node, 0) == error_mark_node)
	nn = error_mark_node;
      else
	{
	  TREE_TYPE (nn) = TREE_TYPE (node) = void_type_node;
	  /* If we returned something different, that's because we
	     inserted a label. Pop the label too. */
	  if (nn != node)
	    {
	      if (CAN_COMPLETE_NORMALLY (node))
		CAN_COMPLETE_NORMALLY (nn) = 1;
	      POP_LABELED_BLOCK ();
	    }
	}
      POP_LOOP ();
      return nn;

    case EXIT_EXPR:
      TREE_OPERAND (node, 0) = java_complete_tree (TREE_OPERAND (node, 0));
      return patch_exit_expr (node);

    case COND_EXPR:
      /* Condition */
      TREE_OPERAND (node, 0) = java_complete_tree (TREE_OPERAND (node, 0));
      if (TREE_OPERAND (node, 0) == error_mark_node)
	return error_mark_node;
      /* then-else branches */
      TREE_OPERAND (node, 1) = java_complete_tree (TREE_OPERAND (node, 1));
      if (TREE_OPERAND (node, 1) == error_mark_node)
	return error_mark_node;
      TREE_OPERAND (node, 2) = java_complete_tree (TREE_OPERAND (node, 2));
      if (TREE_OPERAND (node, 2) == error_mark_node)
	return error_mark_node;
      return patch_if_else_statement (node);
      break;

    case CONDITIONAL_EXPR:
      /* Condition */
      wfl_op1 = TREE_OPERAND (node, 0);
      COMPLETE_CHECK_OP_0 (node);
      wfl_op2 = TREE_OPERAND (node, 1);
      COMPLETE_CHECK_OP_1 (node);
      wfl_op3 = TREE_OPERAND (node, 2);
      COMPLETE_CHECK_OP_2 (node);
      return patch_conditional_expr (node, wfl_op1, wfl_op2);

      /* 3- Expression section */
    case COMPOUND_EXPR:
      wfl_op2 = TREE_OPERAND (node, 1);
      TREE_OPERAND (node, 0) = nn = 
	java_complete_tree (TREE_OPERAND (node, 0));
      if (wfl_op2 == empty_stmt_node)
	CAN_COMPLETE_NORMALLY (node) = CAN_COMPLETE_NORMALLY (nn);
      else
	{
	  if (! CAN_COMPLETE_NORMALLY (nn) && TREE_CODE (nn) != ERROR_MARK)
	    {
	      /* An unreachable condition in a do-while statement
		 is *not* (technically) an unreachable statement. */
	      nn = wfl_op2;
	      if (TREE_CODE (nn) == EXPR_WITH_FILE_LOCATION)
		nn = EXPR_WFL_NODE (nn);
	      if (TREE_CODE (nn) != EXIT_EXPR)
		{
		  SET_WFL_OPERATOR (wfl_operator, node, wfl_op2);
		  parse_error_context (wfl_operator, "Unreachable statement");
		}
	    }
	  TREE_OPERAND (node, 1) = java_complete_tree (TREE_OPERAND (node, 1));
	  if (TREE_OPERAND (node, 1) == error_mark_node)
	    return error_mark_node;
	  CAN_COMPLETE_NORMALLY (node)
	    = CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 1));
	}
      TREE_TYPE (node) = TREE_TYPE (TREE_OPERAND (node, 1));
      break;

    case RETURN_EXPR:
      /* CAN_COMPLETE_NORMALLY (node) = 0; */
      return patch_return (node);

    case EXPR_WITH_FILE_LOCATION:
      if (!EXPR_WFL_NODE (node) /* Or a PRIMARY flag ? */
	  || TREE_CODE (EXPR_WFL_NODE (node)) == IDENTIFIER_NODE)
	{
	  tree wfl = node;
	  node = resolve_expression_name (node, NULL);
	  if (node == error_mark_node)
	    return node;
	  /* Keep line number information somewhere were it doesn't
	     disrupt the completion process. */
	  if (flag_emit_xref)
	    {
	      EXPR_WFL_NODE (wfl) = TREE_OPERAND (node, 1);
	      TREE_OPERAND (node, 1) = wfl;
	    }
	  CAN_COMPLETE_NORMALLY (node) = 1;
	}
      else
	{
	  tree body;
	  int save_lineno = lineno;
	  lineno = EXPR_WFL_LINENO (node);
	  body = java_complete_tree (EXPR_WFL_NODE (node));
	  lineno = save_lineno;
	  EXPR_WFL_NODE (node) = body;
	  TREE_SIDE_EFFECTS (node) = TREE_SIDE_EFFECTS (body);
	  CAN_COMPLETE_NORMALLY (node) = CAN_COMPLETE_NORMALLY (body);
	  if (body == empty_stmt_node)
	    {
	      /* Optimization;  makes it easier to detect empty bodies. */
	      return body;
	    }
	  if (body == error_mark_node)
	    {
	      /* Its important for the evaluation of assignment that
		 this mark on the TREE_TYPE is propagated. */
	      TREE_TYPE (node) = error_mark_node;
	      return error_mark_node;
	    }
	  else
	    TREE_TYPE (node) = TREE_TYPE (EXPR_WFL_NODE (node));
	  
	}
      break;

    case NEW_ARRAY_EXPR:
      /* Patch all the dimensions */
      flag = 0;
      for (cn = TREE_OPERAND (node, 1); cn; cn = TREE_CHAIN (cn))
	{
	  int location = EXPR_WFL_LINECOL (TREE_VALUE (cn));
	  tree dim = java_complete_tree (TREE_VALUE (cn));
	  if (dim == error_mark_node)
	    {
	      flag = 1;
	      continue;
	    }
	  else
	    {
	      TREE_VALUE (cn) = dim;
	      /* Setup the location of the current dimension, for
		 later error report. */
	      TREE_PURPOSE (cn) = 
		build_expr_wfl (NULL_TREE, input_filename, 0, 0);
	      EXPR_WFL_LINECOL (TREE_PURPOSE (cn)) = location;
	    }
	}
      /* They complete the array creation expression, if no errors
         were found. */
      CAN_COMPLETE_NORMALLY (node) = 1;
      return (flag ? error_mark_node
	      : force_evaluation_order (patch_newarray (node)));

    case NEW_CLASS_EXPR:
    case CALL_EXPR:
      /* Complete function's argument(s) first */
      if (complete_function_arguments (node))
	return error_mark_node;
      else
	{
	  tree decl, wfl = TREE_OPERAND (node, 0);
	  int in_this = CALL_THIS_CONSTRUCTOR_P (node);

	  node = patch_method_invocation (node, NULL_TREE, 
					  NULL_TREE, 0, &decl);
	  if (node == error_mark_node)
	    return error_mark_node;

	  check_thrown_exceptions (EXPR_WFL_LINECOL (node), decl);
	  /* If we call this(...), register signature and positions */
	  if (in_this)
	    DECL_CONSTRUCTOR_CALLS (current_function_decl) = 
	      tree_cons (wfl, decl, 
			 DECL_CONSTRUCTOR_CALLS (current_function_decl));
	  CAN_COMPLETE_NORMALLY (node) = 1;
	  return force_evaluation_order (node);
	}

    case MODIFY_EXPR:
      /* Save potential wfls */
      wfl_op1 = TREE_OPERAND (node, 0);
      TREE_OPERAND (node, 0) = nn = java_complete_lhs (wfl_op1);
      if (MODIFY_EXPR_FROM_INITIALIZATION_P (node)
	  && TREE_CODE (nn) == VAR_DECL && TREE_STATIC (nn)
	  && DECL_INITIAL (nn) != NULL_TREE)
	{
	  tree value = fold_constant_for_init (nn, nn);
	  if (value != NULL_TREE)
	    {
	      tree type = TREE_TYPE (value);
	      if (JPRIMITIVE_TYPE_P (type) || type == string_ptr_type_node)
		return empty_stmt_node;
	    }
	  DECL_INITIAL (nn) = NULL_TREE;
	}
      wfl_op2 = TREE_OPERAND (node, 1);

      if (TREE_OPERAND (node, 0) == error_mark_node)
	return error_mark_node;

      if (COMPOUND_ASSIGN_P (wfl_op2))
	{
	  tree lvalue = java_stabilize_reference (TREE_OPERAND (node, 0)); 

	  /* Hand stablize the lhs on both places */
	  TREE_OPERAND (node, 0) = lvalue;
	  TREE_OPERAND (TREE_OPERAND (node, 1), 0) = lvalue;

	  /* Now complete the RHS. We write it back later on. */
	  nn = java_complete_tree (TREE_OPERAND (node, 1));

	  if ((cn = patch_string (nn)))
	    nn = cn;

	  /* The last part of the rewrite for E1 op= E2 is to have 
	     E1 = (T)(E1 op E2), with T being the type of E1. */
	  nn = java_complete_tree (build_cast (EXPR_WFL_LINECOL (wfl_op2), 
					       TREE_TYPE (lvalue), nn));
	}

      /* If we're about to patch a NEW_ARRAY_INIT, we call a special
	 function to complete this RHS */
      else if (TREE_CODE (wfl_op2) == NEW_ARRAY_INIT)
	nn = patch_new_array_init (TREE_TYPE (TREE_OPERAND (node, 0)),
				   TREE_OPERAND (node, 1));
      /* Otherwise we simply complete the RHS */
      else
	nn = java_complete_tree (TREE_OPERAND (node, 1));

      if (nn == error_mark_node)
	return error_mark_node;

      /* Write back the RHS as we evaluated it. */
      TREE_OPERAND (node, 1) = nn;

      /* In case we're handling = with a String as a RHS, we need to
	 produce a String out of the RHS (it might still be a
	 STRING_CST or a StringBuffer at this stage */
      if ((nn = patch_string (TREE_OPERAND (node, 1))))
	TREE_OPERAND (node, 1) = nn;
      node = patch_assignment (node, wfl_op1, wfl_op2);
      CAN_COMPLETE_NORMALLY (node) = 1;
      return node;

    case MULT_EXPR:
    case PLUS_EXPR:
    case MINUS_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case URSHIFT_EXPR:
    case BIT_AND_EXPR:
    case BIT_XOR_EXPR:
    case BIT_IOR_EXPR:
    case TRUNC_MOD_EXPR:
    case RDIV_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case EQ_EXPR: 
    case NE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case LE_EXPR:
      /* Operands 0 and 1 are WFL in certain cases only. patch_binop
	 knows how to handle those cases. */
      wfl_op1 = TREE_OPERAND (node, 0);
      wfl_op2 = TREE_OPERAND (node, 1);

      CAN_COMPLETE_NORMALLY (node) = 1;
      /* Don't complete string nodes if dealing with the PLUS operand. */
      if (TREE_CODE (node) != PLUS_EXPR || !JSTRING_P (wfl_op1))
        {
          nn = java_complete_tree (wfl_op1);
          if (nn == error_mark_node)
            return error_mark_node;
          if ((cn = patch_string (nn)))
            nn = cn;
          TREE_OPERAND (node, 0) = nn;
        }
      if (TREE_CODE (node) != PLUS_EXPR || !JSTRING_P (wfl_op2))
        {
          nn = java_complete_tree (wfl_op2);
          if (nn == error_mark_node)
            return error_mark_node;
          if ((cn = patch_string (nn)))
            nn = cn;
          TREE_OPERAND (node, 1) = nn;
        }
      return force_evaluation_order (patch_binop (node, wfl_op1, wfl_op2));

    case INSTANCEOF_EXPR:
      wfl_op1 = TREE_OPERAND (node, 0);
      COMPLETE_CHECK_OP_0 (node);
      if (flag_emit_xref)
	{
	  TREE_TYPE (node) = boolean_type_node;
	  return node;
	}
      return patch_binop (node, wfl_op1, TREE_OPERAND (node, 1));

    case UNARY_PLUS_EXPR:
    case NEGATE_EXPR:
    case TRUTH_NOT_EXPR:
    case BIT_NOT_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case CONVERT_EXPR:
      /* There are cases were wfl_op1 is a WFL. patch_unaryop knows
	 how to handle those cases. */
      wfl_op1 = TREE_OPERAND (node, 0);
      CAN_COMPLETE_NORMALLY (node) = 1;
      TREE_OPERAND (node, 0) = java_complete_tree (wfl_op1);
      if (TREE_OPERAND (node, 0) == error_mark_node)
	return error_mark_node;
      node = patch_unaryop (node, wfl_op1);
      CAN_COMPLETE_NORMALLY (node) = 1;
      break;

    case ARRAY_REF:
      /* There are cases were wfl_op1 is a WFL. patch_array_ref knows
	 how to handle those cases. */
      wfl_op1 = TREE_OPERAND (node, 0);
      TREE_OPERAND (node, 0) = java_complete_tree (wfl_op1);
      if (TREE_OPERAND (node, 0) == error_mark_node)
	return error_mark_node;
      if (!flag_emit_class_files && !flag_emit_xref)
	TREE_OPERAND (node, 0) = save_expr (TREE_OPERAND (node, 0));
      /* The same applies to wfl_op2 */
      wfl_op2 = TREE_OPERAND (node, 1);
      TREE_OPERAND (node, 1) = java_complete_tree (wfl_op2);
      if (TREE_OPERAND (node, 1) == error_mark_node)
	return error_mark_node;
      if (!flag_emit_class_files && !flag_emit_xref)
	TREE_OPERAND (node, 1) = save_expr (TREE_OPERAND (node, 1));
      return patch_array_ref (node);

    case RECORD_TYPE:
      return node;;

    case COMPONENT_REF:
      /* The first step in the re-write of qualified name handling.  FIXME.
	 So far, this is only to support PRIMTYPE.class -> PRIMCLASS.TYPE. */
      TREE_OPERAND (node, 0) = java_complete_tree (TREE_OPERAND (node, 0));
      if (TREE_CODE (TREE_OPERAND (node, 0)) == RECORD_TYPE)
	{
	  tree name = TREE_OPERAND (node, 1);
	  tree field = lookup_field_wrapper (TREE_OPERAND (node, 0), name);
	  if (field == NULL_TREE)
	    {
	      error ("missing static field `%s'", IDENTIFIER_POINTER (name));
	      return error_mark_node;
	    }
	  if (! FIELD_STATIC (field))
	    {
	      error ("not a static field `%s'", IDENTIFIER_POINTER (name));
	      return error_mark_node;
	    }
	  return field;
	}
      else
	fatal ("unimplemented java_complete_tree for COMPONENT_REF");
      break;

    case THIS_EXPR:
      /* Can't use THIS in a static environment */
      if (!current_this)
	{
	  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);
	  parse_error_context (wfl_operator, "Keyword `this' used outside "
			       "allowed context");
	  TREE_TYPE (node) = error_mark_node;
	  return error_mark_node;
	}
      if (ctxp->explicit_constructor_p)
	{
	  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);
	  parse_error_context 
	    (wfl_operator, "Can't reference `this' or `super' before the "
	     "superclass constructor has been called");
	  TREE_TYPE (node) = error_mark_node;
	  return error_mark_node;
	}
      return current_this;

    default:
      CAN_COMPLETE_NORMALLY (node) = 1;
      /* Ok: may be we have a STRING_CST or a crafted `StringBuffer'
	 and it's time to turn it into the appropriate String object
	 */
      if ((node = patch_string (node)))
	return node;
      fatal ("No case for tree code `%s' - java_complete_tree\n",
	     tree_code_name [TREE_CODE (node)]);
    }
  return node;
}

/* Complete function call's argument. Return a non zero value is an
   error was found.  */

static int
complete_function_arguments (node)
     tree node;
{
  int flag = 0;
  tree cn;

  ctxp->explicit_constructor_p += (CALL_THIS_CONSTRUCTOR_P (node) ? 1 : 0);
  for (cn = TREE_OPERAND (node, 1); cn; cn = TREE_CHAIN (cn))
    {
      tree wfl = TREE_VALUE (cn), parm, temp;
      parm = java_complete_tree (wfl);
      if (parm == error_mark_node)
	{
	  flag = 1;
	  continue;
	}
      /* If have a string literal that we haven't transformed yet or a
	 crafted string buffer, as a result of use of the the String
	 `+' operator. Build `parm.toString()' and expand it. */
      if ((temp = patch_string (parm)))
	parm = temp;
      /* Inline PRIMTYPE.TYPE read access */
      parm = maybe_build_primttype_type_ref (parm, wfl);

      TREE_VALUE (cn) = parm;
    }
  ctxp->explicit_constructor_p -= (CALL_THIS_CONSTRUCTOR_P (node) ? 1 : 0);
  return flag;
}

/* Sometimes (for loops and variable initialized during their
   declaration), we want to wrap a statement around a WFL and turn it
   debugable.  */

static tree
build_debugable_stmt (location, stmt)
    int location;
    tree stmt;
{
  if (TREE_CODE (stmt) != EXPR_WITH_FILE_LOCATION)
    {
      stmt = build_expr_wfl (stmt, input_filename, 0, 0);
      EXPR_WFL_LINECOL (stmt) = location;
    }
  JAVA_MAYBE_GENERATE_DEBUG_INFO (stmt);
  return stmt;
}

static tree
build_expr_block (body, decls)
     tree body, decls;
{
  tree node = make_node (BLOCK);
  BLOCK_EXPR_DECLS (node) = decls;
  BLOCK_EXPR_BODY (node) = body;
  if (body)
    TREE_TYPE (node) = TREE_TYPE (body);
  TREE_SIDE_EFFECTS (node) = 1;
  return node;
}

/* Create a new function block and link it approriately to current
   function block chain */

static tree
enter_block ()
{
  return (enter_a_block (build_expr_block (NULL_TREE, NULL_TREE)));
}

/* Link block B supercontext to the previous block. The current
   function DECL is used as supercontext when enter_a_block is called
   for the first time for a given function. The current function body
   (DECL_FUNCTION_BODY) is set to be block B.  */

static tree
enter_a_block (b)
     tree b;
{
  tree fndecl = current_function_decl; 

  if (!fndecl) {
    BLOCK_SUPERCONTEXT (b) = current_static_block;
    current_static_block = b;
  }

  else if (!DECL_FUNCTION_BODY (fndecl))
    {
      BLOCK_SUPERCONTEXT (b) = fndecl;
      DECL_FUNCTION_BODY (fndecl) = b;
    }
  else
    {
      BLOCK_SUPERCONTEXT (b) = DECL_FUNCTION_BODY (fndecl);
      DECL_FUNCTION_BODY (fndecl) = b;
    }
  return b;
}

/* Exit a block by changing the current function body
   (DECL_FUNCTION_BODY) to the current block super context, only if
   the block being exited isn't the method's top level one.  */

static tree
exit_block ()
{
  tree b;
  if (current_function_decl)
    {
      b = DECL_FUNCTION_BODY (current_function_decl);
      if (BLOCK_SUPERCONTEXT (b) != current_function_decl)
	DECL_FUNCTION_BODY (current_function_decl) = BLOCK_SUPERCONTEXT (b);
    }
  else
    {
      b = current_static_block;

      if (BLOCK_SUPERCONTEXT (b))
	current_static_block = BLOCK_SUPERCONTEXT (b);
    }
  return b;
}

/* Lookup for NAME in the nested function's blocks, all the way up to
   the current toplevel one. It complies with Java's local variable
   scoping rules.  */

static tree
lookup_name_in_blocks (name)
     tree name;
{
  tree b = GET_CURRENT_BLOCK (current_function_decl);

  while (b != current_function_decl)
    {
      tree current;

      /* Paranoid sanity check. To be removed */
      if (TREE_CODE (b) != BLOCK)
	fatal ("non block expr function body - lookup_name_in_blocks");

      for (current = BLOCK_EXPR_DECLS (b); current; 
	   current = TREE_CHAIN (current))
	if (DECL_NAME (current) == name)
	  return current;
      b = BLOCK_SUPERCONTEXT (b);
    }
  return NULL_TREE;
}

static void
maybe_absorb_scoping_blocks ()
{
  while (BLOCK_EXPR_ORIGIN (GET_CURRENT_BLOCK (current_function_decl)))
    {
      tree b = exit_block ();
      java_method_add_stmt (current_function_decl, b);
      SOURCE_FRONTEND_DEBUG (("Absorbing scoping block at line %d", lineno));
    }
}


/* This section of the source is reserved to build_* functions that
   are building incomplete tree nodes and the patch_* functions that
   are completing them.  */

/* Build a super() constructor invocation. Returns empty_stmt_node if
   we're currently dealing with the class java.lang.Object. */

static tree
build_super_invocation ()
{
  if (current_class == object_type_node)
    return empty_stmt_node;
  else
    {
      tree super_wfl = build_wfl_node (super_identifier_node);
      return build_method_invocation (super_wfl, NULL_TREE);
    }
}

/* Build a SUPER/THIS qualified method invocation.  */

static tree
build_this_super_qualified_invocation (use_this, name, args, lloc, rloc)
     int use_this;
     tree name, args;
     int lloc, rloc;

{
  tree invok;
  tree wfl = 
    build_wfl_node (use_this ? this_identifier_node : super_identifier_node);
  EXPR_WFL_LINECOL (wfl) = lloc;
  invok = build_method_invocation (name, args);
  return make_qualified_primary (wfl, invok, rloc);
}

/* Build an incomplete CALL_EXPR node. */

static tree
build_method_invocation (name, args)
    tree name;
    tree args;
{
  tree call = build (CALL_EXPR, NULL_TREE, name, args, NULL_TREE);
  TREE_SIDE_EFFECTS (call) = 1;
  EXPR_WFL_LINECOL (call) = EXPR_WFL_LINECOL (name);
  return call;
}

/* Build an incomplete new xxx(...) node. */

static tree
build_new_invocation (name, args)
    tree name, args;
{
  tree call = build (NEW_CLASS_EXPR, NULL_TREE, name, args, NULL_TREE);
  TREE_SIDE_EFFECTS (call) = 1;
  EXPR_WFL_LINECOL (call) = EXPR_WFL_LINECOL (name);
  return call;
}

/* Build an incomplete assignment expression. */

static tree
build_assignment (op, op_location, lhs, rhs)
     int op, op_location;
     tree lhs, rhs;
{
  tree assignment;
  /* Build the corresponding binop if we deal with a Compound
     Assignment operator. Mark the binop sub-tree as part of a
     Compound Assignment expression */
  if (op != ASSIGN_TK)
    {
      rhs = build_binop (BINOP_LOOKUP (op), op_location, lhs, rhs);
      COMPOUND_ASSIGN_P (rhs) = 1;
    }
  assignment = build (MODIFY_EXPR, NULL_TREE, lhs, rhs);
  TREE_SIDE_EFFECTS (assignment) = 1;
  EXPR_WFL_LINECOL (assignment) = op_location;
  return assignment;
}

/* Print an INTEGER_CST node in a static buffer, and return the buffer. */

char *
print_int_node (node)
    tree node;
{
  static char buffer [80];
  if (TREE_CONSTANT_OVERFLOW (node))
    sprintf (buffer, "<overflow>");
    
  if (TREE_INT_CST_HIGH (node) == 0)
    sprintf (buffer, HOST_WIDE_INT_PRINT_UNSIGNED,
	     TREE_INT_CST_LOW (node));
  else if (TREE_INT_CST_HIGH (node) == -1
	   && TREE_INT_CST_LOW (node) != 0)
    {
      buffer [0] = '-';
      sprintf (&buffer [1], HOST_WIDE_INT_PRINT_UNSIGNED,
	       -TREE_INT_CST_LOW (node));
    }
  else
    sprintf (buffer, HOST_WIDE_INT_PRINT_DOUBLE_HEX,
	     TREE_INT_CST_HIGH (node), TREE_INT_CST_LOW (node));

  return buffer;
}

/* Return 1 if an assignment to a FINAL is attempted in a non suitable
   context.  */

static int
check_final_assignment (lvalue, wfl)
     tree lvalue, wfl;
{
  if (JDECL_P (lvalue) 
      && FIELD_FINAL (lvalue) && !IS_CLINIT (current_function_decl))
    {
      parse_error_context 
        (wfl, "Can't assign a value to the final variable `%s'",
	 IDENTIFIER_POINTER (EXPR_WFL_NODE (wfl)));
      return 1;
    }
  return 0;
}

/* Inline references to java.lang.PRIMTYPE.TYPE when accessed in
   read. This is needed to avoid circularities in the implementation
   of these fields in libjava. */

static tree
maybe_build_primttype_type_ref (rhs, wfl)
    tree rhs, wfl;
{
  tree to_return = NULL_TREE;
  tree rhs_type = TREE_TYPE (rhs);
  if (TREE_CODE (rhs) == COMPOUND_EXPR)
    {
      tree n = TREE_OPERAND (rhs, 1);
      if (TREE_CODE (n) == VAR_DECL 
	  && DECL_NAME (n) == TYPE_identifier_node
	  && rhs_type == class_ptr_type)
	{
	  char *self_name = IDENTIFIER_POINTER (EXPR_WFL_NODE (wfl));
	  if (!strncmp (self_name, "java.lang.", 10))
	    to_return = build_primtype_type_ref (self_name);
	}
    }
  return (to_return ? to_return : rhs );
}

/* 15.25 Assignment operators. */

static tree
patch_assignment (node, wfl_op1, wfl_op2)
     tree node;
     tree wfl_op1;
     tree wfl_op2;
{
  tree rhs = TREE_OPERAND (node, 1);
  tree lvalue = TREE_OPERAND (node, 0), llvalue;
  tree lhs_type, rhs_type, new_rhs = NULL_TREE;
  int error_found = 0;
  int lvalue_from_array = 0;

  /* Can't assign to a final. */
  if (check_final_assignment (lvalue, wfl_op1))
    error_found = 1;

  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);

  /* Lhs can be a named variable */
  if (JDECL_P (lvalue))
    {
      lhs_type = TREE_TYPE (lvalue);
    }
  /* Or Lhs can be a array acccess. Should that be lvalue ? FIXME +
     comment on reason why */
  else if (TREE_CODE (wfl_op1) == ARRAY_REF)
    {
      lhs_type = TREE_TYPE (lvalue);
      lvalue_from_array = 1;
    }
  /* Or a field access */
  else if (TREE_CODE (lvalue) == COMPONENT_REF)
    lhs_type = TREE_TYPE (lvalue);
  /* Or a function return slot */
  else if (TREE_CODE (lvalue) == RESULT_DECL)
    lhs_type = TREE_TYPE (lvalue);
  /* Otherwise, we might want to try to write into an optimized static
     final, this is an of a different nature, reported further on. */
  else if (TREE_CODE (wfl_op1) == EXPR_WITH_FILE_LOCATION
	   && resolve_expression_name (wfl_op1, &llvalue))
    {
      if (check_final_assignment (llvalue, wfl_op1))
	{
	  /* What we should do instead is resetting the all the flags
	     previously set, exchange lvalue for llvalue and continue. */
	  error_found = 1;
	  return error_mark_node;
	}
      else 
	lhs_type = TREE_TYPE (lvalue);
    }
  else 
    {
      parse_error_context (wfl_op1, "Invalid left hand side of assignment");
      error_found = 1;
    }

  rhs_type = TREE_TYPE (rhs);
  /* 5.1 Try the assignment conversion for builtin type. */
  new_rhs = try_builtin_assignconv (wfl_op1, lhs_type, rhs);

  /* 5.2 If it failed, try a reference conversion */
  if (!new_rhs && (new_rhs = try_reference_assignconv (lhs_type, rhs)))
    lhs_type = promote_type (rhs_type);

  /* 15.25.2 If we have a compound assignment, convert RHS into the
     type of the LHS */
  else if (COMPOUND_ASSIGN_P (TREE_OPERAND (node, 1)))
    new_rhs = convert (lhs_type, rhs);

  /* Explicit cast required. This is an error */
  if (!new_rhs)
    {
      char *t1 = strdup (lang_printable_name (TREE_TYPE (rhs), 0));
      char *t2 = strdup (lang_printable_name (lhs_type, 0));
      tree wfl;
      char operation [32];	/* Max size known */

      /* If the assignment is part of a declaration, we use the WFL of
	 the declared variable to point out the error and call it a
	 declaration problem. If the assignment is a genuine =
	 operator, we call is a operator `=' problem, otherwise we
	 call it an assignment problem. In both of these last cases,
	 we use the WFL of the operator to indicate the error. */

      if (MODIFY_EXPR_FROM_INITIALIZATION_P (node))
	{
	  wfl = wfl_op1;
	  strcpy (operation, "declaration");
	}
      else
	{
	  wfl = wfl_operator;
	  if (COMPOUND_ASSIGN_P (TREE_OPERAND (node, 1)))
	    strcpy (operation, "assignment");
	  else if (TREE_CODE (TREE_OPERAND (node, 0)) == RESULT_DECL)
	    strcpy (operation, "`return'");
	  else
	    strcpy (operation, "`='");
	}

      parse_error_context 
	(wfl, (!valid_cast_to_p (rhs_type, lhs_type) ?
	       "Incompatible type for %s. Can't convert `%s' to `%s'" :
	       "Incompatible type for %s. Explicit cast "
	       "needed to convert `%s' to `%s'"), operation, t1, t2);
      free (t1); free (t2);
      error_found = 1;
    }

  /* Inline read access to java.lang.PRIMTYPE.TYPE */
  if (new_rhs)
    new_rhs = maybe_build_primttype_type_ref (new_rhs, wfl_op2);

  if (error_found)
    return error_mark_node;

  /* 10.10: Array Store Exception runtime check */
  if (!flag_emit_class_files
      && !flag_emit_xref
      && lvalue_from_array 
      && JREFERENCE_TYPE_P (TYPE_ARRAY_ELEMENT (lhs_type))
      && !CLASS_FINAL (TYPE_NAME (GET_SKIP_TYPE (rhs_type))))
    {
      tree check;
      tree base = lvalue;

      /* We need to retrieve the right argument for _Jv_CheckArrayStore */
      if (TREE_CODE (lvalue) == COMPOUND_EXPR)
	base = TREE_OPERAND (lvalue, 0);
      else
	{
	  if (flag_bounds_check)
	    base = TREE_OPERAND (TREE_OPERAND (TREE_OPERAND (base, 0), 1), 0);
	  else
	    base = TREE_OPERAND (TREE_OPERAND (base, 0), 0);
	}

      /* Build the invocation of _Jv_CheckArrayStore */
      new_rhs = save_expr (new_rhs);
      check = build (CALL_EXPR, void_type_node,
		     build_address_of (soft_checkarraystore_node),
		     tree_cons (NULL_TREE, base,
				build_tree_list (NULL_TREE, new_rhs)),
		     NULL_TREE);
      TREE_SIDE_EFFECTS (check) = 1;

      /* We have to decide on an insertion point */
      if (TREE_CODE (lvalue) == COMPOUND_EXPR)
	{
	  tree t;
	  if (flag_bounds_check)
	    {
	      t = TREE_OPERAND (TREE_OPERAND (TREE_OPERAND (lvalue, 1), 0), 0);
	      TREE_OPERAND (TREE_OPERAND (TREE_OPERAND (lvalue, 1), 0), 0) =
		build (COMPOUND_EXPR, void_type_node, t, check);
	    }
	  else
	    TREE_OPERAND (lvalue, 1) = build (COMPOUND_EXPR, lhs_type,
					      check, TREE_OPERAND (lvalue, 1));
	}
      else 
	{
	  /* Make sure the bound check will happen before the store check */
	  if (flag_bounds_check)
	    TREE_OPERAND (TREE_OPERAND (lvalue, 0), 0) =
	      build (COMPOUND_EXPR, void_type_node,
		     TREE_OPERAND (TREE_OPERAND (lvalue, 0), 0), check);
	  else
	    lvalue = build (COMPOUND_EXPR, lhs_type, check, lvalue);
	}
    }

  TREE_OPERAND (node, 0) = lvalue;
  TREE_OPERAND (node, 1) = new_rhs;
  TREE_TYPE (node) = lhs_type;
  return node;
}

/* Check that type SOURCE can be cast into type DEST. If the cast
   can't occur at all, return 0 otherwise 1. This function is used to
   produce accurate error messages on the reasons why an assignment
   failed. */

static tree
try_reference_assignconv (lhs_type, rhs)
     tree lhs_type, rhs;
{
  tree new_rhs = NULL_TREE;
  tree rhs_type = TREE_TYPE (rhs);

  if (!JPRIMITIVE_TYPE_P (rhs_type) && JREFERENCE_TYPE_P (lhs_type))
    {
      /* `null' may be assigned to any reference type */
      if (rhs == null_pointer_node)
        new_rhs = null_pointer_node;
      /* Try the reference assignment conversion */
      else if (valid_ref_assignconv_cast_p (rhs_type, lhs_type, 0))
	new_rhs = rhs;
      /* This is a magic assignment that we process differently */
      else if (rhs == soft_exceptioninfo_call_node)
	new_rhs = rhs;
    }
  return new_rhs;
}

/* Check that RHS can be converted into LHS_TYPE by the assignment
   conversion (5.2), for the cases of RHS being a builtin type. Return
   NULL_TREE if the conversion fails or if because RHS isn't of a
   builtin type. Return a converted RHS if the conversion is possible.  */

static tree
try_builtin_assignconv (wfl_op1, lhs_type, rhs)
     tree wfl_op1, lhs_type, rhs;
{
  tree new_rhs = NULL_TREE;
  tree rhs_type = TREE_TYPE (rhs);

  /* Zero accepted everywhere */
  if (TREE_CODE (rhs) == INTEGER_CST 
      && TREE_INT_CST_HIGH (rhs) == 0 && TREE_INT_CST_LOW (rhs) == 0
      && JPRIMITIVE_TYPE_P (rhs_type))
    new_rhs = convert (lhs_type, rhs);

  /* 5.1.1 Try Identity Conversion,
     5.1.2 Try Widening Primitive Conversion */
  else if (valid_builtin_assignconv_identity_widening_p (lhs_type, rhs_type))
    new_rhs = convert (lhs_type, rhs);

  /* Try a narrowing primitive conversion (5.1.3): 
       - expression is a constant expression of type int AND
       - variable is byte, short or char AND
       - The value of the expression is representable in the type of the 
         variable */
  else if (rhs_type == int_type_node && TREE_CONSTANT (rhs)
	   && (lhs_type == byte_type_node || lhs_type == char_type_node
	       || lhs_type == short_type_node))
    {
      if (int_fits_type_p (rhs, lhs_type))
        new_rhs = convert (lhs_type, rhs);
      else if (wfl_op1)		/* Might be called with a NULL */
	parse_warning_context 
	  (wfl_op1, "Constant expression `%s' to wide for narrowing "
	   "primitive conversion to `%s'", 
	   print_int_node (rhs), lang_printable_name (lhs_type, 0));
      /* Reported a warning that will turn into an error further
	 down, so we don't return */
    }

  return new_rhs;
}

/* Return 1 if RHS_TYPE can be converted to LHS_TYPE by identity
   conversion (5.1.1) or widening primitve conversion (5.1.2).  Return
   0 is the conversion test fails.  This implements parts the method
   invocation convertion (5.3).  */

static int
valid_builtin_assignconv_identity_widening_p (lhs_type, rhs_type)
     tree lhs_type, rhs_type;
{
  /* 5.1.1: This is the identity conversion part. */
  if (lhs_type == rhs_type)
    return 1;

  /* Reject non primitive types */
  if (!JPRIMITIVE_TYPE_P (lhs_type) || !JPRIMITIVE_TYPE_P (rhs_type))
    return 0;

  /* 5.1.2: widening primitive conversion. byte, even if it's smaller
     than a char can't be converted into a char. Short can't too, but
     the < test below takes care of that */
  if (lhs_type == char_type_node && rhs_type == byte_type_node)
    return 0;

  /* Accept all promoted type here. Note, we can't use <= in the test
     below, because we still need to bounce out assignments of short
     to char and the likes */
  if (lhs_type == int_type_node
      && (rhs_type == promoted_byte_type_node
	  || rhs_type == promoted_short_type_node
	  || rhs_type == promoted_char_type_node
	  || rhs_type == promoted_boolean_type_node))
    return 1;

  /* From here, an integral is widened if its precision is smaller
     than the precision of the LHS or if the LHS is a floating point
     type, or the RHS is a float and the RHS a double. */
  if ((JINTEGRAL_TYPE_P (rhs_type) && JINTEGRAL_TYPE_P (lhs_type) 
       && (TYPE_PRECISION (rhs_type) < TYPE_PRECISION (lhs_type)))
      || (JINTEGRAL_TYPE_P (rhs_type) && JFLOAT_TYPE_P (lhs_type))
      || (rhs_type == float_type_node && lhs_type == double_type_node))
    return 1;

  return 0;
}

/* Check that something of SOURCE type can be assigned or cast to
   something of DEST type at runtime. Return 1 if the operation is
   valid, 0 otherwise. If CAST is set to 1, we're treating the case
   were SOURCE is cast into DEST, which borrows a lot of the
   assignment check. */

static int
valid_ref_assignconv_cast_p (source, dest, cast)
     tree source;
     tree dest;
     int cast;
{
  /* SOURCE or DEST might be null if not from a declared entity. */
  if (!source || !dest)
    return 0;
  if (JNULLP_TYPE_P (source))
    return 1;
  if (TREE_CODE (source) == POINTER_TYPE)
    source = TREE_TYPE (source);
  if (TREE_CODE (dest) == POINTER_TYPE)
    dest = TREE_TYPE (dest);
  /* Case where SOURCE is a class type */
  if (TYPE_CLASS_P (source))
    {
      if (TYPE_CLASS_P (dest))
	return  source == dest || inherits_from_p (source, dest)
	  || (cast && inherits_from_p (dest, source));
      if (TYPE_INTERFACE_P (dest))
	{
	  /* If doing a cast and SOURCE is final, the operation is
             always correct a compile time (because even if SOURCE
             does not implement DEST, a subclass of SOURCE might). */
	  if (cast && !CLASS_FINAL (TYPE_NAME (source)))
	    return 1;
	  /* Otherwise, SOURCE must implement DEST */
	  return interface_of_p (dest, source);
	}
      /* DEST is an array, cast permited if SOURCE is of Object type */
      return (cast && source == object_type_node ? 1 : 0);
    }
  if (TYPE_INTERFACE_P (source))
    {
      if (TYPE_CLASS_P (dest))
	{
	  /* If not casting, DEST must be the Object type */
	  if (!cast)
	    return dest == object_type_node;
	  /* We're doing a cast. The cast is always valid is class
	     DEST is not final, otherwise, DEST must implement SOURCE */
	  else if (!CLASS_FINAL (TYPE_NAME (dest)))
	    return 1;
	  else
	    return interface_of_p (source, dest);
	}
      if (TYPE_INTERFACE_P (dest))
	{
	  /* If doing a cast, then if SOURCE and DEST contain method
             with the same signature but different return type, then
             this is a (compile time) error */
	  if (cast)
	    {
	      tree method_source, method_dest;
	      tree source_type;
	      tree source_sig;
	      tree source_name;
	      for (method_source = TYPE_METHODS (source); method_source; 
		   method_source = TREE_CHAIN (method_source))
		{
		  source_sig = 
		    build_java_argument_signature (TREE_TYPE (method_source));
		  source_type = TREE_TYPE (TREE_TYPE (method_source));
		  source_name = DECL_NAME (method_source);
		  for (method_dest = TYPE_METHODS (dest);
		       method_dest; method_dest = TREE_CHAIN (method_dest))
		    if (source_sig == 
			build_java_argument_signature (TREE_TYPE (method_dest))
			&& source_name == DECL_NAME (method_dest)
			&& source_type != TREE_TYPE (TREE_TYPE (method_dest)))
		      return 0;
		}
	      return 1;
	    }
	  else
	    return source == dest || interface_of_p (dest, source);
	}
      else			/* Array */
	return (cast ? 
		(DECL_NAME (TYPE_NAME (source)) == java_lang_cloneable) : 0);
    }
  if (TYPE_ARRAY_P (source))
    {
      if (TYPE_CLASS_P (dest))
	return dest == object_type_node;
      /* Can't cast an array to an interface unless the interface is
	 java.lang.Cloneable */
      if (TYPE_INTERFACE_P (dest))
	return (DECL_NAME (TYPE_NAME (dest)) == java_lang_cloneable ? 1 : 0);
      else			/* Arrays */
	{
	  tree source_element_type = TYPE_ARRAY_ELEMENT (source);
	  tree dest_element_type = TYPE_ARRAY_ELEMENT (dest);
	  
	  /* In case of severe errors, they turn out null */
	  if (!dest_element_type || !source_element_type)
	    return 0;
	  if (source_element_type == dest_element_type)
	    return 1;
	  return valid_ref_assignconv_cast_p (source_element_type,
					      dest_element_type, cast);
	}
      return 0;
    }
  return 0;
}

static int
valid_cast_to_p (source, dest)
     tree source;
     tree dest;
{
  if (TREE_CODE (source) == POINTER_TYPE)
    source = TREE_TYPE (source);
  if (TREE_CODE (dest) == POINTER_TYPE)
    dest = TREE_TYPE (dest);

  if (TREE_CODE (source) == RECORD_TYPE && TREE_CODE (dest) == RECORD_TYPE)
    return valid_ref_assignconv_cast_p (source, dest, 1);

  else if (JNUMERIC_TYPE_P (source) && JNUMERIC_TYPE_P (dest))
    return 1;

  return 0;
}

/* Method invocation conversion test. Return 1 if type SOURCE can be
   converted to type DEST through the methond invocation conversion
   process (5.3) */

static tree
do_unary_numeric_promotion (arg)
     tree arg;
{
  tree type = TREE_TYPE (arg);
  if (TREE_CODE (type) == INTEGER_TYPE ? TYPE_PRECISION (type) < 32
      : TREE_CODE (type) == CHAR_TYPE)
    arg = convert (int_type_node, arg);
  return arg;
}

/* Return a non zero value if SOURCE can be converted into DEST using
   the method invocation conversion rule (5.3).  */
static int
valid_method_invocation_conversion_p (dest, source)
     tree dest, source;
{
  return ((JPRIMITIVE_TYPE_P (source) && JPRIMITIVE_TYPE_P (dest)
	   && valid_builtin_assignconv_identity_widening_p (dest, source))
	  || ((JREFERENCE_TYPE_P (source) || JNULLP_TYPE_P (source))
	      && (JREFERENCE_TYPE_P (dest) || JNULLP_TYPE_P (dest))
	      && valid_ref_assignconv_cast_p (source, dest, 0)));
}

/* Build an incomplete binop expression. */

static tree
build_binop (op, op_location, op1, op2)
     enum tree_code op;
     int op_location;
     tree op1, op2;
{
  tree binop = build (op, NULL_TREE, op1, op2);
  TREE_SIDE_EFFECTS (binop) = 1;
  /* Store the location of the operator, for better error report. The
     string of the operator will be rebuild based on the OP value. */
  EXPR_WFL_LINECOL (binop) = op_location;
  return binop;
}

/* Build the string of the operator retained by NODE. If NODE is part
   of a compound expression, add an '=' at the end of the string. This
   function is called when an error needs to be reported on an
   operator. The string is returned as a pointer to a static character
   buffer. */

static char *
operator_string (node)
     tree node;
{
#define BUILD_OPERATOR_STRING(S)					\
  {									\
    sprintf (buffer, "%s%s", S, (COMPOUND_ASSIGN_P (node) ? "=" : ""));	\
    return buffer;							\
  }
  
  static char buffer [10];
  switch (TREE_CODE (node))
    {
    case MULT_EXPR: BUILD_OPERATOR_STRING ("*");
    case RDIV_EXPR: BUILD_OPERATOR_STRING ("/");
    case TRUNC_MOD_EXPR: BUILD_OPERATOR_STRING ("%");
    case PLUS_EXPR: BUILD_OPERATOR_STRING ("+");
    case MINUS_EXPR: BUILD_OPERATOR_STRING ("-");
    case LSHIFT_EXPR: BUILD_OPERATOR_STRING ("<<");
    case RSHIFT_EXPR: BUILD_OPERATOR_STRING (">>");
    case URSHIFT_EXPR: BUILD_OPERATOR_STRING (">>>");
    case BIT_AND_EXPR: BUILD_OPERATOR_STRING ("&");
    case BIT_XOR_EXPR: BUILD_OPERATOR_STRING ("^");
    case BIT_IOR_EXPR: BUILD_OPERATOR_STRING ("|");
    case TRUTH_ANDIF_EXPR: BUILD_OPERATOR_STRING ("&&");
    case TRUTH_ORIF_EXPR: BUILD_OPERATOR_STRING ("||");
    case EQ_EXPR: BUILD_OPERATOR_STRING ("==");
    case NE_EXPR: BUILD_OPERATOR_STRING ("!=");
    case GT_EXPR: BUILD_OPERATOR_STRING (">");
    case GE_EXPR: BUILD_OPERATOR_STRING (">=");
    case LT_EXPR: BUILD_OPERATOR_STRING ("<");
    case LE_EXPR: BUILD_OPERATOR_STRING ("<=");
    case UNARY_PLUS_EXPR: BUILD_OPERATOR_STRING ("+");
    case NEGATE_EXPR: BUILD_OPERATOR_STRING ("-");
    case TRUTH_NOT_EXPR: BUILD_OPERATOR_STRING ("!");
    case BIT_NOT_EXPR: BUILD_OPERATOR_STRING ("~");
    case PREINCREMENT_EXPR:	/* Fall through */
    case POSTINCREMENT_EXPR: BUILD_OPERATOR_STRING ("++");
    case PREDECREMENT_EXPR:	/* Fall through */
    case POSTDECREMENT_EXPR: BUILD_OPERATOR_STRING ("--");
    default:
      fatal ("unregistered operator %s - operator_string",
	     tree_code_name [TREE_CODE (node)]);
    }
  return NULL;
#undef BUILD_OPERATOR_STRING
}

/* Binary operators (15.16 up to 15.18). We return error_mark_node on
   errors but we modify NODE so that it contains the type computed
   according to the expression, when it's fixed. Otherwise, we write
   error_mark_node as the type. It allows us to further the analysis
   of remaining nodes and detects more errors in certain cases.  */

static tree
patch_binop (node, wfl_op1, wfl_op2)
     tree node;
     tree wfl_op1;
     tree wfl_op2;
{
  tree op1 = TREE_OPERAND (node, 0);
  tree op2 = TREE_OPERAND (node, 1);
  tree op1_type = TREE_TYPE (op1);
  tree op2_type = TREE_TYPE (op2);
  tree prom_type = NULL_TREE;
  int code = TREE_CODE (node);

  /* If 1, tell the routine that we have to return error_mark_node
     after checking for the initialization of the RHS */
  int error_found = 0;

  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);

  switch (code)
    {
    /* 15.16 Multiplicative operators */
    case MULT_EXPR:		/* 15.16.1 Multiplication Operator * */
    case RDIV_EXPR:		/* 15.16.2 Division Operator / */
    case TRUNC_MOD_EXPR:	/* 15.16.3 Remainder operator % */
      if (!JPRIMITIVE_TYPE_P (op1_type) || !JPRIMITIVE_TYPE_P (op2_type))
	{
	  if (!JPRIMITIVE_TYPE_P (op1_type))
	    ERROR_CANT_CONVERT_TO_NUMERIC (wfl_operator, node, op1_type);
	  if (!JPRIMITIVE_TYPE_P (op2_type) && (op1_type != op2_type))
	    ERROR_CANT_CONVERT_TO_NUMERIC (wfl_operator, node, op2_type);
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	  break;
	}
      prom_type = binary_numeric_promotion (op1_type, op2_type, &op1, &op2);
      /* Change the division operator if necessary */
      if (code == RDIV_EXPR && TREE_CODE (prom_type) == INTEGER_TYPE)
	TREE_SET_CODE (node, TRUNC_DIV_EXPR);

      /* This one is more complicated. FLOATs are processed by a
	 function call to soft_fmod. Duplicate the value of the
	 COMPOUND_ASSIGN_P flag. */
      if (code == TRUNC_MOD_EXPR)
	{
	  tree mod = build_java_binop (TRUNC_MOD_EXPR, prom_type, op1, op2);
	  COMPOUND_ASSIGN_P (mod) = COMPOUND_ASSIGN_P (node);
	  TREE_SIDE_EFFECTS (mod)
	    = TREE_SIDE_EFFECTS (op1) | TREE_SIDE_EFFECTS (op2);
	  return mod;
	}
      break;

    /* 15.17 Additive Operators */
    case PLUS_EXPR:		/* 15.17.1 String Concatenation Operator + */

      /* Operation is valid if either one argument is a string
	 constant, a String object or a StringBuffer crafted for the
	 purpose of the a previous usage of the String concatenation
	 operator */

      if (TREE_CODE (op1) == STRING_CST 
	  || TREE_CODE (op2) == STRING_CST
	  || JSTRING_TYPE_P (op1_type)
	  || JSTRING_TYPE_P (op2_type)
	  || IS_CRAFTED_STRING_BUFFER_P (op1)
	  || IS_CRAFTED_STRING_BUFFER_P (op2))
	return build_string_concatenation (op1, op2);

    case MINUS_EXPR:		/* 15.17.2 Additive Operators (+ and -) for
				   Numeric Types */
      if (!JPRIMITIVE_TYPE_P (op1_type) || !JPRIMITIVE_TYPE_P (op2_type))
	{
	  if (!JPRIMITIVE_TYPE_P (op1_type))
	    ERROR_CANT_CONVERT_TO_NUMERIC (wfl_operator, node, op1_type);
	  if (!JPRIMITIVE_TYPE_P (op2_type) && (op1_type != op2_type))
	    ERROR_CANT_CONVERT_TO_NUMERIC (wfl_operator, node, op2_type);
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	  break;
	}
      prom_type = binary_numeric_promotion (op1_type, op2_type, &op1, &op2);
      break;

    /* 15.18 Shift Operators */
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case URSHIFT_EXPR:
      if (!JINTEGRAL_TYPE_P (op1_type) || !JINTEGRAL_TYPE_P (op2_type))
	{
	  if (!JINTEGRAL_TYPE_P (op1_type))
	    ERROR_CAST_NEEDED_TO_INTEGRAL (wfl_operator, node, op1_type);
	  else
	    parse_error_context 
	      (wfl_operator, (JPRIMITIVE_TYPE_P (op2_type) ? 
	       "Incompatible type for `%s'. Explicit cast needed to convert "
	       "shift distance from `%s' to integral" : 
	       "Incompatible type for `%s'. Can't convert shift distance from "
	       "`%s' to integral"), 
	       operator_string (node), lang_printable_name (op2_type, 0));
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	  break;
	}

      /* Unary numeric promotion (5.6.1) is performed on each operand
         separatly */
      op1 = do_unary_numeric_promotion (op1);
      op2 = do_unary_numeric_promotion (op2);

      /* The type of the shift expression is the type of the promoted
         type of the left-hand operand */
      prom_type = TREE_TYPE (op1);

      /* Shift int only up to 0x1f and long up to 0x3f */
      if (prom_type == int_type_node)
	op2 = fold (build (BIT_AND_EXPR, int_type_node, op2, 
			   build_int_2 (0x1f, 0)));
      else
	op2 = fold (build (BIT_AND_EXPR, int_type_node, op2, 
			   build_int_2 (0x3f, 0)));

      /* The >>> operator is a >> operating on unsigned quantities */
      if (code == URSHIFT_EXPR && ! flag_emit_class_files)
	{
	  tree to_return;
          tree utype = unsigned_type (prom_type);
          op1 = convert (utype, op1);
	  TREE_SET_CODE (node, RSHIFT_EXPR);
          TREE_OPERAND (node, 0) = op1;
          TREE_OPERAND (node, 1) = op2;
          TREE_TYPE (node) = utype;
	  to_return = convert (prom_type, node);
	  /* Copy the original value of the COMPOUND_ASSIGN_P flag */
	  COMPOUND_ASSIGN_P (to_return) = COMPOUND_ASSIGN_P (node);
	  TREE_SIDE_EFFECTS (to_return)
	    = TREE_SIDE_EFFECTS (op1) | TREE_SIDE_EFFECTS (op2);
	  return to_return;
	}
      break;

      /* 15.19.1 Type Comparison Operator instaceof */
    case INSTANCEOF_EXPR:

      TREE_TYPE (node) = boolean_type_node;

      if (!(op2_type = resolve_type_during_patch (op2)))
	return error_mark_node;

      /* The first operand must be a reference type or the null type */
      if (!JREFERENCE_TYPE_P (op1_type) && op1 != null_pointer_node)
	error_found = 1;	/* Error reported further below */

      /* The second operand must be a reference type */
      if (!JREFERENCE_TYPE_P (op2_type))
	{
	  SET_WFL_OPERATOR (wfl_operator, node, wfl_op2);
	  parse_error_context
	    (wfl_operator, "Invalid argument `%s' for `instanceof'",
	     lang_printable_name (op2_type, 0));
	  error_found = 1;
	}

      if (!error_found && valid_ref_assignconv_cast_p (op1_type, op2_type, 1))
	{
	  /* If the first operand is null, the result is always false */
	  if (op1 == null_pointer_node)
	    return boolean_false_node;
	  else if (flag_emit_class_files)
	    {
	      TREE_OPERAND (node, 1) = op2_type;
	      TREE_SIDE_EFFECTS (node) = TREE_SIDE_EFFECTS (op1);
	      return node;
	    }
	  /* Otherwise we have to invoke instance of to figure it out */
	  else
	    {
	      tree call =
		build (CALL_EXPR, boolean_type_node,
		       build_address_of (soft_instanceof_node),
		       tree_cons 
		       (NULL_TREE, op1,
			build_tree_list (NULL_TREE,
					 build_class_ref (op2_type))),
		       NULL_TREE);
	      TREE_SIDE_EFFECTS (call) = TREE_SIDE_EFFECTS (op1);
	      return call;
	    }
	}
      /* There is no way the expression operand can be an instance of
	 the type operand. This is a compile time error. */
      else
	{
	  char *t1 = strdup (lang_printable_name (op1_type, 0));
	  SET_WFL_OPERATOR (wfl_operator, node, wfl_op1);
	  parse_error_context 
	    (wfl_operator, "Impossible for `%s' to be instance of `%s'",
	     t1, lang_printable_name (op2_type, 0));
	  free (t1);
	  error_found = 1;
	}
      
      break;

      /* 15.21 Bitwise and Logical Operators */
    case BIT_AND_EXPR:
    case BIT_XOR_EXPR:
    case BIT_IOR_EXPR:
      if (JINTEGRAL_TYPE_P (op1_type) && JINTEGRAL_TYPE_P (op2_type))
	/* Binary numeric promotion is performed on both operand and the
	   expression retain that type */
	prom_type = binary_numeric_promotion (op1_type, op2_type, &op1, &op2);

      else if (TREE_CODE (op1_type) == BOOLEAN_TYPE 
	       && TREE_CODE (op1_type) == BOOLEAN_TYPE)
	/* The type of the bitwise operator expression is BOOLEAN */
	prom_type = boolean_type_node;
      else
	{
	  if (!JINTEGRAL_TYPE_P (op1_type))
	    ERROR_CAST_NEEDED_TO_INTEGRAL (wfl_operator, node, op1_type);
	  if (!JINTEGRAL_TYPE_P (op2_type) && (op1_type != op2_type))
	    ERROR_CAST_NEEDED_TO_INTEGRAL (wfl_operator, node, op2_type);
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	  /* Insert a break here if adding thing before the switch's
             break for this case */
	}
      break;

      /* 15.22 Conditional-And Operator */
    case TRUTH_ANDIF_EXPR:
      /* 15.23 Conditional-Or Operator */
    case TRUTH_ORIF_EXPR:
      /* Operands must be of BOOLEAN type */
      if (TREE_CODE (op1_type) != BOOLEAN_TYPE || 
	  TREE_CODE (op2_type) != BOOLEAN_TYPE)
	{
	  if (TREE_CODE (op1_type) != BOOLEAN_TYPE)
	    ERROR_CANT_CONVERT_TO_BOOLEAN (wfl_operator, node, op1_type);
	  if (TREE_CODE (op2_type) != BOOLEAN_TYPE && (op1_type != op2_type))
	    ERROR_CANT_CONVERT_TO_BOOLEAN (wfl_operator, node, op2_type);
	  TREE_TYPE (node) = boolean_type_node;
	  error_found = 1;
	  break;
	}
      /* The type of the conditional operators is BOOLEAN */
      prom_type = boolean_type_node;
      break;

      /* 15.19.1 Numerical Comparison Operators <, <=, >, >= */
    case LT_EXPR:
    case GT_EXPR:
    case LE_EXPR:
    case GE_EXPR:
      /* The type of each of the operands must be a primitive numeric
         type */
      if (!JNUMERIC_TYPE_P (op1_type) || ! JNUMERIC_TYPE_P (op2_type))
	{
	  if (!JNUMERIC_TYPE_P (op1_type))
	    ERROR_CANT_CONVERT_TO_NUMERIC (wfl_operator, node, op1_type);
	  if (!JNUMERIC_TYPE_P (op2_type) && (op1_type != op2_type))
	    ERROR_CANT_CONVERT_TO_NUMERIC (wfl_operator, node, op2_type);
	  TREE_TYPE (node) = boolean_type_node;
	  error_found = 1;
	  break;
	}
      /* Binary numeric promotion is performed on the operands */
      binary_numeric_promotion (op1_type, op2_type, &op1, &op2);
      /* The type of the relation expression is always BOOLEAN */
      prom_type = boolean_type_node;
      break;

      /* 15.20 Equality Operator */
    case EQ_EXPR:
    case NE_EXPR:
      /* 15.20.1 Numerical Equality Operators == and != */
      /* Binary numeric promotion is performed on the operands */
      if (JNUMERIC_TYPE_P (op1_type) && JNUMERIC_TYPE_P (op2_type))
	binary_numeric_promotion (op1_type, op2_type, &op1, &op2);
      
      /* 15.20.2 Boolean Equality Operators == and != */
      else if (TREE_CODE (op1_type) == BOOLEAN_TYPE &&
	  TREE_CODE (op2_type) == BOOLEAN_TYPE)
	;			/* Nothing to do here */
      
      /* 15.20.3 Reference Equality Operators == and != */
      /* Types have to be either references or the null type. If
         they're references, it must be possible to convert either
         type to the other by casting conversion. */
      else if (op1 == null_pointer_node || op2 == null_pointer_node 
	       || (JREFERENCE_TYPE_P (op1_type) && JREFERENCE_TYPE_P (op2_type)
		   && (valid_ref_assignconv_cast_p (op1_type, op2_type, 1)
		       || valid_ref_assignconv_cast_p (op2_type, 
						       op1_type, 1))))
	;			/* Nothing to do here */
	  
      /* Else we have an error figure what can't be converted into
	 what and report the error */
      else
	{
	  char *t1;
	  t1 = strdup (lang_printable_name (op1_type, 0));
	  parse_error_context 
	    (wfl_operator, "Incompatible type for `%s'. Can't convert `%s' "
	     "to `%s'", operator_string (node), t1, 
	     lang_printable_name (op2_type, 0));
	  free (t1);
	  TREE_TYPE (node) = boolean_type_node;
	  error_found = 1;
	  break;
	}
      prom_type = boolean_type_node;
      break;
    }

  if (error_found)
    return error_mark_node;

  TREE_OPERAND (node, 0) = op1;
  TREE_OPERAND (node, 1) = op2;
  TREE_TYPE (node) = prom_type;
  TREE_SIDE_EFFECTS (node) = TREE_SIDE_EFFECTS (op1) | TREE_SIDE_EFFECTS (op2);
  
  if (flag_emit_xref)
    return node;

  /* fold does not respect side-effect order as required for Java but not C.
   * Also, it sometimes create SAVE_EXPRs which are bad when emitting
   * bytecode.
   */
  if (flag_emit_class_files ? (TREE_CONSTANT (op1) && TREE_CONSTANT (op2))
      : ! TREE_SIDE_EFFECTS (node))
    node = fold (node);
  return node;
}

/* Concatenate the STRING_CST CSTE and STRING. When AFTER is a non
   zero value, the value of CSTE comes after the valude of STRING */

static tree
do_merge_string_cste (cste, string, string_len, after)
     tree cste;
     char *string;
     int string_len, after;
{
  int len = TREE_STRING_LENGTH (cste) + string_len;
  char *old = TREE_STRING_POINTER (cste);
  TREE_STRING_LENGTH (cste) = len;
  TREE_STRING_POINTER (cste) = obstack_alloc (expression_obstack, len+1);
  if (after)
    {
      strcpy (TREE_STRING_POINTER (cste), string);
      strcat (TREE_STRING_POINTER (cste), old);
    }
  else
    {
      strcpy (TREE_STRING_POINTER (cste), old);
      strcat (TREE_STRING_POINTER (cste), string);
    }
  return cste;
}

/* Tries to merge OP1 (a STRING_CST) and OP2 (if suitable). Return a
   new STRING_CST on success, NULL_TREE on failure */

static tree
merge_string_cste (op1, op2, after)
     tree op1, op2;
     int after;
{
  /* Handle two string constants right away */
  if (TREE_CODE (op2) == STRING_CST)
    return do_merge_string_cste (op1, TREE_STRING_POINTER (op2), 
				 TREE_STRING_LENGTH (op2), after);
  
  /* Reasonable integer constant can be treated right away */
  if (TREE_CODE (op2) == INTEGER_CST && !TREE_CONSTANT_OVERFLOW (op2))
    {
      static char *boolean_true = "true";
      static char *boolean_false = "false";
      static char *null_pointer = "null";
      char ch[3];
      char *string;
      
      if (op2 == boolean_true_node)
	string = boolean_true;
      else if (op2 == boolean_false_node)
	string = boolean_false;
      else if (op2 == null_pointer_node)
	string = null_pointer;
      else if (TREE_TYPE (op2) == char_type_node)
	{
	  ch[0] = (char )TREE_INT_CST_LOW (op2);
	  ch[1] = '\0';
	  string = ch;
	}
      else
	  string = print_int_node (op2);
      
      return do_merge_string_cste (op1, string, strlen (string), after);
    }
  return NULL_TREE;
}

/* Tries to statically concatenate OP1 and OP2 if possible. Either one
   has to be a STRING_CST and the other part must be a STRING_CST or a
   INTEGRAL constant. Return a new STRING_CST if the operation
   succeed, NULL_TREE otherwise.

   If the case we want to optimize for space, we might want to return
   NULL_TREE for each invocation of this routine. FIXME */

static tree
string_constant_concatenation (op1, op2)
     tree op1, op2;
{
  if (TREE_CODE (op1) == STRING_CST || (TREE_CODE (op2) == STRING_CST))
    {
      tree string, rest;
      int invert;
      
      string = (TREE_CODE (op1) == STRING_CST ? op1 : op2);
      rest   = (string == op1 ? op2 : op1);
      invert = (string == op1 ? 0 : 1 );
      
      /* Walk REST, only if it looks reasonable */
      if (TREE_CODE (rest) != STRING_CST
	  && !IS_CRAFTED_STRING_BUFFER_P (rest)
	  && !JSTRING_TYPE_P (TREE_TYPE (rest))
	  && TREE_CODE (rest) == EXPR_WITH_FILE_LOCATION)
	{
	  rest = java_complete_tree (rest);
	  if (rest == error_mark_node)
	    return error_mark_node;
	  rest = fold (rest);
	}
      return merge_string_cste (string, rest, invert);
    }
  return NULL_TREE;
}

/* Implement the `+' operator. Does static optimization if possible,
   otherwise create (if necessary) and append elements to a
   StringBuffer. The StringBuffer will be carried around until it is
   used for a function call or an assignment. Then toString() will be
   called on it to turn it into a String object. */

static tree
build_string_concatenation (op1, op2)
     tree op1, op2;
{
  tree result;
  int side_effects = TREE_SIDE_EFFECTS (op1) | TREE_SIDE_EFFECTS (op2);

  if (flag_emit_xref)
    return build (PLUS_EXPR, string_type_node, op1, op2);
  
  /* Try to do some static optimization */
  if ((result = string_constant_concatenation (op1, op2)))
    return result;

  /* Discard empty strings on either side of the expression */
  if (TREE_CODE (op1) == STRING_CST && TREE_STRING_LENGTH (op1) == 0)
    {
      op1 = op2;
      op2 = NULL_TREE;
    }
  else if (TREE_CODE (op2) == STRING_CST && TREE_STRING_LENGTH (op2) == 0)
    op2 = NULL_TREE;

  /* If operands are string constant, turn then into object references */
  if (TREE_CODE (op1) == STRING_CST)
    op1 = patch_string_cst (op1);
  if (op2 && TREE_CODE (op2) == STRING_CST)
    op2 = patch_string_cst (op2);

  /* If either one of the constant is null and the other non null
     operand is a String object, return it. */
  if (JSTRING_TYPE_P (TREE_TYPE (op1)) && !op2)
    return op1;

  /* If OP1 isn't already a StringBuffer, create and
     initialize a new one */
  if (!IS_CRAFTED_STRING_BUFFER_P (op1))
    {
      /* Two solutions here: 
	 1) OP1 is a string reference, we call new StringBuffer(OP1)
	 2) OP1 is something else, we call new StringBuffer().append(OP1). */
      if (JSTRING_TYPE_P (TREE_TYPE (op1)))
	op1 = BUILD_STRING_BUFFER (op1);
      else
	{
	  tree aNew = BUILD_STRING_BUFFER (NULL_TREE);
	  op1 = make_qualified_primary (aNew, BUILD_APPEND (op1), 0);
	}
    }

  if (op2)
    {
      /* OP1 is no longer the last node holding a crafted StringBuffer */
      IS_CRAFTED_STRING_BUFFER_P (op1) = 0;
      /* Create a node for `{new...,xxx}.append (op2)' */
      if (op2)
	op1 = make_qualified_primary (op1, BUILD_APPEND (op2), 0);
    }

  /* Mark the last node holding a crafted StringBuffer */
  IS_CRAFTED_STRING_BUFFER_P (op1) = 1;

  TREE_SIDE_EFFECTS (op1) = side_effects;
  return op1;
}

/* Patch the string node NODE. NODE can be a STRING_CST of a crafted
   StringBuffer. If no string were found to be patched, return
   NULL. */

static tree
patch_string (node)
    tree node;
{
  if (node == error_mark_node)
    return error_mark_node;
  if (TREE_CODE (node) == STRING_CST)
    return patch_string_cst (node);
  else if (IS_CRAFTED_STRING_BUFFER_P (node))
    {
      int saved = ctxp->explicit_constructor_p;
      tree invoke = build_method_invocation (wfl_to_string, NULL_TREE);
      tree ret;
      /* Temporary disable forbid the use of `this'. */
      ctxp->explicit_constructor_p = 0;
      ret = java_complete_tree (make_qualified_primary (node, invoke, 0));
      /* Restore it at its previous value */
      ctxp->explicit_constructor_p = saved;
      return ret;
    }
  return NULL_TREE;
}

/* Build the internal representation of a string constant.  */

static tree
patch_string_cst (node)
     tree node;
{
  int location;
  if (! flag_emit_class_files)
    {
      push_obstacks (&permanent_obstack, &permanent_obstack);
      node = get_identifier (TREE_STRING_POINTER (node));
      location = alloc_name_constant (CONSTANT_String, node);
      node = build_ref_from_constant_pool (location);
    }
  TREE_TYPE (node) = string_ptr_type_node;
  TREE_CONSTANT (node) = 1;
  return node;
}

/* Build an incomplete unary operator expression. */

static tree
build_unaryop (op_token, op_location, op1)
     int op_token, op_location;
     tree op1;
{
  enum tree_code op;
  tree unaryop;
  switch (op_token)
    {
    case PLUS_TK: op = UNARY_PLUS_EXPR; break;
    case MINUS_TK: op = NEGATE_EXPR; break;
    case NEG_TK: op = TRUTH_NOT_EXPR; break;
    case NOT_TK: op = BIT_NOT_EXPR; break;
    default: fatal ("Unknown token `%d' for unary operator - build_unaryop",
		    op_token);
    }

  unaryop = build1 (op, NULL_TREE, op1);
  TREE_SIDE_EFFECTS (unaryop) = 1;
  /* Store the location of the operator, for better error report. The
     string of the operator will be rebuild based on the OP value. */
  EXPR_WFL_LINECOL (unaryop) = op_location;
  return unaryop;
}

/* Special case for the ++/-- operators, since they require an extra
   argument to build, which is set to NULL and patched
   later. IS_POST_P is 1 if the operator, 0 otherwise.  */

static tree
build_incdec (op_token, op_location, op1, is_post_p)
     int op_token, op_location;
     tree op1;
     int is_post_p;
{
  static enum tree_code lookup [2][2] = 
    {
      { PREDECREMENT_EXPR, PREINCREMENT_EXPR, },
      { POSTDECREMENT_EXPR, POSTINCREMENT_EXPR, },
    };
  tree node = build (lookup [is_post_p][(op_token - DECR_TK)], 
		     NULL_TREE, op1, NULL_TREE);
  TREE_SIDE_EFFECTS (node) = 1;
  /* Store the location of the operator, for better error report. The
     string of the operator will be rebuild based on the OP value. */
  EXPR_WFL_LINECOL (node) = op_location;
  return node;
}     

/* Build an incomplete cast operator, based on the use of the
   CONVERT_EXPR. Note that TREE_TYPE of the constructed node is
   set. java_complete_tree is trained to walk a CONVERT_EXPR even
   though its type is already set.  */

static tree
build_cast (location, type, exp)
     int location;
     tree type, exp;
{
  tree node = build1 (CONVERT_EXPR, type, exp);
  EXPR_WFL_LINECOL (node) = location;
  return node;
}

/* 15.14 Unary operators. We return error_mark_node in case of error,
   but preserve the type of NODE if the type is fixed.  */

static tree
patch_unaryop (node, wfl_op)
     tree node;
     tree wfl_op;
{
  tree op = TREE_OPERAND (node, 0);
  tree op_type = TREE_TYPE (op);
  tree prom_type = NULL_TREE, value, decl;
  int code = TREE_CODE (node);
  int error_found = 0;

  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);

  switch (code)
    {
      /* 15.13.2 Postfix Increment Operator ++ */
    case POSTINCREMENT_EXPR:
      /* 15.13.3 Postfix Increment Operator -- */
    case POSTDECREMENT_EXPR:
      /* 15.14.1 Prefix Increment Operator ++ */
    case PREINCREMENT_EXPR:
      /* 15.14.2 Prefix Decrement Operator -- */
    case PREDECREMENT_EXPR:
      decl = strip_out_static_field_access_decl (op);
      /* We really should have a JAVA_ARRAY_EXPR to avoid this */
      if (!JDECL_P (decl) 
	  && TREE_CODE (decl) != COMPONENT_REF
	  && !(flag_emit_class_files && TREE_CODE (decl) == ARRAY_REF)
	  && TREE_CODE (decl) != INDIRECT_REF
	  && !(TREE_CODE (decl) == COMPOUND_EXPR
	       && TREE_OPERAND (decl, 1)
	       && (TREE_CODE (TREE_OPERAND (decl, 1)) == INDIRECT_REF)))
	{
	  tree lvalue;
	  /* Before screaming, check that we're not in fact trying to
	     increment a optimized static final access, in which case
	     we issue an different error message. */
	  if (!(TREE_CODE (wfl_op) == EXPR_WITH_FILE_LOCATION
		&& resolve_expression_name (wfl_op, &lvalue)
		&& check_final_assignment (lvalue, wfl_op)))
	    parse_error_context (wfl_operator, "Invalid argument to `%s'",
				 operator_string (node));
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	}
      else if (check_final_assignment (op, wfl_op))
	error_found = 1;

      /* From now on, we know that op if a variable and that it has a
         valid wfl. We use wfl_op to locate errors related to the
         ++/-- operand. */
      else if (!JNUMERIC_TYPE_P (op_type))
	{
	  parse_error_context
	    (wfl_op, "Invalid argument type `%s' to `%s'",
	     lang_printable_name (op_type, 0), operator_string (node));
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	}
      else
	{
	  /* Before the addition, binary numeric promotion is performed on
	     both operands */
	  value = build_int_2 (1, 0);
	  TREE_TYPE (node) = 
	    binary_numeric_promotion (op_type, TREE_TYPE (value), &op, &value);
	  /* And write the promoted incremented and increment */
	  TREE_OPERAND (node, 0) = op;
	  TREE_OPERAND (node, 1) = value;
	  /* Convert the overall back into its original type. */
	  return fold (convert (op_type, node));
	}
      break;

      /* 15.14.3 Unary Plus Operator + */
    case UNARY_PLUS_EXPR:
      /* 15.14.4 Unary Minus Operator - */
    case NEGATE_EXPR:
      if (!JNUMERIC_TYPE_P (op_type))
	{
	  ERROR_CANT_CONVERT_TO_NUMERIC (wfl_operator, node, op_type);
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	}
      /* Unary numeric promotion is performed on operand */
      else
	{
	  op = do_unary_numeric_promotion (op);
	  prom_type = TREE_TYPE (op);
	  if (code == UNARY_PLUS_EXPR)
	    return fold (op);
	}
      break;

      /* 15.14.5 Bitwise Complement Operator ~ */
    case BIT_NOT_EXPR:
      if (!JINTEGRAL_TYPE_P (op_type))
	{
	  ERROR_CAST_NEEDED_TO_INTEGRAL (wfl_operator, node, op_type);
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	}
      else
	{
	  op = do_unary_numeric_promotion (op);
	  prom_type = TREE_TYPE (op);
	}
      break;

      /* 15.14.6 Logical Complement Operator ! */
    case TRUTH_NOT_EXPR:
      if (TREE_CODE (op_type) != BOOLEAN_TYPE)
	{
	  ERROR_CANT_CONVERT_TO_BOOLEAN (wfl_operator, node, op_type);
	  /* But the type is known. We will report an error if further
	     attempt of a assignment is made with this rhs */
	  TREE_TYPE (node) = boolean_type_node;
	  error_found = 1;
	}
      else
	prom_type = boolean_type_node;
      break;

      /* 15.15 Cast Expression */
    case CONVERT_EXPR:
      value = patch_cast (node, wfl_operator);
      if (value == error_mark_node)
	{
	  /* If this cast is part of an assignment, we tell the code
	     that deals with it not to complain about a mismatch,
	     because things have been cast, anyways */
	  TREE_TYPE (node) = error_mark_node;
	  error_found = 1;
	}
      else
	{
	  value = fold (value);
	  TREE_SIDE_EFFECTS (value) = TREE_SIDE_EFFECTS (op);
	  return value;
	}
      break;
    }
  
  if (error_found)
    return error_mark_node;

  /* There are cases where node has been replaced by something else
     and we don't end up returning here: UNARY_PLUS_EXPR,
     CONVERT_EXPR, {POST,PRE}{INCR,DECR}EMENT_EXPR. */
  TREE_OPERAND (node, 0) = fold (op);
  TREE_TYPE (node) = prom_type;
  TREE_SIDE_EFFECTS (node) = TREE_SIDE_EFFECTS (op);
  return fold (node);
}

/* Generic type resolution that sometimes takes place during node
   patching. Returned the resolved type or generate an error
   message. Return the resolved type or NULL_TREE.  */

static tree
resolve_type_during_patch (type)
     tree type;
{
  if (unresolved_type_p (type, NULL))
    {
      tree type_decl = resolve_no_layout (EXPR_WFL_NODE (type), NULL_TREE);
      if (!type_decl)
	{
	  parse_error_context (type, 
			       "Class `%s' not found in type declaration",
			       IDENTIFIER_POINTER (EXPR_WFL_NODE (type)));
	  return NULL_TREE;
	}
      else
	{
	  CLASS_LOADED_P (TREE_TYPE (type_decl)) = 1;
	  return TREE_TYPE (type_decl);
	}
    }
  return type;
}
/* 5.5 Casting Conversion. error_mark_node is returned if an error is
   found. Otherwise NODE or something meant to replace it is returned.  */

static tree
patch_cast (node, wfl_operator)
     tree node;
     tree wfl_operator;
{
  tree op = TREE_OPERAND (node, 0);
  tree op_type = TREE_TYPE (op);
  tree cast_type = TREE_TYPE (node);
  char *t1;

  /* First resolve OP_TYPE if unresolved */
  if (!(cast_type = resolve_type_during_patch (cast_type)))
    return error_mark_node;

  /* Check on cast that are proven correct at compile time */
  if (JNUMERIC_TYPE_P (cast_type) && JNUMERIC_TYPE_P (op_type))
    {
      static tree convert_narrow ();
      /* Same type */
      if (cast_type == op_type)
	return node;

      /* float and double type are converted to the original type main
	 variant and then to the target type. */
      if (JFLOAT_TYPE_P (op_type) && TREE_CODE (cast_type) == CHAR_TYPE)
	op = convert (integer_type_node, op);

      /* Try widening/narowwing convertion. Potentially, things need
	 to be worked out in gcc so we implement the extreme cases
	 correctly. fold_convert() needs to be fixed. */
      return convert (cast_type, op);
    }

  /* It's also valid to cast a boolean into a boolean */
  if (op_type == boolean_type_node && cast_type == boolean_type_node)
    return node;

  /* null can be casted to references */
  if (op == null_pointer_node && JREFERENCE_TYPE_P (cast_type))
    return build_null_of_type (cast_type);

  /* The remaining legal casts involve conversion between reference
     types. Check for their compile time correctness. */
  if (JREFERENCE_TYPE_P (op_type) && JREFERENCE_TYPE_P (cast_type) 
      && valid_ref_assignconv_cast_p (op_type, cast_type, 1))
    {
      TREE_TYPE (node) = promote_type (cast_type);
      /* Now, the case can be determined correct at compile time if
         OP_TYPE can be converted into CAST_TYPE by assignment
         conversion (5.2) */

      if (valid_ref_assignconv_cast_p (op_type, cast_type, 0))
	{
	  TREE_SET_CODE (node, NOP_EXPR);
	  return node;
	}

      if (flag_emit_class_files)
	{
	  TREE_SET_CODE (node, CONVERT_EXPR);
	  return node;
	}

      /* The cast requires a run-time check */
      return build (CALL_EXPR, promote_type (cast_type),
		    build_address_of (soft_checkcast_node),
		    tree_cons (NULL_TREE, build_class_ref (cast_type),
			       build_tree_list (NULL_TREE, op)),
		    NULL_TREE);
    }

  /* Any other casts are proven incorrect at compile time */
  t1 = strdup (lang_printable_name (op_type, 0));
  parse_error_context (wfl_operator, "Invalid cast from `%s' to `%s'",
		       t1, lang_printable_name (cast_type, 0));
  free (t1);
  return error_mark_node;
}

/* Build a null constant and give it the type TYPE.  */

static tree
build_null_of_type (type)
     tree type;
{
  tree node = build_int_2 (0, 0);
  TREE_TYPE (node) = promote_type (type);
  return node;
}

/* Build an ARRAY_REF incomplete tree node. Note that operand 1 isn't
   a list of indices. */
static tree
build_array_ref (location, array, index)
     int location;
     tree array, index;
{
  tree node = build (ARRAY_REF, NULL_TREE, array, index);
  EXPR_WFL_LINECOL (node) = location;
  return node;
}

/* 15.12 Array Access Expression */

static tree
patch_array_ref (node)
     tree node;
{
  tree array = TREE_OPERAND (node, 0);
  tree array_type  = TREE_TYPE (array);
  tree index = TREE_OPERAND (node, 1);
  tree index_type = TREE_TYPE (index);
  int error_found = 0;

  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);

  if (TREE_CODE (array_type) == POINTER_TYPE)
    array_type = TREE_TYPE (array_type);

  /* The array reference must be an array */
  if (!TYPE_ARRAY_P (array_type))
    {
      parse_error_context 
	(wfl_operator, "`[]' can only be applied to arrays. It can't be "
	 "applied to `%s'", lang_printable_name (array_type, 0));
      TREE_TYPE (node) = error_mark_node;
      error_found = 1;
    }

  /* The array index underdoes unary numeric promotion. The promoted
     type must be int */
  index = do_unary_numeric_promotion (index);
  if (TREE_TYPE (index) != int_type_node)
    {
      int could_cast = valid_cast_to_p (index_type, int_type_node);
      parse_error_context 
	(wfl_operator, 
	 (could_cast ? "Incompatible type for `[]'. Explicit cast needed to "
	  "convert `%s' to `int'" : "Incompatible type for `[]'. "
	  "Can't convert `%s' to `int'"),
	 lang_printable_name (index_type, 0));
      TREE_TYPE (node) = error_mark_node;
      error_found = 1;
    }

  if (error_found)
    return error_mark_node;

  array_type = TYPE_ARRAY_ELEMENT (array_type);

  if (flag_emit_class_files || flag_emit_xref)
    {
      TREE_OPERAND (node, 0) = array;
      TREE_OPERAND (node, 1) = index;
    }
  else
    {
      /* The save_expr is for correct evaluation order.  It would be cleaner
	 to use force_evaluation_order (see comment there), but that is
	 difficult when we also have to deal with bounds checking. */
      if (TREE_SIDE_EFFECTS (index))
	array = save_expr (array);
      node = build_java_arrayaccess (array, array_type, index);
      if (TREE_SIDE_EFFECTS (index))
	node = build (COMPOUND_EXPR, array_type, array, node);
    }
  TREE_TYPE (node) = array_type;
  return node;
}

/* 15.9 Array Creation Expressions */

static tree
build_newarray_node (type, dims, extra_dims)
     tree type;
     tree dims;
     int extra_dims;
{
  tree node =
    build (NEW_ARRAY_EXPR, NULL_TREE, type, nreverse (dims), 
	   build_int_2 (extra_dims, 0));
  return node;
}

static tree
patch_newarray (node)
     tree node;
{
  tree type = TREE_OPERAND (node, 0);
  tree dims = TREE_OPERAND (node, 1);
  tree cdim, array_type;
  int error_found = 0;
  int ndims = 0;
  int xdims = TREE_INT_CST_LOW (TREE_OPERAND (node, 2));

  /* Dimension types are verified. It's better for the types to be
     verified in order. */
  for (cdim = dims, ndims = 0; cdim; cdim = TREE_CHAIN (cdim), ndims++ )
    {
      int dim_error = 0;
      tree dim = TREE_VALUE (cdim);

      /* Dim might have been saved during its evaluation */
      dim = (TREE_CODE (dim) == SAVE_EXPR ? dim = TREE_OPERAND (dim, 0) : dim);

      /* The type of each specified dimension must be an integral type. */
      if (!JINTEGRAL_TYPE_P (TREE_TYPE (dim)))
	dim_error = 1;

      /* Each expression undergoes an unary numeric promotion (5.6.1) and the
	 promoted type must be int. */
      else
	{
	  dim = do_unary_numeric_promotion (dim);
	  if (TREE_TYPE (dim) != int_type_node)
	    dim_error = 1;
	}

      /* Report errors on types here */
      if (dim_error)
	{
	  parse_error_context 
	    (TREE_PURPOSE (cdim), 
	     "Incompatible type for dimension in array creation expression. "
	     "%s convert `%s' to `int'", 
	     (valid_cast_to_p (TREE_TYPE (dim), int_type_node) ?
	      "Explicit cast needed to" : "Can't"),
	     lang_printable_name (TREE_TYPE (dim), 0));
	  error_found = 1;
	}

      TREE_PURPOSE (cdim) = NULL_TREE;
    }

  /* Resolve array base type if unresolved */
  if (!(type = resolve_type_during_patch (type)))
    error_found = 1;

  if (error_found)
    {
      /* We don't want further evaluation of this bogus array creation
         operation */
      TREE_TYPE (node) = error_mark_node;
      return error_mark_node;
    }

  /* Set array_type to the actual (promoted) array type of the result. */
  if (TREE_CODE (type) == RECORD_TYPE)
    type = build_pointer_type (type);
  while (--xdims >= 0)
    {
      type = promote_type (build_java_array_type (type, -1));
    }
  dims = nreverse (dims);
  array_type = type;
  for (cdim = dims; cdim; cdim = TREE_CHAIN (cdim))
    {
      type = array_type;
      array_type = build_java_array_type (type,
					  TREE_CODE (cdim) == INTEGER_CST ?
					  TREE_INT_CST_LOW (cdim) : -1);
      array_type = promote_type (array_type);
    }
  dims = nreverse (dims);

  /* The node is transformed into a function call. Things are done
     differently according to the number of dimensions. If the number
     of dimension is equal to 1, then the nature of the base type
     (primitive or not) matters. */
  if (ndims == 1)
    return build_new_array (type, TREE_VALUE (dims));
  
  /* Can't reuse what's already written in expr.c because it uses the
     JVM stack representation. Provide a build_multianewarray. FIXME */
  return build (CALL_EXPR, array_type,
		build_address_of (soft_multianewarray_node),
		tree_cons (NULL_TREE, build_class_ref (TREE_TYPE (array_type)),
			   tree_cons (NULL_TREE, 
				      build_int_2 (ndims, 0), dims )),
		NULL_TREE);
}

/* 10.6 Array initializer.  */

/* Build a wfl for array element that don't have one, so we can
   pin-point errors.  */

static tree
maybe_build_array_element_wfl (node)
     tree node;
{
  if (TREE_CODE (node) != EXPR_WITH_FILE_LOCATION)
    return build_expr_wfl (NULL_TREE, ctxp->filename,
			   ctxp->elc.line, ctxp->elc.prev_col);
  else
    return NULL_TREE;
}

/* Build a NEW_ARRAY_INIT that features a CONSTRUCTOR node. This makes
   identification of initialized arrays easier to detect during walk
   and expansion.  */

static tree
build_new_array_init (location, values)
     int location;
     tree values;
{
  tree constructor = build (CONSTRUCTOR, NULL_TREE, NULL_TREE, values);
  tree to_return = build1 (NEW_ARRAY_INIT, NULL_TREE, constructor);
  EXPR_WFL_LINECOL (to_return) = location;
  return to_return;
}

/* Expand a NEW_ARRAY_INIT node. Return error_mark_node if an error
   occurred.  Otherwise return NODE after having set its type
   appropriately.  */

static tree
patch_new_array_init (type, node)
     tree type, node;
{
  int error_seen = 0;
  tree current, element_type;
  HOST_WIDE_INT length;
  int all_constant = 1;
  tree init = TREE_OPERAND (node, 0);

  if (TREE_CODE (type) != POINTER_TYPE || ! TYPE_ARRAY_P (TREE_TYPE (type)))
    {
      parse_error_context (node,
			   "Invalid array initializer for non-array type `%s'",
			   lang_printable_name (type, 1));
      return error_mark_node;
    }
  type = TREE_TYPE (type);
  element_type = TYPE_ARRAY_ELEMENT (type);

  CONSTRUCTOR_ELTS (init) = nreverse (CONSTRUCTOR_ELTS (init));

  for (length = 0, current = CONSTRUCTOR_ELTS (init);
       current;  length++, current = TREE_CHAIN (current))
    {
      tree elt = TREE_VALUE (current);
      if (elt == NULL_TREE || TREE_CODE (elt) != NEW_ARRAY_INIT)
	{
	  error_seen |= array_constructor_check_entry (element_type, current);
	  elt = TREE_VALUE (current);
	  /* When compiling to native code, STRING_CST is converted to
	     INDIRECT_REF, but still with a TREE_CONSTANT flag. */
	  if (! TREE_CONSTANT (elt) || TREE_CODE (elt) == INDIRECT_REF)
	    all_constant = 0;
	}
      else
	{
	  TREE_VALUE (current) = patch_new_array_init (element_type, elt);
	  TREE_PURPOSE (current) = NULL_TREE;
	  all_constant = 0;
	}
      if (elt && TREE_VALUE (elt) == error_mark_node)
	error_seen = 1;
    }

  if (error_seen)
    return error_mark_node;

  /* Create a new type. We can't reuse the one we have here by
     patching its dimension because it originally is of dimension -1
     hence reused by gcc. This would prevent triangular arrays. */
  type = build_java_array_type (element_type, length);
  TREE_TYPE (init) = TREE_TYPE (TREE_CHAIN (TREE_CHAIN (TYPE_FIELDS (type))));
  TREE_TYPE (node) = promote_type (type);
  TREE_CONSTANT (init) = all_constant;
  TREE_CONSTANT (node) = all_constant;
  return node;
}

/* Verify that one entry of the initializer element list can be
   assigned to the array base type. Report 1 if an error occurred, 0
   otherwise.  */

static int
array_constructor_check_entry (type, entry)
     tree type, entry;
{
  char *array_type_string = NULL;	/* For error reports */
  tree value, type_value, new_value, wfl_value, patched;
  int error_seen = 0;

  new_value = NULL_TREE;
  wfl_value = TREE_VALUE (entry);

  value = java_complete_tree (TREE_VALUE (entry));
  /* patch_string return error_mark_node if arg is error_mark_node */
  if ((patched = patch_string (value)))
    value = patched;
  if (value == error_mark_node)
    return 1;
  
  type_value = TREE_TYPE (value);
  
  /* At anytime, try_builtin_assignconv can report a warning on
     constant overflow during narrowing. */
  SET_WFL_OPERATOR (wfl_operator, TREE_PURPOSE (entry), wfl_value);
  new_value = try_builtin_assignconv (wfl_operator, type, value);
  if (!new_value && (new_value = try_reference_assignconv (type, value)))
    type_value = promote_type (type);
  
  /* Check and report errors */
  if (!new_value)
    {
      char *msg = (!valid_cast_to_p (type_value, type) ?
		   "Can't" : "Explicit cast needed to");
      if (!array_type_string)
	array_type_string = strdup (lang_printable_name (type, 1));
      parse_error_context 
	(wfl_operator, "Incompatible type for array. %s convert `%s' to `%s'",
	 msg, lang_printable_name (type_value, 1), array_type_string);
      error_seen = 1;
    }
  
  if (new_value)
    {
      new_value = maybe_build_primttype_type_ref (new_value, wfl_operator);
      TREE_VALUE (entry) = new_value;
    }

  if (array_type_string)
    free (array_type_string);

  TREE_PURPOSE (entry) = NULL_TREE;
  return error_seen;
}

static tree
build_this (location)
     int location;
{
  tree node = build_wfl_node (this_identifier_node);
  TREE_SET_CODE (node, THIS_EXPR);
  EXPR_WFL_LINECOL (node) = location;
  return node;
}

/* 14.15 The return statement. It builds a modify expression that
   assigns the returned value to the RESULT_DECL that hold the value
   to be returned. */

static tree
build_return (location, op)
     int location;
     tree op;
{
  tree node = build1 (RETURN_EXPR, NULL_TREE, op);
  EXPR_WFL_LINECOL (node) = location;
  node = build_debugable_stmt (location, node);
  return node;
}

static tree
patch_return (node)
     tree node;
{
  tree return_exp = TREE_OPERAND (node, 0);
  tree meth = current_function_decl;
  tree mtype = TREE_TYPE (TREE_TYPE (current_function_decl));
  int error_found = 0;

  TREE_TYPE (node) = error_mark_node;
  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);

  /* It's invalid to have a return value within a function that is
     declared with the keyword void or that is a constructor */
  if (return_exp && (mtype == void_type_node || DECL_CONSTRUCTOR_P (meth)))
    error_found = 1;

  /* It's invalid to use a return statement in a static block */
  if (IS_CLINIT (current_function_decl))
    error_found = 1;

  /* It's invalid to have a no return value within a function that
     isn't declared with the keyword `void' */
  if (!return_exp && (mtype != void_type_node && !DECL_CONSTRUCTOR_P (meth)))
    error_found = 2;

  if (error_found)
    {
      if (IS_CLINIT (current_function_decl))
	parse_error_context (wfl_operator,
			     "`return' inside static initializer.");

      else if (!DECL_CONSTRUCTOR_P (meth))
	{
	  char *t = strdup (lang_printable_name (mtype, 0));
	  parse_error_context (wfl_operator, 
			       "`return' with%s value from `%s %s'",
			       (error_found == 1 ? "" : "out"), 
			       t, lang_printable_name (meth, 0));
	  free (t);
	}
      else
	parse_error_context (wfl_operator, 
			     "`return' with value from constructor `%s'",
			     lang_printable_name (meth, 0));
      return error_mark_node;
    }

  /* If we have a return_exp, build a modify expression and expand
     it. Note: at that point, the assignment is declared valid, but we
     may want to carry some more hacks */
  if (return_exp)
    {
      tree exp = java_complete_tree (return_exp);
      tree modify, patched;

      /* If the function returned value and EXP are booleans, EXP has
      to be converted into the type of DECL_RESULT, which is integer
      (see complete_start_java_method) */
      if (TREE_TYPE (exp) == boolean_type_node &&
	  TREE_TYPE (TREE_TYPE (meth)) == boolean_type_node)
	exp = convert_to_integer (TREE_TYPE (DECL_RESULT (meth)), exp);

      /* `null' can be assigned to a function returning a reference */
      if (JREFERENCE_TYPE_P (TREE_TYPE (TREE_TYPE (meth))) &&
	  exp == null_pointer_node)
	exp = build_null_of_type (TREE_TYPE (TREE_TYPE (meth)));

      if ((patched = patch_string (exp)))
	exp = patched;
      
      modify = build (MODIFY_EXPR, NULL_TREE, DECL_RESULT (meth), exp);
      EXPR_WFL_LINECOL (modify) = EXPR_WFL_LINECOL (node);
      modify = java_complete_tree (modify);

      if (modify != error_mark_node)
	{
	  TREE_SIDE_EFFECTS (modify) = 1;
	  TREE_OPERAND (node, 0) = modify;
	}
      else
	return error_mark_node;
    }
  TREE_TYPE (node) = void_type_node;
  TREE_SIDE_EFFECTS (node) = 1;
  return node;
}

/* 14.8 The if Statement */

static tree
build_if_else_statement (location, expression, if_body, else_body)
     int location;
     tree expression, if_body, else_body;
{
  tree node;
  if (!else_body)
    else_body = empty_stmt_node;
  node = build (COND_EXPR, NULL_TREE, expression, if_body, else_body);
  EXPR_WFL_LINECOL (node) = location;
  node = build_debugable_stmt (location, node);
  return node;
}

static tree
patch_if_else_statement (node)
     tree node;
{
  tree expression = TREE_OPERAND (node, 0);

  TREE_TYPE (node) = error_mark_node;
  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);

  /* The type of expression must be boolean */
  if (TREE_TYPE (expression) != boolean_type_node
      && TREE_TYPE (expression) != promoted_boolean_type_node)
    {
      parse_error_context 
	(wfl_operator, 
	 "Incompatible type for `if'. Can't convert `%s' to `boolean'", 
	 lang_printable_name (TREE_TYPE (expression), 0));
      return error_mark_node;
    }
  
  TREE_TYPE (node) = void_type_node;
  TREE_SIDE_EFFECTS (node) = 1;
  CAN_COMPLETE_NORMALLY (node)
    = CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 1))
    | CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 2));
  return node;
}

/* 14.6 Labeled Statements */

/* Action taken when a lableled statement is parsed. a new
   LABELED_BLOCK_EXPR is created. No statement is attached to the
   label, yet.  LABEL can be NULL_TREE for artificially-generated blocks. */

static tree
build_labeled_block (location, label)
     int location;
     tree label;
{
  tree label_name ;
  tree label_decl, node;
  if (label == NULL_TREE || label == continue_identifier_node)
    label_name = label;
  else
    {
      label_name = merge_qualified_name (label_id, label);
      /* Issue an error if we try to reuse a label that was previously
	 declared */
      if (IDENTIFIER_LOCAL_VALUE (label_name))
	{
	  EXPR_WFL_LINECOL (wfl_operator) = location;
	  parse_error_context (wfl_operator, "Declaration of `%s' shadows "
			       "a previous label declaration",
			       IDENTIFIER_POINTER (label));
	  EXPR_WFL_LINECOL (wfl_operator) = 
	    EXPR_WFL_LINECOL (IDENTIFIER_LOCAL_VALUE (label_name));
	  parse_error_context (wfl_operator, "This is the location of the "
			       "previous declaration of label `%s'",
			       IDENTIFIER_POINTER (label));
	  java_error_count--;
	}
    }

  label_decl = create_label_decl (label_name);
  node = build (LABELED_BLOCK_EXPR, NULL_TREE, label_decl, NULL_TREE);
  EXPR_WFL_LINECOL (node) = location;
  TREE_SIDE_EFFECTS (node) = 1;
  return node;
}

/* A labeled statement LBE is attached a statement.  */

static tree
finish_labeled_statement (lbe, statement)
     tree lbe;			/* Labeled block expr */
     tree statement;
{
  /* In anyways, tie the loop to its statement */
  LABELED_BLOCK_BODY (lbe) = statement;
  pop_labeled_block ();
  POP_LABELED_BLOCK ();
  return lbe;
}

/* 14.10, 14.11, 14.12 Loop Statements */

/* Create an empty LOOP_EXPR and make it the last in the nested loop
   list. */

static tree
build_new_loop (loop_body)
     tree loop_body;
{
  tree loop =  build (LOOP_EXPR, NULL_TREE, loop_body);
  TREE_SIDE_EFFECTS (loop) = 1;
  PUSH_LOOP (loop);
  return loop;
}

/* Create a loop body according to the following structure:
     COMPOUND_EXPR
       COMPOUND_EXPR		(loop main body)
         EXIT_EXPR		(this order is for while/for loops.
         LABELED_BLOCK_EXPR      the order is reversed for do loops)
           LABEL_DECL           (a continue occuring here branches at the 
           BODY			 end of this labeled block)
       INCREMENT		(if any)

  REVERSED, if non zero, tells that the loop condition expr comes
  after the body, like in the do-while loop.

  To obtain a loop, the loop body structure described above is
  encapsulated within a LOOP_EXPR surrounded by a LABELED_BLOCK_EXPR:

   LABELED_BLOCK_EXPR
     LABEL_DECL                   (use this label to exit the loop)
     LOOP_EXPR
       <structure described above> */

static tree
build_loop_body (location, condition, reversed)
     int location;
     tree condition;
     int reversed;
{
  tree first, second, body;

  condition = build (EXIT_EXPR, NULL_TREE, condition); /* Force walk */
  EXPR_WFL_LINECOL (condition) = location; /* For accurate error report */
  condition = build_debugable_stmt (location, condition);
  TREE_SIDE_EFFECTS (condition) = 1;

  body = build_labeled_block (0, continue_identifier_node);
  first = (reversed ? body : condition);
  second = (reversed ? condition : body);
  return 
    build (COMPOUND_EXPR, NULL_TREE, 
	   build (COMPOUND_EXPR, NULL_TREE, first, second), empty_stmt_node);
}

/* Install CONDITION (if any) and loop BODY (using REVERSED to tell
   their order) on the current loop. Unlink the current loop from the
   loop list.  */

static tree
finish_loop_body (location, condition, body, reversed)
     int location;
     tree condition, body;
     int reversed;
{
  tree to_return = ctxp->current_loop;
  tree loop_body = LOOP_EXPR_BODY (to_return);
  if (condition)
    {
      tree cnode = LOOP_EXPR_BODY_CONDITION_EXPR (loop_body, reversed);
      /* We wrapped the EXIT_EXPR around a WFL so we can debug it.
         The real EXIT_EXPR is one operand further. */
      EXPR_WFL_LINECOL (cnode) = location;
      /* This one is for accurate error reports */
      EXPR_WFL_LINECOL (TREE_OPERAND (cnode, 0)) = location;
      TREE_OPERAND (TREE_OPERAND (cnode, 0), 0) = condition;
    }
  LOOP_EXPR_BODY_BODY_EXPR (loop_body, reversed) = body;
  POP_LOOP ();
  return to_return;
}

/* Tailored version of finish_loop_body for FOR loops, when FOR
   loops feature the condition part */

static tree
finish_for_loop (location, condition, update, body)
    int location;
    tree condition, update, body;
{
  /* Put the condition and the loop body in place */
  tree loop = finish_loop_body (location, condition, body, 0);
  /* LOOP is the current loop which has been now popped of the loop
     stack. Install the update block */
  LOOP_EXPR_BODY_UPDATE_BLOCK (LOOP_EXPR_BODY (loop)) = update;
  return loop;
}

/* If the loop isn't surrounded by a labeled statement, create one and
   insert LOOP as its body.  */

static tree
patch_loop_statement (loop)
     tree loop;
{
  tree loop_label;
  tree block = ctxp->current_labeled_block;
  TREE_TYPE (loop) = void_type_node;
  if (block != NULL_TREE)
    {
      tree block_body = LABELED_BLOCK_BODY (block);
      if (IS_FOR_LOOP_P (loop))
	{
	  if (TREE_CODE (block_body) == BLOCK)
	    {
	      block_body = BLOCK_EXPR_BODY (block_body);
	      if (block_body == loop
		  || (TREE_CODE (block_body) == COMPOUND_EXPR
		      && TREE_OPERAND (block_body, 1) == loop))
		return loop;
	    }
	}
      else
	{
	  if (block_body == loop)
	    return loop;
	}
    }
  loop_label = build_labeled_block (0, NULL_TREE);
  LABELED_BLOCK_BODY (loop_label) = loop;
  PUSH_LABELED_BLOCK (loop_label);
  loop = loop_label;
  return loop;
}

/* 14.13, 14.14: break and continue Statements */

/* Build a break or a continue statement. a null NAME indicates an
   unlabeled break/continue statement.  */

static tree
build_bc_statement (location, is_break, name)
     int location, is_break;
     tree name;
{
  tree break_continue, label_block_expr = NULL_TREE;

  if (name)
    {
      if (!(label_block_expr = IDENTIFIER_LOCAL_VALUE 
	    (merge_qualified_name (label_id, EXPR_WFL_NODE (name)))))
	/* Null means that we don't have a target for this named
	   break/continue. In this case, we make the target to be the
	   label name, so that the error can be reported accuratly in
	   patch_bc_statement. */
	label_block_expr = EXPR_WFL_NODE (name);
    }
  /* Unlabeled break/continue will be handled during the
     break/continue patch operation */
  break_continue 
    = build (EXIT_BLOCK_EXPR, NULL_TREE, label_block_expr, NULL_TREE);

  IS_BREAK_STMT_P (break_continue) = is_break;
  TREE_SIDE_EFFECTS (break_continue) = 1;
  EXPR_WFL_LINECOL (break_continue) = location;
  break_continue = build_debugable_stmt (location, break_continue);
  return break_continue;
}

/* Verification of a break/continue statement. */

static tree
patch_bc_statement (node)
     tree node;
{
  tree bc_label = EXIT_BLOCK_LABELED_BLOCK (node), target_stmt;
  tree labeled_block = ctxp->current_labeled_block;
  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);
 
  /* Having an identifier here means that the target is unknown. */
  if (bc_label != NULL_TREE && TREE_CODE (bc_label) == IDENTIFIER_NODE)
    {
      parse_error_context (wfl_operator, "No label definition found for `%s'",
			   IDENTIFIER_POINTER (bc_label));
      return error_mark_node;
    }
  if (! IS_BREAK_STMT_P (node))
    {
      /* It's a continue statement. */
      for (;; labeled_block = TREE_CHAIN (labeled_block))
	{
	  if (labeled_block == NULL_TREE)
	    {
	      if (bc_label == NULL_TREE)
		parse_error_context (wfl_operator,
				     "`continue' must be in loop");
	      else
		parse_error_context 
		  (wfl_operator, "continue label `%s' does not name a loop",
		   IDENTIFIER_POINTER (bc_label));
	      return error_mark_node;
	    }
	  if ((DECL_NAME (LABELED_BLOCK_LABEL (labeled_block))
	       == continue_identifier_node)
	      && (bc_label == NULL_TREE
		  || TREE_CHAIN (labeled_block) == bc_label))
	    {
	      bc_label = labeled_block;
	      break;
	    }
	}
    }
  else if (!bc_label)
    { 
      for (;; labeled_block = TREE_CHAIN (labeled_block))
	{
	  if (labeled_block == NULL_TREE)
	    {
	      parse_error_context (wfl_operator,
				     "`break' must be in loop or switch");
	      return error_mark_node;
	    }
	  target_stmt = LABELED_BLOCK_BODY (labeled_block);
	  if (TREE_CODE (target_stmt) == SWITCH_EXPR
	      || TREE_CODE (target_stmt) == LOOP_EXPR)
	    {
	      bc_label = labeled_block;
	      break;
	    }
	}
    }

  EXIT_BLOCK_LABELED_BLOCK (node) = bc_label;
  CAN_COMPLETE_NORMALLY (bc_label) = 1;

  /* Our break/continue don't return values. */
  TREE_TYPE (node) = void_type_node;
  /* Encapsulate the break within a compound statement so that it's
     expanded all the times by expand_expr (and not clobered
     sometimes, like after a if statement) */
  node = add_stmt_to_compound (NULL_TREE, void_type_node, node);
  TREE_SIDE_EFFECTS (node) = 1;
  return node;
}

/* Process the exit expression belonging to a loop. Its type must be
   boolean.  */

static tree
patch_exit_expr (node)
     tree node;
{
  tree expression = TREE_OPERAND (node, 0);
  TREE_TYPE (node) = error_mark_node;
  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);

  /* The type of expression must be boolean */
  if (TREE_TYPE (expression) != boolean_type_node)
    {
      parse_error_context 
	(wfl_operator, 
	 "Incompatible type for loop conditional. Can't convert `%s' to "
	 "`boolean'", 
	 lang_printable_name (TREE_TYPE (expression), 0));
      return error_mark_node;
    }
  /* Now we know things are allright, invert the condition, fold and
     return */
  TREE_OPERAND (node, 0) = 
    fold (build1 (TRUTH_NOT_EXPR, boolean_type_node, expression));

  if (! integer_zerop (TREE_OPERAND (node, 0))
      && ctxp->current_loop != NULL_TREE
      && TREE_CODE (ctxp->current_loop) == LOOP_EXPR)
    CAN_COMPLETE_NORMALLY (ctxp->current_loop) = 1;
  if (! integer_onep (TREE_OPERAND (node, 0)))
    CAN_COMPLETE_NORMALLY (node) = 1;


  TREE_TYPE (node) = void_type_node;
  return node;
}

/* 14.9 Switch statement */

static tree
patch_switch_statement (node)
     tree node;
{
  tree se = TREE_OPERAND (node, 0), se_type;

  /* Complete the switch expression */
  se = TREE_OPERAND (node, 0) = java_complete_tree (se);
  se_type = TREE_TYPE (se);
  /* The type of the switch expression must be char, byte, short or
     int */
  if (!JINTEGRAL_TYPE_P (se_type))
    {
      EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (node);
      parse_error_context (wfl_operator, "Incompatible type for `switch'. "
			   "Can't convert `%s' to `int'",
			   lang_printable_name (se_type, 0));
      /* This is what java_complete_tree will check */
      TREE_OPERAND (node, 0) = error_mark_node;
      return error_mark_node;
    }

  TREE_OPERAND (node, 1) = java_complete_tree (TREE_OPERAND (node, 1));

  /* Ready to return */
  if (TREE_CODE (TREE_OPERAND (node, 1)) == ERROR_MARK)
    {
      TREE_TYPE (node) = error_mark_node;
      return error_mark_node;
    }
  TREE_TYPE (node) = void_type_node;
  TREE_SIDE_EFFECTS (node) = 1;
  CAN_COMPLETE_NORMALLY (node)
    = CAN_COMPLETE_NORMALLY (TREE_OPERAND (node, 1)) 
      || ! SWITCH_HAS_DEFAULT (node);
  return node;
}

/* 14.18 The try statement */

static tree
build_try_statement (location, try_block, catches)
     int location;
     tree try_block, catches;
{
  tree node = build (TRY_EXPR, NULL_TREE, try_block, catches);
  EXPR_WFL_LINECOL (node) = location;
  return node;
}

static tree
build_try_finally_statement (location, try_block, finally)
     int location;
     tree try_block, finally;
{
  tree node = build (TRY_FINALLY_EXPR, NULL_TREE, try_block, finally);
  EXPR_WFL_LINECOL (node) = location;
  return node;
}

static tree
patch_try_statement (node)
     tree node;
{
  int error_found = 0;
  tree try = TREE_OPERAND (node, 0);
  /* Exception handlers are considered in left to right order */
  tree catch = nreverse (TREE_OPERAND (node, 1));
  tree current, caught_type_list = NULL_TREE;

  /* Check catch clauses, if any. Every time we find an error, we try
     to process the next catch clause. We process the catch clause before
     the try block so that when processing the try block we can check thrown
     exceptions againts the caught type list. */
  for (current = catch; current; current = TREE_CHAIN (current))
    {
      tree carg_decl, carg_type;
      tree sub_current, catch_block, catch_clause;
      int unreachable;

      /* At this point, the structure of the catch clause is
	   CATCH_EXPR		(catch node)
	     BLOCK	        (with the decl of the parameter)
               COMPOUND_EXPR
                 MODIFY_EXPR   (assignment of the catch parameter)
		 BLOCK	        (catch clause block)
       */
      catch_clause = TREE_OPERAND (current, 0);
      carg_decl = BLOCK_EXPR_DECLS (catch_clause);
      carg_type = TREE_TYPE (TREE_TYPE (carg_decl));

      /* Catch clauses can't have more than one parameter declared,
	 but it's already enforced by the grammar. Make sure that the
	 only parameter of the clause statement in of class Throwable
	 or a subclass of Throwable, but that was done earlier. The
	 catch clause parameter type has also been resolved. */
      
      /* Just make sure that the catch clause parameter type inherits
	 from java.lang.Throwable */
      if (!inherits_from_p (carg_type, throwable_type_node))
	{
	  EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (current);
	  parse_error_context (wfl_operator,
			       "Can't catch class `%s'. Catch clause "
			       "parameter type must be a subclass of "
			       "class `java.lang.Throwable'",
			       lang_printable_name (carg_type, 0));
	  error_found = 1;
	  continue;
	}
      
      /* Partial check for unreachable catch statement: The catch
	 clause is reachable iff is no earlier catch block A in
	 the try statement such that the type of the catch
	 clause's parameter is the same as or a subclass of the
	 type of A's parameter */
      unreachable = 0;
      for (sub_current = catch;
	   sub_current != current; sub_current = TREE_CHAIN (sub_current))
	{
	  tree sub_catch_clause, decl;
	  sub_catch_clause = TREE_OPERAND (sub_current, 0);
	  decl = BLOCK_EXPR_DECLS (sub_catch_clause);

	  if (inherits_from_p (carg_type, TREE_TYPE (TREE_TYPE (decl))))
	    {
	      EXPR_WFL_LINECOL (wfl_operator) = EXPR_WFL_LINECOL (current);
	      parse_error_context 
		(wfl_operator, "`catch' not reached because of the catch "
		 "clause at line %d", EXPR_WFL_LINENO (sub_current));
	      unreachable = error_found = 1;
	      break;
	    }
	}
      /* Complete the catch clause block */
      catch_block = java_complete_tree (TREE_OPERAND (current, 0));
      if (catch_block == error_mark_node)
	{
	  error_found = 1;
	  continue;
	}
      if (CAN_COMPLETE_NORMALLY (catch_block))
	CAN_COMPLETE_NORMALLY (node) = 1;
      TREE_OPERAND (current, 0) = catch_block;

      if (unreachable)
	continue;

      /* Things to do here: the exception must be thrown */

      /* Link this type to the caught type list */
      caught_type_list = tree_cons (NULL_TREE, carg_type, caught_type_list);
    }

  PUSH_EXCEPTIONS (caught_type_list);
  if ((try = java_complete_tree (try)) == error_mark_node)
    error_found = 1;
  if (CAN_COMPLETE_NORMALLY (try))
    CAN_COMPLETE_NORMALLY (node) = 1;
  POP_EXCEPTIONS ();

  /* Verification ends here */
  if (error_found) 
    return error_mark_node;

  TREE_OPERAND (node, 0) = try;
  TREE_OPERAND (node, 1) = catch;
  TREE_TYPE (node) = void_type_node;
  return node;
}

/* 14.17 The synchronized Statement */

static tree
patch_synchronized_statement (node, wfl_op1)
    tree node, wfl_op1;
{
  tree expr = java_complete_tree (TREE_OPERAND (node, 0));
  tree block = TREE_OPERAND (node, 1);

  tree enter, exit, expr_decl, assignment;

  if (expr == error_mark_node)
    {
      block = java_complete_tree (block);
      return expr;
    }

  /* The TYPE of expr must be a reference type */
  if (!JREFERENCE_TYPE_P (TREE_TYPE (expr)))
    {
      SET_WFL_OPERATOR (wfl_operator, node, wfl_op1);
      parse_error_context (wfl_operator, "Incompatible type for `synchronized'"
			   ". Can't convert `%s' to `java.lang.Object'",
			   lang_printable_name (TREE_TYPE (expr), 0));
      return error_mark_node;
    }

  if (flag_emit_xref)
    {
      TREE_OPERAND (node, 0) = expr;
      TREE_OPERAND (node, 1) = java_complete_tree (block);
      CAN_COMPLETE_NORMALLY (node) = 1;
      return node;
    }

  /* Generate a try-finally for the synchronized statement, except
     that the handler that catches all throw exception calls
     _Jv_MonitorExit and then rethrow the exception.
     The synchronized statement is then implemented as:
     TRY 
       {
         _Jv_MonitorEnter (expression)
	 synchronized_block
         _Jv_MonitorExit (expression)
       }
     CATCH_ALL
       {
         e = _Jv_exception_info ();
	 _Jv_MonitorExit (expression)
	 Throw (e);
       } */

  expr_decl = build_decl (VAR_DECL, generate_name (), TREE_TYPE (expr));
  BUILD_MONITOR_ENTER (enter, expr_decl);
  BUILD_MONITOR_EXIT (exit, expr_decl);
  CAN_COMPLETE_NORMALLY (enter) = 1;
  CAN_COMPLETE_NORMALLY (exit) = 1;
  assignment = build (MODIFY_EXPR, NULL_TREE, expr_decl, expr);
  TREE_SIDE_EFFECTS (assignment) = 1;
  node = build1 (CLEANUP_POINT_EXPR, NULL_TREE,
		 build (COMPOUND_EXPR, NULL_TREE,
			build (WITH_CLEANUP_EXPR, NULL_TREE,
			       build (COMPOUND_EXPR, NULL_TREE,
				      assignment, enter),
			       NULL_TREE, exit),
			block));
  node = build_expr_block (node, expr_decl);

  return java_complete_tree (node);
}

/* 14.16 The throw Statement */

static tree
patch_throw_statement (node, wfl_op1)
    tree node, wfl_op1;
{
  tree expr = TREE_OPERAND (node, 0);
  tree type = TREE_TYPE (expr);
  int unchecked_ok = 0, tryblock_throws_ok = 0;

  /* Thrown expression must be assignable to java.lang.Throwable */
  if (!try_reference_assignconv (throwable_type_node, expr))
    {
      SET_WFL_OPERATOR (wfl_operator, node, wfl_op1);
      parse_error_context (wfl_operator, "Can't throw `%s'; it must be a "
			   "subclass of class `java.lang.Throwable'",
			   lang_printable_name (type, 0));
      /* If the thrown expression was a reference, we further the
         compile-time check. */
      if (!JREFERENCE_TYPE_P (type))
	return error_mark_node;
    }

  /* At least one of the following must be true */

  /* The type of the throw expression is a not checked exception,
     i.e. is a unchecked expression. */
  unchecked_ok = IS_UNCHECKED_EXCEPTION_P (TREE_TYPE (type));

  /* Throw is contained in a try statement and at least one catch
     clause can receive the thrown expression or the current method is
     declared to throw such an exception. Or, the throw statement is
     contained in a method or constructor declaration and the type of
     the Expression is assignable to at least one type listed in the
     throws clause the declaration. */
  SET_WFL_OPERATOR (wfl_operator, node, wfl_op1);
  if (!unchecked_ok)
    tryblock_throws_ok = check_thrown_exceptions_do (TREE_TYPE (expr));
  if (!(unchecked_ok || tryblock_throws_ok))
    {
      /* If there is a surrounding try block that has no matching
	 clatch clause, report it first. A surrounding try block exits
	 only if there is something after the list of checked
	 exception thrown by the current function (if any). */
      if (IN_TRY_BLOCK_P ())
	parse_error_context (wfl_operator, "Checked exception `%s' can't be "
			     "caught by any of the catch clause(s) "
			     "of the surrounding `try' block",
			     lang_printable_name (type, 0));
      /* If we have no surrounding try statement and the method doesn't have
	 any throws, report it now. FIXME */

      /* We report that the exception can't be throw from a try block
         in all circumstances but when the `throw' is inside a static
         block. */
      else if (!EXCEPTIONS_P (currently_caught_type_list) 
	       && !tryblock_throws_ok)
	{
	  if (IS_CLINIT (current_function_decl))
	    parse_error_context (wfl_operator, "Checked exception `%s' can't "
				 "be thrown in initializer",
				 lang_printable_name (type, 0));
	  else
	    parse_error_context (wfl_operator, "Checked exception `%s' isn't "
				 "thrown from a `try' block", 
				 lang_printable_name (type, 0));
	}
      /* Otherwise, the current method doesn't have the appropriate
         throws declaration */
      else
	parse_error_context (wfl_operator, "Checked exception `%s' doesn't "
			     "match any of current method's `throws' "
			     "declaration(s)", 
			     lang_printable_name (type, 0));
      return error_mark_node;
    }

  if (! flag_emit_class_files && ! flag_emit_xref)
    BUILD_THROW (node, expr);

  /* If doing xrefs, keep the location where the `throw' was seen. */
  if (flag_emit_xref)
    EXPR_WFL_LINECOL (node) = EXPR_WFL_LINECOL (wfl_op1);
  return node;
}

/* Check that exception said to be thrown by method DECL can be
   effectively caught from where DECL is invoked.  */

static void
check_thrown_exceptions (location, decl)
     int location;
     tree decl;
{
  tree throws;
  /* For all the unchecked exceptions thrown by DECL */
  for (throws = DECL_FUNCTION_THROWS (decl); throws; 
       throws = TREE_CHAIN (throws)) 
    if (!check_thrown_exceptions_do (TREE_VALUE (throws)))
      {
#if 1
	/* Temporary hack to suppresses errors about cloning arrays. FIXME */
	if (DECL_NAME (decl) == get_identifier ("clone"))
	  continue;
#endif
	EXPR_WFL_LINECOL (wfl_operator) = location;
	parse_error_context 
	  (wfl_operator, "Exception `%s' must be caught, or it must be "
	   "declared in the `throws' clause of `%s'", 
	   lang_printable_name (TREE_VALUE (throws), 0),
	   IDENTIFIER_POINTER (DECL_NAME (current_function_decl)));
      }
}

/* Return 1 if checked EXCEPTION is caught at the current nesting level of
   try-catch blocks, OR is listed in the `throws' clause of the
   current method.  */

static int
check_thrown_exceptions_do (exception)
     tree exception;
{
  tree list = currently_caught_type_list;
  resolve_and_layout (exception, NULL_TREE);
  /* First, all the nested try-catch-finally at that stage. The
     last element contains `throws' clause exceptions, if any. */
  if (IS_UNCHECKED_EXCEPTION_P (exception))
    return 1;
  while (list)
    {
      tree caught;
      for (caught = TREE_VALUE (list); caught; caught = TREE_CHAIN (caught))
	if (valid_ref_assignconv_cast_p (exception, TREE_VALUE (caught), 0))
	  return 1;
      list = TREE_CHAIN (list);
    }
  return 0;
}

static void
purge_unchecked_exceptions (mdecl)
     tree mdecl;
{
  tree throws = DECL_FUNCTION_THROWS (mdecl);
  tree new = NULL_TREE;

  while (throws)
    {
      tree next = TREE_CHAIN (throws);
      if (!IS_UNCHECKED_EXCEPTION_P (TREE_VALUE (throws)))
	{
	  TREE_CHAIN (throws) = new;
	  new = throws;
	}
      throws = next;
    }
  /* List is inverted here, but it doesn't matter */
  DECL_FUNCTION_THROWS (mdecl) = new;
}

/* 15.24 Conditional Operator ?: */

static tree
patch_conditional_expr (node, wfl_cond, wfl_op1)
     tree node, wfl_cond, wfl_op1;
{
  tree cond = TREE_OPERAND (node, 0);
  tree op1 = TREE_OPERAND (node, 1);
  tree op2 = TREE_OPERAND (node, 2);
  tree resulting_type = NULL_TREE;
  tree t1, t2, patched;
  int error_found = 0;

  /* Operands of ?: might be StringBuffers crafted as a result of a
     string concatenation. Obtain a descent operand here.  */
  if ((patched = patch_string (op1)))
    TREE_OPERAND (node, 1) = op1 = patched;
  if ((patched = patch_string (op2)))
    TREE_OPERAND (node, 2) = op2 = patched;

  t1 = TREE_TYPE (op1);
  t2 = TREE_TYPE (op2);

  /* The first expression must be a boolean */
  if (TREE_TYPE (cond) != boolean_type_node)
    {
      SET_WFL_OPERATOR (wfl_operator, node, wfl_cond);
      parse_error_context (wfl_operator, "Incompatible type for `?:'. Can't "
			   "convert `%s' to `boolean'",
			   lang_printable_name (TREE_TYPE (cond), 0));
      error_found = 1;
    }

  /* Second and third can be numeric, boolean (i.e. primitive),
     references or null. Anything else results in an error */
  if (!((JNUMERIC_TYPE_P (t1) && JNUMERIC_TYPE_P (t2))
	|| ((JREFERENCE_TYPE_P (t1) || op1 == null_pointer_node) 
	    && (JREFERENCE_TYPE_P (t2) || op2 == null_pointer_node))
	|| (t1 == boolean_type_node && t2 == boolean_type_node)))
    error_found = 1;

  /* Determine the type of the conditional expression. Same types are
     easy to deal with */
  else if (t1 == t2)
    resulting_type = t1;

  /* There are different rules for numeric types */
  else if (JNUMERIC_TYPE_P (t1))
    {
      /* if byte/short found, the resulting type is short */
      if ((t1 == byte_type_node && t2 == short_type_node)
	  || (t1 == short_type_node && t2 == byte_type_node))
	resulting_type = short_type_node;

      /* If t1 is a constant int and t2 is of type byte, short or char
	 and t1's value fits in t2, then the resulting type is t2 */
      else if ((t1 == int_type_node && TREE_CONSTANT (TREE_OPERAND (node, 1)))
	  && JBSC_TYPE_P (t2) && int_fits_type_p (TREE_OPERAND (node, 1), t2))
	resulting_type = t2;

      /* If t2 is a constant int and t1 is of type byte, short or char
	 and t2's value fits in t1, then the resulting type is t1 */
      else if ((t2 == int_type_node && TREE_CONSTANT (TREE_OPERAND (node, 2)))
	  && JBSC_TYPE_P (t1) && int_fits_type_p (TREE_OPERAND (node, 2), t1))
	resulting_type = t1;

      /* Otherwise, binary numeric promotion is applied and the
	 resulting type is the promoted type of operand 1 and 2 */
      else 
	resulting_type = binary_numeric_promotion (t1, t2, 
						   &TREE_OPERAND (node, 1), 
						   &TREE_OPERAND (node, 2));
    }

  /* Cases of a reference and a null type */
  else if (JREFERENCE_TYPE_P (t1) && op2 == null_pointer_node)
    resulting_type = t1;

  else if (JREFERENCE_TYPE_P (t2) && op1 == null_pointer_node)
    resulting_type = t2;

  /* Last case: different reference types. If a type can be converted
     into the other one by assignment conversion, the latter
     determines the type of the expression */
  else if ((resulting_type = try_reference_assignconv (t1, op2)))
    resulting_type = promote_type (t1);

  else if ((resulting_type = try_reference_assignconv (t2, op1)))
    resulting_type = promote_type (t2);

  /* If we don't have any resulting type, we're in trouble */
  if (!resulting_type)
    {
      char *t = strdup (lang_printable_name (t1, 0));
      SET_WFL_OPERATOR (wfl_operator, node, wfl_op1);
      parse_error_context (wfl_operator, "Incompatible type for `?:'. Can't "
			   "convert `%s' to `%s'", t,
			   lang_printable_name (t2, 0));
      free (t);
      error_found = 1;
    }

  if (error_found)
    {
      TREE_TYPE (node) = error_mark_node;
      return error_mark_node;
    }

  TREE_TYPE (node) = resulting_type;
  TREE_SET_CODE (node, COND_EXPR);
  CAN_COMPLETE_NORMALLY (node) = 1;
  return node;
}

/* Try to constant fold NODE.
   If NODE is not a constant expression, return NULL_EXPR.
   CONTEXT is a static final VAR_DECL whose initializer we are folding. */

static tree
fold_constant_for_init (node, context)
     tree node;
     tree context;
{
  tree op0, op1, val;
  enum tree_code code = TREE_CODE (node);

  if (code == STRING_CST)
    return node;

  if (code == INTEGER_CST || code == REAL_CST)
    return convert (TREE_TYPE (context), node);
  if (TREE_TYPE (node) != NULL_TREE && code != VAR_DECL)
    return NULL_TREE;

  switch (code)
    {
    case PLUS_EXPR:
    case MINUS_EXPR:
    case MULT_EXPR:
    case TRUNC_MOD_EXPR:
    case RDIV_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case URSHIFT_EXPR:
    case BIT_AND_EXPR:
    case BIT_XOR_EXPR:
    case BIT_IOR_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case EQ_EXPR: 
    case NE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case LE_EXPR:
      op0 = TREE_OPERAND (node, 0);
      op1 = TREE_OPERAND (node, 1);
      val = fold_constant_for_init (op0, context);
      if (val == NULL_TREE || ! TREE_CONSTANT (val))
	return NULL_TREE;
      TREE_OPERAND (node, 0) = val;
      val = fold_constant_for_init (op1, context);
      if (val == NULL_TREE || ! TREE_CONSTANT (val))
	return NULL_TREE;
      TREE_OPERAND (node, 1) = val;
      return patch_binop (node, op0, op1);

    case UNARY_PLUS_EXPR:
    case NEGATE_EXPR:
    case TRUTH_NOT_EXPR:
    case BIT_NOT_EXPR:
    case CONVERT_EXPR:
      op0 = TREE_OPERAND (node, 0);
      val = fold_constant_for_init (op0, context);
      if (val == NULL_TREE || ! TREE_CONSTANT (val))
	return NULL_TREE;
      TREE_OPERAND (node, 0) = val;
      return patch_unaryop (node, op0);
      break;

    case COND_EXPR:
      val = fold_constant_for_init (TREE_OPERAND (node, 0), context);
      if (val == NULL_TREE || ! TREE_CONSTANT (val))
	return NULL_TREE;
      TREE_OPERAND (node, 0) = val;
      val = fold_constant_for_init (TREE_OPERAND (node, 1), context);
      if (val == NULL_TREE || ! TREE_CONSTANT (val))
	return NULL_TREE;
      TREE_OPERAND (node, 1) = val;
      val = fold_constant_for_init (TREE_OPERAND (node, 2), context);
      if (val == NULL_TREE || ! TREE_CONSTANT (val))
	return NULL_TREE;
      TREE_OPERAND (node, 2) = val;
      return integer_zerop (TREE_OPERAND (node, 0)) ? TREE_OPERAND (node, 1)
	: TREE_OPERAND (node, 2);

    case VAR_DECL:
      if (! FIELD_STATIC (node) || ! FIELD_FINAL (node)
	  || DECL_INITIAL (node) == NULL_TREE)
	return NULL_TREE;
      val = DECL_INITIAL (node);
      /* Guard against infinite recursion. */
      DECL_INITIAL (node) = NULL_TREE;
      val = fold_constant_for_init (val, node);
      DECL_INITIAL (node) = val;
      return val;

    case EXPR_WITH_FILE_LOCATION:
      /* Compare java_complete_tree and resolve_expression_name. */
      if (!EXPR_WFL_NODE (node) /* Or a PRIMARY flag ? */
	  || TREE_CODE (EXPR_WFL_NODE (node)) == IDENTIFIER_NODE)
	{
	  tree name = EXPR_WFL_NODE (node);
	  tree decl;
	  if (PRIMARY_P (node))
	    return NULL_TREE;
	  else if (! QUALIFIED_P (name))
	    {
	      decl = lookup_field_wrapper (DECL_CONTEXT (context), name);
	      if (decl == NULL_TREE || ! FIELD_STATIC (decl))
		return NULL_TREE;
	      return fold_constant_for_init (decl, decl);
	    }
	  else
	    {
	      /* Wait until the USE_COMPONENT_REF re-write.  FIXME. */
	      qualify_ambiguous_name (node);
	      if (resolve_field_access (node, &decl, NULL)
		  && decl != NULL_TREE)
		return fold_constant_for_init (decl, decl);
	      return NULL_TREE;
	    }
	}
      else
	{
	  op0 = TREE_OPERAND (node, 0);
	  val = fold_constant_for_init (op0, context);
	  if (val == NULL_TREE || ! TREE_CONSTANT (val))
	    return NULL_TREE;
	  TREE_OPERAND (node, 0) = val;
	  return val;
	}

#ifdef USE_COMPONENT_REF
    case IDENTIFIER:
    case COMPONENT_REF:
      ?;
#endif

    default:
      return NULL_TREE;
    }
}

#ifdef USE_COMPONENT_REF
/* Context is 'T' for TypeName, 'P' for PackageName,
   'M' for MethodName, 'E' for ExpressionName, and 'A' for AmbiguousName. */

tree
resolve_simple_name (name, context)
     tree name;
     int context;
{
}

tree
resolve_qualified_name (name, context)
     tree name;
     int context;
{
}
#endif
