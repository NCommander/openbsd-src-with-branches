/* Handle types for the GNU compiler for the Java(TM) language.
   Copyright (C) 1996, 97-98, 1999 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  

Java and all Java-based marks are trademarks or registered trademarks
of Sun Microsystems, Inc. in the United States and other countries.
The Free Software Foundation is independent of Sun Microsystems, Inc.  */

/* Written by Per Bothner <bothner@cygnus.com> */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "obstack.h"
#include "flags.h"
#include "java-tree.h"
#include "jcf.h"
#include "convert.h"
#include "toplev.h"

tree * type_map;
extern struct obstack permanent_obstack;

/* Set the type of the local variable with index SLOT to TYPE. */

void
set_local_type (slot, type)
     int slot;
     tree type;
{
  int max_locals = DECL_MAX_LOCALS(current_function_decl);
  int nslots = TYPE_IS_WIDE (type) ? 2 : 1;
  if (slot < 0 || slot + nslots - 1 >= max_locals)
    fatal ("invalid local variable index");
  type_map[slot] = type;
  while (--nslots > 0)
    type_map[++slot] = void_type_node;
}

/* Convert an IEEE real to an integer type.  The result of such a
   conversion when the source operand is a NaN isn't defined by
   IEEE754, but by the Java language standard: it must be zero.  This
   conversion produces something like:
   
   ({ double tmp = expr; (tmp != tmp) ? 0 : (int)tmp; })

   */

static tree
convert_ieee_real_to_integer (type, expr)
     tree type, expr;
{
  expr = save_expr (expr);

  return build (COND_EXPR, type, 
		build (NE_EXPR, boolean_type_node, expr, expr),
		convert (type, integer_zero_node),
		convert_to_integer (type, expr));
}  

/* Create an expression whose value is that of EXPR,
   converted to type TYPE.  The TREE_TYPE of the value
   is always TYPE.  This function implements all reasonable
   conversions; callers should filter out those that are
   not permitted by the language being compiled.  */

tree
convert (type, expr)
     tree type, expr;
{
  register enum tree_code code = TREE_CODE (type);

  if (do_not_fold)
    return build1 (NOP_EXPR, type, expr);

  if (type == TREE_TYPE (expr)
      || TREE_CODE (expr) == ERROR_MARK)
    return expr;
  if (TREE_CODE (TREE_TYPE (expr)) == ERROR_MARK)
    return error_mark_node;
  if (code == VOID_TYPE)
    return build1 (CONVERT_EXPR, type, expr);
  if (code == BOOLEAN_TYPE)
    return fold (convert_to_boolean (type, expr));
  if (code == INTEGER_TYPE)
    {
      if (TREE_CODE (TREE_TYPE (expr)) == REAL_TYPE
#ifdef TARGET_SOFT_FLOAT
	  && !TARGET_SOFT_FLOAT
#endif
	  && !flag_emit_class_files
	  && !flag_fast_math
	  && TARGET_FLOAT_FORMAT == IEEE_FLOAT_FORMAT)
	return fold (convert_ieee_real_to_integer (type, expr));
      else
	return fold (convert_to_integer (type, expr));
    }	  
  if (code == REAL_TYPE)
    return fold (convert_to_real (type, expr));
  if (code == CHAR_TYPE)
    return fold (convert_to_char (type, expr));
  if (code == POINTER_TYPE)
    return fold (convert_to_pointer (type, expr));
  error ("conversion to non-scalar type requested");
  return error_mark_node;
}


tree
convert_to_char (type, expr)
    tree type, expr;
{
  return build1 (NOP_EXPR, type, expr);
}

tree
convert_to_boolean (type, expr)
     tree type, expr;
{
  return build1 (NOP_EXPR, type, expr);
}

/* Print an error message for invalid use of an incomplete type.
   VALUE is the expression that was used (or 0 if that isn't known)
   and TYPE is the type that was invalid.  */

void
incomplete_type_error (value, type)
  tree value ATTRIBUTE_UNUSED;
  tree type ATTRIBUTE_UNUSED;
{
  error ("internal error - use of undefined type");
}

/* Return a data type that has machine mode MODE.
   If the mode is an integer,
   then UNSIGNEDP selects between signed and unsigned types.  */

