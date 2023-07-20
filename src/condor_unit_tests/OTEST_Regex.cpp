/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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


/* Test the Regex implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_regex.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

	// test functions
static bool test_match(void);
static bool test_doesnt_match(void);
static bool test_captures(void);

bool OTEST_Regex(void) {
		// beginning junk
	emit_object("Regex");
	emit_comment("A C++ object that provide an inconvenient wrapper around pcre2 regular expressions");

		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_match);
	driver.register_function(test_doesnt_match);
	driver.register_function(test_captures);
	
		// run the tests
	return driver.do_all_functions();
}

static bool test_match() {
	emit_test("A Regex that should match");
	Regex r;
	int errcode = 0;
	int erroffset = 0;
	uint32_t options = 0;
	bool compile_result = r.compile("abcdefg", &errcode, &erroffset, options);
	bool regex_match = false;
	if (compile_result) {
		regex_match = r.match("01234abcdefgzzzzzz");
	}

	emit_input_header();
	emit_output_expected_header();
	emit_output_actual_header();
	emit_param("regex.compile RETURN", "%d", compile_result);
	emit_param("regex.match   RETURN", "%d", regex_match);
	if(!compile_result || !regex_match) {
		FAIL;
	}
	PASS;
}

static bool test_doesnt_match() {
	emit_test("A Regex that should NOT match");
	Regex r;
	int errcode = 0;
	int erroffset = 0;
	uint32_t options = 0;
	bool compile_result = r.compile("abcdefg", &errcode, &erroffset, options);
	bool regex_match = false;
	if (compile_result) {
		regex_match = r.match("01234bcdefgzzzzzz");
	}

	emit_input_header();
	emit_output_expected_header();
	emit_output_actual_header();
	emit_param("regex.compile RETURN", "%d", compile_result);
	emit_param("regex.match   RETURN", "%d", regex_match);
	if(!compile_result || regex_match) {
		FAIL;
	}
	PASS;
}

static bool test_captures() {
	emit_test("A Regex that captures");
	Regex r;
	int errcode = 0;
	int erroffset = 0;
	uint32_t options = 0;
	// pattern is three space separated words, each captured, and a capture around the whole
	// At the start of the first word is a capture of an optional subpattern
	// that won't match anything.
	bool compile_result = r.compile("((0)?([a-z]+) ([a-z]+) ([a-z]+))", &errcode, &erroffset, options);
	bool regex_match = false;
	std::vector<std::string> groups(8);
	if (compile_result) {
		regex_match = r.match("yabba dabba doo", &groups);
	}

	emit_input_header();
	emit_output_expected_header();
	emit_output_actual_header();
	emit_param("regex.compile RETURN", "%d", compile_result);
	emit_param("regex.match      RETURN", "%d", regex_match);
	emit_param("regex.match[0]   RETURN", "%s", groups[0].c_str());
	emit_param("regex.match[1]   RETURN", "%s", groups[1].c_str());
	emit_param("regex.match[2]   RETURN", "%s", groups[2].c_str());
	emit_param("regex.match[3]   RETURN", "%s", groups[3].c_str());
	emit_param("regex.match[4]   RETURN", "%s", groups[4].c_str());
	emit_param("regex.match[5]   RETURN", "%s", groups[5].c_str());
	if(!compile_result || !regex_match || (groups.size() != 6) ||
			(groups[0] != "yabba dabba doo") ||
			(groups[1] != "yabba dabba doo") || (groups[2] != "") ||
			(groups[3] != "yabba") || (groups[4] != "dabba") ||
			(groups[5] != "doo")) {
		FAIL;
	}
	PASS;
}
