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
#include "MyString.h"
#include "condor_string.h"
#include "printf_format.h"

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
	MyString  retval("");
	int		intValue;
	float 	floatValue;
	MyString stringValue;
	char*	bool_str = NULL;
	char *value_from_classad = NULL;

	struct printf_fmt_info fmt_info;
	printf_fmt_t fmt_type;
	const char* tmp_fmt = NULL;

	formats.Rewind();
	attributes.Rewind();
	alternates.Rewind();

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
						retval += alt;
					}
					continue;
				}

					// figure out what kind of format string the
					// caller is using, and print out the appropriate
					// value depending on what they want.
				tmp_fmt = fmt->printfFmt;
				if( ! parsePrintfFormat(&tmp_fmt, &fmt_info) ) {
					if( alt ) {
						retval += alt;
					}
					continue;
				}

					// Now, figure out what kind of value they want
				fmt_type = fmt_info.type;
				switch( fmt_type ) {
				case PFT_STRING:
					if( al->EvalString( attr, NULL, &value_from_classad ) ) {
						stringValue.sprintf( fmt->printfFmt,
											 value_from_classad );
						retval += stringValue;
						free( value_from_classad );
						value_from_classad = NULL;
					} else {
						rhs = tree->RArg();
						if( rhs ) {
							rhs->PrintToNewStr( &bool_str );
							if( bool_str ) {
								stringValue.sprintf(fmt->printfFmt, bool_str);
								retval += stringValue;
								free( bool_str );
								bool_str = NULL;
							}
						} else {
							if ( alt ) {
								retval += alt;
							}
						}
					}
					break;

				case PFT_INT:
				case PFT_FLOAT:
					if( tree->EvalTree (al, NULL, &result) ) {
						switch( result.type ) {
						case LX_FLOAT:
							if( fmt_type == PFT_INT ) {
								stringValue.sprintf( fmt->printfFmt, 
													 (int)result.f );
							} else {
								stringValue.sprintf( fmt->printfFmt, 
													 result.f );
							}
							retval += stringValue;
							break;

						case LX_INTEGER:
							if( fmt_type == PFT_INT ) {
								stringValue.sprintf( fmt->printfFmt, 
													 result.i );
							} else {
								stringValue.sprintf( fmt->printfFmt, 
													 (float)result.i );
							}
							retval += stringValue;
							break;

						default:
								// the thing they want to print
								// doesn't evaulate to an int or a
								// float, so just print the alternate
							if ( alt ) {
								retval += alt;
							}
							break;
						}
					} else {
							// couldn't eval
						if( alt ) {
							retval += alt;
						}
					}
					break;

				default:
					EXCEPT( "Unknown value (%d) from parsePrintfFormat()!",
							(int)fmt_type );
					break;
				}
				break;

		  	case INT_CUSTOM_FMT:
				if( al->EvalInteger( attr, NULL, intValue ) ) {
					retval += (fmt->df)( intValue , al );
				} else {
					retval += alt;
				}
				break;

		  	case FLT_CUSTOM_FMT:
				if( al->EvalFloat( attr, NULL, floatValue ) ) {
					retval += (fmt->ff)( floatValue , al );
				} else {
					retval += alt;
				}
				break;

		  	case STR_CUSTOM_FMT:
				if( al->EvalString( attr, NULL, &value_from_classad ) ) {
					retval += (fmt->sf)(value_from_classad, al);
					free(value_from_classad);
				} else {
					retval += alt;
				}
				break;
	
			default:
				retval += alt;
		}
	}

	// Convert return MyString to new char *.
	return strnewp(retval.Value() );
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