tree
type_for_mode (mode, unsignedp)
     enum machine_mode mode;
     int unsignedp;
{
  if (mode == TYPE_MODE (int_type_node))
    return unsignedp ? unsigned_int_type_node : int_type_node;
  if (mode == TYPE_MODE (long_type_node))
    return unsignedp ? unsigned_long_type_node : long_type_node;
  if (mode == TYPE_MODE (short_type_node))
    return unsignedp ? unsigned_short_type_node : short_type_node;
  if (mode == TYPE_MODE (byte_type_node))
    return unsignedp ? unsigned_byte_type_node : byte_type_node;
  if (mode == TYPE_MODE (float_type_node))
    return float_type_node;
  if (mode == TYPE_MODE (double_type_node))
    return double_type_node;

  return 0;
}

/* Return an integer type with BITS bits of precision,
   that is unsigned if UNSIGNEDP is nonzero, otherwise signed.  */

tree
type_for_size (bits, unsignedp)
     unsigned bits;
     int unsignedp;
{
  if (bits <= TYPE_PRECISION (byte_type_node))
    return unsignedp ? unsigned_byte_type_node : byte_type_node;
  if (bits <= TYPE_PRECISION (short_type_node))
    return unsignedp ? unsigned_short_type_node : short_type_node;
  if (bits <= TYPE_PRECISION (int_type_node))
    return unsignedp ? unsigned_int_type_node : int_type_node;
  if (bits <= TYPE_PRECISION (long_type_node))
    return unsignedp ? unsigned_long_type_node : long_type_node;
  return 0;
}

/* Return a type the same as TYPE except unsigned or
   signed according to UNSIGNEDP.  */

tree
signed_or_unsigned_type (unsignedp, type)
     int unsignedp;
     tree type;
{
  if (! INTEGRAL_TYPE_P (type))
    return type;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (int_type_node))
    return unsignedp ? unsigned_int_type_node : int_type_node;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (byte_type_node))
    return unsignedp ? unsigned_byte_type_node : byte_type_node;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (short_type_node))
    return unsignedp ? unsigned_short_type_node : short_type_node;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (long_type_node))
    return unsignedp ? unsigned_long_type_node : long_type_node;
  return type;
}

/* Return a signed type the same as TYPE in other respects.  */

tree
signed_type (type)
     tree type;
{
  return signed_or_unsigned_type (0, type);
}

/* Return an unsigned type the same as TYPE in other respects.  */

tree
unsigned_type (type)
     tree type;
{
  return signed_or_unsigned_type (1, type);

}

/* Mark EXP saying that we need to be able to take the
   address of it; it should not be allocated in a register.
   Value is 1 if successful.  */

int
mark_addressable (exp)
     tree exp;
{
  register tree x = exp;
  while (1)
    switch (TREE_CODE (x))
      {
      case ADDR_EXPR:
      case COMPONENT_REF:
      case ARRAY_REF:
      case REALPART_EXPR:
      case IMAGPART_EXPR:
	x = TREE_OPERAND (x, 0);
	break;

      case TRUTH_ANDIF_EXPR:
      case TRUTH_ORIF_EXPR:
      case COMPOUND_EXPR:
	x = TREE_OPERAND (x, 1);
	break;

      case COND_EXPR:
	return mark_addressable (TREE_OPERAND (x, 1))
	  & mark_addressable (TREE_OPERAND (x, 2));

      case CONSTRUCTOR:
	TREE_ADDRESSABLE (x) = 1;
	return 1;

      case INDIRECT_REF:
	/* We sometimes add a cast *(TYPE*)&FOO to handle type and mode
	   incompatibility problems.  Handle this case by marking FOO.  */
	if (TREE_CODE (TREE_OPERAND (x, 0)) == NOP_EXPR
	    && TREE_CODE (TREE_OPERAND (TREE_OPERAND (x, 0), 0)) == ADDR_EXPR)
	  {
	    x = TREE_OPERAND (TREE_OPERAND (x, 0), 0);
	    break;
	  }
	if (TREE_CODE (TREE_OPERAND (x, 0)) == ADDR_EXPR)
	  {
	    x = TREE_OPERAND (x, 0);
	    break;
	  }
	return 1;

      case VAR_DECL:
      case CONST_DECL:
      case PARM_DECL:
      case RESULT_DECL:
      case FUNCTION_DECL:
	TREE_ADDRESSABLE (x) = 1;
#if 0  /* poplevel deals with this now.  */
	if (DECL_CONTEXT (x) == 0)
	  TREE_ADDRESSABLE (DECL_ASSEMBLER_NAME (x)) = 1;
#endif
	/* drops through */
      default:
	return 1;
    }
}

