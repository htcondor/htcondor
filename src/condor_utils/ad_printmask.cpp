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
	: overall_max_width(0)
	, row_prefix(NULL)
	, col_prefix(NULL)
	, col_suffix(NULL)
	, row_suffix(NULL)
{
}

AttrListPrintMask::
AttrListPrintMask (const AttrListPrintMask &pm)
	: overall_max_width(0)
	, row_prefix(NULL)
	, col_prefix(NULL)
	, col_suffix(NULL)
	, row_suffix(NULL)
{
	copyList (formats, const_cast<List<Formatter> &>(pm.formats));
	copyList (attributes, const_cast<List<char> &>(pm.attributes));
	copyList (alternates, const_cast<List<char> &>(pm.alternates));
	if (pm.row_prefix) { row_prefix = new_strdup(pm.row_prefix); }
	if (pm.col_prefix) { col_prefix = new_strdup(pm.col_prefix); }
	if (pm.col_suffix) { col_suffix = new_strdup(pm.col_suffix); }
	if (pm.row_suffix) { row_suffix = new_strdup(pm.row_suffix); }
}


AttrListPrintMask::
~AttrListPrintMask ()
{
	clearFormats ();
	clearPrefixes ();
}

void AttrListPrintMask::
SetAutoSep(const char* rpre, const char * cpre, const char * cpost, const char * rpost)
{
	clearPrefixes();
	if (rpre)  { row_prefix = new_strdup(rpre); }
	if (cpre)  { col_prefix = new_strdup(cpre); }
	if (cpost) { col_suffix = new_strdup(cpost); }
	if (rpost) { row_suffix = new_strdup(rpost); }
}

void AttrListPrintMask::
SetOverallWidth(int wid)
{
	overall_max_width = wid;
}

void AttrListPrintMask::
commonRegisterFormat (FormatKind kind, int wid, int opts, const char *print, 
                       StringCustomFmt sf, const char *attr, const char *alt)
{
	Formatter *newFmt = new Formatter;
	memset(newFmt, 0, sizeof(*newFmt));

	newFmt->fmtKind = kind;
	newFmt->sf = sf;
	newFmt->width = abs(wid);
	newFmt->options = opts;
	if (wid < 0)
		newFmt->options |= FormatOptionLeftAlign;
	if (print) {
		newFmt->printfFmt = collapse_escapes(new_strdup(print));

		const char* tmp_fmt = newFmt->printfFmt;
		struct printf_fmt_info fmt_info;
		if (parsePrintfFormat(&tmp_fmt, &fmt_info)) {
			newFmt->fmt_type = (char)fmt_info.type;
			newFmt->fmt_letter = fmt_info.fmt_letter;
			if ( ! wid) {
				newFmt->width = fmt_info.width;
				if (fmt_info.is_left)
					newFmt->options |= FormatOptionLeftAlign;
			}
		} else {
			newFmt->fmt_type = (char)PFT_NONE;
			newFmt->fmt_letter = 0;
		}
	}
	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alt)));
}


void AttrListPrintMask::
registerFormat (const char *fmt, int wid, int opts, const char *attr, const char *alternate)
{
#if 1
	commonRegisterFormat(PRINTF_FMT, wid, opts, fmt, NULL, attr, alternate);
#else
	Formatter *newFmt = new Formatter;
	memset(newFmt, 0, sizeof(*newFmt));

	newFmt->fmtKind = PRINTF_FMT;
	newFmt->width = wid;
	newFmt->options = opts;
	newFmt->printfFmt = collapse_escapes(new_strdup(fmt));
	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
#endif
}

void AttrListPrintMask::
registerFormat (const char *print, int wid, int opts, IntCustomFmt fmt, const char *attr, const char *alternate)
{
#if 1
	commonRegisterFormat(INT_CUSTOM_FMT, wid, opts, print, (StringCustomFmt)fmt, attr, alternate);
#else
	Formatter *newFmt = new Formatter;
	memset(newFmt, 0, sizeof(*newFmt));

	if (print)
		newFmt->printfFmt = collapse_escapes(new_strdup(print));
	newFmt->fmtKind = INT_CUSTOM_FMT;
	newFmt->width = wid;
	newFmt->options = opts;
	newFmt->df = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
#endif
}

