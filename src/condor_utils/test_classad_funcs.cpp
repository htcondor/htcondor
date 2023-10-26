/***************************************************************
 *
 * Copyright (C) 2023, Condor Team, Computer Sciences Department,
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
#include "condor_attributes.h"

#include "classad/classad_distribution.h"
#include "compat_classad.h"
#include "compat_classad_util.h"
#include "classad/sink.h"
#include "condor_string.h" // for getline_trim
#include "tokener.h"
#include "ad_printmask.h"
#include "match_prefix.h"
#include "subsystem_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <utility>

typedef struct {
	const char * key;
	int          value;
	int          mask;
} TestCmd;
typedef nocase_sorted_tokener_lookup_table<TestCmd> TestCmdTable;

enum {
	cmd_CLEAR=1,   // clear one or more ads
	cmd_MY,        // load expressions into MY ad
	cmd_TARGET,    // load expressions into TARGET ad
	cmd_EXPECT,
	cmd_EXPECT_BOOL,
	cmd_EXPECT_INT,
	cmd_EXPECT_FLOAT,
	cmd_EXPECT_NUMBER,
	cmd_EXPECT_STRING,
	cmd_EXPECT_LIST,
	cmd_EXPECT_AD,
	cmd_EXPECT_UNDEFINED,
	cmd_EXPECT_ERROR,
};

#define CK(a) { #a ":", cmd_##a, 0 }
#define CKN(a,n) { #a ":", cmd_##a, n }
static const TestCmd TestCmdItems[] = {
	CK(CLEAR),
	CK(EXPECT),
	CKN(EXPECT_AD, classad::Value::CLASSAD_VALUE),
	CKN(EXPECT_BOOL, classad::Value::BOOLEAN_VALUE),
	CKN(EXPECT_ERROR, classad::Value::ERROR_VALUE),
	CKN(EXPECT_FLOAT, classad::Value::REAL_VALUE),
	CKN(EXPECT_INT, classad::Value::INTEGER_VALUE),
	CKN(EXPECT_LIST, classad::Value::LIST_VALUE),
	CKN(EXPECT_NUMBER, classad::Value::REAL_VALUE | classad::Value::INTEGER_VALUE | classad::Value::BOOLEAN_VALUE),
	CKN(EXPECT_STRING, classad::Value::STRING_VALUE),
	CKN(EXPECT_UNDEFINED, classad::Value::UNDEFINED_VALUE),
	CK(MY),
	CK(TARGET),
};
#undef CK
static const TestCmdTable TestCmds = SORTED_TOKENER_TABLE(TestCmdItems);


int dash_verbose = false;
int fail_count = 0;
int check_count = 0;

static bool check_result (
	const classad::Value & result,
	const std::string & expect,
	int type_mask,
	std::string & message,
	std::string & actual)
{
	bool bad = false;
	if (type_mask) {
		if ( ! (result.GetType() & type_mask)) {
			message = "result is the wrong type";
			bad = true;
		}
	}

	classad::ClassAdUnParser unparser; unparser.SetOldClassAd(true, true);

	if (type_mask == classad::Value::STRING_VALUE || ( ! type_mask && result.IsStringValue())) {

		const char * str;
		if ( ! result.IsStringValue(str)) {
			unparser.Unparse(actual, result);
			message = "expected string result";
			bad = true;
		} else if (YourStringNoCase(str) != expect) {
			actual = str;
			message = "string does not match";
			bad = true;
		}

	} else {

		unparser.Unparse(actual, result);

		classad::ExprTree * tree = nullptr;
		classad::Value expectval;
		if (ParseClassAdRvalExpr(expect.c_str(), tree) != 0) {
			message = "cannot parse expected result";
			bad = true;
		}

		classad::Literal * plit = classad::Literal::MakeLiteral(result);
		if ( ! plit->SameAs(tree)) {
			message = "value does not match";
			bad = true;
		}

		delete tree;
		delete plit;
	}

	return ! bad;
}

static bool check_expectation (
	const std::string exprstr,
	const std::string expect,
	ClassAd & myad,
	ClassAd & targetad,
	int type_mask,
	std::string & message,
	std::string & actual)
{
	actual.clear();

	classad::ClassAdParser parser; parser.SetOldClassAd( true );
	ExprTree * tree = parser.ParseExpression(exprstr);
	if ( ! tree) {
		message = std::string("could not parse expr: ") + exprstr;
		return false;
	}

	classad::Value result;

	std::string attr;
	if (ExprTreeIsAttrRef(tree, attr)) {
		if ( ! EvalAttr(attr.c_str(), &myad, &targetad, result)) {
			message = std::string("could not evaluate: ") + attr;
			return false;
		}
	} else {
		if ( ! EvalExprTree(tree, &myad, &targetad, result, classad::Value::ALL_VALUES)) {
			message = std::string("could not evaluate: ") + exprstr;
			return false;
		}
	}

	return check_result(result, expect, type_mask, message, actual);
}

static void log_tag_once(const char * tag, bool & logged_tag) {
	if (logged_tag) return;
	fprintf(stdout, "------- %s -------\n", tag);
	logged_tag = true;
}

static void testing_from(
	const char * tag,
	SimpleInputStream & stream,
	int verbose,
	std::vector<std::string> * lines = nullptr)
{
	ClassAd myad, targetad;
	ClassAd * curad = &myad;

	std::string buffer, expectbuf, exprbuf, message, actual;

	bool logged_tag = false;
	if (verbose) { log_tag_once(tag, logged_tag); }

	int lineno = 0;
	int starting_check_count = check_count;
	int starting_fail_count = fail_count;

	tokener toke("");
	while (toke.set(stream.nextline())) {
		++lineno;
		if (lines) { lines->push_back(toke.content()); }

		if ( ! toke.next() || toke.matches("#"))
			continue;

		const TestCmd * pcmd = TestCmds.lookup_token(toke);
		if ( ! pcmd) {
			if (lines) { lines->back().insert(0,"  "); }
			if ( ! InsertLongFormAttrValue(*curad, toke.content().c_str(), false)) {
				fprintf(stderr, "PARSE error in %s line %d : %s\n", tag, lineno, toke.content().c_str());
			}
			continue;
		}
		switch (pcmd->value) {
		case cmd_CLEAR:
			toke.next();
			if (toke.matches("MY")) { myad.Clear(); }
			else if (toke.matches("TARGET")) { targetad.Clear(); }
			else { myad.Clear(); targetad.Clear(); }
			break;

		case cmd_MY:
			// fall through
		case cmd_TARGET:
			curad = (pcmd->value == cmd_MY) ? &myad : &targetad;
			if (toke.next()) {
				toke.copy_to_end(buffer);
				trim(buffer);
				if ( ! buffer.empty()) {
					InsertLongFormAttrValue(*curad, buffer.c_str(), false);
				}
			}
			break;

		case cmd_EXPECT_BOOL:
		case cmd_EXPECT_INT:
		case cmd_EXPECT_FLOAT:
		case cmd_EXPECT_NUMBER:
		case cmd_EXPECT_STRING:
		case cmd_EXPECT_LIST:
		case cmd_EXPECT_AD:
		case cmd_EXPECT_UNDEFINED:
		case cmd_EXPECT_ERROR:
		case cmd_EXPECT:
			++check_count;
			toke.mark_after();
			while (toke.next()) {
				if ( ! toke.matches("FROM")) { continue; }
				toke.copy_marked(expectbuf);
				trim(expectbuf);
				break;
			}
			if (toke.next()) {
				toke.copy_to_end(exprbuf);
				if ( ! check_expectation(exprbuf, expectbuf, myad, targetad, pcmd->mask, message, actual)) {
					++fail_count;
					formatstr(buffer, "check FAILED line %d : %s EXPECT: %s ACTUAL: %s FROM: %s\n",
						lineno, message.c_str(), expectbuf.c_str(), actual.c_str(), exprbuf.c_str());
				} else if (verbose) {
					formatstr(buffer, "check OK line %d : EXPECT: %s  FROM: %s\n", lineno, expectbuf.c_str(), exprbuf.c_str());
				} else {
					buffer.clear();
				}
				if ( ! buffer.empty()) {
					log_tag_once(tag, logged_tag);
					fputs(buffer.c_str(), stdout);
				}
			}
			break;
		}

		buffer.clear();
	}

	int checks = check_count - starting_check_count;
	int fails = fail_count - starting_fail_count;
	if (fails || verbose) {
		log_tag_once(tag, logged_tag);
		if (fails) { formatstr(buffer, "\t%d FAILED checks out of %d checks\n", fails, checks); }
		else { formatstr(buffer, "\tALL checks passed (%d)\n", checks); }
		fputs(buffer.c_str(), stdout);
	}
}

void testing_join(int verbose)
{
	const char * tests = {
		"MY:\n"
		"  l3 = {\"aa\",\"bb\",\"cc\"}\n"
		"  A = \"aa\"\n"
		"  B = \"BB\"\n"
		"EXPECT:           FROM join()\n"
		"EXPECT: 10203     FROM join(0,1,2,3)\n"
		"EXPECT:           FROM join(A)\n"
		"EXPECT: aa        FROM join(B,A)\n"
		"EXPECT: aaBBaa    FROM join(B,A,A)\n"
		"EXPECT: aabbcc    FROM join(l3)\n"
		"EXPECT: undefined FROM join(undefined)\n"
		"EXPECT: undefined FROM join(UN)\n"
		"EXPECT: aa        FROM join(UN,A)\n"
		"EXPECT: undefined FROM join(A,UN)\n"
		"EXPECT: BB        FROM join(a,UN,b)\n"
		"EXPECT: BB        FROM join(a,B,UN)\n"
		"EXPECT: BBaaBB    FROM join(a,UN,b,B)\n"
		"EXPECT: BBaaBB    FROM join(a,B,UN,B)\n"
	};

	StringLiteralInputStream stm(tests);
	testing_from("join", stm, verbose);
}

void testing_stringListMember(int verbose)
{
	const char * tests = {
		"MY:\n"
		"  l3 = {\"aa\",\"bb\",\"cc\"}\n"
		"  sl3 = \"aa bb cc\"\n"
		"  sl3c = \"cc,bb,aa\"\n"
		"  sl3ce = \"cc,bb,aa,,\"\n"
		"  sl3cb = \"cc,bb,aa,,\"\n"
		"  sl3ws = \"cc, BB, , aa\"\n"
		"  sl2b = \"bb,aa\"\n"
		"  A = \"aa\"\n"
		"  B = \"BB\"\n"
		"  com = \",\"\n"
		"  sp = \" \"\n"
		"  empty = \"\"\n"

		"EXPECT: true      FROM stringListMember(a,sl3)\n"
		"EXPECT: false     FROM stringListMember(B,sl3)\n"
		"EXPECT: true      FROM stringListIMember(B,sl3)\n"
		"EXPECT: false     FROM stringListMember(s12b,sl3)\n"
		"EXPECT: true      FROM stringListMember(B,sl3ws)\n"
		"EXPECT: true      FROM stringListMember(B,sl3ws,com)\n"
		"EXPECT: false     FROM stringListMember(sp,sl3ws,com)\n"
		"EXPECT: false     FROM stringListMember(empty,sl3ws,com)\n"
		"EXPECT: false     FROM stringListIMember(s12b,sl3)\n"

		"EXPECT: error     FROM stringListMember()\n"
		"EXPECT: error     FROM stringListMember(a,l3)\n"
		"EXPECT: error     FROM stringListMember(a,b,A,B)\n"

		"EXPECT: false     FROM stringListMember(\"\",sl3c)\n"
		"EXPECT: false     FROM stringListMember(\"\",sl3ce)\n"
		"EXPECT: false     FROM stringListMember(\"\",sl3cb)\n"
		"EXPECT: false     FROM stringListMember(un,sl3cb)\n"
		"EXPECT: undefined FROM stringListMember(un,un)\n"
		"EXPECT: false     FROM stringListMember(un,empty)\n"
		"EXPECT: false     FROM stringListMember(empty,un)\n"
		"EXPECT: false     FROM stringListMember(empty,empty)\n"

		"EXPECT: false     FROM stringListMember(b,join(com,l3))\n"
		"EXPECT: true      FROM stringListMember(b,toupper(join(com,l3)))\n"
		"EXPECT: true      FROM stringListIMember(b,join(com,l3))\n"
	};

	StringLiteralInputStream stm(tests);
	testing_from("stringListMember", stm, verbose);
}

void testing_stringListSubsetMatch(int verbose)
{
	const char * tests = {
		"MY:\n"
		"  l3 = {\"aa\",\"bb\",\"cc\"}\n"
		"  sl3 = \"aa bb cc\"\n"
		"  sl3c = \"cc,bb,aa\"\n"
		"  sl3ce = \"cc,bb,aa,,\"\n"
		"  sl3cb = \"cc,bb,aa,,\"\n"
		"  sl3ws = \"cc, BB, , aa\"\n"
		"  sl2b = \"bb,aa\"\n"
		"  A = \"aa\"\n"
		"  B = \"BB\"\n"
		"  com = \",\"\n"
		"  sp = \" \"\n"
		"  empty = \"\"\n"

		"EXPECT: true      FROM stringListSubsetMatch(a,sl3)\n"
		"EXPECT: true      FROM stringListSubsetMatch(a,sl3c)\n"
		"EXPECT: false     FROM stringListSubsetMatch(B,sl3)\n"
		"EXPECT: false     FROM stringListSubsetMatch(B,sl3c)\n"
		"EXPECT: true      FROM stringListISubsetMatch(B,sl3)\n"
		"EXPECT: true      FROM stringListSubsetMatch(s12b,sl3)\n"
		"EXPECT: true      FROM stringListSubsetMatch(s12b,sl3c)\n"
		"EXPECT: true      FROM stringListSubsetMatch(B,sl3ws)\n"
		"EXPECT: true      FROM stringListSubsetMatch(B,sl3ws,com)\n"
		"EXPECT: false     FROM stringListSubsetMatch(sp,sl3ws,com)\n"
		"EXPECT: true      FROM stringListSubsetMatch(empty,sl3ws,com)\n"
		"EXPECT: true      FROM stringListSubsetMatch(UN,sl3ws,com)\n"
		"EXPECT: true      FROM stringListISubsetMatch(s12b,sl3ce)\n"

		"EXPECT: error     FROM stringListSubsetMatch()\n"
		"EXPECT: error     FROM stringListSubsetMatch(a,l3)\n"
		"EXPECT: error     FROM stringListSubsetMatch(a,b,A,B)\n"

		"EXPECT: true      FROM stringListSubsetMatch(\"\",sl3c)\n"
		"EXPECT: true      FROM stringListSubsetMatch(\"\",sl3ce)\n"
		"EXPECT: true      FROM stringListSubsetMatch(\"\",sl3cb)\n"
		"EXPECT: true      FROM stringListSubsetMatch(un,sl3cb)\n"
		"EXPECT: false     FROM stringListSubsetMatch(B,UN)\n"
		"EXPECT: undefined FROM stringListSubsetMatch(un,un)\n"
		"EXPECT: true      FROM stringListSubsetMatch(un,empty)\n"
		"EXPECT: true      FROM stringListSubsetMatch(empty,un)\n"
		"EXPECT: true      FROM stringListSubsetMatch(empty,empty)\n"

		"EXPECT: false     FROM stringListSubsetMatch(b,join(com,l3))\n"
		"EXPECT: true      FROM stringListSubsetMatch(b,toupper(join(com,l3)))\n"
		"EXPECT: true      FROM stringListISubsetMatch(b,join(com,l3))\n"
	};

	StringLiteralInputStream stm(tests);
	testing_from("stringListSubsetMatch", stm, verbose);
}

int main(int /*argc*/, const char *argv[])
{
	int test_flags = 0;
	const char * pcolon;
	std::vector<const char *> fromfiles;
	const char * capture_file = nullptr;

	for (int ii = 1; argv[ii]; ++ii) {
		const char *arg = argv[ii];
		if (is_dash_arg_prefix(arg, "verbose", 1)) {
			dash_verbose = 1;
		} else if (is_dash_arg_colon_prefix(arg, "test", &pcolon, 1)) {
			if (pcolon) {
				while (*++pcolon) {
					switch (*pcolon) {
					case 'j': test_flags |= 0x0001; break; // stringlist
					case 'm': test_flags |= 0x0002; break; // stringlist
					case 's': test_flags |= 0x0004; break; // stringlist
					}
				}
			} else {
				test_flags = -1;
			}
		} else if (is_dash_arg_colon_prefix(arg, "from", &pcolon, 1)) {
			// read checks from a file
			if ( ! argv[ii+1]) {
				fprintf(stderr, "-from requires a filename argument\n");
				return 1;
			}
			test_flags = 0;
			fromfiles.push_back(argv[++ii]);
		} else if (is_dash_arg_colon_prefix(arg, "capture", &pcolon, 3)) {
			// read checks from a file
			if ( ! argv[ii+1]) {
				fprintf(stderr, "-capture requires a filename argument\n");
				return 1;
			}
			capture_file = argv[++ii];
		} else {
			fprintf(stderr, "unknown argument %s\n", arg);
			fprintf(stderr, "usage: %s [ -verbose ] [ -test[:<tests> | -from <file> ] ]\n", argv[0]);
			fprintf(stderr,
				"   one or more single letters select individual tests:\n"
				"      j join\n"
				"      m stringlistMember\n"
				"      s stringlistSubsetMatch\n"
				"   default is to run all tests\n"
				"   If -verbose is specified, all tests show output, otherwise\n"
				"   only failed tests produce output\n"
			);
			return 1;
		}
	}

	// init the config subsystem, but without reading any config files
	set_mySubSystem("TOOL", false, SUBSYSTEM_TYPE_TOOL);
	config_host(NULL, CONFIG_OPT_USE_THIS_ROOT_CONFIG, "ONLY_ENV");

	ClassAdReconfig();	// to register compat classad functions

	bool used_stdin = false;
	if ( ! fromfiles.empty()) {
		for (auto * file : fromfiles) {
			FILE * fp = nullptr;
			bool close_fp = false;
			if (MATCH == strcmp("-", file)) {
				if (!used_stdin) {
					fp = stdin;
					close_fp = false;
					used_stdin = true;
				} else {
					fprintf(stderr, "ERROR: stdin already consumed\n");
					++fail_count;
					continue;
				}
			} else {
				fp = fopen(file, "rb");
				if ( ! fp) {
					fprintf(stderr, "Failed to open file %s\n", file);
					++fail_count;
					continue;
				}
				close_fp = true;
			}

			SimpleFileInputStream stm(fp, close_fp);
			if (capture_file) {
				std::vector<std::string> capture;
				testing_from(file, stm, dash_verbose, &capture);
				for (auto & line : capture) {
					fprintf(stdout, "\t\"%s\\n\"\n", EscapeChars(line, "\"",'\\').c_str());
				}
			} else {
				testing_from(file, stm, dash_verbose);
			}
		}
	}
	else if ( ! test_flags) {
		test_flags = -1; // set all test flags
	}

	if (test_flags & 0x0001) testing_join(dash_verbose);
	if (test_flags & 0x0002) testing_stringListMember(dash_verbose);
	if (test_flags & 0x0004) testing_stringListSubsetMatch(dash_verbose);
#if 0
	if (test_flags & 0x0008) testing_4(dash_verbose);
	if (test_flags & 0x0010) testing_5(dash_verbose);
	if (test_flags & 0x0020) testing_6(dash_verbose);
	if (test_flags & 0x0040) testing_7(dash_verbose);
	if (test_flags & 0x0080) testing_8(dash_verbose);
	if (test_flags & 0x0080) testing_9(dash_verbose);
	if (test_flags & 0x0100) testing_a(dash_verbose);
	if (test_flags & 0x0200) testing_b(dash_verbose);
	if (test_flags & 0x0400) testing_c(dash_verbose);
	if (test_flags & 0x0800) testing_d(dash_verbose);
	if (test_flags & 0x1000) testing_f(dash_verbose);
#endif
	return fail_count > 0;
}
