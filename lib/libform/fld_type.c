/*-----------------------------------------------------------------------------+
|           The ncurses form library is  Copyright (C) 1995-1997               |
|             by Juergen Pfeifer <Juergen.Pfeifer@T-Online.de>                 |
|                          All Rights Reserved.                                |
|                                                                              |
| Permission to use, copy, modify, and distribute this software and its        |
| documentation for any purpose and without fee is hereby granted, provided    |
| that the above copyright notice appear in all copies and that both that      |
| copyright notice and this permission notice appear in supporting             |
| documentation, and that the name of the above listed copyright holder(s) not |
| be used in advertising or publicity pertaining to distribution of the        |
| software without specific, written prior permission.                         | 
|                                                                              |
| THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO  |
| THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-  |
| NESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR   |
| ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RE- |
| SULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, |
| NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH    |
| THE USE OR PERFORMANCE OF THIS SOFTWARE.                                     |
+-----------------------------------------------------------------------------*/

#include "form.priv.h"

MODULE_ID("Id: fld_type.c,v 1.4 1997/05/01 16:47:54 juergen Exp $")

static FIELDTYPE const default_fieldtype = {
  0,                   /* status                                      */
  0L,                  /* reference count                             */
  (FIELDTYPE *)0,      /* pointer to left  operand                    */
  (FIELDTYPE *)0,      /* pointer to right operand                    */
  NULL,                /* makearg function                            */
  NULL,                /* copyarg function                            */
  NULL,                /* freearg function                            */
  NULL,                /* field validation function                   */
  NULL,                /* Character check function                    */
  NULL,                /* enumerate next function                     */
  NULL                 /* enumerate previous function                 */
};

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELDTYPE *new_fieldtype(
|                       bool (* const field_check)(FIELD *,const void *),
|                       bool (* const char_check) (int, const void *) ) 
|   
|   Description   :  Create a new fieldtype. The application programmer must
|                    write a field_check and a char_check function and give
|                    them as input to this call.
|                    If an error occurs, errno is set to                    
|                       E_BAD_ARGUMENT  - invalid arguments
|                       E_SYSTEM_ERROR  - system error (no memory)
|
|   Return Values :  Fieldtype pointer or NULL if error occured
+--------------------------------------------------------------------------*/
FIELDTYPE *new_fieldtype(
 bool (* const field_check)(FIELD *,const void *),
 bool (* const char_check) (int,const void *) )
{
  FIELDTYPE *nftyp = (FIELDTYPE *)0;
  
  if ( (field_check) && (char_check) )
    {
      nftyp = (FIELDTYPE *)malloc(sizeof(FIELDTYPE));
      if (nftyp)
	{
	  *nftyp = default_fieldtype;
	  nftyp->fcheck = field_check;
	  nftyp->ccheck = char_check;
	}
      else
	{
	  SET_ERROR( E_SYSTEM_ERROR );
	}
    }
  else
    {
      SET_ERROR( E_BAD_ARGUMENT );
    }
  return nftyp;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELDTYPE *link_fieldtype(
|                                FIELDTYPE *type1,
|                                FIELDTYPE *type2)
|   
|   Description   :  Create a new fieldtype built from the two given types.
|                    They are connected by an logical 'OR'.
|                    If an error occurs, errno is set to                    
|                       E_BAD_ARGUMENT  - invalid arguments
|                       E_SYSTEM_ERROR  - system error (no memory)
|
|   Return Values :  Fieldtype pointer or NULL if error occured.
+--------------------------------------------------------------------------*/
FIELDTYPE *link_fieldtype(FIELDTYPE * type1, FIELDTYPE * type2)
{
  FIELDTYPE *nftyp = (FIELDTYPE *)0;

  if ( type1 && type2 )
    {
      nftyp = (FIELDTYPE *)malloc(sizeof(FIELDTYPE));
      if (nftyp)
	{
	  *nftyp = default_fieldtype;
	  nftyp->status |= _LINKED_TYPE;
	  if ((type1->status & _HAS_ARGS) || (type2->status & _HAS_ARGS) )
	    nftyp->status |= _HAS_ARGS;
	  if ((type1->status & _HAS_CHOICE) || (type2->status & _HAS_CHOICE) )
	    nftyp->status |= _HAS_CHOICE;
	  nftyp->left  = type1;
	  nftyp->right = type2; 
	  type1->ref++;
	  type2->ref++;
	}
      else
	{
	  SET_ERROR( E_SYSTEM_ERROR );
	}
    }
  else
    {
      SET_ERROR( E_BAD_ARGUMENT );
    }
  return nftyp;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int free_fieldtype(FIELDTYPE *typ)
|   
|   Description   :  Release the memory associated with this fieldtype.
|
|   Return Values :  E_OK            - success
|                    E_CONNECTED     - there are fields referencing the type
|                    E_BAD_ARGUMENT  - invalid fieldtype pointer
+--------------------------------------------------------------------------*/
int free_fieldtype(FIELDTYPE *typ)
{
  if (!typ)
    RETURN(E_BAD_ARGUMENT);

  if (typ->ref!=0)
    RETURN(E_CONNECTED);

  if (typ->status & _RESIDENT)
    RETURN(E_CONNECTED);

  if (typ->status & _LINKED_TYPE)
    {
      if (typ->left ) typ->left->ref--;
      if (typ->right) typ->right->ref--;
    }
  free(typ);
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_fieldtype_arg(
|                            FIELDTYPE *typ,
|                            void * (* const make_arg)(va_list *),
|                            void * (* const copy_arg)(const void *),
|                            void   (* const free_arg)(void *) )
|   
|   Description   :  Connects to the type additional arguments necessary
|                    for a set_field_type call. The various function pointer
|                    arguments are:
|                       make_arg : allocates a structure for the field
|                                  specific parameters.
|                       copy_arg : duplicate the structure created by
|                                  make_arg
|                       free_arg : Release the memory allocated by make_arg
|                                  or copy_arg
|
|                    At least one of those functions must be non-NULL.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid argument
+--------------------------------------------------------------------------*/
int set_fieldtype_arg(FIELDTYPE * typ,
		      void * (* const make_arg)(va_list *),
		      void * (* const copy_arg)(const void *),
		      void   (* const free_arg)(void *))
{
  if ( !typ || !make_arg || !copy_arg || !free_arg )
    RETURN(E_BAD_ARGUMENT);

  typ->status |= _HAS_ARGS;
  typ->makearg = make_arg;
  typ->copyarg = copy_arg;
  typ->freearg = free_arg;
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_fieldtype_choice(
|                          FIELDTYPE *typ,
|                          bool (* const next_choice)(FIELD *,const void *),
|                          bool (* const prev_choice)(FIELD *,const void *))
|
|   Description   :  Define implementation of enumeration requests.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid arguments
+--------------------------------------------------------------------------*/
int set_fieldtype_choice(FIELDTYPE * typ,
			 bool (* const next_choice) (FIELD *,const void *),
			 bool (* const prev_choice) (FIELD *,const void *))
{
  if ( !typ || !next_choice || !prev_choice ) 
    RETURN(E_BAD_ARGUMENT);

  typ->status |= _HAS_CHOICE;
  typ->next = next_choice;
  typ->prev = prev_choice;
  RETURN(E_OK);
}

/* fld_type.c ends here */
