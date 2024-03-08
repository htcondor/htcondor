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
#include "printf_format.h"
#include "format_time.h"

static char *new_strdup (const char *);

AttrListPrintMask::
AttrListPrintMask ()
	: overall_max_width(0)
	, row_prefix(NULL)
	, col_prefix(NULL)
	, col_suffix(NULL)
	, row_suffix(NULL)
	, stringpool(3)
{
}

AttrListPrintMask::
~AttrListPrintMask ()
{
	clearFormats ();
	clearPrefixes ();
	stringpool.clear();
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
commonRegisterFormat (int wid, int opts, const char *print,
                      const CustomFormatFn & sf,
					  const char *attr
					 )
{
	Formatter *newFmt = new Formatter;
	memset(newFmt, 0, sizeof(*newFmt));

	newFmt->fmtKind = sf.Kind();
	newFmt->sf = sf;
	newFmt->width = abs(wid);
	newFmt->options = opts;
	newFmt->altKind = (char)((opts & (AltQuestion | AltWide | AltMask)) / AltQuestion);
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
	formats.emplace_back (newFmt);

	attributes.emplace_back(new_strdup (attr));
}

void AttrListPrintMask::
registerFormat (const char *print, int wid, int opts, const CustomFormatFn & fmt, const char *attr)
{
	commonRegisterFormat(wid, opts, print, fmt, attr);
}

void AttrListPrintMask::
registerFormat (const char *fmt, int wid, int opts, const char *attr)
{
	commonRegisterFormat(wid, opts, fmt, CustomFormatFn(), attr);
}

void AttrListPrintMask::
clearFormats (void)
{
	clearList (formats);
	clearList (attributes);
	headings.clear();
}

void AttrListPrintMask::
clearPrefixes (void)
{
	if (row_prefix) { delete [] row_prefix; row_prefix = NULL; }
	if (col_prefix) { delete [] col_prefix; col_prefix = NULL; }
	if (col_suffix) { delete [] col_suffix; col_suffix = NULL; }
	if (row_suffix) { delete [] row_suffix; row_suffix = NULL; }
}

int AttrListPrintMask::
adjust_formats(int (*pfn)(void*pv, int index, Formatter * fmt, const char * attr), void* pv)
{
	// for each item registered in the print mask, call pfn giving it a change to make adjustments.
	int index = 0;
	int ret = 0;

	auto ait = attributes.begin();
	for (auto *fmt : formats) {
		if (ait == attributes.end()) break; // just in case it is shorter
		ret = pfn(pv, index, fmt, *ait++);
		if (ret < 0) break;
		++index;
	}
	return ret;
}

int AttrListPrintMask::
walk(int (*pfn)(void*pv, int index, Formatter * fmt, const char * attr, const char * head), void* pv, const std::vector<const char *> * pheadings /*=NULL*/) const
{
	const std::vector<const char *> * phead = &headings;
	if (pheadings) phead = const_cast< std::vector<const char *> * >(pheadings);


	auto attrit = attributes.begin();
	auto headit = phead->begin();

	// for each item registered in the print mask, call pfn giving it a change to make adjustments.
	int index = 0;
	int ret = 0;
	for (auto *fmt: formats) {
		if (attrit == attributes.end()) break;
		ret = pfn(pv, index, fmt, *attrit++, *headit++);
		if (ret < 0) break;
		++index;
	}
	return ret;
}



void AttrListPrintMask::set_heading(const char * heading)
{
	if (heading && heading[0]) {
		headings.emplace_back(stringpool.insert(heading));
	} else {
		headings.emplace_back("");
	}
}


char * AttrListPrintMask::display_Headings(std::vector<const char *> & headings)
{
	int columns = formats.size();
	int icol = 0;

	std::string retval;
	if (row_prefix)
		retval = row_prefix;

	auto headit = headings.begin();

	// for each item registered in the print mask
	for (auto *fmt: formats) 
	{
		if (headit == headings.end()) break;
		const char * pszHead = *headit++;
		if ( ! pszHead) break;

		if (fmt->options & FormatOptionHideMe) {
			++icol;
			continue;
		}

		if ((icol != 0) && col_prefix && ! (fmt->options & FormatOptionNoPrefix)) {
			retval += col_prefix;
		}

		std::string tmp_fmt;
		if (fmt->width) {
			formatstr(tmp_fmt, "%%-%ds", fmt->width);
			formatstr_cat(retval, tmp_fmt.c_str(), pszHead);
		} else {
			retval += pszHead;
		}

		if ((++icol < columns) && col_suffix && ! (fmt->options & FormatOptionNoSuffix)) {
			retval += col_suffix;
		}

	}

	if (overall_max_width && (int)retval.length() > overall_max_width)
		retval.erase(overall_max_width);

	if (row_suffix)
		retval += row_suffix;

	// Convert return std::string to new char *.
	return strdup(retval.c_str() );
}

char * AttrListPrintMask::
display_Headings(const char * pszzHead)
{
	std::vector<const char *> headings;

	// init headings List from input. 
	// input is assumed to have a \0 between each heading
	// with a \0\0 representing the end of the string.
	const char * pszz = pszzHead;
	size_t cch = strlen(pszz);
	while (cch > 0) {
		headings.emplace_back(pszz);
		pszz += cch+1;
		cch = strlen(pszz);
	}
	return display_Headings(headings);
}

int AttrListPrintMask::
display_Headings (FILE *file, std::vector<const char *> & headings) {
	char * head = display_Headings(headings);
	if (head) {
		fputs(head, file);
		free(head);
		return 0;
	}
	return 1;
}

int AttrListPrintMask::
display (FILE *file, ClassAd *al, ClassAd *target /* =NULL */)
{
	std::string temp;
	display(temp, al, target);

	if ( ! temp.empty()) {
		fputs(temp.c_str(), file);
		return 0;
	}
	return 1;

}

// append column data to the row string
//
void AttrListPrintMask::
PrintCol(std::string * prow, Formatter & fmt, const char * value)
{
	char tmp_fmt[40];

	if (col_prefix && ! (fmt.options & FormatOptionNoPrefix))
		(*prow) += col_prefix;

	int col_start = prow->length();

	const char * printfFmt = fmt.printfFmt;
	if ( ! printfFmt && fmt.width) {
		int width = (fmt.options & FormatOptionLeftAlign) ? -fmt.width : fmt.width;
		if (fmt.options & FormatOptionNoTruncate) {
			snprintf(tmp_fmt, sizeof(tmp_fmt), "%%%ds", width);
		} else {
			snprintf(tmp_fmt, sizeof(tmp_fmt), "%%%d.%ds", width, fmt.width);
		}
		printfFmt = tmp_fmt;
		fmt.fmt_type = (char)PFT_STRING;
		fmt.fmt_letter = 's';
	}

	if (printfFmt && (fmt.fmt_type == PFT_STRING)) {
		formatstr_cat(*prow, printfFmt, value ? value : "");
	} else if (value) {
		(*prow) += value;
	}

	if (fmt.options & FormatOptionAutoWidth) {
		int col_width = prow->length() - col_start;
		fmt.width = MAX(fmt.width, col_width);
	}

	if (col_suffix && ! (fmt.options & FormatOptionNoSuffix))
		(*prow) += col_suffix;
}

static void appendFieldofChar(std::string & buf, int width, char ch = '?')
{
	int cq = width;
	if ( ! cq)
		return;
	if (cq < 0) 
		cq = 0-width;

	if (cq < 3) {
		buf += ch;
	} else {
		buf.reserve(buf.length() + cq+1);
		buf += '[';
		--cq;
		while (--cq) buf += ch;
		buf += ']';
	}
}

MyRowOfValues::~MyRowOfValues()
{
	if( pdata != NULL ) {
		delete [] pdata;
		pdata = NULL;
	}

	if( pvalid != NULL ) {
		delete [] pvalid;
		pvalid = NULL;
	}

	cmax = cols = 0;
}

int MyRowOfValues::SetMaxCols(int max_cols)
{
	if (max_cols > cmax) {
		classad::Value * pd = new classad::Value[(unsigned int)max_cols];
		unsigned char * pv = new unsigned char[max_cols];
		memset(pv, '\0', max_cols);

		if (pdata) {
			for (int ii = 0; ii < cmax; ++ii) { pd[ii] = pdata[ii]; pv[ii] = pvalid[ii]; }
			delete [] pdata;
			delete [] pvalid;
		}
		pdata = pd;
		pvalid = pv;
		cmax = max_cols;
	}
	return cmax;
}

int MyRowOfValues::cat(const classad::Value & s)
{
	if (pdata && (cols < cmax)) { pvalid[cols] = true; pdata[cols++] = s; }
	return cols;
}

classad::Value * MyRowOfValues::next(int & index) {
	if (pdata && (cols < cmax)) {
		index = cols++;
		pvalid[index] = false;
		return &pdata[index];
	}
	return NULL;
}

static void append_alt(std::string & buf, Formatter & fmt)
{
	static const char alt_chars[] = { ' ', '?', '*', '.', '-', '_', '#', '0', };
	int alt = (int)fmt.altKind * AltQuestion;
	char alt_char = alt_chars[fmt.altKind & 7];
	if (alt & AltWide) {
		appendFieldofChar(buf, fmt.width, alt_char);
	} else if (alt_char != ' ') {
		buf += alt_char;
	}
}

static const char * set_alt(std::string & buf, Formatter & fmt)
{
	buf = "";
	append_alt(buf, fmt);
	return buf.c_str();
}

template <typename t>
static const char * format_value(std::string & str, t & val, printf_fmt_t fmt_type, const Formatter & fmt)
{
	switch (fmt_type) {
	case PFT_FLOAT: formatstr(str, fmt.printfFmt, (double)val);
		break;
	case PFT_POINTER:
	case PFT_CHAR:
	case PFT_INT:   formatstr(str, fmt.printfFmt, (long long)val);
		break;
	case PFT_TIME:  str = format_time(val);
		break;
	case PFT_DATE:  str = format_date((time_t)val);
		break;
	case PFT_RAW: // we don't really expect to see raw and value here...
	case PFT_VALUE:
	case PFT_STRING:  formatstr(str, fmt.printfFmt, val);
		break;
	default:
		ASSERT(0);
		break;
	}
	if (fmt.width > (int)str.length()) {
		str.insert(0, (size_t)fmt.width - str.length(), ' ');
	}
	return str.c_str();
}

#ifdef WIN32
template <>
static const char * format_value<const char *>(std::string & str, const char* & val, printf_fmt_t fmt_type, const Formatter & fmt)
#else
// we really expect this template to catch only the case where val is type const char*
template <typename t>
static const t * format_value(std::string & str, const t* & val, printf_fmt_t fmt_type, const Formatter & fmt)
#endif
{
	switch (fmt_type) {
	case PFT_FLOAT:
	case PFT_TIME:
	case PFT_DATE:
	case PFT_INT:
		ASSERT(0);
		break;
	case PFT_CHAR:
	case PFT_POINTER:
		formatstr(str, fmt.printfFmt, val);
		break;
	case PFT_RAW:
	case PFT_VALUE:
	case PFT_STRING:
		if (fmt.printfFmt) {
			formatstr(str, fmt.printfFmt, val);
		} else {
			char tfmt[40];
			int width = (fmt.options & FormatOptionLeftAlign) ? -fmt.width : fmt.width;
			if ( ! width) {
				str = val ? val : "";
			} else {
				if (fmt.options & FormatOptionNoTruncate) {
					snprintf(tfmt, sizeof(tfmt), "%%%ds", width);
				} else {
					snprintf(tfmt, sizeof(tfmt), "%%%d.%ds", width, fmt.width);
				}
				formatstr(str, tfmt, val);
			}
		}
		break;
	default:
		str = val ? val : "";
		break;
	}
	return str.c_str();
}


//
// ###########################################################
//

static int calc_column_width(Formatter *fmt, classad::Value * pval)
{
	std::string tmp;
	printf_fmt_t fmt_type = (printf_fmt_t)(fmt->fmt_type);
	switch (pval->GetType()) {
	case classad::Value::REAL_VALUE: {
		double realValue  = 0;
		(void) pval->IsRealValue(realValue);
		if (fmt_type == PFT_FLOAT || fmt_type == PFT_INT || fmt_type == PFT_TIME || fmt_type == PFT_DATE) {
			format_value<double>(tmp, realValue, fmt_type, *fmt);
			return (int)tmp.length();
			}
		else if (fmt_type == PFT_VALUE || fmt_type == PFT_STRING || fmt_type == PFT_RAW) {
			classad::ClassAdUnParser unparser;
			std::string buf;
			unparser.Unparse(buf, *pval);
			return (int)buf.length();
			}
		}
		break;

	case classad::Value::INTEGER_VALUE: {
		long long intValue = 0;
		pval->IsNumber(intValue);
		if (fmt_type == PFT_FLOAT || fmt_type == PFT_INT || fmt_type == PFT_TIME || fmt_type == PFT_DATE || fmt_type == PFT_POINTER) {
			format_value<long long>(tmp, intValue, fmt_type, *fmt);
			return (int)tmp.length();
			}
		else if (fmt_type == PFT_VALUE || fmt_type == PFT_STRING || fmt_type == PFT_RAW) {
			formatstr(tmp, "%lld", intValue);
			return (int)tmp.length();
			}
		}
		break;

	case classad::Value::STRING_VALUE: {
		int length = fmt->width;
		if (pval->IsStringValue(length))
			return length;
		}
		break;

	default: /// g++ bitches if there is no default case... <sigh>
		break;
	}
	return fmt->width;
}

//
//
//
int AttrListPrintMask::
render (MyRowOfValues & rov, ClassAd *al, ClassAd *target /* = NULL */)
{
	struct printf_fmt_info fmt_info;
	printf_fmt_t fmt_type = PFT_NONE;
	const char* tmp_fmt = nullptr;

	rov.reset(); // in case a non-empty one was passed in.

	// for each item registered in the print mask
	auto attrit = attributes.begin();
	for (auto *fmt: formats) {
		if (attrit == attributes.end()) break;
		auto *attr = *attrit++;
		int icol = 0; // to hold the icol out param from rov.next
		classad::Value * pval = rov.next(icol);

		// first determine the basic type (int, string, float)
		// that we want to retrieve, and whether we even want to bother
		// to evaluate the attribute/expression
		bool print_no_data = false;
		switch (fmt->fmtKind) {
			case CustomFormatFn::INT_CUSTOM_FMT: fmt_type = PFT_INT;   break;
			case CustomFormatFn::FLT_CUSTOM_FMT: fmt_type = PFT_FLOAT; break;
			case CustomFormatFn::STR_CUSTOM_FMT: fmt_type = PFT_STRING; break;
			case CustomFormatFn::VALUE_CUSTOM_FMT: fmt_type = PFT_VALUE; break;
			default:
					// figure out what kind of format string the
					// caller is using, we will fetch the appropriate
					// value depending on what they want. we also determine
					// that no data is needed here.
				tmp_fmt = fmt->printfFmt;
				if ( ! parsePrintfFormat(&tmp_fmt, &fmt_info) ) {
					print_no_data = true;
				}
				fmt_type = fmt_info.type;
			break;
			case CustomFormatFn::STR_CUSTOM_RENDER:
			case CustomFormatFn::INT_CUSTOM_RENDER:
			case CustomFormatFn::FLT_CUSTOM_RENDER:
			case CustomFormatFn::VALUE_CUSTOM_RENDER:
				fmt_type = PFT_VALUE;
			break;
		}

		if (print_no_data) {
			pval->SetStringValue(fmt->printfFmt ? fmt->printfFmt : "");
			if ((fmt->options & FormatOptionAutoWidth)) { 
				int wid = 0; pval->IsStringValue(wid);
				fmt->width = MAX(fmt->width, wid);
			}
			rov.set_valid(true);
			continue;
		}

			// if we got here, there's some data to be fetched
			// so we'll need to get the expression tree of the
			// attribute they asked for...
			// If we decide that the "attr" requested is actually
			// an expression, we need to remember that, as 1.
			// it needs to be deleted, and 2. there is some
			// special handling for the string case.
		bool attr_is_expr = false;
		ExprTree *tree = NULL;
		if ( ! (tree = al->LookupExpr (attr))) {
				// we couldn't find it. Maybe it's an expression?
			tree = NULL;
			if (ParseClassAdRvalExpr(attr, tree) != 0) {
				delete tree;
				tree = NULL;
			} else {
				attr_is_expr = true; // so we know to delete tree
			}
		}

		bool col_is_valid = false;
		if (tree) {
			// The string format type is a very special case, because it will
			// UNPARSE when the attr is not an expression and doesn't evaluate to a string...
			if (fmt->fmtKind == CustomFormatFn::PRINTF_FMT && (fmt_type == PFT_STRING) && ! attr_is_expr) {
				std::string value_from_classad;
				if (EvalString(attr, al, target, value_from_classad)) {
					pval->SetStringValue(value_from_classad);
					col_is_valid = true;
				} else {
					// For the %s format, if we can't evaluate then unparse.
					fmt_type = PFT_RAW;
				}
			}
			if (fmt_type ==  PFT_RAW) {
				// special case for attr_is_expr when the expression is really an attribute reference.
				if (tree->GetKind() == classad::ExprTree::NodeKind::ATTRREF_NODE) {
					pval->SetStringValue("undefined");
				} else {
					std::string buff;
					classad::ClassAdUnParser unparser;
					unparser.SetOldClassAd(true, true);
					unparser.Unparse(buff, tree);
					pval->SetStringValue(buff);
				}
				col_is_valid = true;
			} else {
				col_is_valid = EvalExprTree(tree, al, target, *pval, classad::Value::ValueType::SAFE_VALUES);
				if (col_is_valid) {
					// since we want to hold on to these Values after we throw away the input ClassAd.
					// We have to unchain any chained ads after flatting out the chain.
					classad::ClassAd * pclassad = NULL;
					if( pval->IsClassAdValue( pclassad ) && pclassad && pclassad->GetChainedParentAd() ) {
						classad::ClassAd * copy = new ClassAd();
						copy->CopyFromChain(*pclassad);
						copy->SetParentScope(nullptr);
						classad_shared_ptr<classad::ClassAd> ca( copy );
						pval->SetClassAdValue(ca);
					}
				}
			}

			// if we made the tree, then unmake it now.
			if (attr_is_expr) { delete tree; tree = NULL; }
		}

		// now give custom renderers a chance to render
		switch (fmt->fmtKind) {
			case CustomFormatFn::INT_CUSTOM_RENDER: {
				long long intValue = 0;
				pval->IsNumber(intValue);
				col_is_valid = fmt->dr(intValue, al, *fmt);
				pval->SetIntegerValue(intValue);
			} break;

			case CustomFormatFn::FLT_CUSTOM_RENDER: {
				double realValue = 0;
				pval->IsNumber(realValue);
				col_is_valid = fmt->fr(realValue , al, *fmt);
				pval->SetRealValue(realValue);
			} break;

			case CustomFormatFn::STR_CUSTOM_RENDER: {
				std::string str;
				pval->IsStringValue(str);
				col_is_valid = fmt->sr(str, al, *fmt);
				pval->SetStringValue(str);
			} break;

			case CustomFormatFn::VALUE_CUSTOM_RENDER:
				col_is_valid = fmt->vr(*pval, al, *fmt);
			break;

			default:
				// for printf and formatters, make sure that the value is of the correct type
				if ( ! col_is_valid) break;
				if (fmt_type == PFT_INT || fmt_type == PFT_CHAR || fmt_type == PFT_TIME) {
					long long intValue = 0;
					col_is_valid = pval->IsNumber(intValue);
					pval->SetIntegerValue(intValue);
				} else if (fmt_type == PFT_FLOAT) {
					double realValue = 0;
					col_is_valid = pval->IsNumber(realValue);
					pval->SetRealValue(realValue);
				} else if (fmt_type == PFT_STRING) {
					col_is_valid = pval->IsStringValue();
				} else if (fmt_type == PFT_RAW) {
					// unparsing should have already happened.
				} else if (fmt_type == PFT_DATE) {
					long long intValue = 0;
					classad::abstime_t date;
					col_is_valid = pval->IsNumber(intValue);
					if (col_is_valid) {
						pval->SetIntegerValue(intValue);
					} else {
						col_is_valid = pval->IsAbsoluteTimeValue(date);
					}
				} else {
					//PRAGMA_REMIND("tj: need other cases here??")
				}
			break;
		}

		if (col_is_valid && (fmt->options & FormatOptionAutoWidth)) {
			int col_width = calc_column_width(fmt, pval);
			fmt->width = MAX(fmt->width, (int)(col_width));
		}

		rov.set_valid(col_is_valid);
	} // while

	return rov.ColCount();
}

int AttrListPrintMask::
display (std::string & out, MyRowOfValues & rov)
{
	std::string mstrValue;
	std::string strValue;
	std::string tfmt;
	long long intValue;
	double    realValue;
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );

	//PRAGMA_REMIND("tj: change this to write directly into the output string")
	int columns = formats.size();
	size_t row_start = out.length();

	if (row_prefix)
		out += row_prefix;

	int icol = 0;

	// for each item registered in the print mask
	for (auto *fmt : formats) {
		if (fmt->options & FormatOptionHideMe) {
			++icol;
			continue;
		}

		if (col_prefix && icol > 0 && ! (fmt->options & FormatOptionNoPrefix))
			out += col_prefix;

		//size_t field_start = out.length();

		classad::Value * pval = rov.Column(icol);
		unsigned char col_is_valid = rov.is_valid(icol);
		const char * pszVal = NULL;
		const char * printfFmt = fmt->printfFmt;
		if (printfFmt && ( ! printfFmt[0] || (printfFmt[0] == '%' && printfFmt[1] == 's' && printfFmt[2] == 0))) printfFmt = NULL;

		switch (fmt->fmtKind) {
		case CustomFormatFn::INT_CUSTOM_FMT:
			if (col_is_valid || (fmt->options & FormatOptionAlwaysCall)) {
				(void) pval->IsNumber(intValue);
				pszVal = fmt->df(intValue, *fmt);
				col_is_valid = true; printfFmt = NULL;
			}
			break;
		case CustomFormatFn::FLT_CUSTOM_FMT:
			if (col_is_valid || (fmt->options & FormatOptionAlwaysCall)) {
				(void) pval->IsNumber(realValue);
				pszVal = fmt->ff(realValue, *fmt);
				col_is_valid = true; printfFmt = NULL;
			}
			break;
		case CustomFormatFn::STR_CUSTOM_FMT:
			if (col_is_valid || (fmt->options & FormatOptionAlwaysCall)) {
				(void) pval->IsStringValue(pszVal);
				pszVal = fmt->sf(pszVal, *fmt);
				col_is_valid = true; printfFmt = NULL;
			}
			break;
		case CustomFormatFn::VALUE_CUSTOM_FMT:
			if (col_is_valid || (fmt->options & FormatOptionAlwaysCall)) {
				pszVal = fmt->vf(*pval, *fmt);
				col_is_valid = true; printfFmt = NULL;
			}
			break;
		default:
			// handle printf formatting below
			break;
		}

		if ( ! col_is_valid) {
			pszVal = set_alt(mstrValue, *fmt);
			printfFmt = NULL;
		} else if (printfFmt) {
			struct printf_fmt_info fmt_info;
			const char * ptag = printfFmt;
			if ( ! parsePrintfFormat(&ptag, &fmt_info)) {
				pszVal = printfFmt;
			} else {
				switch (fmt_info.type) {
				case PFT_TIME:
				case PFT_DATE:
				case PFT_INT:
				case PFT_POINTER:
				case PFT_CHAR:
					(void) pval->IsNumber(intValue);
					pszVal = format_value<long long>(mstrValue, intValue, fmt_info.type, *fmt);
					break;
				case PFT_FLOAT:
					(void) pval->IsNumber(realValue);
					pszVal = format_value<double>(mstrValue, realValue, fmt_info.type, *fmt);
					break;
				case PFT_STRING:
					if ( ! pszVal) pval->IsStringValue(pszVal);
					pszVal = format_value(mstrValue, pszVal, fmt_info.type, *fmt);
					break;
				case PFT_RAW:
				case PFT_VALUE:
					// for 'v' type values, we print strings rather than unparsing them. (for cap V we unparse)
					if ( ! pszVal && fmt_info.fmt_letter != 'V') pval->IsStringValue(pszVal);
					if ( ! pszVal) {
						strValue.clear();
						unparser.Unparse(strValue, *pval);
						pszVal = strValue.c_str();
					}
					tfmt = printfFmt; tfmt[(ptag-1)-printfFmt] = 's';
					formatstr(mstrValue, tfmt.c_str(), pszVal);
					pszVal = mstrValue.c_str();
					break;
				default:  // just here to make g++ shut up
					break;
				}
			}
		} else if ( ! pszVal) {
			(void) pval->IsStringValue(pszVal);
		}

		// at this point, pszVal is the display text for the column
		// now we want to align it to the column width and possibly truncate it.
		//
		size_t col_width = pszVal ? strlen(pszVal) : 0;
		if (fmt->options & FormatOptionAutoWidth) {
			fmt->width = MAX(fmt->width, (int)(col_width));
		}
		if ( ! fmt->width) {
			if (col_width > 0) out += pszVal;
		} else {
			// insert the pszVal aligned and/or truncated to the given column width
			size_t wid = (size_t)((fmt->width < 0) ? -fmt->width : fmt->width);
			if (col_width > wid) {
				if (fmt->options & FormatOptionNoTruncate) {
					out.append(pszVal);
				} else {
					out.append(pszVal, wid);
				}
			} else {
				if ((fmt->width < 0) || (fmt->options & FormatOptionLeftAlign)) {
					if (col_width > 0) out.append(pszVal);
					out.append(wid - col_width, ' '); 
				} else {
					if (wid > col_width) out.append(wid - col_width, ' '); 
					if (col_width > 0) out.append(pszVal);
				}
			}
		}

		++icol;
		if (col_suffix && (icol < columns) && ! (fmt->options & FormatOptionNoSuffix))
			out += col_suffix;
	}

	int width = (int)(out.length() - row_start);
	if ((overall_max_width > 0) && (width > overall_max_width))
		out.erase(row_start + overall_max_width, std::string::npos);

	if (row_suffix)
		out += row_suffix;

	// return number of chars that we added.
	return (int)(out.length() - row_start);
}

