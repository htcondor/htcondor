/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include <time.h>
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif
#include "match_prefix.h"
#include "filename_tools.h"
#include <submit_utils.h>
#include <param_info.h> // for BinaryLookup

#include <algorithm>
#include <string>
#include <set>

void usage(FILE * out); // from submit.cpp

// this is debug code: it must be kept in sync with submit_utils.cpp
struct SimpleSubmitKeyword {
	char const *key;  // submit key
	char const *attr; // job attribute
	int opts;         // zero or more option flags
};

// these are used to help build the intrinsics table
#define ATTR(a,s,d) {#a, s, 0},
#define JOB_ATTR(a,d) {#a, a, 0x1000 | (d<<24)},
#define JOB_ATTR_PRE(a,d) {#a, a "*", 0x1800 | (d<<24)},
#define SLOT_ATTR(a,d) {#a, a, 0x2000 | (d<<24)},
#define OTHER_ATTR(a,d) {#a, a, 0x4000 | (d<<24)},
#define UN_ATTR(a,d) {#a, a, 0x5000 | (d<<24)},
#define NO_ATTR(a,d) {#a, a, 0x6000 | (d<<24)},

static const SimpleSubmitKeyword intrinsics[] = {
// future: generate from condor_attributes.h
//#include "the_attributes_table.lst"
	{ NULL, NULL, 0} // end of table
};

// read a file containing a single attribute name on each line into a set
static int ingest_attrs_from_file(const char * filename, classad::References & out)
{
	FILE * fp = safe_fopen_no_create_follow(filename, "rb");
	if ( ! fp) {
		int en = errno;
		fprintf(stderr, "can't open %s. err=%d : %s", filename, en, strerror(en));
		return 0;
	}

	std::string attr;
	while (readLine(attr, fp, false)) {
		trim(attr);
		out.insert(attr);
	}
	fclose(fp);
	return (int)out.size();
}

enum {
	HELP_INFO_COMMANDS = 0x01, HELP_INFO_ATTRIBUTES = 0x02, HELP_INFO_INTRINSICS = 0x04, HELP_INFO_DEFINES = 0x08,
	HELP_INFO_TABLE = 0x1000, HELP_INFO_UNKNOWN = 0x0100, HELP_INFO_JOB = 0x200, HELP_INFO_SUBMIT = 0x400
};

void help_info(FILE* out, int num_topics, const char ** topics)
{
	if (num_topics <= 0 || !*topics) {
		usage(out);
		return;
	}

	int options = 0;
	const char * doclist = NULL;
	for (const auto &info: StringTokenIterator(*topics)) {
		if (is_arg_prefix(info.c_str(), "commands", 2)) {
			options |= HELP_INFO_COMMANDS;
		} else if (is_arg_prefix(info.c_str(), "attributes", 2)) {
			options |= HELP_INFO_ATTRIBUTES;
		} else if (is_arg_prefix(info.c_str(), "intrinsics", 2)) {
			options |= HELP_INFO_INTRINSICS; // from condor_attributes
		} else if (is_arg_prefix(info.c_str(), "defines", 2)) {
			options |= HELP_INFO_DEFINES | HELP_INFO_INTRINSICS;
		} else if (is_arg_prefix(info.c_str(), "table", 2)) {
			options |= HELP_INFO_TABLE;
		} else if (is_arg_prefix(info.c_str(), "unknown", 2)) {
			options |= HELP_INFO_UNKNOWN | HELP_INFO_TABLE;
		} else if (is_arg_prefix(info.c_str(), "job", 2)) {
			options |= HELP_INFO_JOB | HELP_INFO_TABLE;
		} else if (is_arg_prefix(info.c_str(), "submit", 2)) {
			options |= HELP_INFO_SUBMIT | HELP_INFO_TABLE;
		} else if (is_arg_colon_prefix(info.c_str(), "doclist", &doclist, 3)) {
			options |= HELP_INFO_TABLE;
			if (doclist) ++doclist;
		}
	}

	std::string line;
	const struct SimpleSubmitKeyword * submit_keywords = get_submit_keywords();

	classad::References docattrs;
	if (doclist) { ingest_attrs_from_file(doclist, docattrs); }
	classad::References submitattrs;
	for (auto k = submit_keywords; k->key || k->attr; ++k) { if (k->attr) submitattrs.insert(k->attr); }

	// print a table based on condor_attributes.h
	int lines = 0;
	if (options & HELP_INFO_TABLE) {

		bool show_attrs = options & HELP_INFO_ATTRIBUTES;
		options &= ~HELP_INFO_ATTRIBUTES;

		for (auto k = intrinsics; k->key; ++k) {
			int opts = k->opts;
			if ( ! opts) {
				if (submitattrs.find(k->attr) != submitattrs.end()) opts = 0x01000;
			}

			bool unk_only = ((options & ~HELP_INFO_TABLE) == HELP_INFO_UNKNOWN);
			bool job_only = ((options & ~HELP_INFO_TABLE) == HELP_INFO_JOB);
			bool submit_only = ((options & ~HELP_INFO_TABLE) == HELP_INFO_SUBMIT);
			if (opts) {
				if (unk_only) continue;
				const char * typ = "OTHER";
				bool is_prefix = (opts & 0x800);
				int  is_doc = (opts & 0xF000000) >> 24;
				opts &= ~0xFF000800;
				if (job_only && opts != 0x1000) continue;
				if (submit_only && submitattrs.find(k->attr) == submitattrs.end()) continue;

				if (doclist) {
					if (docattrs.find(k->attr) != docattrs.end()) is_doc |= 1;
				}

				if (show_attrs) {
					fprintf(out, "%s\n", k->attr);
					++lines;
				} else {
					if (opts == 0x1000) typ = "  JOB";
					else if (opts == 0x2000) typ = " SLOT";
					else if (opts == 0x5000) typ = "   UN";
					else if (opts == 0x6000) typ = "   NO";
					fprintf(out, "%s_ATTR%s(%s,%d) // \"%s\"\n", typ, is_prefix ? "_PRE" : "", k->key, is_doc, k->attr);
					++lines;
				}
			} else {
				if (unk_only) {
					fprintf(out, "%s\n", k->key);
					++lines;
				} else {
					fprintf(out, "    ATTR(%s,\"%s\",0)\n", k->key, k->attr);
					++lines;
				}
			}

		}

		if ( ! lines) {
			usage(stdout);
		}

		return;
	}

	for (auto k = submit_keywords; k->key || k->attr; ++k) {
		line.clear();
		if (options & HELP_INFO_COMMANDS) {
			if (k->key) {
				if (! line.empty()) line += ", ";
				line += k->key;
			}
		}
		if (options & HELP_INFO_ATTRIBUTES) {
			if (k->attr) {
				if (! line.empty()) line += ", ";
				line += k->attr;
			}
		}
		if (! line.empty()) {
			line += "\n";
			fputs(line.c_str(), out);
			++lines;
		}
	}

	if (options & HELP_INFO_INTRINSICS) {
		for (auto k = intrinsics; k->key || k->attr; ++k) {
			line.clear();
			if (options & HELP_INFO_DEFINES) {
				if (k->key) {
					if (! line.empty()) line += ", ";
					line += k->key;
				}
			}
			if (options & HELP_INFO_ATTRIBUTES) {
				if (k->attr) {
					if (! line.empty()) line += ", ";
					line += k->attr;
				}
			}
			if (! line.empty()) {
				line += "\n";
				fputs(line.c_str(), out);
				++lines;
			}
		}
	}

	if ( ! lines) {
		usage(stdout);
	}
}

