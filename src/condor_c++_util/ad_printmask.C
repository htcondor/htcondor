/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
	Formatter *fmt;
	char 	*attr, *alt;
	ExprTree *tree;
	EvalResult result;
	int 	retval = 1;
	int		intValue;
	float 	floatValue;
	char  	stringValue[1024];

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
					if (alt) fprintf (file, "%s", alt);
					continue;
				}

				// print the result
				if (tree->EvalTree (al, NULL, &result))
				{
					switch (result.type)
					{
					case LX_STRING:
						fprintf(file,fmt->printfFmt,collapse_escapes(result.s));
						break;

					case LX_FLOAT:
						fprintf (file, fmt->printfFmt, result.f);
						break;

					case LX_INTEGER:
						fprintf (file, fmt->printfFmt, result.i);
						break;

					default:
						fputs( alt , file );
						retval = 0;
						continue;
					}
				}
				break;

		  	case INT_CUSTOM_FMT:
				if( al->EvalInteger( attr, NULL, intValue ) ) {
					fputs( (fmt->df)( intValue , al ) , file );
				} else {
					fputs( alt, file );
					retval = 0;
				}
				break;

		  	case FLT_CUSTOM_FMT:
				if( al->EvalFloat( attr, NULL, floatValue ) ) {
					fputs( (fmt->ff)( floatValue , al ) , file );
				} else {
					fputs( alt, file );
					retval = 0;
				}
				break;

		  	case STR_CUSTOM_FMT:
				if( al->EvalString( attr, NULL, stringValue ) ) {
					fputs( (fmt->sf)( stringValue , al ) , file );
				} else {
					fputs( alt, file );
					retval = 0;
				}
				break;
	
			default:
				fputs( alt, file );
				retval = 0;
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
    while (al = (AttrList *) list->Next())
    {
		if (!display (file, al))
			retval = 0;
    }
    list->Close ();

	return retval;
}

void AttrListPrintMask::
clearList (List<char> &l)
{
    char *x;
    l.Rewind ();
    while (x = l.Next ())
    {
        delete [] x;
        l.DeleteCurrent ();
    }
}

void AttrListPrintMask::
clearList (List<Formatter> &l)
{
    Formatter *x;
    l.Rewind ();
    while (x = l.Next ())
    {
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
	while (item = from.Next ())
	{
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
	while (item = from.Next ())
		to.Append (new_strdup (item));
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