void AttrListPrintMask::
registerFormat (const char *print, int wid, int opts, FloatCustomFmt fmt, const char *attr, const char *alternate)
{
#if 1
	commonRegisterFormat(FLT_CUSTOM_FMT, wid, opts, print, (StringCustomFmt)fmt, attr, alternate);
#else
	Formatter *newFmt = new Formatter;
	memset(newFmt, 0, sizeof(*newFmt));

	if (print)
		newFmt->printfFmt = collapse_escapes(new_strdup(print));
	newFmt->fmtKind = FLT_CUSTOM_FMT;
	newFmt->width = wid;
	newFmt->options = opts;
	newFmt->ff = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
#endif
}


void AttrListPrintMask::
registerFormat (const char *print, int wid, int opts, StringCustomFmt fmt, const char *attr, const char *alternate)
{
#if 1
	commonRegisterFormat(STR_CUSTOM_FMT, wid, opts, print, fmt, attr, alternate);
#else
	Formatter *newFmt = new Formatter;
	memset(newFmt, 0, sizeof(*newFmt));

	if (print)
		newFmt->printfFmt = collapse_escapes(new_strdup(print));
	newFmt->fmtKind = STR_CUSTOM_FMT;
	newFmt->width = wid;
	newFmt->options = opts;
	newFmt->sf = fmt;

	formats.Append (newFmt);

	attributes.Append(new_strdup (attr));
	alternates.Append(collapse_escapes(new_strdup(alternate)));
#endif
}

void AttrListPrintMask::
clearFormats (void)
{
	clearList (formats);
	clearList (attributes);
	clearList (alternates);
}

void AttrListPrintMask::
clearPrefixes (void)
{
	if (row_prefix) { delete [] row_prefix; row_prefix = NULL; }
	if (col_prefix) { delete [] col_prefix; col_prefix = NULL; }
	if (col_suffix) { delete [] col_suffix; col_suffix = NULL; }
	if (row_suffix) { delete [] row_suffix; row_suffix = NULL; }
}

#if 0  // future

int AttrListPrintMask::
calc_widths(AttrList * al, AttrList *target /*=NULL*/ )
{
	Formatter *fmt;
	char 	*attr, *alt;
	const char * pszVal;
	char *value_from_classad = NULL;

	formats.Rewind();
	attributes.Rewind();
	alternates.Rewind();

	// for each item registered in the print mask
	while ((fmt=formats.Next()) && (attr=attributes.Next()) && (alt=alternates.Next()))
	{
			// If we decide that the "attr" requested is actually
			// an expression, we need to remember that, as 1.
			// it needs to be deleted, and 2. there is some
			// special handling for the string case.
		MyString  colval(alt);

		switch (fmt->fmtKind)
		{
		case PRINTF_FMT:
			if (fmt->fmt_type != (char)PFT_STRING && 
				fmt->fmt_type != (char)PFT_VALUE) 
			{
				// don't need to calc widths for anything but string or value formats
				// for now we assume that INT, FLOAT and NONE formats have fixed width.
				continue; 
			}

			{
				ExprTree *tree = NULL;
				classad::Value result;
				bool eval_ok = false;

				if(tree = al->LookupExpr (attr)) {
					eval_ok = EvalExprTree(tree, al, target, result);
				} else {
						// drat, we couldn't find it. Maybe it's an
						// expression?
					tree = NULL;
					if( 0 == ParseClassAdRvalExpr(attr, tree) != 0 ) {
						eval_ok = EvalExprTree(tree, al, target, result);
						delete tree;
						tree = NULL;
					}
				}
				if (eval_ok) {
					bool fQuote = fmt->fmt_letter == 'V';
					std::string buff;
					if ( fQuote || !result.IsStringValue( buff ) ) {
						classad::ClassAdUnParser unparser;
						unparser.SetOldClassAd( true, true );
						unparser.Unparse( buff, val );
					}
					colval = buff.c_str();
				}
			}
			break;

		case INT_CUSTOM_FMT:
			{
				int intValue;
				if (al->EvalInteger(attr, target, intValue)) {
					colval = (fmt->df)(intValue , al, *fmt);
				}
			}
			break;

		case FLT_CUSTOM_FMT:
			{
				double realValue;
				if (al->EvalFloat(attr, target, realValue)) {
					colval = (fmt->ff)(realValue , al, *fmt);
				}
			}
			break;

		case STR_CUSTOM_FMT:
			if (al->EvalString(attr, target, &value_from_classad)) {
				colval = (fmt->sf)(value_from_classad, al, *fmt);
				free(value_from_classad);
			}
			break;

		default:
			continue;
			break;
		}

		int width = colval.Length();
		fmt->width = MAX(fmt->width, width);
	}

	return 0;
}
#endif

