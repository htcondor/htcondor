/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
registerFormat (const char *fmt, const char *attr, const char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = PRINTF_FMT;
	newFmt->printfFmt = collapse_escapes(new_strdup(fmt));
	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
}

void AttrListPrintMask::
registerFormat (IntCustomFmt fmt, const char *attr, const char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = INT_CUSTOM_FMT;
	newFmt->df = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
}

void AttrListPrintMask::
registerFormat (FloatCustomFmt fmt, const char *attr, const char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = FLT_CUSTOM_FMT;
	newFmt->ff = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
}

void AttrListPrintMask::
registerFormat (StringCustomFmt fmt, const char *attr, const char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = STR_CUSTOM_FMT;
	newFmt->sf = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
}

void AttrListPrintMask::
clearFormats (void)
{
	clearList (formats);
	clearList (attributes);
	clearList (alternates);
}


int AttrListPrintMask::
display (FILE *file, AttrList *al, AttrList *target /* =NULL */)
{
	char * temp = display(al, target);

	if (temp != NULL) {
		fputs(temp, file);
		delete [] temp;
		return 0;
	}
	return 1;

}

// returns a new char * that is your responsibility to delete.
char * AttrListPrintMask::
display (AttrList *al, AttrList *target /* = NULL */)
{
	Formatter *fmt;
	char 	*attr, *alt;
	ExprTree *tree, *rhs;
	EvalResult result;
	MyString  retval("");
	int		intValue;
	float 	floatValue;
	MyString stringValue;
	const char*	bool_str = NULL;
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
			// If we decide that the "attr" requested is actually
			// an expression, we need to remember that, as 1.
			// it needs to be deleted, and 2. there is some
			// special handling for the string case.
		bool attr_is_expr = false;
		switch( fmt->fmtKind )
		{
		  	case PRINTF_FMT:
					// figure out what kind of format string the
					// caller is using, and print out the appropriate
					// value depending on what they want.
				tmp_fmt = fmt->printfFmt;
				if( ! parsePrintfFormat(&tmp_fmt, &fmt_info) ) {
						/*
						  if parsePrintfFormat() returned 0, it means
						  there are no conversion strings in the
						  format string.  some users do this as a hack
						  for always printing a newline after printing
						  some attributes that might not be there.
						  since we don't have a better alternative
						  yet, we should support this kind of use.
						  in this case, we should just print out the
						  format string directly, and not worry about
						  looking up attributes at all.  however, if
						  there is an alt string defined, we should
						  print that, instead. -Derek 2004-10-15
						*/
					if( alt && alt[0] ) {
						retval += alt;
					} else { 
						retval += fmt->printfFmt;
					}
					continue;
				}

					// if we got here, there's some % conversion
					// string, so we'll need to get the expression
					// tree of the attribute they asked for...
				if( !(tree = al->LookupExpr (attr)) ) {
						// drat, we couldn't find it. Maybe it's an
						// expression?
					tree = NULL;
					if( ParseClassAdRvalExpr(attr, tree) != 0 ) {
						delete tree;

							// drat, still no luck.  if there's an
							// alt string, use that, otherwise bail.
						if ( alt ) {
							retval += alt;
						}
						continue;
					}
					ASSERT(tree);
					attr_is_expr = true;
				}

					// Now, figure out what kind of value they want,
					// based on the kind of % conversion in their
					// format string 
				fmt_type = fmt_info.type;
				switch( fmt_type ) {
				case PFT_STRING:
					if( attr_is_expr ) {
						if( EvalExprTree(tree, al, target, &result) &&
							result.type == LX_STRING && result.s ) {
							retval.sprintf_cat(fmt->printfFmt, result.s);
						} else {
							// couldn't eval
							if( alt ) {
								retval += alt;
							}
						}
					} else if( al->EvalString( attr, target, &value_from_classad ) ) {
						stringValue.sprintf( fmt->printfFmt,
											 value_from_classad );
						retval += stringValue;
						free( value_from_classad );
						value_from_classad = NULL;
					} else {
						bool_str = ExprTreeToString( tree );
						if( bool_str ) {
							stringValue.sprintf(fmt->printfFmt, bool_str);
							retval += stringValue;
						} else {
							if ( alt ) {
								retval += alt;
							}
						}
					}
					break;

				case PFT_INT:
				case PFT_FLOAT:
					if( EvalExprTree(tree, al, target, &result) ) {
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
				if(attr_is_expr) { delete tree; tree = NULL; }
				break;

		  	case INT_CUSTOM_FMT:
				if( al->EvalInteger( attr, target, intValue ) ) {
					retval += (fmt->df)( intValue , al );
				} else {
					retval += alt;
				}
				break;

		  	case FLT_CUSTOM_FMT:
				if( al->EvalFloat( attr, target, floatValue ) ) {
					retval += (fmt->ff)( floatValue , al );
				} else {
					retval += alt;
				}
				break;

		  	case STR_CUSTOM_FMT:
				if( al->EvalString( attr, target, &value_from_classad ) ) {
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
display (FILE *file, AttrListList *list, AttrList *target /* = NULL */)
{
	int retval = 1;
	AttrList *al;

	list->Open();
    while( (al = (AttrList *) list->Next()) ) {
		if( !display (file, al, target) ) {
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
