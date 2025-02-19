/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
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

// TEST: Range Based For Loop DAG File Parser and DAG line lexer

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "stl_string_utils.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "dag_parser.h"

#include <vector>

//------------------------------------------------------------------------------------
const std::string int_max_str = std::to_string(std::numeric_limits<int>::max());
const int SUCCESS_TEST = 0;
const int FAILURE_TEST = 1;
const int MISSING_NEWLINE_TEST = 2;
const int SKIP_LINE_TEST = 3;
const int NO_SUBMIT_DESC_END_TEST = 4;
const int NO_INLINE_NODE_DESC_END_TEST = 5;

//------------------------------------------------------------------------------------
std::vector<std::pair<const char*, const char*>> TEST_FILES = {
	{
		// Successful DAG file
		"successful.dag",
		// Contents (Note single string broken across many lines)
		"REJECT\n"
		"# This is a comment and should be ignored\n"
		"JOB A foo.sub\n"
		"JOB B foo.sub DIR subdir/test NOOP\n"
		"JOB C bar.sub DONE\n"
		"JOB D {\n"
		"    executable = /bin/sleep\n"
		"    queue 1\n"
		"} NOOP DIR foo\n"
		"PROVISIONER E foo.sub\n"
		"FINAL F bar.sub\n"
		"SERVICE G testing.sub DIR one/two/three\n"
		"SUBDAG EXTERNAL INNER_DAG test.dag DIR subdir NOOP DONE\n"
		"SPLICE test-splice test.dag DIR test/subdir\n"
		"SUBMIT_DESCRIPTION echo-desc @=desc\n"
		"    executable = /bin/echo\n"
		"    arguments  = 'Hello World!'\n"
		"\n     \n\n  \n" // Pure whitespace should be ignored
		"    output     = $(ClusterId).out\n"
		"    log        = echo.log        \n"
		"    queue 10\n"
		"@desc\n"
		"PARENT A CHILD B\n"
		"PARENT A CHILD C D E\n"
		"PARENT C D E CHILD F\n"
		"PARENT G H I J K CHILD L M N O P\n"
		"SCRIPT DEFER 3 20 DEBUG foo ALL PRE A my.sh\n"
		"SCRIPT DEBUG bar STDOUT DEFER 8 100 PRE B my.sh $NODES \"this is a test\"      \n"
		"SCRIPT DEFER 5 10 POST C my.sh 1 1 testing\n"
		"SCRIPT DEBUG script.err STDERR POST D my.sh\n"
		"SCRIPT HOLD E my.sh\n"
		"\n     \n\n  \n" // Pure whitespace should be ignored
		"RETRY A 100 UNLESS-EXIT 2\n"
		"RETRY B 42\n"
		"ABORT-DAG-ON B 32 RETURN 8\n"
		"ABORT_DAG_ON C 5\n"
		"VARS A PREPEND foo = Bar    baz= foo  k=v a =32\n"
		"VARS B APPEND a =\"woah =!= UNDEFINED\" test='12\\'34'\n"
		"VARS C country=USA\n"
		"PRIORITY D -82\n"
		"PRE-SKIP E 12\n"
		"SAVE-POINT-FILE B\n"
		"SAVE_POINT_FILE C ../custom/path/important.save\n"
		"CATEGORY C CAT-1\n"
		"MAXJOBS CAT-1 2\n"
		"// This is also a comment that should be ignored\n"
		"CONFIG /path/to/custom/dag.conf\n"
		"DOT visual.dot\n"
		"DOT visual-2.dot UPDATE OVERWRITE\n"
		"DOT visual-3.dot DONT-UPDATE DONT-OVERWRITE INCLUDE ../path/to/header.dot\n"
		"NODE_STATUS_FILE foo 82 ALWAYS-UPDATE 1000\n"
		"ENV SET   foo bar baz \n"
		"ENV GET  foo=bar;baz=10;      \n"
		"CONNECT S1 S2\n"
		"PIN_IN A 2\n"
		"PIN_OUT A 3\n"
		"DONE A\n"
		"INCLUDE include.dag\n"
		"JOBSTATE_LOG some.log\n"
		"SET_JOB_ATTR foo=bar\n",
	},
	{
		// Each command in this file is invalid/produces an error
		"parse-failure.dag",
		// Contents (Note single string broken across many lines)
		"JOB\n"
		"JOB ALL_NODES\n"
		"JOB PARENT\n"
		"JOB CHILD\n"
		"SERVICE FOO+BAR submit.desc\n"
		"FINAL A\n"
		"PROVISIONER A foo.sub DIR\n"
		"JOB A foo.sub garbage\n"
		"SUBDAG EXTERNAL A foo.sub NOOP DONE DIR subdir garbage\n"
		"SUBDAG\n"
		"SPLICE\n"
		"SPLICE name\n"
		"SPLICE name splice.dag DIR\n"
		"SPLICE name splice.dag garbage\n"
		"SPLICE name splice.dag DIR subdir garbage\n"
		"PARENT\n"
		"PARENT CHILD A B C\n"
		"PARENT A B C\n"
		"PARENT A CHILD\n"
		"SCRIPT\n"
		"SCRIPT DEFER\n"
		"SCRIPT DEFER foo\n"
		"SCRIPT DEFER 1\n"
		"SCRIPT DEFER 1 foo\n"
		"SCRIPT DEBUG\n"
		"SCRIPT DEBUG script.out\n"
		"SCRIPT DEBUG script.out UNKNOWN\n"
		"SCRIPT PRE\n"
		"SCRIPT PRE A\n"
		"SCRIPT PRE A       \n"
		"RETRY\n"
		"RETRY A\n"
		"RETRY A foo\n"
		"RETRY A 1 garbage\n"
		"RETRY A 1 UNLESS-EXIT\n"
		"RETRY A 1 UNLESS-EXIT foo\n"
		"RETRY A 1 UNLESS-EXIT 1 garbage\n"
		"ABORT_DAG_ON\n"
		"ABORT_DAG_ON A\n"
		"ABORT_DAG_ON A foo\n"
		"ABORT_DAG_ON A 1 garbage\n"
		"ABORT_DAG_ON A 1 RETURN\n"
		"ABORT_DAG_ON A 1 RETURN foo\n"
		"ABORT_DAG_ON A 1 RETURN 1 garbage\n"
		"VARS\n"
		"VARS A no-pair\n"
		"VARS A +='testing 123'\n"
		"VARS A QUEUE_invalid=1\n"
		"VARS A\n"
		"PRIORITY\n"
		"PRIORITY A\n"
		"PRIORITY A foo\n"
		"PRIORITY A 1 garbage\n"
		"PRE_SKIP\n"
		"PRE_SKIP A\n"
		"PRE_SKIP A foo\n"
		"PRE_SKIP A 1 garbage\n"
		"SAVE_POINT_FILE\n"
		"SAVE_POINT_FILE A file garbage\n"
		"CATEGORY\n"
		"CATEGORY A\n"
		"CATEGORY A FOO garbage\n"
		"MAXJOBS\n"
		"MAXJOBS FOO\n"
		"MAXJOBS FOO foo\n"
		"MAXJOBS FOO 1 garbage\n"
		"CONFIG\n"
		"CONFIG file garbage\n"
		"DOT\n"
		"DOT file garbage\n"
		"DOT file INCLUDE\n"
		"NODE_STATUS_FILE\n"
		"NODE_STATUS_FILE file garbage\n"
		"ENV\n"
		"ENV garbage\n"
		"ENV SET\n"
		"ENV GET     \n"
		"DONE\n"
		"DONE A garbage\n"
		"INCLUDE\n"
		"INCLUDE file garbage\n"
		"JOBSTATE_LOG\n"
		"JOBSTATE_LOG file garbage\n"
		"SET_JOB_ATTR\n"
		"REJECT garbage\n"
		"DNE blah blah blah blah\n"
		"SUBMIT_DESCRIPTION\n"
		"SUBMIT_DESCRIPTION foo\n"
		"SUBMIT_DESCRIPTION foo non-inline-tag\n"
		"CONNECT\n"
		"CONNECT s1\n"
		"CONNECT s1 s2 garbage\n"
		// PIN IN/OUT parsed the same
		"PIN_IN\n"
		"PIN_OUT A\n"
		"PIN_IN A foo\n"
		"PIN_OUT A 1 garbage\n"
	},
	{
		// DAG with missing final newline
		"missing-final-newline.dag",
		"REJECT",
	},
	{
		// DAG that ends with skipped lines (i.e. pure whitespace and comments)
		// Produced infinite loops during development (no longer infinite loops)
		"trailing-skipped-lines.dag",
		"REJECT\n\n\n\n"
		"# Comment 1\n\n\n"
		"// Comment 2\n\n",
	},
	{
		// Check specific failure of SUBMIT_DESCRIPTION is missing ending token
		"submit-desc-no-end.dag",
		"SUBMIT-DESCRIPTION foo @=end\n"
		"    executable = /bin/sleep\n"
		"    arguments  = 0\n"
		"    log        = $(ClusterId).log\n"
		"# Note: no closing @end token\n"
		"JOB A test.sub\n"
	},
	{
		// Check specific failure of Node inline descriptions missing ending token
		// Note: This applies to all node types except SUBDAGs (as SUBDAGs can not be inlined)
		"inline-node-no-desc-end.dag",
		"JOB A {\n"
		"    executable = /bin/sleep\n"
		"    arguments  = 0\n"
		"    log        = $(ClusterId).log\n"
		"# Note: no closing } token\n"
		"JOB B test.sub\n"
	}
};