char * AttrListPrintMask::
display_Headings(List<const char> & headings)
{
	Formatter *fmt;
	formats.Rewind();

	int columns = formats.Length();
	int icol = 0;

	MyString retval("");
	if (row_prefix)
		retval = row_prefix;

	headings.Rewind();

	// for each item registered in the print mask
	while ((fmt = formats.Next()) != NULL)
	{
		const char * pszHead = headings.Next();
		if ( ! pszHead) break;

		if ((icol != 0) && col_prefix && ! (fmt->options & FormatOptionNoPrefix)) {
			retval += col_prefix;
		}

		MyString tmp_fmt;
		if (fmt->width) {
			tmp_fmt.formatstr("%%-%ds", fmt->width);
			retval.formatstr_cat(tmp_fmt.Value(), pszHead);
		} else {
			retval += pszHead;
		}

		if ((++icol < columns) && col_suffix && ! (fmt->options & FormatOptionNoSuffix)) {
			retval += col_suffix;
		}

	}

	if (overall_max_width && retval.Length() > overall_max_width)
		retval.setChar(overall_max_width, 0);

	if (row_suffix)
		retval += row_suffix;

	// Convert return MyString to new char *.
	return strnewp(retval.Value() );
}

char * AttrListPrintMask::
display_Headings(const char * pszzHead)
{
	List<const char> headings;

	// init headings List from input. 
	// input is assumed to have a \0 between each heading
	// with a \0\0 representing the end of the string.
	const char * pszz = pszzHead;
	size_t cch = strlen(pszz);
	while (cch > 0) {
		headings.Append(pszz);
		pszz += cch+1;
		cch = strlen(pszz);
	}
	return display_Headings(headings);
}

