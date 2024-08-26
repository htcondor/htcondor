/***************************************************************
 *
 * Copyright (C) 1990-2023, Condor Team, Computer Sciences Department,
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

/*
	This code tests the vector<string> functions
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "stl_string_utils.h"

static bool test_split_valid(void);
static bool test_split_valid_trim_whitespace(void);
static bool test_split_valid_notrim_whitespace(void);
static bool test_split_empty(void);
static bool test_contains_return_false(void);
static bool test_contains_return_false_substring(void);
static bool test_contains_return_false_case(void);
static bool test_contains_return_true_one(void);
static bool test_contains_return_true_many(void);
static bool test_contains_anycase_return_false(void);
static bool test_contains_anycase_return_false_substring(void);
static bool test_contains_anycase_return_true_one(void);
static bool test_contains_anycase_return_true_many(void);
static bool test_prefix_return_false_invalid(void);
static bool test_prefix_return_false_almost(void);
static bool test_prefix_return_false_reverse(void);
static bool test_prefix_return_false_case(void);
static bool test_prefix_return_true_identical(void);
static bool test_prefix_withwildcard_return_true(void);
static bool test_prefix_withwildcard_return_false(void);
static bool test_prefix_return_true_many(void);
static bool test_contains_withwildcard_return_false(void);
static bool test_contains_withwildcard_return_false_substring(void);
static bool test_contains_withwildcard_return_false_case(void);
static bool test_contains_withwildcard_return_false_wild(void);
static bool test_contains_withwildcard_return_true_no_wild(void);
static bool test_contains_withwildcard_return_true_only_wild(void);
static bool test_contains_withwildcard_return_true_start(void);
static bool test_contains_withwildcard_return_true_mid(void);
static bool test_contains_withwildcard_return_true_mid_and_end(void);
static bool test_contains_withwildcard_return_true_start_and_end(void);
static bool test_contains_withwildcard_return_false_mid_and_end(void);
static bool test_contains_withwildcard_return_false_start_and_end(void);
static bool test_contains_withwildcard_return_true_end(void);
static bool test_contains_withwildcard_return_true_same_wild(void);
static bool test_contains_withwildcard_return_true_multiple(void);
static bool test_contains_withwildcard_return_true_many(void);
static bool test_contains_anycase_wwc_return_false(void);
static bool test_contains_anycase_wwc_return_false_substring(void);
static bool test_contains_anycase_wwc_return_false_wild(void);
static bool test_contains_anycase_wwc_return_true_no_wild(void);
static bool test_contains_anycase_wwc_return_true_only_wild(void);
static bool test_contains_anycase_wwc_return_true_start(void);
static bool test_contains_anycase_wwc_return_true_mid(void);
static bool test_contains_anycase_wwc_return_true_end(void);
static bool test_contains_anycase_wwc_return_true_same_wild(void);
static bool test_contains_anycase_wwc_return_true_multiple(void);
static bool test_contains_anycase_wwc_return_true_many(void);
static bool test_join_empty(void);
static bool test_join_one(void);
static bool test_join_many_short(void);
static bool test_join_many_long(void);

bool FTEST_string_vector(void) {
	emit_object("string list");
	emit_comment("These functions operate on a vector of strings: splitting, "
		"joining, testing membership.");

		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_split_valid);
	driver.register_function(test_split_valid_trim_whitespace);
	driver.register_function(test_split_valid_notrim_whitespace);
	driver.register_function(test_split_empty);
	driver.register_function(test_contains_return_false);
	driver.register_function(test_contains_return_false_substring);
	driver.register_function(test_contains_return_false_case);
	driver.register_function(test_contains_return_true_one);
	driver.register_function(test_contains_return_true_many);
	driver.register_function(test_contains_anycase_return_false);
	driver.register_function(test_contains_anycase_return_false_substring);
	driver.register_function(test_contains_anycase_return_true_one);
	driver.register_function(test_contains_anycase_return_true_many);
	driver.register_function(test_prefix_return_false_invalid);
	driver.register_function(test_prefix_return_false_almost);
	driver.register_function(test_prefix_return_false_reverse);
	driver.register_function(test_prefix_return_false_case);
	driver.register_function(test_prefix_return_true_identical);
	driver.register_function(test_prefix_withwildcard_return_true);
	driver.register_function(test_prefix_withwildcard_return_false);
	driver.register_function(test_prefix_return_true_many);
	driver.register_function(test_contains_withwildcard_return_false);
	driver.register_function(test_contains_withwildcard_return_false_substring);
	driver.register_function(test_contains_withwildcard_return_false_case);
	driver.register_function(test_contains_withwildcard_return_false_wild);
	driver.register_function(test_contains_withwildcard_return_true_no_wild);
	driver.register_function(test_contains_withwildcard_return_true_only_wild);
	driver.register_function(test_contains_withwildcard_return_true_start);
	driver.register_function(test_contains_withwildcard_return_true_mid);
	driver.register_function(test_contains_withwildcard_return_true_mid_and_end);
	driver.register_function(test_contains_withwildcard_return_true_start_and_end);
	driver.register_function(test_contains_withwildcard_return_false_mid_and_end);
	driver.register_function(test_contains_withwildcard_return_false_start_and_end);
	driver.register_function(test_contains_withwildcard_return_true_end);
	driver.register_function(test_contains_withwildcard_return_true_same_wild);
	driver.register_function(test_contains_withwildcard_return_true_multiple);
	driver.register_function(test_contains_withwildcard_return_true_many);
	driver.register_function(test_contains_anycase_wwc_return_false);
	driver.register_function(test_contains_anycase_wwc_return_false_substring);
	driver.register_function(test_contains_anycase_wwc_return_false_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_no_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_only_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_start);
	driver.register_function(test_contains_anycase_wwc_return_true_mid);
	driver.register_function(test_contains_anycase_wwc_return_true_end);
	driver.register_function(test_contains_anycase_wwc_return_true_same_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_multiple);
	driver.register_function(test_contains_anycase_wwc_return_true_many);
	driver.register_function(test_join_empty);
	driver.register_function(test_join_one);
	driver.register_function(test_join_many_short);
	driver.register_function(test_join_many_long);

		// run the tests
	return driver.do_all_functions();
}

static bool test_split_valid() {
	emit_test("Test split when passed a valid string.");
	std::vector<std::string> sl = split("a;b;c", ";");
	const char* expect = "a,b,c";
	std::string retVal = join(sl, ",");
	emit_input_header();
	emit_param("STRING", "a;b;c");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", retVal.c_str());
	if(strcmp(expect, retVal.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_split_valid_trim_whitespace() {
	emit_test("Test split when passed "
		" a valid string with whitespace to be trimmed.");
	std::vector<std::string> sl = split(" a ; b b ; c ", ";");
	const char* expect = "a,b b,c";
	std::string retVal = join(sl, ",");
	emit_input_header();
	emit_param("STRING", " a ; b b ; c ");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", retVal.c_str());
	if(strcmp(expect, retVal.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_split_valid_notrim_whitespace() {
	emit_test("Test split when passed "
		" a valid string with whitespace that shouldn't be trimmed.");
	std::vector<std::string> sl = split(" a ; b b ; c ", ";", STI_NO_TRIM);
	const char* expect = " a , b b , c ";
	std::string retVal = join(sl, ",");
	emit_input_header();
	emit_param("STRING", " a ; b b ; c ");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", retVal.c_str());
	if(strcmp(expect, retVal.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_split_empty() {
	emit_test("Test split when passed an empty string.");
	std::vector<std::string> sl = split("", ";");
	const char* expect = "";
	std::string retVal = join(sl, ",");
	emit_input_header();
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_param("list", "");
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_param("list", retVal.c_str());
	emit_retval("%s", retVal.c_str());
	if(strcmp(retVal.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_contains_return_false() {
	emit_test("Does contains() return false when passed a string not in the "
		"list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "d";
	bool retVal = contains(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_return_false_substring() {
	emit_test("Does contains() return false when passed a string not in the "
		"list, but the list has a substring of the string?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "boo";
	bool retVal = contains(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_return_false_case() {
	emit_test("Does contains() return false when passed a string in the "
		"list when ignoring case?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "A";
	bool retVal = contains(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_return_true_one() {
	emit_test("Does contains() return true when passed one string in the "
		"list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "a";
	bool retVal = contains(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_return_true_many() {
	emit_test("Does contains() return true for multiple calls with different "
		"strings in the list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check1 = "a";
	const char* check2 = "b";
	const char* check3 = "c";
	bool retVal = contains(sl, check3) && contains(sl, check2)
		&& contains(sl, check1);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check3);
	emit_param("STRING", check2);
	emit_param("STRING", check1);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_return_false() {
	emit_test("Does contains_anycase() return false when passed a string not "
		"in the list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "d";
	bool retVal = contains_anycase(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_return_false_substring() {
	emit_test("Does contains_anycase() return false when passed a string not "
		"in the list, but the list has a substring of the string?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "boo";
	bool retVal = contains_anycase(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_return_true_one() {
	emit_test("Does contains_anycase() return true when passed one string in "
		"the list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "a";
	bool retVal = contains_anycase(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_return_true_many() {
	emit_test("Does contains_anycase() return true for multiple calls with "
		"different strings in the list?");
	std::vector<std::string> sl = split("a;b;C;D", ";");
	std::string orig = join(sl, ",");
	const char* check1 = "a";
	const char* check2 = "B";
	const char* check3 = "C";
	const char* check4 = "d";
	bool retVal = contains_anycase(sl, check4) && contains_anycase(sl, check3)
		&& contains_anycase(sl, check2) && contains_anycase(sl, check1);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check4);
	emit_param("STRING", check3);
	emit_param("STRING", check2);
	emit_param("STRING", check1);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_return_false_invalid() {
	emit_test("Does contains_prefix() return false when passed a string not in the "
		"list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "d";
	bool retVal = contains_prefix(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_return_false_almost() {
	emit_test("Does contains_prefix() return false when the list contains a "
		"string that is almost a prefix of the string?");
	std::vector<std::string> sl = split("abc;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "abd";
	bool retVal = contains_prefix(sl, check);
	emit_input_header();
	emit_param(";ist", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_return_false_reverse() {
	emit_test("Does contains_prefix() return false when the passed string is a "
		"prefix of one of the list's strings?");
	std::vector<std::string> sl = split("aah;boo;car", ";");
	std::string orig = join(sl, ",");
	const char* check = "bo";
	bool retVal = contains_prefix(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_return_false_case() {
	emit_test("Does contains_prefix() return false when the list contains a "
		"string that is a prefix of the string when ignoring case?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "B";
	bool retVal = contains_prefix(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_withwildcard_return_true() {
	emit_test("Does contains_prefix_withwildcard() return true when it should?");
	std::vector<std::string> sl = split("/home/*/aaa;b;/home/*/htc", ";");
	std::string orig = join(sl, ",");
	const char* check = "/home/tannenba/htc";
	bool retVal = contains_prefix_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_withwildcard_return_false() {
	emit_test("Does contains_prefix_withwildcard() return false when it should?");
	std::vector<std::string> sl = split("/home/*/aaa/*;b;/home/*/hpc", ";");
	std::string orig = join(sl, ",");
	const char* check = "/home/tannenba/htc";
	bool retVal = contains_prefix_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_return_true_identical() {
	emit_test("Does contains_prefix() return true when the passed string is in "
		"the list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "a";
	bool retVal = contains_prefix(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_prefix_return_true_many() {
	emit_test("Does contains_prefix() return true when the list contains "
		"prefixes of the string for multiple calls?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check1 = "car";
	const char* check2 = "bar";
	const char* check3 = "aar";
	bool retVal = contains_prefix(sl, check1) && contains_prefix(sl, check2)
		&& contains_prefix(sl, check3);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check1);
	emit_param("STRING", check2);
	emit_param("STRING", check3);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_false() {
	emit_test("Does contains_withwildcard() return false when passed a string"
		" not in the list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "d";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_false_substring() {
	emit_test("Does contains_withwildcard() return false when the list "
		"contains a substring of the string?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "boo";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_false_case() {
	emit_test("Does contains_withwildcard() return false when the list "
		"contains the string when ignoring case?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "A";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_false_wild() {
	emit_test("Does contains_withwildcard() return false when the passed "
		"string is not in the list when using a wildcard?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "d*";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_no_wild() {
	emit_test("Does contains_withwildcard() return true when the list "
		"contains the string when not using a wildcard?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "a";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_only_wild() {
	emit_test("Does contains_withwildcard() return true when the list "
		"passed contains a string consisting of only the wildcard?");
	std::vector<std::string> sl = split("*;a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "foo";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_start() {
	emit_test("Does contains_withwildcard() return true when list "
		"contains the string with a wildcard at the start?");
	std::vector<std::string> sl = split("a;*r;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "bar";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_mid() {
	emit_test("Does contains_withwildcard() return true when list "
		"contains the string with a wildcard in the middle of the string?");
	std::vector<std::string> sl = split("a;b*r;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "bar";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_mid_and_end() {
	emit_test("Does contains_withwildcard() return true when list "
		"contains the string with a wildcard in the middle and end of the string?");
	std::vector<std::string> sl = split("a;b*r*;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "bartbarboozehound";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_start_and_end() {
	emit_test("Does contains_withwildcard() return true when list "
		"contains the string with a wildcard at the start and end of the string?");
	std::vector<std::string> sl = split("a;*bar*;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "thebarboozebarhound";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (!retVal) {
		FAIL;
	}
	PASS;
}


static bool test_contains_withwildcard_return_false_mid_and_end() {
	emit_test("Does contains_withwildcard() return false when list "
		"does NOT contain the string with a wildcard in the middle and end of the string?");
	std::vector<std::string> sl = split("a;b*r*;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "batbaboozehound";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_false_start_and_end() {
	emit_test("Does contains_withwildcard() return false when list "
		"does NOT contain the string with a wildcard at the start and end of the string?");
	std::vector<std::string> sl = split("a;*bar*;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "thebaarboozebathoundba";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (retVal) {
		FAIL;
	}
	PASS;
}


static bool test_contains_withwildcard_return_true_end() {
	emit_test("Does contains_withwildcard() return true when list "
		"contains the string with a wildcard at the end of the string?");
	std::vector<std::string> sl = split("a;b*;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "bar";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_same_wild() {
	emit_test("Does contains_withwildcard() return true when list "
		"contains the string but the wildcard is used too?");
	std::vector<std::string> sl = split("a*;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "a";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_multiple() {
	emit_test("Does contains_withwildcard() return true when the list "
		"contains multiple matches of the string?");
	std::vector<std::string> sl = split("a*;ar*;are*;", ";");
	std::string orig = join(sl, ",");
	const char* check = "are";
	bool retVal = contains_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_withwildcard_return_true_many() {
	emit_test("Does contains_withwildcard() return true when list "
		"contains the strings for many calls?");
	std::vector<std::string> sl = split("a*;ba*;car*", ";");
	std::string orig = join(sl, ",");
	const char* check1 = "car";
	const char* check2 = "bar";
	const char* check3 = "aar";
	bool retVal = contains_withwildcard(sl, check1)
		&& contains_withwildcard(sl, check2)
		&& contains_withwildcard(sl, check3);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check1);
	emit_param("STRING", check2);
	emit_param("STRING", check3);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_false() {
	emit_test("Does contains_anycase_withwildcard() return false when passed "
		"a string not in the list?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "d";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_false_substring() {
	emit_test("Does contains_anycase_withwildcard() return false when the "
		"list contains a substring of the string?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "boo";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_false_wild() {
	emit_test("Does contains_anycase_withwildcard() return false when the "
		"passed string is not in the list when using a wildcard?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "d*";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_no_wild() {
	emit_test("Does contains_anycase_withwildcard() return true when the "
		"list contains the string when not using a wildcard?");
	std::vector<std::string> sl = split("a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "A";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_only_wild() {
	emit_test("Does contains_anycase_withwildcard() return true when the "
		"list passed contains a string consisting of only the wildcard?");
	std::vector<std::string> sl = split("*;a;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "foo";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_start() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"list contains the string with a wildcard at the start?");
	std::vector<std::string> sl = split("A;*R;C", ";");
	std::string orig = join(sl, ",");
	const char* check = "bar";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_mid() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"list contains the string with a wildcard in the middle of the "
		"string?");
	std::vector<std::string> sl = split("a;b*r;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "Bar";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_end() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"list contains the string with a wildcard at the end of the "
		"string?");
	std::vector<std::string> sl = split("a;b*;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "BAR";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_same_wild() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"list contains the string but the wildcard is used too?");
	std::vector<std::string> sl = split("a*;b;c", ";");
	std::string orig = join(sl, ",");
	const char* check = "A";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_multiple() {
	emit_test("Does contains_anycase_withwildcard() return true when the "
		"list contains multiple matches of the string?");
	std::vector<std::string> sl = split("a*;AR*;Are*;", ";");
	std::string orig = join(sl, ",");
	const char* check = "are";
	bool retVal = contains_anycase_withwildcard(sl, check);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_contains_anycase_wwc_return_true_many() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"list contains the strings for many calls?");
	std::vector<std::string> sl = split("a*;ba*;CAR*", ";");
	std::string orig = join(sl, ",");
	const char* check1 = "car";
	const char* check2 = "bar";
	const char* check3 = "AAR";
	bool retVal = contains_anycase_withwildcard(sl, check1)
		&& contains_anycase_withwildcard(sl, check2)
		&& contains_anycase_withwildcard(sl, check3);
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("STRING", check1);
	emit_param("STRING", check2);
	emit_param("STRING", check3);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		FAIL;
	}
	PASS;
}

static bool test_join_empty() {
	emit_test("Test join() on an empty list.");
	std::vector<std::string> sl;
	std::string ps = join(sl, ",");
	const char* expect = "";
	emit_input_header();
	emit_param("list", expect);
	emit_param("DELIM", ",");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(ps.c_str());
	if(strcmp(ps.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_join_one() {
	emit_test("Test join() on a list with only one string.");
	std::vector<std::string> sl = split("foo", "");
	std::string ps = join(sl, "!");
	const char* expect = "foo";
	emit_input_header();
	emit_param("list", expect);
	emit_param("DELIM", "!");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(ps.c_str());
	if(strcmp(ps.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_join_many_short() {
	emit_test("Test join() with a short delimiter on a "
		"list with many strings.");
	std::vector<std::string> sl = split("a;b;c;dog;eel;fish;goat", ";");
	std::string orig = join(sl, ",");
	std::string ps = join(sl, "&");
	const char* expect = "a&b&c&dog&eel&fish&goat";
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("DELIM", "&");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(ps.c_str());
	if(strcmp(ps.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_join_many_long() {
	emit_test("Test join() with a long delimiter on a "
		"list with many strings.");
	std::vector<std::string> sl = split("a;b;c;dog;eel;fish;goat", ";");
	std::string orig = join(sl, ",");
	std::string ps = join(sl, " and ");
	const char* expect = "a and b and c and dog and eel and fish and goat";
	emit_input_header();
	emit_param("list", orig.c_str());
	emit_param("DELIM", " and ");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(ps.c_str());
	if(strcmp(ps.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}
