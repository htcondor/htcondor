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

ClassAdPrintMask::
ClassAdPrintMask ()
{
}

ClassAdPrintMask::
ClassAdPrintMask (const ClassAdPrintMask &pm)
{
	copyList (formats, (List<Formatter> &) pm.formats);
	copyList (attributes, (List<char> &) pm.attributes);
	copyList (alternates, (List<char> &) pm.alternates);
}


ClassAdPrintMask::
~ClassAdPrintMask ()
{
	clearFormats ();
}


void ClassAdPrintMask::
registerFormat (char *fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = PRINTF_FMT;
	newFmt->printfFmt = collapse_escapes(new_strdup(fmt));
	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void ClassAdPrintMask::
registerFormat (IntCustomFmt fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = INT_CUSTOM_FMT;
	newFmt->df = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void ClassAdPrintMask::
registerFormat (FloatCustomFmt fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = FLT_CUSTOM_FMT;
	newFmt->ff = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void ClassAdPrintMask::
registerFormat (StringCustomFmt fmt, const char *attr, char *alternate)
{
	Formatter *newFmt = new Formatter;

	newFmt->fmtKind = STR_CUSTOM_FMT;
	newFmt->sf = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}

void ClassAdPrintMask::
clearFormats (void)
{
	clearList (formats);
	clearList (attributes);
	clearList (alternates);
}


int ClassAdPrintMask::
display (FILE *file, ClassAd *ad)
{
	char * temp = display(ad);

	if (temp != NULL) {
		fputs(temp, file);
		delete [] temp;
		return 0;
	}
	return 1;

}

// returns a new char * that is your responsibility to delete.
char * ClassAdPrintMask::
display (ClassAd *ad)
{
	Formatter *fmt;
	char 	*attr, *alt;
	ExprTree *tree;
//	EvalResult result;
	Value 	result;				// NAC
	char * 	retval = new char[1024 * 8];
	int		intValue;
	float 	floatValue;
	char  	stringValue[1024];

	int 	tempInt;			// NAC
	double 	tempReal;			// NAC
	char 	tempString[1024];	// NAC

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
				if (!(tree = ad->Lookup (attr))){
					if ( alt ) {
						sprintf( stringValue, "%s", alt );
						strcat( retval, stringValue );
					}
					continue;
				}

				// print the result
//				if (tree->EvalTree (ad, NULL, &result)){
				if(ad->EvaluateAttr(attr, result))					// NAC
				{
//					switch (result.type)
					switch (result.GetType())						// NAC
					{
//					case LX_STRING:
//						sprintf( stringValue, fmt->printfFmt,
//							collapse_escapes(result.s) );
					case (result.STRING_VALUE):					    // NAC
						result.IsStringValue( tempString, 1024 );		// NAC
						sprintf( stringValue, fmt->printfFmt,   		// NAC
								 collapse_escapes(tempString) );		// NAC
						strcat( retval, stringValue );
						break;

//					case LX_FLOAT:
//						sprintf( stringValue, fmt->printfFmt, result.f);
					case (result.REAL_VALUE):						// NAC
						result.IsRealValue( tempReal );		 			// NAC
						sprintf( stringValue, fmt->printfFmt, 			// NAC
								 (float)tempReal );						// NAC
						strcat( retval, stringValue );
						break;

//					case LX_INTEGER:
//						sprintf( stringValue, fmt->printfFmt, result.i);
					case (result.INTEGER_VALUE):					// NAC
						result.IsIntegerValue( tempInt );				// NAC
						sprintf( stringValue, fmt->printfFmt, tempInt );	// NAC
						strcat( retval, stringValue );				
						break;

					default:
						strcat( retval, alt );
						continue;
					}
				}
				break;
					
			case INT_CUSTOM_FMT:
//				if( ad->EvalInteger( attr, NULL, intValue ) ) {
				if( ad->EvaluateAttrInt( attr, intValue ) ) {	// NAC
					strcat( retval, (fmt->df)( intValue , ad ) );
				} else {
					strcat( retval, alt );
				}
				break;

		  	case FLT_CUSTOM_FMT:
//				if( ad->EvalFloat( attr, NULL, floatValue ) ) {
				if( ad->EvaluateAttrReal( attr, tempReal ) ) {		//NAC
					floatValue = (float)tempReal;					//NAC
					strcat( retval, (fmt->ff)( floatValue , ad ) );
				} else {
					strcat( retval, alt );
				}
				break;

		  	case STR_CUSTOM_FMT:
//				if( ad->EvalString( attr, NULL, stringValue ) ) {
				if( ad->EvaluateAttrString( attr, stringValue, 1024 ) ) {
					strcat( retval, (fmt->sf)( stringValue , ad ) );
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


int ClassAdPrintMask::
display (FILE *file, ClassAdList *list)
{
	int retval = 1;
	ClassAd *ad;

	list->Open();
    while( (ad = (ClassAd *) list->Next()) ) {
		if( !display (file, ad) ) {
			retval = 0;
		}
    }
    list->Close ();

	return retval;
}

void ClassAdPrintMask::
clearList (List<char> &l)
{
    char *x;
    l.Rewind ();
    while( (x = l.Next()) ) {
        delete [] x;
        l.DeleteCurrent ();
    }
}

void ClassAdPrintMask::
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

void ClassAdPrintMask::
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


void ClassAdPrintMask::
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