/* Thorough checking of the arrayness of TYPE.  */

int
is_array_type_p (type)
     tree type;
{
  return TREE_CODE (type) == POINTER_TYPE
    && TREE_CODE (TREE_TYPE (type)) == RECORD_TYPE
    && TYPE_ARRAY_P (TREE_TYPE (type));
}

/* Return the length of a Java array type.
   Return -1 if the length is unknown or non-constant. */

HOST_WIDE_INT
java_array_type_length (array_type)
     tree array_type;
{
  tree arfld;
  if (TREE_CODE (array_type) == POINTER_TYPE)
    array_type = TREE_TYPE (array_type);
  arfld = TREE_CHAIN (TREE_CHAIN (TYPE_FIELDS (array_type)));
  if (arfld != NULL_TREE)
    {
      tree index_type = TYPE_DOMAIN (TREE_TYPE (arfld));
      tree high = TYPE_MAX_VALUE (index_type);
      if (TREE_CODE (high) == INTEGER_CST)
	return TREE_INT_CST_LOW (high) + 1;
    }
  return -1;
}

tree
build_prim_array_type (element_type, length)
     tree element_type;
     HOST_WIDE_INT length;
{
  tree max_index = build_int_2 (length - 1, 0);
  TREE_TYPE (max_index) = sizetype;
  return build_array_type (element_type, build_index_type (max_index));
}

/* Return a Java array type with a given ELEMENT_TYPE and LENGTH.
   These are hashed (shared) using IDENTIFIER_SIGNATURE_TYPE.
   The LENGTH is -1 if the length is unknown. */

tree
build_java_array_type (element_type, length)
     tree element_type;
     HOST_WIDE_INT length;
{
  tree sig, t, fld;
  char buf[12];
  tree elsig = build_java_signature (element_type);
  tree el_name = element_type;
  sprintf (buf, length >= 0 ? "[%d" : "[", length);
  sig = ident_subst (IDENTIFIER_POINTER (elsig), IDENTIFIER_LENGTH (elsig),
		     buf, 0, 0, "");
  t = IDENTIFIER_SIGNATURE_TYPE (sig);
  if (t != NULL_TREE)
    return TREE_TYPE (t);
  t = make_class ();
  IDENTIFIER_SIGNATURE_TYPE (sig) = build_pointer_type (t);
  TYPE_ARRAY_P (t) = 1;

  if (TREE_CODE (el_name) == POINTER_TYPE)
    el_name = TREE_TYPE (el_name);
  el_name = TYPE_NAME (el_name);
  if (TREE_CODE (el_name) == TYPE_DECL)
    el_name = DECL_NAME (el_name);
  TYPE_NAME (t) = identifier_subst (el_name, "", '.', '.', "[]");

  set_java_signature (t, sig);
  set_super_info (0, t, object_type_node, 0);
  if (TREE_CODE (element_type) == RECORD_TYPE)
    element_type = promote_type (element_type);
  TYPE_ARRAY_ELEMENT (t) = element_type;

  /* Add length pseudo-field. */
  push_obstacks (&permanent_obstack, &permanent_obstack);
  fld = build_decl (FIELD_DECL, get_identifier ("length"), int_type_node);
  TYPE_FIELDS (t) = fld;
  DECL_CONTEXT (fld) = t;
  FIELD_PUBLIC (fld) = 1;
  FIELD_FINAL (fld) = 1;

  if (length >= 0)
    {
      tree atype = build_prim_array_type (element_type, length);
      tree arfld = build_decl (FIELD_DECL, get_identifier ("data"), atype);
      DECL_CONTEXT (arfld) = t;
      TREE_CHAIN (fld) = arfld;
    }
  else
    TYPE_ALIGN (t) = TYPE_ALIGN (element_type);
  pop_obstacks ();

  /* We could layout_class, but that loads java.lang.Object prematurely.
   * This is called by the parser, and it is a bad idea to do load_class
   * in the middle of parsing, because of possible circularity problems. */
  push_super_field (t, object_type_node);
  layout_type (t);

  return t;
}

/* Promote TYPE to the type actually used for fields and parameters. */

