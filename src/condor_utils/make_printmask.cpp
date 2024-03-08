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
#include "condor_string.h"	// for getline
#include "tokener.h"
#include <charconv>
#include <string>

/* Construct a PrintMask and query from a formatted file/string

  The format is

  SELECT [FROM AUTOCLUSTER | UNIQUE | <AdType>] [BARE | NOTITLE | NOHEADER | NOSUMMARY] [LABEL [SEPARATOR <string>]] [<record-sep>]
    <expr> [AS <label>] [PRINTF <format-string> | PRINTAS <function-name> | WIDTH [AUTO | [-]<INT>] ] [TRUNCATE] [LEFT | RIGHT] [NOPREFIX] [NOSUFFIX]
    ... repeat the above line as needed...
  [JOIN AdType2 (ON <expr-with-adtype2> | USING <keyfield1>,<keyfield2>...)]
  [WHERE <constraint-expr>] // AdType2 constraint if join.
  [AND <constraint-expr>] // Adttype1 constraint if join
  [GROUP BY <sort-expr> [ASCENDING | DESCENDING] ]
    [<sort-expr2> [ASCENDING | DESCENDING]]
    ... repeat the above line as needed...
  [SUMMARY [STANDARD | NONE]]

 *======================================================*/

const char * SimpleFileInputStream::nextline()
{
	// getline from condor_string.h automatically collapses line continuations.
	// and uses a static internal buffer to hold the latest line. 
	const char * line = getline_trim(file, lines_read);
	return line;
}


typedef struct {
	const char * key;
	int          value;
	int          options;
} Keyword;
typedef case_sensitive_sorted_tokener_lookup_table<Keyword> KeywordTable;

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
	kw_OR,
	kw_ALWAYS,
	kw_FIT,
};

