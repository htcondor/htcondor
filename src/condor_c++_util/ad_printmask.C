/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "ad_printmask.h"
#include "escapes.h"

static char *new_strdup (const char *);

AttrListPrintMask::
AttrListPrintMask ()
{
}

AttrListPrintMask::
AttrListPrintMask (const AttrListPrintMask &pm)
{
	copyList (formats, (List<Formatter> &) pm.formats);
	copyList (attributes, (List<char> &) pm.attributes);
	copyList (alternates, (List<char> &) pm.alternates);
}


AttrListPrintMask::
~AttrListPrintMask ()
{
	clearFormats ();
}


void AttrListPrintMask::
registerFormat (char *fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = PRINTF_FMT;
	newFmt->printfFmt = collapse_escapes(new_strdup(fmt));
	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void AttrListPrintMask::
registerFormat (IntCustomFmt fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = INT_CUSTOM_FMT;
	newFmt->df = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void AttrListPrintMask::
registerFormat (FloatCustomFmt fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = FLT_CUSTOM_FMT;
	newFmt->ff = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void AttrListPrintMask::
registerFormat (StringCustomFmt fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = STR_CUSTOM_FMT;
	newFmt->sf = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void AttrListPrintMask::
clearFormats (void)
{
	clearList (formats);
	clearList (attributes);
	clearList (alternates);
}


int AttrListPrintMask::
display (FILE *file, AttrList *al)
{
	char * temp = display(al);

	if (temp != NULL) {
		fputs(temp, file);
		delete [] temp;
		return 0;
	}
	return 1;

}

// returns a new char * that is your responsibility to delete.
char * AttrListPrintMask::
display (AttrList *al)
{
	Formatter *fmt;
	char 	*attr, *alt;
	ExprTree *tree, *rhs;
	EvalResult result;
	char * 	retval = new char[1024 * 8];
	int		intValue;
	float 	floatValue;
	char  	stringValue[1024];
	char*	bool_str = NULL;

	formats.Rewind();
	attributes.Rewind();
	alternates.Rewind();

	retval[0] = '\0'; // Clear the return value;

	// for each item registered in the print mask
	while ((fmt=formats.Next()) && (attr=attributes.Next()) &&
				(alt=alternates.Next()))
	{
		switch( fmt->fmtKind )
		{
		  	case PRINTF_FMT:
				// get the expression tree of the attribute
				if (!(tree = al->Lookup (attr)))
				{
					if ( alt ) {
						sprintf( stringValue, "%s", alt );
						strcat( retval, stringValue );
					}
					continue;
				}

				// print the result
				if (tree->EvalTree (al, NULL, &result))
				{
					switch (result.type)
					{
					case LX_STRING:
						sprintf( stringValue, fmt->printfFmt,
							result.s );
						delete[] result.s;
						strcat( retval, stringValue );
						break;

					case LX_FLOAT:
						sprintf( stringValue, fmt->printfFmt, result.f);
						strcat( retval, stringValue );
						break;

					case LX_INTEGER:
						sprintf( stringValue, fmt->printfFmt, result.i);
						strcat( retval, stringValue );
						break;

					case LX_UNDEFINED:
					case LX_BOOL:

							// if the classad lookup worked, but the
							// evaluation gave us an undefined result,
							// (or if it's a bool), we know the
							// attribute is there.  so, instead of
							// printing out the evaluated result, just
							// print out the unevaluated expression.
						rhs = tree->RArg();
						if( rhs ) {
							rhs->PrintToNewStr( &bool_str );
							if( bool_str ) {
								sprintf( stringValue, fmt->printfFmt,
										 bool_str );
								strcat( retval, stringValue );
								free( bool_str );
								break;
							}
						}

					default:
						strcat( retval, alt );
						continue;
					}
				}
				break;

		  	case INT_CUSTOM_FMT:
				if( al->EvalInteger( attr, NULL, intValue ) ) {
					strcat( retval, (fmt->df)( intValue , al ) );
				} else {
					strcat( retval, alt );
				}
				break;

		  	case FLT_CUSTOM_FMT:
				if( al->EvalFloat( attr, NULL, floatValue ) ) {
					strcat( retval, (fmt->ff)( floatValue , al ) );
				} else {
					strcat( retval, alt );
				}
				break;

		  	case STR_CUSTOM_FMT:
				if( al->EvalString( attr, NULL, stringValue ) ) {
					strcat( retval, (fmt->sf)( stringValue , al ) );
				} else {
					strcat( retval, alt );
				}
				break;
	
			default:
				strcat( retval, alt );
		}
	}

	return retval;
}


int AttrListPrintMask::
display (FILE *file, AttrListList *list)
{
	int retval = 1;
	AttrList *al;

	list->Open();
    while( (al = (AttrList *) list->Next()) ) {
		if( !display (file, al) ) {
			retval = 0;
		}
    }
    list->Close ();

	return retval;
}

void AttrListPrintMask::
clearList (List<char> &l)
{
    char *x;
    l.Rewind ();
    while( (x = l.Next()) ) {
        delete [] x;
        l.DeleteCurrent ();
    }
}

void AttrListPrintMask::
clearList (List<Formatter> &l)
{
    Formatter *x;
    l.Rewind ();
    while( (x = l.Next ()) ) {
		if( x->fmtKind == PRINTF_FMT ) delete [] x->printfFmt;
		delete x;
        l.DeleteCurrent ();
    }
}

void AttrListPrintMask::
copyList (List<Formatter> &to, List<Formatter> &from)
{
	Formatter *item, *newItem;

	clearList (to);
	from.Rewind ();
	while( (item = from.Next()) ) {
		newItem = new Formatter;
		*newItem = *item;
		if( newItem->fmtKind == PRINTF_FMT )
			newItem->printfFmt = new_strdup( item->printfFmt );
		to.Append (newItem);
	}
}


void AttrListPrintMask::
copyList (List<char> &to, List<char> &from)
{
	char *item;

	clearList (to);
	from.Rewind ();
	while( (item = from.Next()) ) {
		to.Append (new_strdup (item));
	}
}


// strdup() which uses new
static char *new_strdup (const char *str)
{
    char *x = new char [strlen (str) + 1];
    if (!x)
    {
		return 0;
    }
    strcpy (x, str);
    return x;
}
