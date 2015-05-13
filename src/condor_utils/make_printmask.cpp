/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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
//#include "escapes.h"
//#include "MyString.h"
#include "condor_string.h"	// for getline
#include <string>
//#include "printf_format.h"

const char * SimpleFileInputStream::nextline()
{
	// getline from condor_string.h automatically collapses line continuations.
	// and uses a static internal buffer to hold the latest line. 
	const char * line = getline_trim(file, lines_read);
	return line;
}

// collapse a c++ escapes in-place in a std::string
bool collapse_escapes(std::string & str)
{
	const char *strp = str.c_str();
	const char *cp = strp;

	// skip over leading non escape characters
	while (*cp && *cp != '\\') ++cp;
	if ( ! *cp) return false;

	size_t ix = cp - strp;
	int cEscapes = 0;

	while (*cp) {
		// ASSERT: *cp == '\\'

		// assume we found an escape
		++cEscapes;

		switch (*++cp) {
			case 'a':	str[ix] = '\a'; break;
			case 'b':	str[ix] = '\b'; break;
			case 'f':	str[ix] = '\f'; break;
			case 'n':	str[ix] = '\n'; break;
			case 'r':	str[ix] = '\r'; break;
			case 't':	str[ix] = '\t'; break;
			case 'v':	str[ix] = '\v'; break;

			case '\\':
			case '\'':
			case '"':
			case '?':	str[ix] = *cp; break;

			case 'X':
			case 'x':	{ // hexadecimal sequence
				int value = 0;
				while (cp[1] && isxdigit(cp[1])) {
					int ch = cp[1];
					value += value*16 + isdigit(ch) ? (ch - '0') : (tolower(ch) - 'a' + 10);
					++cp;
				}
				str[ix] = (char)value;
			}
			break;

			default:
				// octal sequence
				if (isdigit(*cp) ) {
					int value = *cp - '0';
					while (cp[1] && isdigit(cp[1])) {
						int ch = cp[1];
						value += value*8 + (ch - '0');
						++cp;
					}
					str[ix] = (char)value;
				} else {
 					// not really an escape, so copy both characters
					--cEscapes;
					str[ix] = '\\';
					str[++ix] = *cp;
				}
			break;
		}
		// at this point cp points to the last char of the escape sequence
		// and ix is the offset of the collapsed escape character that we just wrote.
		if ( ! str[ix]) break;

		// scan forward, copying characters until we get to next \ or copy the terminating null.
		do { str[++ix] = *++cp; } while (*cp && *cp != '\\');
	}

	// ASSERT str[ix] == 0

	// return true if there were escapes.
	if ( ! cEscapes) {
		return false;
	}
	str.resize(ix);
	return true;
}


// this is a simple tokenizer class for parsing keywords out of a line of text
// token separator defaults to whitespace, "" or '' can be used to have tokens
// containing whitespace, but there is no way to escape " inside a "" string or
// ' inside a '' string. outer "" and '' are not considered part of the token.
// next() advances to the next token and returns false if there is no next token.
// matches() can compare the current token to a string without having to extract it
// copy_marked() returns all of the text from the most recent mark() to the start
// of the current token. (may include leading and trailing whitespace).
//
class tokener {
public:
	tokener(const char * line_in) : line(line_in), ix_cur(0), cch(0), ix_next(0), ix_mk(0), sep(" \t\r\n") { }
	bool set(const char * line_in) { if ( ! line_in) return false; line=line_in; ix_cur = ix_next = 0; return true; }
	bool next() {
		ix_cur = line.find_first_not_of(sep, ix_next);
		if (ix_cur != std::string::npos && (line[ix_cur] == '"' || line[ix_cur] == '\'')) {
			ix_next = line.find(line[ix_cur], ix_cur+1);
			ix_cur += 1; // skip leading "
			cch = ix_next - ix_cur;
			if (ix_next != std::string::npos) { ix_next += 1; /* skip trailing " */}
		} else {
			ix_next = line.find_first_of(sep, ix_cur);
			cch = ix_next - ix_cur;
		}
		return ix_cur != std::string::npos;
	};
	bool matches(const char * pat) const { return line.substr(ix_cur, cch) == pat; }
	bool starts_with(const char * pat) const { size_t cpat = strlen(pat); return cpat >= cch && line.substr(ix_cur, cpat) == pat; }
	bool less_than(const char * pat) const { return line.substr(ix_cur, cch) < pat; }
	void copy_token(std::string & value) const { value = line.substr(ix_cur, cch); }
	void copy_to_end(std::string & value) const { value = line.substr(ix_cur); }
	bool at_end() const { return ix_next == std::string::npos; }
	void mark() { ix_mk = ix_cur; }
	void mark_after() { ix_mk = ix_next; }
	void copy_marked(std::string & value) const { value = line.substr(ix_mk, ix_cur - ix_mk); }
	std::string & content() { return line; }
	size_t offset() { return ix_cur; }
	bool is_quoted_string() const { return ix_cur > 0 && (line[ix_cur-1] == '"' || line[ix_cur-1] == '\''); }
private:
	std::string line;  // the line currently being tokenized
	size_t ix_cur;     // start of the current token
	size_t cch;        // length of the current token
	size_t ix_next;    // start of the next token
	size_t ix_mk;      // start of current 'marked' region
	const char * sep;  // separator characters used to delimit tokens
};