tree
promote_type (type)
     tree type;
{
  switch (TREE_CODE (type))
    {
    case RECORD_TYPE:
      return build_pointer_type (CLASS_TO_HANDLE_TYPE (type));
    case BOOLEAN_TYPE:
      if (type == boolean_type_node)
	return promoted_boolean_type_node;
      goto handle_int;
    case CHAR_TYPE:
      if (type == char_type_node)
	return promoted_char_type_node;
      goto handle_int;
    case INTEGER_TYPE:
    handle_int:
      if (TYPE_PRECISION (type) < TYPE_PRECISION (int_type_node))
	{
	  if (type == short_type_node)
	    return promoted_short_type_node;
	  if (type == byte_type_node)
	    return promoted_byte_type_node;
	  return int_type_node;
	}
      /* ... else fall through ... */
    default:
      return type;
    }
}

/* Parse a signature string, starting at *PTR and ending at LIMIT.
   Return the seen TREE_TYPE, updating *PTR. */

static tree
parse_signature_type (ptr, limit)
     const unsigned char **ptr, *limit;
{
  tree type;
  if ((*ptr) >= limit)
    fatal ("bad signature string");
  switch (*(*ptr))
    {
    case 'B':  (*ptr)++;  return byte_type_node;
    case 'C':  (*ptr)++;  return char_type_node;
    case 'D':  (*ptr)++;  return double_type_node;
    case 'F':  (*ptr)++;  return float_type_node;
    case 'S':  (*ptr)++;  return short_type_node;
    case 'I':  (*ptr)++;  return int_type_node;
    case 'J':  (*ptr)++;  return long_type_node;
    case 'Z':  (*ptr)++;  return boolean_type_node;
    case 'V':  (*ptr)++;  return void_type_node;
    case '[':
      for ((*ptr)++; (*ptr) < limit && ISDIGIT (**ptr); ) (*ptr)++;
      type = parse_signature_type (ptr, limit);
      type = build_java_array_type (type, -1); 
      break;
    case 'L':
      {
	const unsigned char *start = ++(*ptr);
	register const unsigned char *str = start;
	for ( ; ; str++)
	  {
	    if (str >= limit)
	      fatal ("bad signature string");
	    if (*str == ';')
	      break;
	  }
	*ptr = str+1;
	type = lookup_class (unmangle_classname (start, str - start));
	break;
      }
    default:
      fatal ("unrecognized signature string");
    }
  return promote_type (type);
}

/* Parse a Java "mangled" signature string, starting at SIG_STRING,
   and SIG_LENGTH bytes long.
   Return a gcc type node. */

tree
parse_signature_string (sig_string, sig_length)
     const unsigned char *sig_string;
     int sig_length;
{
  tree result_type;
  const unsigned char *str = sig_string;
  const unsigned char *limit = str + sig_length;

  push_obstacks (&permanent_obstack, &permanent_obstack);
  if (str < limit && str[0] == '(')
    {
      tree argtype_list = NULL_TREE;
      str++;
      while (str < limit && str[0] != ')')
	{
	  tree argtype = parse_signature_type (&str, limit);
	  argtype_list = tree_cons (NULL_TREE, argtype, argtype_list);
	}
      if (str++, str >= limit)
	fatal ("bad signature string");
      result_type = parse_signature_type (&str, limit);
      argtype_list = chainon (nreverse (argtype_list), end_params_node);
      result_type = build_function_type (result_type, argtype_list);
    }
  else
    result_type = parse_signature_type (&str, limit);
  if (str != limit)
    error ("junk at end of signature string");
  pop_obstacks ();
  return result_type;
}

/* Convert a signature to its type.
 * Uses IDENTIFIER_SIGNATURE_TYPE as a cache (except for primitive types).
 */

tree
get_type_from_signature (tree signature)
{
  unsigned char *sig = (unsigned char *) IDENTIFIER_POINTER (signature);
  int len = IDENTIFIER_LENGTH (signature);
  tree type;
  /* Primitive types aren't cached. */
  if (len <= 1)
    return parse_signature_string (sig, len);
  type = IDENTIFIER_SIGNATURE_TYPE (signature);
  if (type == NULL_TREE)
    {
      type = parse_signature_string (sig, len);
      IDENTIFIER_SIGNATURE_TYPE (signature) = type;
    }
  return type;
}

/* Return the signature string for the arguments of method type TYPE. */

