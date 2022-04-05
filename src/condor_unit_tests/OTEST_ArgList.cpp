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

/* Test the ArgList implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "condor_arglist.h"

static bool test_append_args_v1_wacked_or_v2_quoted_return(void);
static bool test_append_args_v1_wacked_or_v2_quoted_count(void);
static bool test_append_args_v1_wacked_or_v2_quoted_args(void);
static bool test_append_args_v2_quoted_return(void);
static bool test_append_args_v2_quoted_count(void);
static bool test_append_args_v2_quoted_args(void);
static bool test_append_args_v2_raw_return(void);
static bool test_append_args_v2_raw_count(void);
static bool test_append_args_v2_raw_args(void);
static bool test_append_args_v1_raw_win32_return(void);
static bool test_append_args_v1_raw_win32_count(void);
static bool test_append_args_v1_raw_win32_args(void);
static bool test_append_args_v1_raw_win32_was(void);
static bool test_append_args_v1_raw_unix_return(void);
static bool test_append_args_v1_raw_unix_count(void);
static bool test_append_args_v1_raw_unix_was(void);
static bool test_append_args_v1_raw_unknown_return(void);
static bool test_append_args_v1_raw_unknown_count(void);
static bool test_append_args_v1_raw_unknown_was(void);
static bool test_get_args_string_v2_quoted_return(void);
static bool test_get_args_string_v2_quoted_append_return(void);
static bool test_get_args_string_v2_quoted_append_count(void);
static bool test_get_args_string_v2_quoted_append_args(void);
static bool test_get_args_string_v2_raw_return(void);
static bool test_get_args_string_v1or2_raw_return(void);
static bool test_get_args_string_v1or2_raw_append_return(void);
static bool test_get_args_string_v1or2_raw_append_count(void);
static bool test_get_args_string_v1or2_raw_append_args(void);
static bool test_get_args_string_win32_return(void);
static bool test_get_args_string_win32_append_return(void);
static bool test_get_args_string_win32_append_count(void);
static bool test_get_args_string_win32_append_args(void);
static bool test_get_args_string_win32_was(void);
static bool test_insert_count(void);
static bool test_insert_args(void);
static bool test_append_count(void);
static bool test_append_args(void);
static bool test_remove_mid_count(void);
static bool test_remove_mid_args(void);
static bool test_remove_beg_count(void);
static bool test_remove_beg_args(void);
static bool test_split_args_ret_true(void);
static bool test_split_args_ret_true_array(void);
static bool test_split_args_ret_false(void);
static bool test_split_args_ret_false_null(void);
static bool test_split_args_num(void);
static bool test_split_args_args(void);
static bool test_split_args_args_array(void);
static bool test_join_args_ret(void);
static bool test_join_args_num(void);
static bool test_join_args_args(void);
static bool test_join_args_equivalent(void);

//global variables
static char const *test_string = "This '''quoted''' arg' 'string 'contains "
	"''many'' \"\"surprises\\'";
static char const *test_cooked_string = "\"This '''quoted''' arg' 'string "
	"'contains ''many'' \"\"\"\"surprises\\'\"";
static char const *test_v1_wacked = "one \\\"two\\\" 'three\\ four'";
static char const *test_win32_v1 = "one \"two three\" four\\ \"five "
	"\\\"six\\\\\\\"\" \"seven\\\\\\\"\"";
static char const *test_unix_v1 = "one 'two three' four";

static ArgList arglist;

bool OTEST_ArgList(void) {
	emit_object("ArgList");
	emit_comment("This module contains functions for manipulating argument "
		"strings, such as the argument string specified for a job in a submit "
		"file.");
	emit_comment("Note that these tests are not exhaustive, but only test "
		"what was tested in condor_c++_util/test_condor_arglist.cpp.");

	FunctionDriver driver;
	driver.register_function(test_append_args_v1_wacked_or_v2_quoted_return);
	driver.register_function(test_append_args_v1_wacked_or_v2_quoted_count);
	driver.register_function(test_append_args_v1_wacked_or_v2_quoted_args);
	driver.register_function(test_append_args_v2_quoted_return);
	driver.register_function(test_append_args_v2_quoted_count);
	driver.register_function(test_append_args_v2_quoted_args);
	driver.register_function(test_append_args_v2_raw_return);
	driver.register_function(test_append_args_v2_raw_count);
	driver.register_function(test_append_args_v2_raw_args);
	driver.register_function(test_append_args_v1_raw_win32_return);
	driver.register_function(test_append_args_v1_raw_win32_count);
	driver.register_function(test_append_args_v1_raw_win32_args);
	driver.register_function(test_append_args_v1_raw_win32_was);
	driver.register_function(test_append_args_v1_raw_unix_return);
	driver.register_function(test_append_args_v1_raw_unix_count);
	driver.register_function(test_append_args_v1_raw_unix_was);
	driver.register_function(test_append_args_v1_raw_unknown_return);
	driver.register_function(test_append_args_v1_raw_unknown_count);
	driver.register_function(test_append_args_v1_raw_unknown_was);
	driver.register_function(test_get_args_string_v2_quoted_return);
	driver.register_function(test_get_args_string_v2_quoted_append_return);
	driver.register_function(test_get_args_string_v2_quoted_append_count);
	driver.register_function(test_get_args_string_v2_quoted_append_args);
	driver.register_function(test_get_args_string_v2_raw_return);
	driver.register_function(test_get_args_string_v1or2_raw_return);
	driver.register_function(test_get_args_string_v1or2_raw_append_return);
	driver.register_function(test_get_args_string_v1or2_raw_append_count);
	driver.register_function(test_get_args_string_v1or2_raw_append_args);
	driver.register_function(test_get_args_string_win32_return);
	driver.register_function(test_get_args_string_win32_append_return);
	driver.register_function(test_get_args_string_win32_append_count);
	driver.register_function(test_get_args_string_win32_append_args);
	driver.register_function(test_get_args_string_win32_was);
	driver.register_function(test_insert_count);
	driver.register_function(test_insert_args);
	driver.register_function(test_append_count);
	driver.register_function(test_append_args);
	driver.register_function(test_remove_mid_count);
	driver.register_function(test_remove_mid_args);
	driver.register_function(test_remove_beg_count);
	driver.register_function(test_remove_beg_args);
	driver.register_function(test_split_args_ret_true);
	driver.register_function(test_split_args_ret_true_array);
	driver.register_function(test_split_args_ret_false);
	driver.register_function(test_split_args_ret_false_null);
	driver.register_function(test_split_args_num);
	driver.register_function(test_split_args_args);
	driver.register_function(test_split_args_args_array);
	driver.register_function(test_join_args_ret);
	driver.register_function(test_join_args_num);
	driver.register_function(test_join_args_args);
	driver.register_function(test_join_args_equivalent);
	
	return driver.do_all_functions();
}

static bool test_append_args_v1_wacked_or_v2_quoted_return() {
	emit_test("Test that AppendArgsV1WackedOrV2Quoted() returns true for a "
		"valid V1Wacked string.");
	bool ret_val = arglist.AppendArgsV1WackedOrV2Quoted(test_v1_wacked, NULL);
	emit_input_header();
	emit_param("Args", test_v1_wacked);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_wacked_or_v2_quoted_count() {
	emit_test("Test that AppendArgsV1WackedOrV2Quoted() adds the correct number"
		"of args for a valid V1Wacked string.");
	arglist.AppendArgsV1WackedOrV2Quoted(test_v1_wacked, NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", test_v1_wacked);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_wacked_or_v2_quoted_args() {
	emit_test("Test that AppendArgsV1WackedOrV2Quoted() adds the correct args "
		"for a valid V1Wacked string.");
	arglist.AppendArgsV1WackedOrV2Quoted(test_v1_wacked, NULL);
	MyString expect("one,\"two\",'three\\,four'");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("Args", test_v1_wacked);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("ArgList", "%s", actual.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", expect.Value());
	arglist.Clear();
	if(expect != actual) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v2_quoted_return() {
	emit_test("Test that AppendArgsV2Quoted returns true for a valid V2Quoted "
		"string.");
	bool ret_val = arglist.AppendArgsV2Quoted(test_cooked_string, NULL);
	emit_input_header();
	emit_param("Args", test_cooked_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v2_quoted_count() {
	emit_test("Test that AppendArgsV2Quoted() adds the correct number of args "
		"for a valid V2Quoted string.");
	arglist.AppendArgsV2Quoted(test_cooked_string, NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", test_cooked_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v2_quoted_args() {
	emit_test("Test that AppendArgsV2Quoted() adds the correct args for a valid"
		" V2Quoted string.");
	arglist.AppendArgsV2Quoted(test_cooked_string, NULL);
	MyString expect("This,'quoted',arg string,contains 'many' \"\"surprises\\");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("Args", test_cooked_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("ArgList", "%s", actual.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", expect.Value());
	arglist.Clear();
	if(expect != actual) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v2_raw_return() {
	emit_test("Test that AppendArgsV2Raw returns true for a valid V2Raw "
		"string.");
	bool ret_val = arglist.AppendArgsV2Raw(test_string, NULL);
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v2_raw_count() {
	emit_test("Test that AppendArgsV2Raw() adds the correct number of args "
		"for a valid V2Raw string.");
	arglist.AppendArgsV2Raw(test_string, NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v2_raw_args() {
	emit_test("Test that AppendArgsV2Raw() adds the correct args for a valid"
		" V2Raw string.");
	arglist.AppendArgsV2Raw(test_string, NULL);
	MyString expect("This,'quoted',arg string,contains 'many' \"\"surprises\\");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("ArgList", "%s", actual.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", expect.Value());
	arglist.Clear();
	if(expect != actual) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_win32_return() {
	emit_test("Test that AppendArgsV1Raw() returns true for a valid V1Raw "
		"string after setting V1 syntax to WIN32.");
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	bool ret_val = arglist.AppendArgsV1Raw(test_win32_v1,NULL);
	emit_input_header();
	emit_param("Args", test_win32_v1);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_win32_count() {
	emit_test("Test that AppendArgsV1Raw() adds the correct number of args "
		"for a valid V1Raw string after setting V1 syntax to WIN32.");
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(test_win32_v1, NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", test_win32_v1);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "5");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 5) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_win32_args() {
	emit_test("Test that AppendArgsV2Raw() adds the correct args for a valid"
		" V2Raw string after setting V1 syntax to WIN32.");
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(test_win32_v1, NULL);
	MyString expect("one,two three,four\\,five \"six\\\",seven\\\"");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("Args", test_win32_v1);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("ArgList", "%s", actual.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", expect.Value());
	arglist.Clear();
	if(expect != actual) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_win32_was() {
	emit_test("Test that InputWasV1() returns false after calling "
		"AppendArgsV1Raw() for a valid V1Raw string after setting V1 syntax to "
		"WIN32.");
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(test_win32_v1,NULL);
	bool ret_val = arglist.InputWasV1();
	emit_input_header();
	emit_param("Args", test_win32_v1);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_unix_return() {
	emit_test("Test that AppendArgsV1Raw returns true for a valid V1Raw "
		"string after setting V1 syntax to UNIX.");
	arglist.SetArgV1Syntax(ArgList::UNIX_ARGV1_SYNTAX);
	bool ret_val = arglist.AppendArgsV1Raw(test_unix_v1, NULL);
	emit_input_header();
	emit_param("Args", test_unix_v1);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_unix_count() {
	emit_test("Test that AppendArgsV1Raw() adds the correct number of args "
		"for a valid V1Raw string after setting V1 syntax to UNIX.");
	arglist.SetArgV1Syntax(ArgList::UNIX_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(test_unix_v1, NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", test_unix_v1);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_unix_was() {
	emit_test("Test that InputWasV1() returns false after calling "
		"AppendArgsV1Raw() for a valid V1Raw string after setting V1 syntax to "
		"UNIX.");
	arglist.SetArgV1Syntax(ArgList::UNIX_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(test_unix_v1,NULL);
	bool ret_val = arglist.InputWasV1();
	emit_input_header();
	emit_param("Args", test_unix_v1);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_unknown_return() {
	emit_test("Test that AppendArgsV1Raw returns true for a valid V1Raw "
		"string after setting V1 syntax to UNKNOWN.");
	arglist.SetArgV1Syntax(ArgList::UNKNOWN_ARGV1_SYNTAX);
	bool ret_val = arglist.AppendArgsV1Raw(test_unix_v1,NULL);
	emit_input_header();
	emit_param("Args", test_unix_v1);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_unknown_count() {
	emit_test("Test that AppendArgsV1Raw() adds the correct number of args "
		"for a valid V1Raw string after setting V1 syntax to UNKNOWN.");
	arglist.SetArgV1Syntax(ArgList::UNIX_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(test_unix_v1, NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", test_unix_v1);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_append_args_v1_raw_unknown_was() {
	emit_test("Test that InputWasV1() returns true after calling "
		"AppendArgsV1Raw() for a valid V1Raw string after setting V1 syntax to "
		"UNKNOWN.");
	arglist.SetArgV1Syntax(ArgList::UNKNOWN_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(test_unix_v1,NULL);
	bool ret_val = arglist.InputWasV1();
	emit_input_header();
	emit_param("Args", test_unix_v1);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v2_quoted_return() {
	emit_test("Test that GetArgsStringV2Quoted() returns true on an ArgList "
		"initialized with AppendArgsV2Quoted().");
	MyString v2_cooked_args;
	arglist.AppendArgsV2Quoted(test_cooked_string, NULL);
	bool ret_val = arglist.GetArgsStringV2Quoted(&v2_cooked_args, NULL);
	emit_input_header();
	emit_param("Args", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v2_quoted_append_return() {
	emit_test("Test that AppendArgsV1WackedOrV2Quoted() returns true after "
		"getting the args with GetArgsStringV2Quoted() on an ArgList "
		"initialized with AppendArgsV2Quoted().");
	MyString v2_cooked_args;
	arglist.AppendArgsV2Quoted(test_cooked_string, NULL);
	arglist.GetArgsStringV2Quoted(&v2_cooked_args, NULL);
	arglist.Clear();
	bool ret_val = arglist.AppendArgsV1WackedOrV2Quoted(v2_cooked_args.Value(), 
		NULL);
	emit_input_header();
	emit_param("Args", v2_cooked_args.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v2_quoted_append_count() {
	emit_test("Test that AppendArgsV1WackedOrV2Quoted() adds the correct number"
		" of args after getting the args with GetArgsStringV2Quoted() on an "
		"ArgList initialized with AppendArgsV2Quoted().");
	MyString v2_cooked_args;
	arglist.AppendArgsV2Quoted(test_cooked_string, NULL);
	arglist.GetArgsStringV2Quoted(&v2_cooked_args, NULL);
	arglist.Clear();
	arglist.AppendArgsV1WackedOrV2Quoted(v2_cooked_args.Value(), NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", v2_cooked_args.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v2_quoted_append_args() {
	emit_test("Test that AppendArgsV1WackedOrV2Quoted() adds the correct args "
		"after getting the args with GetArgsStringV2Quoted() on an ArgList "
		"initialized with AppendArgsV2Quoted().");
	MyString v2_cooked_args;
	arglist.AppendArgsV2Quoted(test_cooked_string, NULL);
	arglist.GetArgsStringV2Quoted(&v2_cooked_args, NULL);
	arglist.Clear();
	arglist.AppendArgsV1WackedOrV2Quoted(v2_cooked_args.Value(), NULL);
	MyString expect("This,'quoted',arg string,contains 'many' \"\"surprises\\");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("Args", v2_cooked_args.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("ArgList", "%s", expect.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", actual.Value());
	arglist.Clear();
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v2_raw_return() {
	emit_test("Test that GetArgsStringV2Raw() returns true on an ArgList "
		"initialized with AppendArgsV2Raw().");
	MyString v2_args;
	arglist.AppendArgsV2Raw(test_string, NULL);
	bool ret_val = arglist.GetArgsStringV2Raw(&v2_args, NULL);
	emit_input_header();
	emit_param("Result", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v1or2_raw_return() {
	emit_test("Test that GetArgsStringV1or2Raw() returns true on an ArgList "
		"initialized with AppendArgsV2Raw().");
	MyString v1or2_args;
	arglist.AppendArgsV2Raw(test_string, NULL);
	bool ret_val = arglist.GetArgsStringV1or2Raw(&v1or2_args, NULL);
	emit_input_header();
	emit_param("Result", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v1or2_raw_append_return() {
	emit_test("Test that AppendArgsV1or2Raw() returns true after getting the "
		"args with GetArgsStringV1or2Raw() on an ArgList initialized with "
		"AppendArgsV2Raw().");
	MyString v1or2_args;
	arglist.AppendArgsV2Raw(test_string, NULL);
	arglist.GetArgsStringV1or2Raw(&v1or2_args, NULL);
	arglist.Clear();
	bool ret_val = arglist.AppendArgsV1or2Raw(v1or2_args.Value(), NULL);
	emit_input_header();
	emit_param("Args", v1or2_args.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v1or2_raw_append_count() {
	emit_test("Test that AppendArgsV1or2Raw() adds the correct number of args "
		"after getting the args with GetArgsStringV1or2Raw() on an ArgList "
		"initialized with AppendArgsV2Raw().");
	MyString v1or2_args;
	arglist.AppendArgsV2Raw(test_string, NULL);
	arglist.GetArgsStringV1or2Raw(&v1or2_args, NULL);
	arglist.Clear();
	arglist.AppendArgsV1or2Raw(v1or2_args.Value(), NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", v1or2_args.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_v1or2_raw_append_args() {
	emit_test("Test that AppendArgsV1or2Raw() adds the correct args after "
		"getting the args with GetArgsStringV1or2Raw() on an ArgList "
		"initialized with AppendArgsV2Raw().");
	MyString v1or2_args;
	arglist.AppendArgsV2Raw(test_string, NULL);
	arglist.GetArgsStringV1or2Raw(&v1or2_args, NULL);
	arglist.Clear();
	arglist.AppendArgsV1or2Raw(v1or2_args.Value(), NULL);
	MyString expect("This,'quoted',arg string,contains 'many' \"\""
		"surprises\\");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("Args", v1or2_args.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("ArgList", "%s", expect.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", actual.Value());
	arglist.Clear();
	if(expect != actual) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_win32_return() {
	emit_test("Test that GetArgsStringWin32() returns true on an ArgList "
		"initialized with AppendArgsV1Raw().");
	MyString win32_result;
	arglist.AppendArgsV1Raw(test_win32_v1,NULL);
	bool ret_val = arglist.GetArgsStringWin32(&win32_result, 1);
	emit_input_header();
	emit_param("Result", "");
	emit_param("Skip Arg", "1");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_win32_append_return() {
	emit_test("Test that AppendArgsV1Raw() returns true after getting the args "
		"with GetArgsStringWin32() on an ArgList initialized with "
		"AppendArgsV1Raw().");
	MyString win32_result;
	arglist.AppendArgsV1Raw(test_win32_v1,NULL);
	arglist.GetArgsStringWin32(&win32_result, 1);
	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	bool ret_val = arglist.AppendArgsV1Raw(win32_result.Value(),NULL);
	emit_input_header();
	emit_param("Args", win32_result.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_win32_append_count() {
	emit_test("Test that AppendArgsV1Raw() adds the correct number of args "
		"after getting the args with GetArgsStringWin32() on an ArgList "
		"initialized with AppendArgsV1Raw().");
	MyString win32_result;
	arglist.AppendArgsV1Raw(test_win32_v1,NULL);
	arglist.GetArgsStringWin32(&win32_result, 1);
	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(win32_result.Value(),NULL);
	int count = arglist.Count();
	emit_input_header();
	emit_param("Args", win32_result.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Count", "4");
	emit_output_actual_header();
	emit_param("Count", "%d", count);
	arglist.Clear();
	if(count != 4) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_win32_append_args() {
	emit_test("Test that AppendArgsV1Raw() adds the args after getting the "
		"args with GetArgsStringWin32() on an ArgList initialized with "
		"AppendArgsV1Raw().");
	MyString win32_result;
	arglist.AppendArgsV1Raw(test_win32_v1,NULL);
	arglist.GetArgsStringWin32(&win32_result, 1);
	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(win32_result.Value(),NULL);
	MyString expect("two three,four\\,five \"six\\\",seven\\\"");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("Args", win32_result.Value());
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("ArgList", "%s", expect.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", actual.Value());
	arglist.Clear();
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_args_string_win32_was() {
	emit_test("Test that InputWasV1() returns false after calling "
		"AppendArgsV1Raw() for a valid V1Raw string obtained with "
		"GetArgsStringWin32().");
	MyString win32_result;
	arglist.AppendArgsV1Raw(test_win32_v1,NULL);
	arglist.GetArgsStringWin32(&win32_result, 1);
	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	arglist.AppendArgsV1Raw(win32_result.Value(),NULL);
	bool ret_val = arglist.InputWasV1();
	emit_input_header();
	emit_param("ArgList", win32_result.Value());
	emit_output_expected_header();
	emit_retval("%s", "FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	arglist.Clear();
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_insert_count() {
	emit_test("Test that InsertArg() changes the number of args in an existing "
		"ArgList.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.InsertArg("inserted",0);
	int count = arglist.Count();
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Arg", "inserted");
	emit_param("Pos", "0");
	emit_output_expected_header();
	emit_retval("%d", 5);
	emit_output_actual_header();
	emit_retval("%d", count);
	arglist.Clear();
	if(count != 5) {
		FAIL;
	}
	PASS;
}

static bool test_insert_args() {
	emit_test("Test that InsertArg() inserts the arg into the ArgList and does"
		" not modify the other args in the ArgList.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.InsertArg("inserted",0);
	MyString expect("inserted,This,'quoted',arg string,contains 'many' "
		"\"\"surprises\\");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Arg", "inserted");
	emit_param("Pos", "0");
	emit_output_expected_header();
	emit_param("ArgList", "%s", expect.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", actual.Value());
	arglist.Clear();
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_append_count() {
	emit_test("Test that AppendArg() changes the number of args in an existing "
		"ArgList.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.AppendArg("appended");
	int count = arglist.Count();
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Arg", "appended");
	emit_output_expected_header();
	emit_retval("%d", 5);
	emit_output_actual_header();
	emit_retval("%d", count);
	arglist.Clear();
	if(count != 5) {
		FAIL;
	}
	PASS;
}

static bool test_append_args() {
	emit_test("Test that AppendArg() inserts the arg into the ArgList and does"
		" not modify the other args.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.AppendArg("appended");
	MyString expect("This,'quoted',arg string,contains 'many' \"\"surprises\\,"
		"appended");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Arg", "appended");
	emit_output_expected_header();
	emit_param("ArgList", "%s", expect.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", actual.Value());
	arglist.Clear();
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_remove_mid_count() {
	emit_test("Test that RemoveArg() changes the number of args after removing "
		"an arg in the middle of the ArgList.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.RemoveArg(2);
	int count = arglist.Count();
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Pos", "2");
	emit_output_expected_header();
	emit_param("Count", "3");
	emit_output_actual_header();
	emit_param("Count", "%d", count); 
	arglist.Clear();
	if(count != 3) {
		FAIL;
	}
	PASS;
}

static bool test_remove_mid_args() {
	emit_test("Test that RemoveArg() removes an arg in the middle of the "
		"ArgList and does not modify the other args.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.RemoveArg(2);
	MyString expect("This,'quoted',contains 'many' \"\"surprises\\");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Pos", "2");
	emit_output_expected_header();
	emit_param("ArgList", "%s", expect.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", actual.Value()); 
	arglist.Clear();
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_remove_beg_count() {
	emit_test("Test that RemoveArg() changes the number of args after removing "
		"an arg at the beginning of the ArgList.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.RemoveArg(0);
	int count = arglist.Count();
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Pos", "0");
	emit_output_expected_header();
	emit_param("Count", "3");
	emit_output_actual_header();
	emit_param("Count", "%d", count); 
	arglist.Clear();
	if(count != 3) {
		FAIL;
	}
	PASS;
}

static bool test_remove_beg_args() {
	emit_test("Test that RemoveArg() removes an arg at the beginning of the "
		"ArgList and does not modify the other args.");
	arglist.AppendArgsV2Raw(test_string,NULL);
	arglist.RemoveArg(0);
	MyString expect("'quoted',arg string,contains 'many' \"\"surprises\\");
	MyString actual;
	for(int i = 0; i < arglist.Count(); i++) {
		actual.append_to_list(arglist.GetArg(i), ",");
	}
	emit_input_header();
	emit_param("ArgList", test_string);
	emit_param("Pos", "0");
	emit_output_expected_header();
	emit_param("ArgList", "%s", expect.Value());
	emit_output_actual_header();
	emit_param("ArgList", "%s", actual.Value()); 
	arglist.Clear();
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_split_args_ret_true() {
	emit_test("Test that split_args() returns true for a valid arg string.");
	SimpleList<MyString> args;
	bool ret_val = split_args(test_string, &args, NULL);
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val)); 
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_split_args_ret_true_array() {
	emit_test("Test that split_args() returns true for a valid arg string when "
		"used with a string array.");
	char** string_array;
	bool ret_val = split_args(test_string, &string_array, NULL);
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Args Array", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val)); 
	arglist.Clear();
	delete_helper(string_array, 4);
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_split_args_ret_false() {
	emit_test("Test that split_args() returns false when passed a non-NULL "
		"error message MyString for an invalid arg string due to an unterminated "
		"quote.");
	SimpleList<MyString> args;
	MyString error_msg;
	bool ret_val = split_args("Unterminated 'quote", &args, &error_msg);
	emit_input_header();
	emit_param("Args", "Unterminated 'quote");
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val)); 
	arglist.Clear();
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_split_args_ret_false_null() {
	emit_test("Test that split_args() returns false when passed a NULL error "
		"message MyString for an invalid arg string due to an unterminated "
		"quote.");
	SimpleList<MyString> args;
	bool ret_val = split_args("Unterminated 'quote", &args, NULL);
	emit_input_header();
	emit_param("Args", "Unterminated 'quote");
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val)); 
	arglist.Clear();
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_split_args_num() {
	emit_test("Test that split_args() adds the correct number of args to the "
		"SimpleList for a valid arg string.");
	SimpleList<MyString> args;
	split_args(test_string, &args, NULL);
	int number = args.Number();
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Number", "4");
	emit_output_actual_header();
	emit_param("Number", "%d", number); 
	arglist.Clear();
	if(number != 4) {
		FAIL;
	}
	PASS;
}

static bool test_split_args_args() {
	emit_test("Test that split_args() adds the correct args to the SimpleList "
		"for a valid arg string.");
	SimpleList<MyString> args;
	SimpleListIterator<MyString> it(args);
	MyString arg1, arg2, arg3, arg4;
	MyString expect1("This"), expect2("'quoted'"), expect3("arg string"), 
		expect4("contains 'many' \"\"surprises\\");
	split_args(test_string, &args, NULL);
	it.ToBeforeFirst();
	it.Next(arg1);
	it.Next(arg2);
	it.Next(arg3);
	it.Next(arg4);
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Arg 1", "%s", expect1.Value());
	emit_param("Arg 2", "%s", expect2.Value());
	emit_param("Arg 3", "%s", expect3.Value());
	emit_param("Arg 4", "%s", expect4.Value());
	emit_output_actual_header();
	emit_param("Arg 1", "%s", arg1.Value()); 
	emit_param("Arg 2", "%s", arg2.Value()); 
	emit_param("Arg 3", "%s", arg3.Value()); 
	emit_param("Arg 4", "%s", arg4.Value()); 
	arglist.Clear();
	if(arg1 != expect1 || arg2 != expect2 || arg3 != expect3 || 
		arg4 != expect4) 
	{
		FAIL;
	}
	PASS;
}

static bool test_split_args_args_array() {
	emit_test("Test that split_args() adds the correct args to the string array"
		" for a valid arg string.");
	char **string_array;
	const char *expect1 = "This", *expect2 = "'quoted'", *expect3 = "arg string", 
		*expect4 = "contains 'many' \"\"surprises\\";
	split_args(test_string, &string_array, NULL);
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Args Array", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Arg 1", "%s", expect1);
	emit_param("Arg 2", "%s", expect2);
	emit_param("Arg 3", "%s", expect3);
	emit_param("Arg 4", "%s", expect4);
	emit_param("Arg 5 is NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Arg 1", "%s", string_array[0]); 
	emit_param("Arg 2", "%s", string_array[1]); 
	emit_param("Arg 3", "%s", string_array[2]); 
	emit_param("Arg 4", "%s", string_array[3]); 
	emit_param("Arg 5 is NULL", "%s", tfstr(string_array[4] == NULL)); 
	arglist.Clear();
	if(strcmp(string_array[0], expect1) != MATCH || 
		strcmp(string_array[1], expect2) != MATCH || 
		strcmp(string_array[2], expect3) != MATCH || 
		strcmp(string_array[3], expect4) != MATCH ||
		string_array[4] != NULL) 
	{
		delete_helper(string_array, 4);
		FAIL;
	}
	delete_helper(string_array, 4);
	PASS;
}

static bool test_join_args_ret() {
	emit_test("Test that split_args() returns true after obtaining the args "
		"with join_args() for a valid arg string.");
	MyString joined_args;
	SimpleList<MyString> args;
	split_args(test_string, &args, NULL);
	join_args(args, &joined_args);
	args.Clear();
	bool ret_val = split_args(joined_args.Value(), &args, NULL);
	emit_input_header();
	emit_param("Args", joined_args.Value());
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val)); 
	arglist.Clear();
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_join_args_num() {
	emit_test("Test that split_args() adds the correct number of args to the "
		"SimpleList after obtaining the args with join_args() for a valid arg "
		"string.");
	MyString joined_args;
	SimpleList<MyString> args;
	split_args(test_string, &args, NULL);
	join_args(args, &joined_args);
	args.Clear();
	split_args(joined_args.Value(), &args, NULL);
	int number = args.Number();
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Number", "4");
	emit_output_actual_header();
	emit_param("Number", "%d", number); 
	arglist.Clear();
	if(number != 4) {
		FAIL;
	}
	PASS;
}

static bool test_join_args_args() {
	emit_test("Test that split_args() adds the correct args to the SimpleList "
		"after obtaining the args with join_args() for a valid arg string.");
	SimpleList<MyString> args;
	SimpleListIterator<MyString> it(args);
	MyString arg1, arg2, arg3, arg4, joined_args;
	MyString expect1("This"), expect2("'quoted'"), expect3("arg string"), 
		expect4("contains 'many' \"\"surprises\\");
	split_args(test_string, &args, NULL);
	join_args(args, &joined_args);
	args.Clear();
	split_args(joined_args.Value(), &args, NULL);
	it.ToBeforeFirst();
	it.Next(arg1);
	it.Next(arg2);
	it.Next(arg3);
	it.Next(arg4);
	emit_input_header();
	emit_param("Args", test_string);
	emit_param("Args List", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Arg 1", "%s", expect1.Value());
	emit_param("Arg 2", "%s", expect2.Value());
	emit_param("Arg 3", "%s", expect3.Value());
	emit_param("Arg 4", "%s", expect4.Value());
	emit_output_actual_header();
	emit_param("Arg 1", "%s", arg1.Value()); 
	emit_param("Arg 2", "%s", arg2.Value()); 
	emit_param("Arg 3", "%s", arg3.Value()); 
	emit_param("Arg 4", "%s", arg4.Value()); 
	arglist.Clear();
	if(arg1 != expect1 || arg2 != expect2 || arg3 != expect3 || 
		arg4 != expect4) 
	{
		FAIL;
	}
	PASS;
}

static bool test_join_args_equivalent() {
	emit_test("Test that each join_args() method returns an equivalent MyString"
		" if called on a SimpleList or a string array.");
	MyString joined_args, joined_args2;
	SimpleList<MyString> args;
	char** string_array;
	split_args(test_string, &args, NULL);
	join_args(args, &joined_args);
	split_args(test_string, &string_array, NULL);
	join_args(string_array, &joined_args2);
	emit_input_header();
	emit_param("Args", test_string);
	emit_output_expected_header();
	emit_output_actual_header();
	emit_param("SimpleList Joined Args  ", "%s", joined_args.Value());
	emit_param("String Array Joined Args", "%s", joined_args2.Value());
	arglist.Clear();
	delete_helper(string_array, 4);
	if(joined_args != joined_args2) {
		FAIL;
	}
	PASS;
}