int AttrListPrintMask::
display (std::string & out, ClassAd *al, ClassAd *target /* = NULL */)
{
	int columns = formats.size();
	MyRowOfValues rov;
	rov.SetMaxCols(columns);
	render(rov, al, target);
	return display(out, rov);
}


void AttrListPrintMask::
dump(std::string & out, const CustomFormatFnTable * pFnTable, std::vector<const char *> * pheadings /*=NULL*/)
{

	if ( ! pheadings) pheadings = &headings;

	auto attrit = attributes.begin();
	auto headit = pheadings->begin();

	std::string item;
	std::string scratch;

	// for each item registered in the print mask
	for (auto *fmt: formats) {
		if (attrit == attributes.end()) break;
		const char * pszHead = *headit++;
		char 	*attr = *attrit++;
		const char * fnName = "";
		item.clear();
		if (pszHead) { formatstr(item, "HEAD: '%s'\n", pszHead); out += item; }
		if (attr) { formatstr(item, "ATTR: '%s'\n", attr); out += item; }

		// if there is a custom format function, attempt to turn that into a string
		// by looking it up in the given table.
		if (fmt->sf) {
			if (pFnTable) {
				const CustomFormatFnTableItem * ptable = pFnTable->pTable;
				for (int ii = 0; ii < (int)pFnTable->cItems; ++ii) {
					if ((StringCustomFormat)ptable[ii].cust == fmt->sf) {
						fnName = ptable[ii].key;
						break;
					}
				}
			} else {
				formatstr(scratch, "%p", fmt->sf);
				fnName = scratch.c_str();
			}
		}

		formatstr(item, "FMT: %4d %05x %d %d %d %d %s %s\n",
			fmt->width, fmt->options, fmt->fmt_letter, fmt->fmt_type, fmt->fmtKind, fmt->altKind,
			fmt->printfFmt ? fmt->printfFmt : "", fnName);
		out += item;
	}
}

