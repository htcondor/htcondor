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

bool FTEST_dirname(void) {
	emit_function("function:  condor_dirname(const char *path)");
	emit_comment("comment: A dirname() function that is happy on both Unix and NT. This allocates space for a new string that holds the path of the parent directory of the path it was given.  If the given path has no directory delimiters, or is NULL, we just return '.'.  In all cases, the string we return is new space, and must be deallocated with free().   Derek Wright 9/23/99");
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
	return driver.do_all_functions();
}

static bool test_null() {
	emit_test("Does a NULL path return a period?");
	const char *param = "NULL";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_empty_string() {
	emit_test("Does an empty path return a period?");
	const char *param = "";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_current_directory() {
	emit_test("Does the current directory return the current directory?");
	const char *param = ".";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_path_1() {
	emit_test("Does a single character path return the current directory?");
	const char *param = "f";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_path_2() {
	emit_test("Does a multiple character path return the currect directory?");
	const char *param = "foo";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_directory_1() {
	emit_test("Does a path with starting with a forward slash return a forward slash?");
	const char *param = "/foo";
	const char *expect = "/";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_simple_directory_2() {
	emit_test("Does a path starting with a backslash return a backslash?");
	const char *param = "\\foo";
	const char *expect = "\\";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_file_1() {
	emit_test("Does a path with both a directory and file return the parent directory?");
	const char *param = "foo/bar";
	const char *expect = "foo";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_file_2() {
	emit_test("Does a path with both a directory and file in the root directory return the parent directory?");
	const char *param = "/foo/bar";
	const char *expect = "/foo";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_root_directory() {
	emit_test("Does a path with a backslash in the root directory return the parent directory?");
	const char *param = "\\foo\\bar";
	const char *expect = "\\foo";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_directory() {
	emit_test("Does a path with two directories return the full parent directory?");
	const char *param = "foo/bar/";
	const char *expect = "foo/bar";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_directory_and_directory_in_root() {
	emit_test("Does a path with two directories in the root directory return the full parent directory?");
	const char *param = "/foo/bar/";
	const char *expect = "/foo/bar";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_forward_slash() {
	emit_test("Does a path with only a forward slash return a forward slash?");
	const char *param = "/";
	const char *expect = "/";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_backslash() {
	emit_test("Does a path with only a backslash return a backslash?");
	const char *param = "\\";
	const char *expect = "\\";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash_1() {
	emit_test("Does a path with a period and backslash return a period?");
	const char *param = "./bar";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash_1() {
	emit_test("Does a path with a period and forwardslash return a period?");
	const char *param = ".\\bar";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash_2() {
	emit_test("Does a path with only a period then forwardslash return a period?");
	const char *param = "./";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash_2() {
	emit_test("Does a path with only a period then backslash return a period?");
	const char *param = ".\\";
	const char *expect = ".";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_backslash_and_period() {
	emit_test("Does a path with only a backslash then a period return a backslash?");
	const char *param = "\\.";
	const char *expect = "\\";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_forward_slash_and_file_extension() {
	emit_test("Does a path with two directories and a file extension return the the full parent directory?");
	const char *param = "foo/bar/zap.txt";
	const char *expect = "foo/bar";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_backslash_and_file_extension() {
	emit_test("Does a path with two directories and a file extension using backslashes return the full parent directory?");
	const char *param = "foo\\bar\\zap.txt";
	const char *expect = "foo\\bar";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash() {
	emit_test("Does a path using both a period and a forward slash return the parent directory");
	const char *param = ".foo/bar";
	const char *expect = ".foo";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash() {
	emit_test("Does a path using both a period and a backslash return the parent directory?");
	const char *param = ".foo\\bar";
	const char *expect = ".foo";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_forward_slash_with_special_file() {
	emit_test("Does a path using both a period and a forward slash return the parent directory?");
	const char *param = ".foo/.bar.txt";
	const char *expect = ".foo";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_period_and_backslash_with_special_file() {
	emit_test("Does a path using a period and a backslash return the parent directory?");
	const char *param = ".foo\\.bar.txt";
	const char *expect = ".foo";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_double_forward_slash() {
	emit_test("Does a path with one directory using a double forward slash and the other using a single forward slashe return the full parent directory?");
	const char *param = "//foo/bar/zap.txt";
	const char *expect = "//foo/bar";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_double_backslash() {
	emit_test("Does a path with one directory using a double backslash and the other using a single backslash return the full parent directory?");
	const char *param = "\\\\foo\\bar\\zap.txt";
	const char *expect = "\\\\foo\\bar";
	emit_input_header();
	emit_param("STRING", param);
	emit_output_expected_header();
	emit_retval("%s", expect);
	std::string path = condor_dirname(param);
	emit_output_actual_header();
	emit_retval("%s", path.c_str());
	if(strcmp(path.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