template <class T>
const T * tokener_lookup_table<T>::find_match(const tokener & toke) const {
	if (cItems <= 0) return NULL;
	if (is_sorted) {
		for (int ixLower = 0, ixUpper = cItems-1; ixLower <= ixUpper;) {
			int ix = (ixLower + ixUpper) / 2;
			if (toke.matches(pTable[ix].key))
				return &pTable[ix];
			else if (toke.less_than(pTable[ix].key))
				ixUpper = ix-1;
			else
				ixLower = ix+1;
		}
	} else {
		for (int ix = 0; ix < (int)cItems; ++ix) {
			if (toke.matches(pTable[ix].key))
				return &pTable[ix];
		}
	}
	return NULL;
}

typedef struct {
	const char * key;
	int          value;
	int          options;
} Keyword;
typedef tokener_lookup_table<Keyword> KeywordTable;

enum {
	kw_AS=1,
	kw_AUTO,
	kw_PRINTF,
	kw_PRINTAS,
	kw_NOPREFIX,
	kw_NOSUFFIX,
	kw_TRUNCATE,
	kw_WIDTH,
	kw_LEFT,
	kw_RIGHT,
};

#define KW(a) { #a, kw_##a, 0 }
static const Keyword SelectKeywordItems[] = {
	KW(AS),
	KW(AUTO),
	KW(LEFT),
	KW(NOPREFIX),
	KW(NOSUFFIX),
	KW(PRINTAS),
	KW(PRINTF),
	KW(RIGHT),
	KW(TRUNCATE),
	KW(WIDTH),
};
#undef KW
static const KeywordTable SelectKeywords = SORTED_TOKENER_TABLE(SelectKeywordItems);

enum {
	gw_AS=1,
	gw_ASCENDING,
	gw_DECENDING,
};
#define GW(a) { #a, gw_##a, 0 }
static const Keyword GroupKeywordItems[] = {
	GW(AS),
	GW(ASCENDING),
	GW(DECENDING),
};
#undef KW
static const KeywordTable GroupKeywords = SORTED_TOKENER_TABLE(GroupKeywordItems);

static void unexpected_token(std::string & message, const char * tag, SimpleInputStream & stream, tokener & toke)
{
	std::string tok; toke.copy_token(tok);
	formatstr_cat(message, "%s was unexpected at line %d offset %d in %s\n",
		tok.c_str(), stream.count_of_lines_read(), (int)toke.offset(), tag);
}

static void expected_token(std::string & message, const char * reason, const char * tag, SimpleInputStream & stream, tokener & toke)
{
	std::string tok; toke.copy_token(tok);
	formatstr_cat(message, "expected %s at line %d offset %d in %s\n",
		reason, stream.count_of_lines_read(), (int)toke.offset(), tag);
}