#define KW(a) { #a, kw_##a, 0 }
static const Keyword SelectKeywordItems[] = {
	KW(ALWAYS),
	KW(AS),
	KW(AUTO),
	KW(FIT),
	KW(LEFT),
	KW(NOPREFIX),
	KW(NOSUFFIX),
	KW(OR),
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
	gw_DESCENDING,
};
#define GW(a) { #a, gw_##a, 0 }
static const Keyword GroupKeywordItems[] = {
	GW(AS),
	GW(ASCENDING),
	GW(DESCENDING),
};
#undef GW
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
	const CustomFormatFnTable * FnTable, // in: table of custom output functions for SELECT
	AttrListPrintMask & prmask, // out: columns and headers set in SELECT
	PrintMaskMakeSettings & pmms,
	std::vector<GroupByKeyInfo> & group_by, // out: ordered set of attributes/expressions in GROUP BY
	AttrListPrintMask * summask, // out: columns and headers set in SUMMMARY
	std::string & error_message) // out, if return is non-zero, this will be an error message
{
	//ClassAd ad; // so we can GetExprReferences
	enum section_t { NOWHERE=0, SELECT, SUMMARY, JOIN, WHERE, AND, GROUP};
	enum cust_t { PRINTAS_STRING, PRINTAS_INT, PRINTAS_FLOAT };

	bool label_fields = false;
	const char * labelsep = " = ";
	const char * prowpre = NULL;
	const char * pcolpre = " ";
	const char * pcolsux = NULL;
	const char * prowsux = "\n";
	prmask.SetAutoSep(prowpre, pcolpre, pcolsux, prowsux);
	AttrListPrintMask * mask = NULL;
	classad::References * attrs = NULL;
	classad::References * scopes = NULL;
	const CustomFormatFnTable GlobalFnTable = getGlobalPrintFormatTable();

	error_message.clear();

	// dont reset the whole pmms
	// we will leave untouched what the parser doesn't set.
	//pmms.reset();
	pmms.aggregate = PR_NO_AGGREGATION;

	printmask_headerfooter_t usingHeadFoot = HF_NOSUMMARY; // default to no-summary
	section_t sect = SELECT;
	tokener toke("");
	while (toke.set(stream.nextline())) {
		if ( ! toke.next())
			continue;

		if (toke.matches("#")) continue;

		if (toke.matches("SELECT"))	{
			sect = SELECT;
			mask = &prmask;
			attrs = &pmms.attrs;
			scopes = &pmms.scopes;
			while (toke.next()) {
				if (toke.matches("FROM")) {
					if (toke.next()) {
						toke.copy_token(pmms.select_from);
						if (toke.matches("AUTOCLUSTER")) {
							pmms.aggregate = PR_FROM_AUTOCLUSTER;
						}// else {
						//	std::string aa; toke.copy_token(aa);
						//	formatstr_cat(error_message, "Warning: Unknown header argument %s for SELECT FROM\n", aa.c_str());
						//}
					} else {
						expected_token(error_message, "data set name after FROM", "SELECT", stream, toke);
					}
				} else if (toke.matches("UNIQUE")) {
					pmms.aggregate = PR_COUNT_UNIQUE;
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
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); labelsep = mask->store(tmp.c_str()); }
				} else if (toke.matches("RECORDPREFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); prowpre = mask->store(tmp.c_str()); }
				} else if (toke.matches("RECORDSUFFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); prowsux = mask->store(tmp.c_str()); }
				} else if (toke.matches("FIELDPREFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); pcolpre = mask->store(tmp.c_str()); }
				} else if (toke.matches("FIELDSUFFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); pcolsux = mask->store(tmp.c_str()); }
				} else {
					std::string aa; toke.copy_token(aa);
					formatstr_cat(error_message, "Warning: Unknown header argument %s for SELECT\n", aa.c_str());
				}
			}
			prmask.SetAutoSep(prowpre, pcolpre, pcolsux, prowsux);
			continue;
		} else if (toke.matches("JOIN")) {
			sect = JOIN;
			if ( ! toke.next()) continue;
		} else if (toke.matches("WHERE")) {
			sect = WHERE;
			if ( ! toke.next()) continue;
		} else if (toke.matches("AND")) {
			sect = AND;
			if ( ! toke.next()) continue;
		} else if (toke.matches("GROUP")) {
			sect = GROUP;
			if ( ! toke.next() || (toke.matches("BY") && ! toke.next())) continue;
		} else if (toke.matches("SUMMARY")) {
			sect = SUMMARY;
			mask = summask;
			attrs = &pmms.sumattrs;
			scopes = &pmms.sumattrs;
			usingHeadFoot = (printmask_headerfooter_t)((usingHeadFoot | HF_CUSTOM) & ~HF_NOSUMMARY);
			while (toke.next()) {
				if (toke.matches("STANDARD")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot & ~HF_CUSTOM);
				} else if (toke.matches("NONE")) {
					usingHeadFoot = (printmask_headerfooter_t)((usingHeadFoot | HF_NOSUMMARY) & ~HF_CUSTOM);
				} else {
					std::string aa; toke.copy_token(aa);
					formatstr_cat(error_message, "Unknown argument %s for SELECT\n", aa.c_str());
					//PRAGMA_REMIND("parse LABEL & SEPARATOR keywords for summary line here.")
				}
			}
			continue;
		}

		switch (sect) {
		case SUMMARY:
			// if there is a summary mask, parse it just the way we parse the print mask
			if ( ! mask) {
				break;
			}
		// fall through
		case SELECT: {
			toke.mark();
			std::string attr;
			std::string name;
			int opts = 0;
			int def_opts = FormatOptionAutoWidth | FormatOptionNoTruncate;
			const char * def_fmt = "%v";
			const char * fmt = def_fmt;
			int wid = 0;
			bool width_from_label = true;
			CustomFormatFn cust;

			bool got_attr = false;
			while (toke.next()) {
				const Keyword * pkw = SelectKeywords.lookup_token(toke);
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
						fmt = mask->store(val.c_str());
					} else {
						expected_token(error_message, "format after PRINTF", "SELECT", stream, toke);
					}
				} break;
				case kw_PRINTAS: {
					if (toke.next()) {
						const CustomFormatFnTableItem * pcffi = NULL;
						if (FnTable){ pcffi = FnTable->lookup_token(toke); } //Check for local print format table options
						if (!pcffi) { pcffi = GlobalFnTable.lookup_token(toke); } //If pcffi is Null then check for global print format table options

						if (pcffi) {
							cust = pcffi->cust;
							fmt = pcffi->printfFmt;
							if (fmt) fmt = mask->store(fmt);
							//cust_type = pcffi->cust;
							const char * pszz = pcffi->extra_attribs;
							if (pszz) {
								size_t cch = strlen(pszz);
								while (cch > 0) {
									attrs->insert(pszz);
									pszz += cch+1; cch = strlen(pszz);
								}
							}
						} else {
							std::string aa; toke.copy_token(aa);
							formatstr_cat(error_message, "Unknown argument %s for PRINTAS\n", aa.c_str());
						}
					} else {
						expected_token(error_message, "function name after PRINTAS", "SELECT", stream, toke);
					}
				} break;
				case kw_OR: {
					if (toke.next()) {
						std::string val; toke.copy_token(val);
						const char alt_chars[] = " ?*.-_#0";
						if (val.length() > 0 && strchr(alt_chars, val[0])) {
							int ix = (int)(strchr(alt_chars, val[0]) - &alt_chars[0]);
							opts |= (ix*AltQuestion);
							if (val.length() > 1 && val[0] == val[1]) { opts |= AltWide; }
						} else {
							formatstr_cat(error_message, "Unknown argument %s for OR\n", val.c_str());
						}
					} else {
						expected_token(error_message, "? or ?? after OR", "SELECT", stream, toke);
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
				case kw_FIT: {
					opts &= ~FormatOptionFitToData;
				} break;
				case kw_TRUNCATE: {
					opts &= ~FormatOptionNoTruncate;
					def_opts &= ~FormatOptionNoTruncate;
				} break;
				case kw_ALWAYS: {
					opts |= FormatOptionAlwaysCall;
				} break;
				case kw_WIDTH: {
					if (toke.next()) {
						std::string val; toke.copy_token(val);
						if (toke.matches("AUTO")) {
							opts |= FormatOptionAutoWidth;
							def_opts |= (FormatOptionAutoWidth | FormatOptionNoTruncate);
						} else {
							wid = atoi(val.c_str());
							def_opts &= ~(FormatOptionAutoWidth | FormatOptionNoTruncate);
							width_from_label = false;
							//PRAGMA_REMIND("TJ: decide how LEFT & RIGHT interact with pos and neg widths."
							if (fmt == def_fmt) {
								fmt = NULL;
							}
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

			opts |= def_opts;

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
				lbl = mask->store(label.c_str());
				fmt = lbl;
				wid = 0;
			} else {
				if (width_from_label) { wid = 0 - (int)strlen(lbl); }
				mask->set_heading(lbl);
				lbl = NULL;
				if (cust) {
					lbl = fmt;
				} else if ( ! fmt) {
					// if we get to here and there is no format, that means that
					// a width was specified, but no custom or printf format. so we
					// need to manufacture a printf format based on the given width.
					if ( ! wid) { fmt = def_fmt; }
					else {
						char tmp[40] = "%"; char *p = tmp+1;
						if (wid < 0) { opts |= FormatOptionLeftAlign; wid = -wid; *p++ = '-'; }
						auto [end, ec] = std::to_chars(p, p + 12, wid);
						p = end;
						if ( ! (opts & FormatOptionNoTruncate)) {
							*p = '.';
							p++;
							auto [end, ec] = std::to_chars(p, p + 12, wid);
							p = end;
						}
						*p++ = 'v'; *p = 0;
						fmt = mask->store(tmp);
					}
				}
			}
			if (cust) {
				mask->registerFormat (lbl, wid, opts, cust, attr.c_str());
			} else {
				mask->registerFormat(fmt, wid, opts, attr.c_str());
			}

			if ( ! IsValidClassAdExpression(attr.c_str(), attrs, scopes)) {
				formatstr_cat(error_message, "attribute or expression is not valid: %s\n", attr.c_str());
			}
		}
		break;

		case JOIN: {
			/* future
			*/
			if (toke.matches("ON") || toke.matches("USING")) {
				expected_token(error_message, "data set name before ON or USING", "JOIN", stream, toke);
			} else {
				//std::string join_to; toke.copy_token(join_to);
				toke.next();
				if ( ! toke.matches("ON") || ! toke.matches("USING")) {
					unexpected_token(error_message, "JOIN", stream, toke);
				} else {
					//toke.join_is_on = toke.matches("ON");
					//toke.copy_to_end(pmms.join_expression);
					//trim(pmms.join_expression);
				}
			}
		}
		break;

		case WHERE: {
			toke.copy_to_end(pmms.where_expression);
			trim(pmms.where_expression);
		}
		break;

		case AND: {
			/* future
			toke.copy_to_end(pmms.and_expression);
			trim(pmms.and_expression);
			*/
		}
		break;

		case GROUP: {
			toke.mark();
			GroupByKeyInfo key;

			// in case we end up finding no keywords, copy the remainder of the line now as the expression
			toke.copy_to_end(key.expr);
			bool got_expr = false;

			while (toke.next()) {
				const Keyword * pgw = GroupKeywords.lookup_token(toke);
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
				case gw_DESCENDING: {
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

			if ( ! IsValidClassAdExpression(key.expr.c_str(), &pmms.attrs, &pmms.scopes)) {
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

	pmms.headfoot = usingHeadFoot;

	return 0;
}

#include "printf_format.h"
static void reverse_engineer_width(int &wid, int &wflags, const Formatter & fmt, int hwid)
{
	wid = fmt.width;
	wflags = fmt.options & (FormatOptionAutoWidth | FormatOptionNoTruncate);
	bool width_from_label = hwid && (fmt.width == hwid);

	if (wid > 0 && (fmt.options & FormatOptionLeftAlign)) { wid = 0 - fmt.width; }

	// The default for flags is autowidth + truncate with a format of %v
	// in that case the column width will automatically grow to fit the data.
	// from an initial (i.e. minimum) width specified in fmt.width
	// where fmt.width is deduced from the width of the heading.

	if (wflags == FormatOptionAutoWidth) {

		const char* tmp_fmt = fmt.printfFmt;
		struct printf_fmt_info info;
		if ( ! fmt.printfFmt) {
		} else if (parsePrintfFormat(&tmp_fmt, &info)) {
			if (info.fmt_letter == 'v' && info.width == 0 && info.precision == -1) {
				wflags |= FormatOptionNoTruncate;
				if (width_from_label) {
					// don't display the width, but also don't display AUTO flag
					wflags &= ~FormatOptionAutoWidth;
					wid = 0;
				}
			}
		}
		//if (width_from_label && !wid) { wflags &= ~FormatOptionAutoWidth; }
	}

	// cant display both a width and the AUTO keyword.
	// so if we return a non-zero width here. we must turn off the AUTO flag
	if (wid) wflags &= ~FormatOptionAutoWidth;
}

struct PrintPrintMaskWalkArgs {
	std::string & fout;
	const CustomFormatFnTable & FnTable;
};

static int PrintPrintMaskWalkFunc(void*pv, int /*index*/, Formatter*fmt, const char *attr, const char * phead)
{
	struct PrintPrintMaskWalkArgs & args = *(struct PrintPrintMaskWalkArgs*)pv;
	const CustomFormatFnTableItem * ptable = args.FnTable.pTable;
	std::string & fout = args.fout;

	std::string printas = "";
	std::string head = "";
	int wid=0, hwid=0, wflags=0;
	if (phead && (YourString(phead) != attr)) {
		if (strchr(phead,'\'')) { head += "AS \""; head += phead; head += "\""; }
		else if (strpbrk(phead," \t\n\r\"")) { head += "AS '"; head += phead; head += "'"; }
		else { head += "AS "; head += phead; }
		hwid = strlen(phead);
	}
	if (fmt->sf) {
		for (int ii = 0; ii < (int)args.FnTable.cItems; ++ii) {
			if ((StringCustomFormat)ptable[ii].cust == fmt->sf) {
				if (fmt->printfFmt) {
					printas = "PRINTF "; printas += fmt->printfFmt;
					printas += " RENDERAS ";
				} else {
					printas = "PRINTAS ";
				}
				printas += ptable[ii].key;
				break;
			}
		}
	} else if (fmt->printfFmt) {
		printas = "PRINTF ";
		if (strchr(fmt->printfFmt,'\'')) { printas += "\""; printas += fmt->printfFmt; printas += "\""; }
		else if (strpbrk(fmt->printfFmt," \t\n\r\"")) { printas += "'"; printas += fmt->printfFmt; printas += "'"; }
		else  { printas += fmt->printfFmt; }
		if (YourString("%v") == fmt->printfFmt) { printas = ""; }
	}
	std::string width = "";
	reverse_engineer_width(wid, wflags, *fmt, hwid);
	if (wid) { formatstr(width, "WIDTH %3d", wid); }
	else if (wflags & FormatOptionAutoWidth) { width = "WIDTH AUTO"; }
	if (wflags & FormatOptionLeftAlign)  { width += " LEFT"; }
	if (!(wflags & FormatOptionNoTruncate)) { width += " TRUNCATE"; }

	int fit_mask = FormatOptionFitToData | FormatOptionSpecialMask;
	if (fmt->options & fit_mask) { width += " FIT"; }

	if ((fmt->options & FormatOptionNoPrefix))   { width += " NOPREFIX"; }
	if ((fmt->options & FormatOptionNoSuffix))   { width += " NOSUFFIX"; }
	if ((fmt->options & FormatOptionAlwaysCall)) { width += " ALWAYS"; }

	if ((fmt->options & FormatOptionHideMe))     { width += " HIDDEN"; }

	if (phead && (fmt->width == (int)strlen(phead))) {
		// special case for width defaulting from the column heading
		// PRAGMA_REMIND("figure out how to reduce decorations to the minimum")
	}
	trim(width); if ( ! width.empty()) { width += " "; }
	printas.insert(0, width); trim(printas);
	if (fmt->options & AltMask) {
		printas += " OR ";
		const char alt_chars[] = " ?*.-_#0";
		char or_val[3] = {0,0,0};
		or_val[0] = alt_chars[((fmt->options&AltMask)/AltQuestion)];
		if (fmt->options & AltWide) { or_val[1] = or_val[0]; }
		printas += or_val;
	}

	size_t pos = fout.size();
	fout.append(3, ' ');
	fout += attr?attr:"NULL";

	if ( ! head.empty()) {
		fout += " "; fout += head;
	}
	if ( ! printas.empty()) {
		const int attr_width = 30;
		size_t align = (fout.size() < pos + attr_width) ? pos+attr_width-fout.size() : 1;
		fout.append(align, ' ');
		fout += printas;
	}
	fout += "\n";
	return 0;
}

int PrintPrintMask(std::string & fout,
	const CustomFormatFnTable & FnTable,  // in: table of custom output functions for SELECT
	const AttrListPrintMask & mask,       // in: columns and headers set in SELECT
	const std::vector<const char *> * pheadings,   // in: headings override
	const PrintMaskMakeSettings & mms, // in: modifed by parsing the stream. BUT NOT CLEARED FIRST! (so the caller can set defaults)
	const std::vector<GroupByKeyInfo> & /*group_by*/, // in: ordered set of attributes/expressions in GROUP BY
	AttrListPrintMask * sumymask) // out: columns and headers set in SUMMMARY
{
	fout += "SELECT";
	if ( ! mms.select_from.empty()) { fout += " FROM "; fout += mms.select_from; }
	if (mms.headfoot == HF_BARE) { fout += " BARE"; }
	else {
		if (mms.headfoot & HF_NOTITLE) { fout += " NOTITLE"; }
		if (mms.headfoot & HF_NOHEADER) { fout += " NOHEADER"; }
		//if (mms.headfoot & HF_NOSUMMARY) { fout += " NOSUMMARY"; }
	}
	fout += "\n";

	struct PrintPrintMaskWalkArgs args = {fout, FnTable};
	mask.walk(PrintPrintMaskWalkFunc, &args, pheadings);

	//PRAGMA_REMIND("print JOIN")

	if ( ! mms.where_expression.empty()) {
		fout += "WHERE "; fout += mms.where_expression;
		fout += "\n";
	}

	//PRAGMA_REMIND("print AND")
	//PRAGMA_REMIND("print GROUP BY")

	if (mms.headfoot != HF_BARE) {
		fout += "SUMMARY ";
		if ((mms.headfoot & (HF_CUSTOM | HF_NOSUMMARY)) == HF_CUSTOM) {
			if (sumymask) { sumymask->walk(PrintPrintMaskWalkFunc, &args, NULL); }
		} else {
			fout += (mms.headfoot&HF_NOSUMMARY) ? "NONE" : "STANDARD";
		}
		fout += "\n";
	}

	return 0;
}