//------------------------------------------------------------------------------------
std::vector<std::vector<std::string>> TEST_EXPECTED_RESULTS = {
	{
		// Expected successful DAG file command output
		"REJECT > successful.dag:1",
		"JOB > A foo.sub {NONE}  F F",
		"JOB > B foo.sub {NONE} subdir/test T F",
		"JOB > C bar.sub {NONE}  F T",
		"JOB > D INLINE {executable = /bin/sleep\x1Fqueue 1\x1F} foo T F",
		"PROVISIONER > E foo.sub {NONE}  F F",
		"FINAL > F bar.sub {NONE}  F F",
		"SERVICE > G testing.sub {NONE} one/two/three F F",
		"SUBDAG > INNER_DAG test.dag {NONE} subdir T T",
		"SPLICE > test-splice test.dag test/subdir",
		"SUBMIT_DESCRIPTION > echo-desc {executable = /bin/echo""\x1F""arguments  = 'Hello World!'\x1Foutput     = $(ClusterId).out\x1Flog        = echo.log\x1Fqueue 10\x1F}",
		"PARENT > [ A ] --> [ B ]",
		"PARENT > [ A ] --> [ C D E ]",
		"PARENT > [ C D E ] --> [ F ]",
		"PARENT > [ G H I J K ] --> [ L M N O P ]",
		"SCRIPT > A PRE 'my.sh' 20 3 foo ALL",
		"SCRIPT > B PRE 'my.sh $NODES \"this is a test\"' 100 8 bar STDOUT",
		"SCRIPT > C POST 'my.sh 1 1 testing' 10 5  NONE",
		"SCRIPT > D POST 'my.sh' 0 -1 script.err STDERR",
		"SCRIPT > E HOLD 'my.sh' 0 -1  NONE",
		"RETRY > A 100 2",
		"RETRY > B 42 0",
		"ABORT_DAG_ON > B 32 8",
		"ABORT_DAG_ON > C 5 " + int_max_str,
		"VARS > A PREPEND [a=32] [baz=foo] [foo=Bar] [k=v]",
		"VARS > B APPEND [a=\"woah =!= UNDEFINED\"] [test='12'34']",
		"VARS > C [country=USA]",
		"PRIORITY > D -82",
		"PRE_SKIP > E 12",
		"SAVE_POINT_FILE > B B-successful.dag.save",
		"SAVE_POINT_FILE > C ../custom/path/important.save",
		"CATEGORY > CAT-1 C",
		"MAXJOBS > CAT-1 2",
		"CONFIG > /path/to/custom/dag.conf",
		"DOT > visual.dot  F F",
		"DOT > visual-2.dot  T T",
		"DOT > visual-3.dot ../path/to/header.dot F F",
		"NODE_STATUS_FILE > foo 1000 T",
		"ENV > SET foo bar baz",
		"ENV > GET foo=bar;baz=10;",
		"CONNECT > [S1]--[S2]",
		"PIN_IN > A 2 IN",
		"PIN_OUT > A 3 OUT",
		"DONE > A",
		"INCLUDE > include.dag",
		"JOBSTATE_LOG > some.log",
		"SET_JOB_ATTR > foo=bar",
	},
	{
		// *All expected command parsing failures (note some internal developer errors not included)
		// Note All of the JOB Commands apply to node likes (i.e. SERVICE, PROVISIONER, FINAL, and SUBDAG)
		"parse-failure.dag:1 Failed to parse JOB command: Missing node name",
		"parse-failure.dag:2 Failed to parse JOB command: Node name is a reserved word",
		"parse-failure.dag:3 Failed to parse JOB command: Node name is a reserved word",
		"parse-failure.dag:4 Failed to parse JOB command: Node name is a reserved word",
		"parse-failure.dag:5 Failed to parse SERVICE command: Node name contains illegal charater",
		"parse-failure.dag:6 Failed to parse FINAL command: No submit description provided",
		"parse-failure.dag:7 Failed to parse PROVISIONER command: No directory path provided for DIR subcommand",
		"parse-failure.dag:8 Failed to parse JOB command: Unexpected token 'garbage'",
		"parse-failure.dag:9 Failed to parse SUBDAG command: Unexpected token 'garbage'",
		// Unique SUBDAG error
		"parse-failure.dag:10 Failed to parse SUBDAG command: Missing EXTERNAL keyword",
		"parse-failure.dag:11 Failed to parse SPLICE command: Missing splice name",
		"parse-failure.dag:12 Failed to parse SPLICE command: Missing DAG file",
		"parse-failure.dag:13 Failed to parse SPLICE command: No directory path provided for DIR subcommand",
		"parse-failure.dag:14 Failed to parse SPLICE command: Unexpected token 'garbage'",
		"parse-failure.dag:15 Failed to parse SPLICE command: Unexpected token 'garbage'",
		"parse-failure.dag:16 Failed to parse PARENT command: No parent node(s) specified",
		"parse-failure.dag:17 Failed to parse PARENT command: No parent node(s) specified",
		"parse-failure.dag:18 Failed to parse PARENT command: Missing CHILD specifier",
		"parse-failure.dag:19 Failed to parse PARENT command: No children node(s) specified",
		// Script parsing same for all script types
		"parse-failure.dag:20 Failed to parse SCRIPT command: Missing script type (PRE, POST, HOLD) and optional sub-commands (DEFER, DEBUG)",
		"parse-failure.dag:21 Failed to parse SCRIPT command: DEFER missing status value",
		"parse-failure.dag:22 Failed to parse SCRIPT command: Invalid DEFER status value 'foo'",
		"parse-failure.dag:23 Failed to parse SCRIPT command: DEFER missing time value",
		"parse-failure.dag:24 Failed to parse SCRIPT command: Invalid DEFER time value 'foo'",
		"parse-failure.dag:25 Failed to parse SCRIPT command: DEBUG missing filename",
		"parse-failure.dag:26 Failed to parse SCRIPT command: DEBUG missing output stream type (STDOUT, STDERR, ALL)",
		"parse-failure.dag:27 Failed to parse SCRIPT command: Unknown DEBUG output stream type 'UNKNOWN'",
		"parse-failure.dag:28 Failed to parse SCRIPT command: No node name specified",
		"parse-failure.dag:29 Failed to parse SCRIPT command: No script specified",
		"parse-failure.dag:30 Failed to parse SCRIPT command: No script specified",
		"parse-failure.dag:31 Failed to parse RETRY command: No node name specified",
		"parse-failure.dag:32 Failed to parse RETRY command: Missing max retry value",
		"parse-failure.dag:33 Failed to parse RETRY command: Invalid max retry value 'foo' (Must be positive integer)",
		"parse-failure.dag:34 Failed to parse RETRY command: Unexpected token 'garbage'",
		"parse-failure.dag:35 Failed to parse RETRY command: UNLESS-EXIT missing exit code",
		"parse-failure.dag:36 Failed to parse RETRY command: Invalid UNLESS-EXIT code 'foo' specified",
		"parse-failure.dag:37 Failed to parse RETRY command: Unexpected token 'garbage'",
		"parse-failure.dag:38 Failed to parse ABORT_DAG_ON command: No node name specified",
		"parse-failure.dag:39 Failed to parse ABORT_DAG_ON command: Missing exit status to abort on",
		"parse-failure.dag:40 Failed to parse ABORT_DAG_ON command: Invalid exit status 'foo' specified",
		"parse-failure.dag:41 Failed to parse ABORT_DAG_ON command: Unexpected token 'garbage'",
		"parse-failure.dag:42 Failed to parse ABORT_DAG_ON command: RETURN is missing value",
		"parse-failure.dag:43 Failed to parse ABORT_DAG_ON command: Invalid RETURN code 'foo' (Must be between 0 and 255)",
		"parse-failure.dag:44 Failed to parse ABORT_DAG_ON command: Unexpected token 'garbage'",
		"parse-failure.dag:45 Failed to parse VARS command: No node name specified",
		"parse-failure.dag:46 Failed to parse VARS command: Non key=value token specified: no-pair",
		"parse-failure.dag:47 Failed to parse VARS command: Variable name must contain at least one alphanumeric character",
		"parse-failure.dag:48 Failed to parse VARS command: Illegal variable name 'QUEUE_invalid': name can not begin with 'queue'",
		"parse-failure.dag:49 Failed to parse VARS command: No key=value pairs specified",
		"parse-failure.dag:50 Failed to parse PRIORITY command: No node name specified",
		"parse-failure.dag:51 Failed to parse PRIORITY command: Missing priority value",
		"parse-failure.dag:52 Failed to parse PRIORITY command: Invalid priority value 'foo'",
		"parse-failure.dag:53 Failed to parse PRIORITY command: Unexpected token 'garbage'",
		"parse-failure.dag:54 Failed to parse PRE_SKIP command: No node name specified",
		"parse-failure.dag:55 Failed to parse PRE_SKIP command: Missing exit code",
		"parse-failure.dag:56 Failed to parse PRE_SKIP command: Invalid exit code 'foo'",
		"parse-failure.dag:57 Failed to parse PRE_SKIP command: Unexpected token 'garbage'",
		"parse-failure.dag:58 Failed to parse SAVE_POINT_FILE command: No node name specified",
		"parse-failure.dag:59 Failed to parse SAVE_POINT_FILE command: Unexpected token 'garbage'",
		"parse-failure.dag:60 Failed to parse CATEGORY command: No node name specified",
		"parse-failure.dag:61 Failed to parse CATEGORY command: No category name specified",
		"parse-failure.dag:62 Failed to parse CATEGORY command: Unexpected token 'garbage'",
		"parse-failure.dag:63 Failed to parse MAXJOBS command: No category name specified",
		"parse-failure.dag:64 Failed to parse MAXJOBS command: No throttle limit specified",
		"parse-failure.dag:65 Failed to parse MAXJOBS command: Invalid throttle limit 'foo'",
		"parse-failure.dag:66 Failed to parse MAXJOBS command: Unexpected token 'garbage'",
		"parse-failure.dag:67 Failed to parse CONFIG command: No configuration file specified",
		"parse-failure.dag:68 Failed to parse CONFIG command: Unexpected token 'garbage'",
		"parse-failure.dag:69 Failed to parse DOT command: No file specified",
		"parse-failure.dag:70 Failed to parse DOT command: Unexpected token 'garbage'",
		"parse-failure.dag:71 Failed to parse DOT command: Missing INCLUDE header file",
		"parse-failure.dag:72 Failed to parse NODE_STATUS_FILE command: No file specified",
		"parse-failure.dag:73 Failed to parse NODE_STATUS_FILE command: Unexpected token 'garbage'",
		"parse-failure.dag:74 Failed to parse ENV command: Missing action (SET or GET) and variables",
		"parse-failure.dag:75 Failed to parse ENV command: Unexpected token 'garbage'",
		"parse-failure.dag:76 Failed to parse ENV command: No environment variables provided",
		"parse-failure.dag:77 Failed to parse ENV command: No environment variables provided",
		"parse-failure.dag:78 Failed to parse DONE command: No node name specified",
		"parse-failure.dag:79 Failed to parse DONE command: Unexpected token 'garbage'",
		"parse-failure.dag:80 Failed to parse INCLUDE command: No include file specified",
		"parse-failure.dag:81 Failed to parse INCLUDE command: Unexpected token 'garbage'",
		"parse-failure.dag:82 Failed to parse JOBSTATE_LOG command: No include file specified",
		"parse-failure.dag:83 Failed to parse JOBSTATE_LOG command: Unexpected token 'garbage'",
		"parse-failure.dag:84 Failed to parse SET_JOB_ATTR command: No attribute line (key = value) provided",
		"parse-failure.dag:85 Failed to parse REJECT command: Unexpected token 'garbage'",
		"parse-failure.dag:86 'DNE' is not a valid DAG command",
		"parse-failure.dag:87 Failed to parse SUBMIT_DESCRIPTION command: No submit description name provided",
		"parse-failure.dag:88 Failed to parse SUBMIT_DESCRIPTION command: No inline description provided",
		"parse-failure.dag:89 Failed to parse SUBMIT_DESCRIPTION command: No inline description provided",
		"parse-failure.dag:90 Failed to parse CONNECT command: Missing splice(s) to connect",
		"parse-failure.dag:91 Failed to parse CONNECT command: Missing splice(s) to connect",
		"parse-failure.dag:92 Failed to parse CONNECT command: Unexpected token 'garbage'",
		"parse-failure.dag:93 Failed to parse PIN_IN command: No node name specified",
		"parse-failure.dag:94 Failed to parse PIN_OUT command: No pin number specified",
		"parse-failure.dag:95 Failed to parse PIN_IN command: Invalid pin number 'foo'",
		"parse-failure.dag:96 Failed to parse PIN_OUT command: Unexpected token 'garbage'",
	},
};