tree
build_java_argument_signature (type)
     tree type;
{
  extern struct obstack temporary_obstack;
  tree sig = TYPE_ARGUMENT_SIGNATURE (type);
  if (sig == NULL_TREE)
    {
      tree args = TYPE_ARG_TYPES (type);
      if (TREE_CODE (type) == METHOD_TYPE)
	args = TREE_CHAIN (args);  /* Skip "this" argument. */
      for (; args != end_params_node; args = TREE_CHAIN (args))
	{
	  tree t = build_java_signature (TREE_VALUE (args));
	  obstack_grow (&temporary_obstack,
			IDENTIFIER_POINTER (t), IDENTIFIER_LENGTH (t));
	}
      obstack_1grow (&temporary_obstack, '\0');

      sig = get_identifier (obstack_base (&temporary_obstack));
      TYPE_ARGUMENT_SIGNATURE (type) = sig;
      obstack_free (&temporary_obstack, obstack_base (&temporary_obstack));
    }
  return sig;
}

/* Return the signature of the given TYPE. */

tree
build_java_signature (type)
     tree type;
{
  tree sig, t;
  push_obstacks (&permanent_obstack, &permanent_obstack);
  while (TREE_CODE (type) == POINTER_TYPE)
    type = TREE_TYPE (type);
  if (TYPE_LANG_SPECIFIC (type) == NULL)
    {
      TYPE_LANG_SPECIFIC (type) = (struct lang_type *)
	perm_calloc (1, sizeof (struct lang_type));
    }
  sig = TYPE_LANG_SPECIFIC (type)->signature;
  if (sig == NULL_TREE)
    {
      char sg[2];
      switch (TREE_CODE (type))
	{
	case BOOLEAN_TYPE: sg[0] = 'Z';  goto native;
	case CHAR_TYPE:    sg[0] = 'C';  goto native;
	case VOID_TYPE:    sg[0] = 'V';  goto native;
	case INTEGER_TYPE:
	  switch (TYPE_PRECISION (type))
	    {
	    case  8:       sg[0] = 'B';  goto native;
	    case 16:       sg[0] = 'S';  goto native;
	    case 32:       sg[0] = 'I';  goto native;
	    case 64:       sg[0] = 'J';  goto native;
	    default:  goto bad_type;
	    }
	case REAL_TYPE:
	  switch (TYPE_PRECISION (type))
	    {
	    case 32:       sg[0] = 'F';  goto native;
	    case 64:       sg[0] = 'D';  goto native;
	    default:  goto bad_type;
	    }
	native:
	  sg[1] = 0;
	  sig = get_identifier (sg);
	  break;
	case RECORD_TYPE:
	  if (TYPE_ARRAY_P (type))
	    {
	      t = build_java_signature (TYPE_ARRAY_ELEMENT (type));
	      sig = ident_subst (IDENTIFIER_POINTER (t), IDENTIFIER_LENGTH (t),
				 "[", 0, 0, "");
	    }
	  else
	    {
	      t = DECL_NAME (TYPE_NAME (type));
	      sig = ident_subst (IDENTIFIER_POINTER (t), IDENTIFIER_LENGTH (t),
				 "L", '.', '/', ";");
	    }
	  break;
	case METHOD_TYPE:
	case FUNCTION_TYPE:
	  {
	    extern struct obstack temporary_obstack;
	    sig = build_java_argument_signature (type);
	    obstack_1grow (&temporary_obstack, '(');
	    obstack_grow (&temporary_obstack,
			  IDENTIFIER_POINTER (sig), IDENTIFIER_LENGTH (sig));
	    obstack_1grow (&temporary_obstack, ')');

	    t = build_java_signature (TREE_TYPE (type));
	    obstack_grow0 (&temporary_obstack,
			   IDENTIFIER_POINTER (t), IDENTIFIER_LENGTH (t));

	    sig = get_identifier (obstack_base (&temporary_obstack));
	    obstack_free (&temporary_obstack,
			  obstack_base (&temporary_obstack));
	  }
	  break;
	bad_type:
	default:
	  fatal ("internal error - build_java_signature passed invalid type");
	}
      TYPE_LANG_SPECIFIC (type)->signature = sig;
    }
  pop_obstacks ();
  return sig;
}

/* Save signature string SIG (an IDENTIFIER_NODE) in TYPE for future use. */