// Read a stream a line at a time, and parse it to fill out the print mask,
// header, group_by, where expression, and projection attributes.
//
int SetAttrListPrintMaskFromStream (
	SimpleInputStream & stream, // in: fetch lines from this stream until nextline() returns NULL
	const CustomFormatFnTable & FnTable, // in: table of custom output functions for SELECT
	AttrListPrintMask & mask, // out: columns and headers set in SELECT
	printmask_headerfooter_t & headfoot, // out: header and footer flags set in SELECT or SUMMARY
	printmask_aggregation_t & aggregate, // out: aggregation mode in SELECT
	std::vector<GroupByKeyInfo> & group_by, // out: ordered set of attributes/expressions in GROUP BY
	std::string & where_expression, // out: classad expression from WHERE
	StringList & attrs, // out ClassAd attributes referenced in mask or group_by outputs
	std::string & error_message) // out, if return is non-zero, this will be an error message
{
	ClassAd ad; // so we can GetExprReferences
	enum section_t { NOWHERE=0, SELECT, SUMMARY, WHERE, GROUP};
	enum cust_t { PRINTAS_STRING, PRINTAS_INT, PRINTAS_FLOAT };

	bool label_fields = false;
	const char * labelsep = " = ";
	const char * prowpre = NULL;
	const char * pcolpre = " ";
	const char * pcolsux = NULL;
	const char * prowsux = "\n";
	mask.SetAutoSep(prowpre, pcolpre, pcolsux, prowsux);

	error_message.clear();
	aggregate = PR_NO_AGGREGATION;

	printmask_headerfooter_t usingHeadFoot = (printmask_headerfooter_t)(HF_CUSTOM | HF_NOSUMMARY);
	section_t sect = SELECT;
	tokener toke("");
	while (toke.set(stream.nextline())) {
		if ( ! toke.next())
			continue;

		if (toke.matches("#")) continue;

		if (toke.matches("SELECT"))	{
			while (toke.next()) {
				if (toke.matches("FROM")) {
					if (toke.next()) {
						if (toke.matches("AUTOCLUSTER")) {
							aggregate = PR_FROM_AUTOCLUSTER;
						} else {
							std::string aa; toke.copy_token(aa);
							formatstr_cat(error_message, "Warning: Unknown header argument %s for SELECT FROM\n", aa.c_str());
						}
					}
				} else if (toke.matches("UNIQUE")) {
					aggregate = PR_COUNT_UNIQUE;
				} else if (toke.matches("BARE")) {
					usingHeadFoot = HF_BARE;
				} else if (toke.matches("NOTITLE")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOTITLE);
				} else if (toke.matches("NOHEADER")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOHEADER);
				} else if (toke.matches("NOSUMMARY")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOSUMMARY);
				} else if (toke.matches("LABEL")) {
					label_fields = true;
				} else if (label_fields && toke.matches("SEPARATOR")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); labelsep = mask.store(tmp.c_str()); }
				} else if (toke.matches("RECORDPREFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); prowpre = mask.store(tmp.c_str()); }
				} else if (toke.matches("RECORDSUFFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); prowsux = mask.store(tmp.c_str()); }
				} else if (toke.matches("FIELDPREFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); pcolpre = mask.store(tmp.c_str()); }
				} else if (toke.matches("FIELDSUFFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); pcolsux = mask.store(tmp.c_str()); }
				} else {
					std::string aa; toke.copy_token(aa);
					formatstr_cat(error_message, "Warning: Unknown header argument %s for SELECT\n", aa.c_str());
				}
			}
			mask.SetAutoSep(prowpre, pcolpre, pcolsux, prowsux);
			sect = SELECT;
			continue;
		} else if (toke.matches("WHERE")) {
			sect = WHERE;
			if ( ! toke.next()) continue;
		} else if (toke.matches("GROUP")) {
			sect = GROUP;
			if ( ! toke.next() || (toke.matches("BY") && ! toke.next())) continue;
		} else if (toke.matches("SUMMARY")) {
			usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot & ~HF_NOSUMMARY);
			while (toke.next()) {
				if (toke.matches("STANDARD")) {
					// attrs.insert(ATTR_JOB_STATUS);
				} else if (toke.matches("NONE")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOSUMMARY);
				} else {
					std::string aa; toke.copy_token(aa);
					formatstr_cat(error_message, "Unknown argument %s for SELECT\n", aa.c_str());
				}
			}
			sect = SUMMARY;
			continue;
		}

		switch (sect) {
		case SELECT: {
			toke.mark();
			std::string attr;
			std::string name;
			int opts = FormatOptionAutoWidth | FormatOptionNoTruncate;
			const char * fmt = "%v";
			int wid = 0;
			CustomFormatFn cust;

			bool got_attr = false;
			while (toke.next()) {
				const Keyword * pkw = SelectKeywords.find_match(toke);
				if ( ! pkw)
					continue;

				// next token is a keyword, if we havent set the attribute yet
				// it's everything up to the current token.
				int kw = pkw->value;
				if ( ! got_attr) {
					toke.copy_marked(attr);
					got_attr = true;
				}

				switch (kw) {
				case kw_AS: {
					if (toke.next()) {
						toke.copy_token(name);
						if (toke.is_quoted_string()) { collapse_escapes(name); }
					} else {
						expected_token(error_message, "column name after AS", "SELECT", stream, toke);
					}
					toke.mark_after();
				} break;
				case kw_PRINTF: {
					if (toke.next()) {
						std::string val; toke.copy_token(val);
						fmt = mask.store(val.c_str());
					} else {
						expected_token(error_message, "format after PRINTF", "SELECT", stream, toke);
					}
				} break;
				case kw_PRINTAS: {
					if (toke.next()) {
						const CustomFormatFnTableItem * pcffi = FnTable.find_match(toke);
						if (pcffi) {
							cust = pcffi->cust;
							//cust_type = pcffi->cust;
							const char * pszz = pcffi->extra_attribs;
							if (pszz) {
								size_t cch = strlen(pszz);
								while (cch > 0) { attrs.insert(pszz); pszz += cch+1; cch = strlen(pszz); }
							}
						} else {
							std::string aa; toke.copy_token(aa);
							formatstr_cat(error_message, "Unknown argument %s for PRINTAS\n", aa.c_str());
						}
					} else {
						expected_token(error_message, "function name after PRINTAS", "SELECT", stream, toke);
					}
				} break;
				case kw_NOSUFFIX: {
					opts |= FormatOptionNoSuffix;
				} break;
				case kw_NOPREFIX: {
					opts |= FormatOptionNoPrefix;
				} break;
				case kw_LEFT: {
					opts |= FormatOptionLeftAlign;
				} break;
				case kw_RIGHT: {
					opts &= ~FormatOptionLeftAlign;
				} break;
				case kw_TRUNCATE: {
					opts &= ~FormatOptionNoTruncate;
				} break;
				case kw_WIDTH: {
					if (toke.next()) {
						std::string val; toke.copy_token(val);
						if (toke.matches("AUTO")) {
							opts |= FormatOptionAutoWidth;
						} else {
							wid = atoi(val.c_str());
							//if (wid) opts &= ~FormatOptionAutoWidth;
							//PRAGMA_REMIND("TJ: decide how LEFT & RIGHT interact with pos and neg widths."
						}
					} else {
						expected_token(error_message, "number or AUTO after WIDTH", "SELECT", stream, toke);
					}
				} break;
				default:
					unexpected_token(error_message, "SELECT", stream, toke);
				break;
				} // switch
			} // while

			if ( ! got_attr) { attr = toke.content(); }
			trim(attr);
			if (attr.empty() || attr[0] == '#') continue;

			const char * lbl = name.empty() ? attr.c_str() : name.c_str();
			if (label_fields) {
				// build a format string that contains the label
				std::string label(lbl);
				if (labelsep) { label += labelsep; }
				if (fmt) { label += fmt; } 
				else {
					label += "%";
					if (wid) {
						if (opts & FormatOptionNoTruncate)
							formatstr_cat(label, "%d", wid);
						else
							formatstr_cat(label, "%d.%d", wid, wid < 0 ? -wid : wid);
					}
					label += cust ? "s" : "v";
				}
				lbl = mask.store(label.c_str());
				fmt = lbl;
				wid = 0;
			} else {
				if ( ! wid) { wid = 0 - (int)strlen(lbl); }
				mask.set_heading(lbl);
				lbl = NULL;
			}
			if (cust) {
				mask.registerFormat (lbl, wid, opts, cust, attr.c_str());
			} else {
				mask.registerFormat(fmt, wid, opts, attr.c_str());
			}
			ad.GetExprReferences(attr.c_str(), NULL, &attrs);
		}
		break;

		case WHERE: {
			toke.copy_to_end(where_expression);
			trim(where_expression);
		}
		break;

		case SUMMARY: {
		}
		break;

		case GROUP: {
			toke.mark();
			GroupByKeyInfo key;

			// in case we end up finding no keywords, copy the remainder of the line now as the expression
			toke.copy_to_end(key.expr);
			bool got_expr = false;

			while (toke.next()) {
				const Keyword * pgw = GroupKeywords.find_match(toke);
				if ( ! pgw)
					continue;

				// got a keyword
				int gw = pgw->value;
				if ( ! got_expr) {
					toke.copy_marked(key.expr);
					got_expr = true;
				}

				switch (gw) {
				case gw_AS: {
					if (toke.next()) { toke.copy_token(key.name); }
					toke.mark_after();
				} break;
				case gw_DECENDING: {
					key.decending = true;
					toke.mark_after();
				} break;
				case gw_ASCENDING: {
					key.decending = false;
					toke.mark_after();
				} break;
				default:
					unexpected_token(error_message, "GROUP BY", stream, toke);
				break;
				} // switch
			} // while toke.next

			trim(key.expr);
			if (key.expr.empty() || key.expr[0] == '#')
				continue;

			if ( ! ad.GetExprReferences(key.expr.c_str(), NULL, &attrs)) {
				formatstr_cat(error_message, "GROUP BY expression is not valid: %s\n", key.expr.c_str());
			} else {
				group_by.push_back(key);
			}
		}
		break;

		default:
		break;
		}
	}

	headfoot = usingHeadFoot;

	return 0;
}