//------------------------------------------------------------------------------------
static bool test_dag_file_parser_success() {
	const auto& [dag, _] = TEST_FILES[SUCCESS_TEST];
	const auto& expected = TEST_EXPECTED_RESULTS[SUCCESS_TEST];
	DagParser test(dag);
	size_t i = 0;
	std::string msg = "DagParser unexpectedly failed: ";

	emit_test("Test Parser Parses all DAG Commands Successfully");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (i >= expected.size()) {
			emit_comment("More iterations than expected made in for loop");
			FAIL;
		} else if ( ! item) { break; }

		emit_output_expected_header();
		emit_retval(expected[i].c_str());

		emit_output_actual_header();
		emit_retval(item->GetDetails().c_str());

		if (expected[i] != item->GetDetails()) {
			FAIL;
		}

		i++;
	}

	if (test.failed()) {
		msg += test.error();
		emit_comment(msg.c_str());
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_file_parser_search_filter() {
	const auto& [dag, _] = TEST_FILES[SUCCESS_TEST];
	const std::vector<std::string> expected = {
		"PARENT > [ A ] --> [ B ]",
		"PARENT > [ A ] --> [ C D E ]",
		"PARENT > [ C D E ] --> [ F ]",
		"PARENT > [ G H I J K ] --> [ L M N O P ]",
	};
	DagParser test(dag);
	test.SearchFor(DAG::CMD::PARENT_CHILD);

	size_t i = 0;
	std::string msg = "DagParser unexpectedly failed: ";

	emit_test("Test Parser Parses all DAG Commands Successfully");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (i == expected.size()) {
			continue;
		} else if (i > expected.size()) {
			emit_comment("More iterations than expected made in for loop");
			FAIL;
		} else if ( ! item) { break; }

		emit_output_expected_header();
		emit_retval(expected[i].c_str());

		emit_output_actual_header();
		emit_retval(item->GetDetails().c_str());

		if (expected[i] != item->GetDetails()) {
			FAIL;
		}

		i++;
	}

	if (test.failed()) {
		msg += test.error();
		emit_comment(msg.c_str());
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_file_parser_ignore_filter() {
	const auto& [dag, _] = TEST_FILES[SUCCESS_TEST];
	const std::vector<std::string> expected = {
		"PARENT > [ A ] --> [ B ]",
		"PARENT > [ A ] --> [ C D E ]",
		"PARENT > [ C D E ] --> [ F ]",
		"PARENT > [ G H I J K ] --> [ L M N O P ]",
	};

	std::set<DAG::CMD> search = {
		DAG::CMD::PARENT_CHILD,
		DAG::CMD::JOB,
	};


	DagParser test(dag);
	test.SearchFor(search).Ignore(DAG::CMD::JOB);

	size_t i = 0;
	std::string msg = "DagParser unexpectedly failed: ";

	emit_test("Test Parser Parses all DAG Commands Successfully");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (i == expected.size()) {
			continue;
		} else if (i > expected.size()) {
			emit_comment("More iterations than expected made in for loop");
			FAIL;
		} else if ( ! item) { break; }

		emit_output_expected_header();
		emit_retval(expected[i].c_str());

		emit_output_actual_header();
		emit_retval(item->GetDetails().c_str());

		if (expected[i] != item->GetDetails()) {
			FAIL;
		}

		i++;
	}

	if (test.failed()) {
		msg += test.error();
		emit_comment(msg.c_str());
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_file_parse_failure() {
	const auto& [dag, _] = TEST_FILES[FAILURE_TEST];
	const auto& expected = TEST_EXPECTED_RESULTS[FAILURE_TEST];
	DagParser test(dag);
	size_t i = 0;

	emit_test("Test Parser fails at first parse failure");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (i++ >= 1) {
			emit_comment("More iterations than expected made in for loop");
			FAIL;
		}
	}

	emit_output_expected_header();
	emit_retval(expected[0].c_str());

	emit_output_actual_header();
	emit_retval(test.c_error());

	if ( ! test.failed() || expected[0] != test.error()) {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_file_parse_all_failures() {
	const auto& [dag, _] = TEST_FILES[FAILURE_TEST];
	const auto& expected = TEST_EXPECTED_RESULTS[FAILURE_TEST];
	DagParser test(dag);
	test.ContOnParseFailure();
	size_t i = 0;

	emit_test("Test Parser Continue on parse failure captures all failures");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (i++ > 1) {
			emit_comment("More iterations than expected made in for loop");
			FAIL;
		}
	}

	if ( ! test.failed()) {
		FAIL;
	} else if (test.NumErrors() != expected.size()) {
		emit_comment("Unexpected number of errors from parser");
	}

	i = 0;
	for (const auto& err : test.GetErrorList()) {
		emit_output_expected_header();
		emit_retval(expected[i].c_str());

		emit_output_actual_header();
		emit_retval(err.c_str());

		if (expected[i] != err) {
			FAIL;
		}

		i++;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_no_submit_description_end_failure() {
	const auto& [dag, _] = TEST_FILES[NO_SUBMIT_DESC_END_TEST];
	const std::string expected = std::string(dag) + ":1 Failed to parse SUBMIT_DESCRIPTION command: Missing inline description closing token: @end";
	DagParser test(dag);
	size_t i = 0;

	emit_test("Test Parser fails when no inline submit description end is specified");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (i++ >= 1) {
			emit_comment("More iterations than expected made in for loop");
			FAIL;
		}
	}

	emit_output_expected_header();
	emit_retval(expected.c_str());

	emit_output_actual_header();
	emit_retval(test.c_error());

	if ( ! test.failed() || expected != test.error()) {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_no_node_inline_desc_end_failure() {
	const auto& [dag, _] = TEST_FILES[NO_INLINE_NODE_DESC_END_TEST];
	const std::string expected = std::string(dag) + ":1 Failed to parse JOB command: Missing inline description closing token: }";
	DagParser test(dag);
	size_t i = 0;

	emit_test("Test Parser fails when no node inline submit description end is specified");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (i++ >= 1) {
			emit_comment("More iterations than expected made in for loop");
			FAIL;
		}
	}

	emit_output_expected_header();
	emit_retval(expected.c_str());

	emit_output_actual_header();
	emit_retval(test.c_error());

	if ( ! test.failed() || expected != test.error()) {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_allow_illegal_chars() {
	const auto& [dag, _] = TEST_FILES[FAILURE_TEST];
	const std::string expected = "FOO+BAR";
	std::string result;
	DagParser test(dag);

	test.AllowIllegalChars().ContOnParseFailure();

	emit_test("Test Parser fails at first parse failure");
	emit_input_header();
	emit_param("DAG File", dag);

	for (const auto& item : test) {
		if (item && item->GetCommand() == DAG::CMD::SERVICE) {
			result = ((ServiceCommand*)item.get())->GetName();
			break;
		}
	}

	emit_output_expected_header();
	emit_retval(expected.c_str());

	emit_output_actual_header();
	emit_retval(result.c_str());

	if (expected != result) {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_missing_final_newline() {
	const char* file = TEST_FILES[MISSING_NEWLINE_TEST].first;
	DagParser test(file);
	int i = 0;

	emit_test("Test Parser does not break with no final newline");
	emit_input_header();
	emit_param("DAG File", file);

	for (const auto& cmd : test) {
		if (i++ > 1) { FAIL; }
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_trailing_skip_lines() {
	const char* file = TEST_FILES[SKIP_LINE_TEST].first;
	DagParser test(file);
	int i = 0;

	emit_test("Test Parser does not get in infinite loop when final lines in DAG are skipped");
	emit_input_header();
	emit_param("DAG File", file);

	for (const auto& cmd : test) {
		if (i++ > 1) { FAIL; }
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_c_string_init() {
	const char* file = TEST_FILES[SUCCESS_TEST].first;
	DagParser test(file);

	emit_test("Test Parser initializes with C string");
	emit_input_header();
	emit_param("File Variable type", "const char*");

	emit_output_expected_header();
	emit_retval("FALSE");

	emit_output_actual_header();
	emit_retval(tfstr(test.failed()));

	if (test.failed()) {
		emit_comment(test.c_error());
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_std_string_init() {
	std::string file(TEST_FILES[SUCCESS_TEST].first);
	DagParser test(file);

	emit_test("Test Parser initializes with std::string");
	emit_input_header();
	emit_param("File Variable type", "std::string");

	emit_output_expected_header();
	emit_retval("FALSE");

	emit_output_actual_header();
	emit_retval(tfstr(test.failed()));

	if (test.failed()) {
		emit_comment(test.c_error());
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_std_path_init() {
	std::filesystem::path file(TEST_FILES[SUCCESS_TEST].first);
	DagParser test(file);

	emit_test("Test Parser initializes with std::filesystem::path");
	emit_input_header();
	emit_param("File Variable type", "std::filesystem::path");

	emit_output_expected_header();
	emit_retval("FALSE");

	emit_output_actual_header();
	emit_retval(tfstr(test.failed()));

	if (test.failed()) {
		emit_comment(test.c_error());
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_empty_path() {
	DagParser test("");

	emit_test("Test Parser initialization fail with no path");

	emit_output_expected_header();
	emit_retval("No file path provided");

	emit_output_actual_header();
	emit_retval(test.c_error());

	if ( ! test.failed()) {
		emit_comment("Parser initialization did not fail");
		FAIL;
	} else if (test.error() != "No file path provided") {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_file_dne() {
	DagParser test("tHiS-sHoUlD-nOt.ExIsT");

	emit_test("Test Parser initialization fails for non-existent file");

	emit_output_expected_header();
	emit_retval("Provided file path does not exist");

	emit_output_actual_header();
	emit_retval(test.c_error());

	if ( ! test.failed()) {
		emit_comment("Parser initialization did not fail");
		FAIL;
	} else if (test.error() != "Provided file path does not exist") {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_path_is_not_file() {
	DagParser test(".");

	emit_test("Test Parser initialization fails for non-file paths");

	emit_output_expected_header();
	emit_retval("Provided file path is not a file");

	emit_output_actual_header();
	emit_retval(test.c_error());

	if ( ! test.failed()) {
		emit_comment("Parser initialization did not fail");
		FAIL;
	} else if (test.error() != "Provided file path is not a file") {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool generate_file(const char* name, const char* contents) {
	if ( ! name) {
		emit_comment("No filename provided");
		return false;
	} else if ( ! contents) {
		emit_comment("No DAG file contents provided");
		return false;
	} else {
		std::string msg("Generating DAG file: "); msg += name;
		emit_comment(msg.c_str());
	}

	FILE* f = fopen(name, "w");
	if ( ! f) {
		std::string msg("Failed to open file "); msg += name;
		emit_comment(msg.c_str());
		return false;
	}

	size_t len = strlen(contents);
	if (fwrite(contents, sizeof(char), len, f) != len) {
		std::string msg("Failed to write contents to file "); msg += name;
		emit_comment(msg.c_str());
		fclose(f);
		return false;
	}

	fflush(f);

	fclose(f);
	return true;
}

static bool make_dag_files() {
	for (const auto& [file, contents] : TEST_FILES) {
		if ( ! generate_file(file, contents)) { return false; }
	}
	return true;
}

//------------------------------------------------------------------------------------
bool OTEST_DagFileParser() {
	emit_object("DagFileParser");
	emit_comment("Testing ranged based compatible DAG file parser.");

	if ( ! make_dag_files()) { return false; }

	FunctionDriver driver;

	// Test invalid initialization
	driver.register_function(test_empty_path);
	driver.register_function(test_file_dne);
	driver.register_function(test_path_is_not_file);

	// Test Successful initialization
	driver.register_function(test_c_string_init);
	driver.register_function(test_std_string_init);
	driver.register_function(test_std_path_init);

	// Special test cases in handling weird file formatting (prevent infinite loops)
	driver.register_function(test_missing_final_newline);
	driver.register_function(test_trailing_skip_lines);

	// Test Successful parsing of all commands
	driver.register_function(test_dag_file_parser_success);

	// Test various parsing failures for all commands
	driver.register_function(test_dag_file_parse_failure);
	driver.register_function(test_dag_file_parse_all_failures);
	driver.register_function(test_no_submit_description_end_failure);
	driver.register_function(test_no_node_inline_desc_end_failure);

	// Other parser functionality testing
	driver.register_function(test_allow_illegal_chars);
	driver.register_function(test_dag_file_parser_search_filter);
	driver.register_function(test_dag_file_parser_ignore_filter);

	return driver.do_all_functions();
}

//====================================================================================

struct testcase1 {
	std::string input{};
	std::vector<std::string> expected{};
	bool trim_quotes{false};
};

const testcase1 test_table1[] = {
	{"foo bar baz", {"foo", "bar", "baz", ""}},
	{"'this is a single token'", {"'this is a single token'", ""}},
	{"'this is a single token'", {"this is a single token", ""}, true},
	{"\"One Two\" Three Four \"Five 'Six'\"", {"\"One Two\"", "Three", "Four", "\"Five 'Six'\"", ""}},
	{"'One \\'Two\\' Three' Four", {"'One 'Two' Three'", "Four", ""}},
	{"k1=v1 k2= v2 k3 = v3 k4 =v4 k5      =        v5", {"k1=v1", "k2=v2", "k3=v3", "k4=v4", "k5=v5", ""}},
	{"k1 = \"v1\" k2 = 'v2' k3 = 'kn = vn'", {"k1=\"v1\"", "k2='v2'", "k3='kn = vn'", ""}},
	{"'One \\\\' 'Two \\''", {"'One \\'", "'Two ''", ""}},
};

//------------------------------------------------------------------------------------
struct testcase2 {
	std::string input{};
	std::vector<std::string> expected{};
	std::string error{};
};

const testcase2 test_table2[] = {
	{"One Two 'Three Four", {"One", "Two", ""}, "Invalid quoting: no ending quote found"},
	{"One Two \"Three Four", {"One", "Two", ""}, "Invalid quoting: no ending quote found"},
	{"One Two Three=", {"One", "Two", ""}, "Invalid key value pair: no value discovered"},
	{"One Two Three  =", {"One", "Two", ""}, "Invalid key value pair: no value discovered"},
	{"One Two Three=     ", {"One", "Two", ""}, "Invalid key value pair: no value discovered"},
};

//------------------------------------------------------------------------------------
struct testcase3 {
	std::string input{};
	std::vector<std::string> expected{};
	int remainder_at_i{0};
};

const testcase3 test_table3[] = {
	{"One Two Three Four Five", {"One", "Two", "Three Four Five", ""}, 3},
	{"One      Two   Three    ", {"One", "Two   Three    ", ""}, 2},
	{"One 'Two Three' \"Four Five\" key   =   value", {"One", "'Two Three'", "\"Four Five\" key   =   value", ""}, 3},
};

//////////////////////////////////////////////
// do some template magic to automatically register one
// test function per test case in the above tables

// one templated function gets generated for every test case, so we keep
// each one very light weight as a wrapper around the main test function
// for each table

#define ARRAY_LEN(arr) (sizeof arr / sizeof arr[0])

#define TEST_TABLE_SETUP(n, funcname)                                         \
                                                                              \
static const int TEST_TABLE##n##_COUNT = ARRAY_LEN(test_table##n);            \
                                                                              \
static bool funcname(int N);                                                  \
                                                                              \
template <int N>                                                              \
static bool funcname##_tmpl() { return funcname(N); }                         \
                                                                              \
template <int N>                                                              \
static void driver_register_all##n(FunctionDriver &driver)                    \
{                                                                             \
    driver.register_function(funcname##_tmpl<N>);                             \
    driver_register_all##n<N+1>(driver);                                      \
}                                                                             \
                                                                              \
template <>                                                                   \
void driver_register_all##n<TEST_TABLE##n##_COUNT>(FunctionDriver &driver)    \
{                                                                             \
    (void) driver;                                                            \
}                                                                             \
                                                                              \
// one for each test_tableN and corresponding test function
TEST_TABLE_SETUP(1, test_dag_lexer_next)
TEST_TABLE_SETUP(2, test_dag_lexer_failures)
TEST_TABLE_SETUP(3, test_dag_lexer_remainder)
//////////////////////////////////////////////

;
static bool test_dag_lexer_next(int N) {
	const testcase1 &test = test_table1[N];
	DagLexer lex(test.input);

	emit_test("Test DagLexer functions correctly");
	emit_input_header();
	emit_param("Line", test.input.c_str());

	for (const auto& expected : test.expected) {
		emit_output_expected_header();
		emit_retval(expected.c_str());

		std::string result = lex.next(test.trim_quotes);

		emit_output_actual_header();
		emit_retval(result.c_str());

		if (expected != result) {
			FAIL;
		}
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_lexer_failures(int N) {
	const testcase2 &test = test_table2[N];
	DagLexer lex(test.input);

	emit_test("Test DagLexer functions correctly");
	emit_input_header();
	emit_param("Line", test.input.c_str());

	for (const auto& expected : test.expected) {
		emit_output_expected_header();
		emit_retval(expected.c_str());

		std::string result = lex.next();

		emit_output_actual_header();
		emit_retval(result.c_str());

		if (expected != result) {
			FAIL;
		}
	}

	emit_output_expected_header();
	emit_retval(test.error.c_str());

	emit_output_actual_header();
	emit_retval(lex.c_error());

	if ( ! lex.failed() || lex.error() != test.error) {
		FAIL;
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_lexer_remainder(int N) {
	const testcase3 &test = test_table3[N];
	int i = 0;
	DagLexer lex(test.input);

	emit_test("Test DagLexer gets remainder correctly");
	emit_input_header();
	emit_param("Line", test.input.c_str());

	for (const auto& expected : test.expected) {
		emit_output_expected_header();
		emit_retval(expected.c_str());

		std::string result = (++i == test.remainder_at_i) ? lex.remain() : lex.next();

		emit_output_actual_header();
		emit_retval(result.c_str());

		if (expected != result) {
			FAIL;
		}
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_lexer_new_parse() {
	const std::string initial = "One Two Three Four Five Six Seven";
	const std::string swap = "Foo Bar Baz";
	std::vector<std::string> expected = {"One", "Two", "Foo", "Bar", "Baz"};

	const int swap_at_i = 2;
	int i = 0;

	DagLexer lex(initial);

	emit_test("Test DagLexer parse new string");
	emit_input_header();
	emit_param("Initial", initial.c_str());
	emit_param("Swap", swap.c_str());

	for (const auto& check : expected) {
		if (i++ == swap_at_i) {
			lex.parse(swap);
		}

		emit_output_expected_header();
		emit_retval(check.c_str());

		std::string result = lex.next();

		emit_output_actual_header();
		emit_retval(result.c_str());

		if (check != result) {
			FAIL;
		}
	}

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dag_lexer_reset() {
	const std::string initial = "One Two Three Four Five Six Seven";
	std::vector<std::string> expected = {"One", "Two", "One", "Two", "Three", "Four", "Five", "Six", "Seven"};

	const int reset_at_i = 2;
	int i = 0;

	DagLexer lex(initial);

	emit_test("Test DagLexer reset lexer to start of string");
	emit_input_header();
	emit_param("Line", initial.c_str());

	for (const auto& check : expected) {
		if (i++ == reset_at_i) {
			lex.reset();
		}

		emit_output_expected_header();
		emit_retval(check.c_str());

		std::string result = lex.next();

		emit_output_actual_header();
		emit_retval(result.c_str());

		if (check != result) {
			FAIL;
		}
	}

	PASS;
}

//------------------------------------------------------------------------------------
bool OTEST_DagLexer() {
	emit_object("DagLexer");
	emit_comment("Testing DagLexer functionality");

	FunctionDriver driver;

	driver_register_all1<0>(driver);
	driver_register_all2<0>(driver);
	driver_register_all3<0>(driver);

	driver.register_function(test_dag_lexer_new_parse);
	driver.register_function(test_dag_lexer_reset);

	return driver.do_all_functions();
}
