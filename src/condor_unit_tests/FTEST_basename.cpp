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

/*
	This code tests the condor_basename() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "basename.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "string.h"

static bool test_null(void);
static bool test_empty_string(void);
static bool test_current_directory(void);
static bool test_simple_path_1(void);
static bool test_simple_path_2(void);
static bool test_simple_directory_1(void);
static bool test_simple_directory_2(void);
static bool test_directory_and_file_1(void);
static bool test_directory_and_file_2(void);
static bool test_root_directory(void);
static bool test_directory_and_directory(void);
static bool test_directory_and_directory_in_root(void);
static bool test_forward_slash(void);
static bool test_backslash(void);
static bool test_period_and_forward_slash_1(void);
static bool test_period_and_backslash_1(void);
static bool test_period_and_forward_slash_2(void);
static bool test_period_and_backslash_2(void);
static bool test_backslash_and_period(void);
static bool test_forward_slash_and_file_extension(void);
static bool test_backslash_and_file_extension(void);
static bool test_period_and_forward_slash(void);
static bool test_period_and_backslash(void);
static bool test_period_and_forward_slash_with_special_file(void);
static bool test_period_and_backslash_with_special_file(void);
static bool test_double_forward_slash(void);
static bool test_double_backslash(void);

bool FTEST_basename(void) {
	emit_function("const char* condor_basename(const char *path)");
	emit_comment("A basename() function that is happy on both Unix and NT. It returns a pointer to the last element of the path it was given, or the whole string, if there are no directory delimiters.  There's no memory allocated, overwritten or changed in anyway.");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_null);
	driver.register_function(test_empty_string);
	driver.register_function(test_current_directory);
	driver.register_function(test_simple_path_1);
	driver.register_function(test_simple_path_2);
	driver.register_function(test_simple_directory_1);
	driver.register_function(test_simple_directory_2);
	driver.register_function(test_directory_and_file_1);
	driver.register_function(test_directory_and_file_2);
	driver.register_function(test_root_directory);
	driver.register_function(test_directory_and_directory);
	driver.register_function(test_directory_and_directory_in_root);
	driver.register_function(test_forward_slash);
	driver.register_function(test_backslash);
	driver.register_function(test_period_and_forward_slash_1);
	driver.register_function(test_period_and_backslash_1);
	driver.register_function(test_period_and_forward_slash_2);
	driver.register_function(test_period_and_backslash_2);
	driver.register_function(test_backslash_and_period);
	driver.register_function(test_forward_slash_and_file_extension);
	driver.register_function(test_backslash_and_file_extension);
	driver.register_function(test_period_and_forward_slash);
	driver.register_function(test_period_and_backslash);
	driver.register_function(test_period_and_forward_slash_with_special_file);
	driver.register_function(test_period_and_backslash_with_special_file);
	driver.register_function(test_double_forward_slash);
	driver.register_function(test_double_backslash);

		// run the tests
	return driver.do_all_functions();
}

static bool test_null() {
	emit_test("Does a NULL path return the empty string?");
	const char *param = NULL;
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", "NULL");
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_empty_string() {
	emit_test("Does a empty path return the empty string?");
	const char *param = "";
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_current_directory() {
	emit_test("Does the current directory return the current directory");
	const char *param = ".";
	const char *expected = ".";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_path_1() {
	emit_test("Does a single character path return the base name?");
	const char *param = "f";
	const char *expected = "f";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_path_2() {
	emit_test("Does a multiple character path return the base name?");
	const char *param = "foo";
	const char *expected = "foo";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_directory_1() {
	emit_test("Does a path with a directory return the directory base name?");
	const char *param = "/foo";
	const char *expected = "foo";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_directory_2() {
	emit_test("Does a path with a backslash return the directory base name?");
	const char *param = "\\foo";
	const char *expected = "foo";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_file_1() {
	emit_test("Does a path with both a directory and file return the file base?");
	const char *param = "foo/bar";
	const char *expected = "bar";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_file_2() {
	emit_test("Does a path with both a directory and file in the root directory return the base name?");
	const char *param = "/foo/bar";
	const char *expected = "bar";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_root_directory() {
	emit_test("Does a path with a backslash in the root directory return the base name?");
	const char *param = "\\foo\\bar";
	const char *expected = "bar";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_directory() {
	emit_test("Does a path with two directories return the empty string?");
	const char *param = "foo/bar/";
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_directory_in_root() {
	emit_test("Does a path with two directories in the root directory return the empty string?");
	const char *param = "/foo/bar/";
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_forward_slash() {
	emit_test("Does a path with only a forward slash return the empty string?");
	const char *param = "/";
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_backslash() {
	emit_test("Does a path with only a backslash return the empty string?");
	const char *param = "\\";
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash_1() {
	emit_test("Does a path with a period and backslash return the directory base name?");
	const char *param = "./bar";
	const char *expected = "bar";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash_1() {
	emit_test("Does a path with a period and forwardslash return the directory base name?");
	const char *param = ".\\bar";
	const char *expected = "bar";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash_2() {
	emit_test("Does a path with only a period then forwardslash return the empty string?");
	const char *param = "./";
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash_2() {
	emit_test("Does a path with only a period then backslash return the empty string?");
	const char *param = ".\\";
	const char *expected = "";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_backslash_and_period() {
	emit_test("Does a path with only a backslash then a period return a period?");
	const char *param = "\\.";
	const char *expected = ".";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_forward_slash_and_file_extension() {
	emit_test("Does a path with two directories and a file extension return the file name and its extension?");
	const char *param = "foo/bar/zap.txt";
	const char *expected = "zap.txt";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_backslash_and_file_extension() {
	emit_test("Does a path with two directories and a file extension using backslashes return the file name and its extension?");
	const char *param = "foo\\bar\\zap.txt";
	const char *expected = "zap.txt";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash() {
	emit_test("Does a path using both a period and a forwardslash return the base name?");
	const char *param = ".foo/bar";
	const char *expected = "bar";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash() {
	emit_test("Does a path with one directory using a period and another using a backslash return the base name?");
	const char *param = ".foo\\bar";
	const char *expected = "bar";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash_with_special_file() {
	emit_test("Does a path with one directory using a period and another using a forwardslash return the file name starting with a period and its extension?");
	const char *param = ".foo/.bar.txt";
	const char *expected = ".bar.txt";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash_with_special_file() {
	emit_test("Does a path with one directory using a period and another using a backslash return the file name starting with a period and its extension?");
	const char *param = ".foo\\.bar.txt";
	const char *expected = ".bar.txt";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_double_forward_slash() {
	emit_test("Does a path with one directory using a double forwardslash and the others using single forwardslashes return the file name and its extension?");
	const char *param = "//foo/bar/zap.txt";
	const char *expected = "zap.txt";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_double_backslash() {
	emit_test("Does a path with one directory using a double backslash and the othere using single backslashes return the file name and its extension?");
	const char *param = "\\\\foo\\bar\\zap.txt";
	const char *expected = "zap.txt";
	emit_input_header();
	emit_param("STRING", param);
	const char *path = condor_basename(param);
	emit_output_expected_header();
	emit_retval("%s", expected);
	emit_output_actual_header();
	emit_retval("%s", path);
	if(strcmp(path, expected) != MATCH) {
		FAIL;
	}
	PASS;
}
