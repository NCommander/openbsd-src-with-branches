
/*
 * THIS CODE IS SPECIFICALLY EXEMPTED FROM THE NCURSES PACKAGE COPYRIGHT.
 * You may freely copy it for use as a template for your own field types.
 * If you develop a field type that might be of general use, please send
 * it back to the ncurses maintainers for inclusion in the next version.
 */
/***************************************************************************
*                                                                          *
*  Author : Juergen Pfeifer, Juergen.Pfeifer@T-Online.de                   *
*                                                                          *
***************************************************************************/

#include "form.priv.h"

MODULE_ID("Id: fty_int.c,v 1.7 1997/04/19 15:22:40 juergen Exp $")

typedef struct {
  int precision;
  long low;
  long high;
} integerARG;

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void *Make_Integer_Type( va_list * ap )
|   
|   Description   :  Allocate structure for integer type argument.
|
|   Return Values :  Pointer to argument structure or NULL on error
+--------------------------------------------------------------------------*/
static void *Make_Integer_Type(va_list * ap)
{
  integerARG *argp = (integerARG *)malloc(sizeof(integerARG));

  if (argp)
    {
      argp->precision = va_arg(*ap,int);
      argp->low       = va_arg(*ap,long);
      argp->high      = va_arg(*ap,long);
    }
  return (void *)argp;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void *Copy_Integer_Type(const void * argp)
|   
|   Description   :  Copy structure for integer type argument.  
|
|   Return Values :  Pointer to argument structure or NULL on error.
+--------------------------------------------------------------------------*/
static void *Copy_Integer_Type(const void * argp)
{
  const integerARG *ap = (const integerARG *)argp;
  integerARG *new = (integerARG *)0;

  if (argp)
    {
      new = (integerARG *)malloc(sizeof(integerARG));
      if (new)
	*new = *ap;
    }
  return (void *)new;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void Free_Integer_Type(void * argp)
|   
|   Description   :  Free structure for integer type argument.
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
static void Free_Integer_Type(void * argp)
{
  if (argp) 
    free(argp);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Check_Integer_Field(
|                                                    FIELD * field,
|                                                    const void * argp)
|   
|   Description   :  Validate buffer content to be a valid integer value
|
|   Return Values :  TRUE  - field is valid
|                    FALSE - field is invalid
+--------------------------------------------------------------------------*/
static bool Check_Integer_Field(FIELD * field, const void * argp)
{
  const integerARG *argi = (const integerARG *)argp;
  long low          = argi->low;
  long high         = argi->high;
  int prec          = argi->precision;
  unsigned char *bp = (unsigned char *)field_buffer(field,0);
  char *s           = (char *)bp;
  long val;
  char buf[100];

  while( *bp && *bp==' ') bp++;
  if (*bp)
    {
      if (*bp=='-') bp++;
      while (*bp)
	{
	  if (!isdigit(*bp)) break;
	  bp++;
	}
      while(*bp && *bp==' ') bp++;
      if (*bp=='\0')
	{
	  val = atol(s);
	  if (low<high)
	    {
	      if (val<low || val>high) return FALSE;
	    }
	  sprintf(buf,"%.*ld",(prec>0?prec:0),val);
	  set_field_buffer(field,0,buf);
	  return TRUE;
	}
    }
  return FALSE;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Check_Integer_Character(
|                                      int c,
|                                      const void * argp)
|   
|   Description   :  Check a character for the integer type.
|
|   Return Values :  TRUE  - character is valid
|                    FALSE - character is invalid
+--------------------------------------------------------------------------*/
static bool Check_Integer_Character(int c, const void * argp GCC_UNUSED)
{
  return ((isdigit(c) || (c=='-')) ? TRUE : FALSE);
}

static FIELDTYPE typeINTEGER = {
  _HAS_ARGS | _RESIDENT,
  1,                           /* this is mutable, so we can't be const */
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  Make_Integer_Type,
  Copy_Integer_Type,
  Free_Integer_Type,
  Check_Integer_Field,
  Check_Integer_Character,
  NULL,
  NULL
};

FIELDTYPE* TYPE_INTEGER = &typeINTEGER;

/* fty_int.c ends here */