int AttrListPrintMask::
display_Headings (FILE *file, List<const char> & headings) {
	char * head = display_Headings(headings);
	if (head) {
		fputs(head, file);
		delete [] head;
		return 0;
	}
	return 1;
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

// append column data to the row string
//
void AttrListPrintMask::
PrintCol(MyString * prow, Formatter & fmt, const char * value)
{
	char tmp_fmt[40];

	if (col_prefix && ! (fmt.options & FormatOptionNoPrefix))
		(*prow) += col_prefix;

	int col_start = prow->Length();

	const char * printfFmt = fmt.printfFmt;
	if ( ! printfFmt && fmt.width) {
		int width = (fmt.options & FormatOptionLeftAlign) ? -fmt.width : fmt.width;
		if (fmt.options & FormatOptionNoTruncate) {
			sprintf(tmp_fmt, "%%%ds", width);
		} else {
			sprintf(tmp_fmt, "%%%d.%ds", width, fmt.width);
		}
		printfFmt = tmp_fmt;
		fmt.fmt_type = (char)PFT_STRING;
		fmt.fmt_letter = 's';
	}

	if (printfFmt && (fmt.fmt_type == PFT_STRING)) {
		prow->formatstr_cat(printfFmt, value ? value : "");
	} else if (value) {
		(*prow) += value;
	}

	if (fmt.options & FormatOptionAutoWidth) {
		int col_width = prow->Length() - col_start;
		fmt.width = MAX(fmt.width, col_width);
	}

	if (col_suffix && ! (fmt.options & FormatOptionNoSuffix))
		(*prow) += col_suffix;
}

// returns a new char * that is your responsibility to delete.
char * AttrListPrintMask::
display (AttrList *al, AttrList *target /* = NULL */)
{
	Formatter *fmt;
	char 	*attr, *alt;
	ExprTree *tree;
	classad::Value result;
	MyString  retval("");
	int		intValue;
	double	realValue;
	MyString stringValue;
	const char*	bool_str = NULL;
	char *value_from_classad = NULL;

	struct printf_fmt_info fmt_info;
	printf_fmt_t fmt_type;
	const char* tmp_fmt = NULL;

	formats.Rewind();
	attributes.Rewind();
	alternates.Rewind();

	int columns = formats.Length();
	int icol = 0;

	if (row_prefix)
		retval = row_prefix;

	// for each item registered in the print mask
	while ((fmt=formats.Next()) && (attr=attributes.Next()) &&
				(alt=alternates.Next()))
	{
		if (icol == 0) {
			fmt->options |= FormatOptionNoPrefix;
		}
		if (++icol == columns) {
			fmt->options |= FormatOptionNoSuffix;
		}
			// If we decide that the "attr" requested is actually
			// an expression, we need to remember that, as 1.
			// it needs to be deleted, and 2. there is some
			// special handling for the string case.
		bool attr_is_expr = false;
		int  col_start;
		switch( fmt->fmtKind )
		{
		  	case PRINTF_FMT:
				if (col_prefix && ! (fmt->options & FormatOptionNoPrefix))
					retval += col_prefix;
				col_start = retval.Length();

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
					if (fmt->options & FormatOptionAutoWidth) {
						int col_width = retval.Length() - col_start;
						fmt->width = MAX(fmt->width, col_width);
					}
					if (col_suffix && ! (fmt->options & FormatOptionNoSuffix))
						retval += col_suffix;
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
						if (fmt->options & FormatOptionAutoWidth) {
							int col_width = retval.Length() - col_start;
							fmt->width = MAX(fmt->width, col_width);
						}
						if (col_suffix && ! (fmt->options & FormatOptionNoSuffix))
							retval += col_suffix;
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
						std::string buff;
						if( EvalExprTree(tree, al, target, result) &&
							result.IsStringValue(buff) ) {
							retval.formatstr_cat(fmt->printfFmt, buff.c_str());
						} else {
							// couldn't eval
							if( alt ) {
								retval += alt;
							}
						}
					} else if( al->EvalString( attr, target, &value_from_classad ) ) {
						stringValue.formatstr( fmt->printfFmt,
											 value_from_classad );
						retval += stringValue;
						free( value_from_classad );
						value_from_classad = NULL;
					} else {
						bool_str = ExprTreeToString( tree );
						if( bool_str ) {
							stringValue.formatstr(fmt->printfFmt, bool_str);
							retval += stringValue;
						} else {
							if ( alt ) {
								retval += alt;
							}
						}
					}
					break;

				case PFT_VALUE:
					{
#if 1
						const char * pszValue = alt;
						std::string buff;
						if( EvalExprTree(tree, al, target, result) ) {
							// Only strings are formatted differently for
							// %v vs %V
							bool fQuote = (fmt_info.fmt_letter == 'V');
							if ( fQuote || !result.IsStringValue(buff) ) {
								classad::ClassAdUnParser unparser;
								unparser.SetOldClassAd( true, true );
								unparser.Unparse( buff, result );
							}
							pszValue = buff.c_str();
						}

						if ((fmt->options & FormatOptionAutoWidth) && strlen(fmt->printfFmt) == 2) {
							char tfmt[40];
							int width = (fmt->options & FormatOptionLeftAlign) ? -fmt->width : fmt->width;
							if ( ! width) {
								stringValue = pszValue;
							} else {
								if (fmt->options & FormatOptionNoTruncate) {
									sprintf(tfmt, "%%%ds", width);
								} else {
									sprintf(tfmt, "%%%d.%ds", width, fmt->width);
								}
								stringValue.formatstr( tfmt, pszValue );
							}
						} else {
							char * tfmt = strdup(fmt->printfFmt); ASSERT(tfmt);
							char * ptag = tfmt + ((tmp_fmt-1) - fmt->printfFmt);
							//bool fQuote = (*ptag == 'V');
							if (*ptag == 'v' || *ptag == 'V')
								*ptag = 's'; // convert printf format to %s
							stringValue.formatstr( tfmt, pszValue );
							free(tfmt);
						}
						retval += stringValue;
#else
						// copy the printf format so we can change %v to some other format specifier.
						char * tfmt = strdup(fmt->printfFmt); ASSERT(tfmt);
						char * ptag = tfmt + ((tmp_fmt-1) - fmt->printfFmt);
						bool fQuote = (*ptag == 'V');
						classad::Value val;
						std::string buff;
						if (*ptag == 'v' || *ptag == 'V')
							*ptag = 's'; // convert printf format to %s
						if( EvalExprTree(tree, al, target, val) ) {
							// Only strings are formatted differently for
							// %v vs %V
							if ( fQuote || !val.IsStringValue( buff ) ) {
								classad::ClassAdUnParser unparser;
								unparser.SetOldClassAd( true, true );
								unparser.Unparse( buff, val );
								stringValue.sprintf( tfmt, buff.c_str() );
							}
							stringValue.sprintf( tfmt, buff.c_str() );
							retval += stringValue;
						} else {
								// couldn't eval
							if( alt ) {
								stringValue.sprintf( tfmt, alt );
								retval += stringValue;
							}
						}
						free(tfmt);
#endif
					}
					break;

				case PFT_INT:
				case PFT_FLOAT:
					if( EvalExprTree(tree, al, target, result) ) {
						switch( result.GetType() ) {
						case classad::Value::REAL_VALUE:
							double d;
							result.IsRealValue( d );
							if( fmt_type == PFT_INT ) {
								stringValue.formatstr( fmt->printfFmt, 
													 (int)d );
							} else {
								stringValue.formatstr( fmt->printfFmt, d );
							}
							retval += stringValue;
							break;

						case classad::Value::INTEGER_VALUE:
							int i;
							result.IsIntegerValue( i );
							if( fmt_type == PFT_INT ) {
								stringValue.formatstr( fmt->printfFmt, 
													 i );
							} else {
								stringValue.formatstr( fmt->printfFmt, 
													 (double)i );
							}
							retval += stringValue;
							break;

						case classad::Value::BOOLEAN_VALUE:
							bool b;
							result.IsBooleanValue( b );
							if( fmt_type == PFT_INT ) {
								stringValue.formatstr( fmt->printfFmt, 
													 b ? 1 : 0 );
							} else {
								stringValue.formatstr( fmt->printfFmt, 
													 b ? 1.0 : 0.0 );
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
				if (fmt->options & FormatOptionAutoWidth) {
					int col_width = retval.Length() - col_start;
					fmt->width = MAX(fmt->width, col_width);
				}
				if (col_suffix && ! (fmt->options & FormatOptionNoSuffix))
					retval += col_suffix;
				if(attr_is_expr) { delete tree; tree = NULL; }
				break;

			{
			const char * pszVal;

		  	case INT_CUSTOM_FMT:
				if( al->EvalInteger( attr, target, intValue ) ) {
					pszVal = (fmt->df)(intValue , al, *fmt);
				} else {
					pszVal = alt;
				}
				PrintCol(&retval, *fmt, pszVal);
				break;

		  	case FLT_CUSTOM_FMT:
				if( al->EvalFloat( attr, target, realValue ) ) {
					pszVal = (fmt->ff)(realValue , al, *fmt);
				} else {
					pszVal = alt;
				}
				PrintCol(&retval, *fmt, pszVal);
				break;

		  	case STR_CUSTOM_FMT:
				if( al->EvalString( attr, target, &value_from_classad ) ) {
					pszVal = (fmt->sf)(value_from_classad, al, *fmt);
					free(value_from_classad);
				} else {
					pszVal = alt;
				}
				PrintCol(&retval, *fmt, pszVal);
				break;
	
			default:
				PrintCol(&retval, *fmt, alt);
				break;
			}
		}
	}

	if (overall_max_width && retval.Length() > overall_max_width)
		retval.setChar(overall_max_width, 0);

	if (row_suffix)
		retval += row_suffix;

	// Convert return MyString to new char *.
	return strnewp(retval.Value() );
}


int AttrListPrintMask::
display (FILE *file, AttrListList *list, AttrList *target /* = NULL */, List<const char> * pheadings /* = NULL */)
{
	int retval = 1;

	list->Open();

	AttrList *al = (AttrList *) list->Next();

	if (al && pheadings) {
		// render the first line to a string so the column widths update
		char * tmp = display(al, target);
		delete [] tmp;
		display_Headings(file, *pheadings);
	}

	while( al ) {
		if( !display (file, al, target) ) {
			retval = 0;
		}
		al = (AttrList *) list->Next();
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
		//if( x->fmtKind == PRINTF_FMT ) delete [] x->printfFmt;
		if( x->printfFmt ) delete [] x->printfFmt;
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
		//if( newItem->fmtKind == PRINTF_FMT )
		if( item->printfFmt )
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

/*
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
*/