void
set_java_signature (type, sig)
     tree type;
     tree sig;
{
  tree old_sig;
  while (TREE_CODE (type) == POINTER_TYPE)
    type = TREE_TYPE (type);
  if (TYPE_LANG_SPECIFIC (type) == NULL)
    {
      TYPE_LANG_SPECIFIC (type) = (struct lang_type *)
	perm_calloc (1, sizeof (struct lang_type));
      
    }
  old_sig = TYPE_LANG_SPECIFIC (type)->signature;
  if (old_sig != NULL_TREE && old_sig != sig)
    fatal ("internal error - set_java_signature");
  TYPE_LANG_SPECIFIC (type)->signature = sig;
#if 0 /* careful about METHOD_TYPE */
  if (IDENTIFIER_SIGNATURE_TYPE (sig) == NULL_TREE && TREE_PERMANENT (type))
    IDENTIFIER_SIGNATURE_TYPE (sig) = type;
#endif
}

/* Search in class CLAS (and its superclasses) for a method
   matching METHOD_NAME and argument signature METHOD_SIGNATURE.
   Return a FUNCTION_DECL on success, or NULL_TREE if none found.
   (Contrast lookup_java_method, which takes into account return type.) */

tree
lookup_argument_method (clas, method_name, method_signature)
     tree clas, method_name, method_signature;
{
  tree method;
  while (clas != NULL_TREE)
    {
      for (method = TYPE_METHODS (clas);
	   method != NULL_TREE;  method = TREE_CHAIN (method))
	{
	  tree method_sig = build_java_argument_signature (TREE_TYPE (method));
	  tree name = DECL_NAME (method);
	  if ((TREE_CODE (name) == EXPR_WITH_FILE_LOCATION ?
	       EXPR_WFL_NODE (name) : name) == method_name 
	      && method_sig == method_signature)
	    return method;
	}
      clas = CLASSTYPE_SUPER (clas);
    }
  return NULL_TREE;
}

/* Search in class CLAS (and its superclasses) for a method
   matching METHOD_NAME and signature METHOD_SIGNATURE.
   Return a FUNCTION_DECL on success, or NULL_TREE if none found.
   (Contrast lookup_argument_method, which ignores return type.) */

tree
lookup_java_method (clas, method_name, method_signature)
     tree clas, method_name, method_signature;
{
  tree method;
  while (clas != NULL_TREE)
    {
      for (method = TYPE_METHODS (clas);
	   method != NULL_TREE;  method = TREE_CHAIN (method))
	{
	  tree method_sig = build_java_signature (TREE_TYPE (method));
	  if (DECL_NAME (method) == method_name 
	      && method_sig == method_signature)
	    return method;
	}
      clas = CLASSTYPE_SUPER (clas);
    }
  return NULL_TREE;
}

/* Search in class CLAS for a constructor matching METHOD_SIGNATURE.
   Return a FUNCTION_DECL on success, or NULL_TREE if none found. */

tree
lookup_java_constructor (clas, method_signature)
     tree clas, method_signature;
{
  tree method = TYPE_METHODS (clas);
  for ( ; method != NULL_TREE;  method = TREE_CHAIN (method))
    {
      tree method_sig = build_java_signature (TREE_TYPE (method));
      if (DECL_CONSTRUCTOR_P (method) && method_sig == method_signature)
	return method;
    }
  return NULL_TREE;
}

/* Return a type which is the Binary Numeric Promotion of the pair T1,
   T2 and convert EXP1 and/or EXP2. See 5.6.2 Binary Numeric
   Promotion. It assumes that both T1 and T2 are elligible to BNP. */

tree
binary_numeric_promotion (t1, t2, exp1, exp2)
     tree t1;
     tree t2;
     tree *exp1;
     tree *exp2;
{
  if (t1 == double_type_node || t2 == double_type_node)
    {
      if (t1 != double_type_node)
	*exp1 = convert (double_type_node, *exp1);
      if (t2 != double_type_node)
	*exp2 = convert (double_type_node, *exp2);
      return double_type_node;
    }
  if (t1 == float_type_node || t2 == float_type_node)
    {
      if (t1 != float_type_node)
	*exp1 = convert (float_type_node, *exp1);
      if (t2 != float_type_node)
	*exp2 = convert (float_type_node, *exp2);
      return float_type_node;
    }
  if (t1 == long_type_node || t2 == long_type_node)
    {
      if (t1 != long_type_node)
	*exp1 = convert (long_type_node, *exp1);
      if (t2 != long_type_node)
	*exp2 = convert (long_type_node, *exp2);
      return long_type_node;
    }

  if (t1 != int_type_node)
    *exp1 = convert (int_type_node, *exp1);
  if (t2 != int_type_node)
    *exp2 = convert (int_type_node, *exp2);
  return int_type_node;
}