int AttrListPrintMask::
display (FILE *file, ClassAdList *list, ClassAd *target /* = NULL */, std::vector<const char *> * pheadings /* = NULL */)
{
	int retval = 1;

	list->Open();

	ClassAd *al = list->Next();

	if (al && pheadings) {
		// render the first line to a string so the column widths update
		std::string tmp;
		display(tmp, al, target);
		display_Headings(file, *pheadings);
	}

	while( al ) {
		if( !display (file, al, target) ) {
			retval = 0;
		}
		al = list->Next();
	}
    list->Close ();

	return retval;
}

void AttrListPrintMask::
clearList (std::vector<char *> &l)
{
	for (auto *x: l) {
        delete [] x;
    }
	l.clear();
}

void AttrListPrintMask::
clearList (std::vector<Formatter *> &l)
{
	for (auto *x: l) {
		if( x->printfFmt ) delete [] x->printfFmt;
		delete x;
    }
	l.clear();
}

void AttrListPrintMask::
copyList (std::vector<Formatter *> &to, std::vector<Formatter *> &from)
{
	clearList (to);
	for (auto *item: from) {
		Formatter *newItem = new Formatter;
		*newItem = *item;
		//if( newItem->fmtKind == PRINTF_FMT )
		if( item->printfFmt )
			newItem->printfFmt = new_strdup( item->printfFmt );
		to.emplace_back(newItem);
	}
}