void schedd_capabilities_help(FILE * out, const ClassAd &ad, const std::string &helpex, DCSchedd * schedd, int options)
{
	std::string attr;

	fprintf (out, "Schedd %s\n", schedd ? schedd->name() : "");

	if (!ad.size()) {
		fprintf(out, "Has no capabilities ad\n");
	}

	if (options > 1) {
		formatAd(attr, ad, "#  ");
		if (attr.back() != '\n') { attr += '\n'; }
		fprintf(out, "# Raw ad:\n%s###\n", attr.c_str());
		attr.clear();
	}

	bool latemat=false, jobsets=false;
	int latematver=0, jobsetsver=0;
	ad.LookupBool("LateMaterialize", latemat);
	ad.LookupInteger("LateMaterializeVersion", latematver);
	ad.LookupBool("UseJobsets", jobsets);
	ad.LookupInteger("UseJobsetsVersion", jobsetsver);

	if (latemat) {
		fprintf(out, " Has Late Materialization enabled\n");
		//fprintf(out, " Has Late Materialization v%d\n", latematver);
	}
	if (jobsets) {
		fprintf(out, " Has Jobsets enabled\n");
		//fprintf(out, " Has Jobsets v%d\n", jobsetsver);
	}

	ExprTree * extensions = ad.Lookup("ExtendedSubmitCommands");
	if (extensions && (extensions->GetKind() == classad::ExprTree::CLASSAD_NODE)) {
		fprintf(out, " Has Extended submit commands:\n");

		classad::ClassAd* extendedCmds = dynamic_cast<classad::ClassAd*>(extensions);
		std::map<std::string, std::string, CaseIgnLTYourString> cmd_info;
		size_t keywidth = 0;
		for (auto it = extendedCmds->begin(); it != extendedCmds->end(); ++it) {
			classad::Value val;
			if (keywidth < it->first.size()) { keywidth = it->first.size(); }
			cmd_info[it->first] = "expression";
			if (ExprTreeIsLiteral(it->second, val)) {
				if (val.GetType() == classad::Value::UNDEFINED_VALUE) {
					continue; // not a real extended command
				} else if (val.GetType() == classad::Value::ERROR_VALUE) {
					cmd_info[it->first] = "forbidden";
				} else if (val.GetType() == classad::Value::BOOLEAN_VALUE) {
					cmd_info[it->first] = "boolean true/false"; 
				} else if (val.GetType() == classad::Value::INTEGER_VALUE) {
					long long ll;
					val.IsIntegerValue(ll);
					cmd_info[it->first] = (ll < 0) ? "signed integer" : "unsigned integer";
				} else if (val.GetType() == classad::Value::STRING_VALUE) {
					std::string str;
					val.IsStringValue(str);
					bool is_list = strchr(str.c_str(), ',') != NULL;
					bool is_file = starts_with_ignore_case(str, "file");
					cmd_info[it->first] = is_file ? (is_list ? "filename list" : "filename") : (is_list ? "stringlist" : "string");
				}
			}
		}
		for (const auto& it : cmd_info) {
			attr = it.first;
			attr.append(keywidth + 1 - it.first.size(), ' ');
			fprintf(out, "\t%s value is %s\n", attr.c_str(), it.second.c_str());
		}
	}

	if ( ! helpex.empty()) {
		fprintf(out, " Has Extended help:\n\t%s\n", helpex.c_str());
	}
}