void AttrListPrintMask::
copyList (std::vector<char *> &to, std::vector<char *> &from)
{
	clearList (to);
	for (auto *item: from) {
		to.emplace_back(new_strdup (item));
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

int parse_autoformat_args (
	int /*argc*/,
	const char* argv[],
	int ixArg,
	const char *popts,
	AttrListPrintMask & print_mask,
	classad::References & attr_refs,
	bool diagnostic)
{
	bool flabel = false;
	bool fCapV  = false;
	bool fRaw = false;
	bool fheadings = false;
	bool fJobId = false;
	const char * prowpre = NULL;
	const char * pcolpre = " ";
	const char * pcolsux = NULL;
	if (popts) {
		while (*popts) {
			switch (*popts)
			{
				case ',': pcolsux = ","; break;
				case 'n': pcolsux = "\n"; break;
				case 'g': pcolpre = NULL; prowpre = "\n"; break;
				case 't': pcolpre = "\t"; break;
				case 'l': flabel = true; break;
				case 'V': fCapV = true; break;
				case 'r': case 'o': fRaw = true; break;
				case 'h': fheadings = true; break;
				case 'j': fJobId = true; break;
			}
			++popts;
		}
	}
	print_mask.SetAutoSep(prowpre, pcolpre, pcolsux, "\n");

	if (fJobId) {
		if (fheadings || print_mask.has_headings()) {
			print_mask.set_heading(" ID");
			print_mask.registerFormat (flabel ? "ID = %4d." : "%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
			print_mask.set_heading(" ");
			print_mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix, ATTR_PROC_ID);
		} else {
			print_mask.registerFormat (flabel ? "ID = %d." : "%d.", 0, FormatOptionNoSuffix, ATTR_CLUSTER_ID);
			print_mask.registerFormat ("%d", 0, FormatOptionNoPrefix, ATTR_PROC_ID);
		}
	}

	while (argv[ixArg] && *(argv[ixArg]) != '-') {

		const char * parg = argv[ixArg];
		const char * pattr = parg;

		if ( ! IsValidClassAdExpression(pattr, &attr_refs, NULL)) {
			if (diagnostic) {
				printf ("Arg %d --- quitting on invalid expression: [%s]\n", ixArg, pattr);
			}
			return -ixArg;
		}

		std::string lbl = "";
		int wid = 0;
		int opts = FormatOptionNoTruncate;
		if (fheadings || print_mask.has_headings()) {
			const char * hd = fheadings ? parg : "(expr)";
			wid = 0 - (int)strlen(hd);
			opts = FormatOptionAutoWidth | FormatOptionNoTruncate;
			print_mask.set_heading(hd);
		}
		else if (flabel) { formatstr(lbl, "%s = ", parg); wid = 0; opts = 0; }

		lbl += fRaw ? "%r" : (fCapV ? "%V" : "%v");
		if (diagnostic) {
			printf ("Arg %d --- register format [%s] width=%d, opt=0x%x [%s]\n",
				ixArg, lbl.c_str(), wid, opts, pattr);
		}
		print_mask.registerFormat(lbl.c_str(), wid, opts, pattr);
		++ixArg;
	}
	return ixArg;
}


