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
	This code tests the fullpath() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "basename.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "string.h"

static bool test_forward_slash(void);
static bool test_name(void);
static bool test_drive_path_backslash(void);
static bool test_colon_backslash(void);
static bool test_backslash(void);
static bool test_drive_path_forward_slash(void);
static bool test_colon_forward_slash(void);

bool FTEST_fullpath(void) {
	emit_function("int fullpath( const char* path )");
	emit_comment("return TRUE if the given path is a full pathname, FALSE if not.  by full pathname, we mean it either begins with '/' or '\' or '*:\' (something like 'c:\\...' on windoze).");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_forward_slash);
	driver.register_function(test_name);
	driver.register_function(test_drive_path_backslash);
	driver.register_function(test_colon_backslash);
	driver.register_function(test_backslash);
	driver.register_function(test_drive_path_forward_slash);
	driver.register_function(test_colon_forward_slash);
	
		// run the tests
	return driver.do_all_functions();
}

static bool test_forward_slash() {
	emit_test("Does a path starting with a forward slash return true?");
	const char *param = "/tmp/foo";
	int expected = 1;
	emit_input_header();
	emit_param("STRING", param);
	int result = fullpath(param);
	emit_output_expected_header();
	emit_retval("%d", expected);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(expected != result) {
		FAIL;
	}
	PASS;
}

static bool test_name() {
	emit_test("Does a path starting with a name return false?");
	const char *param = "tmp/foo";
	int expected = 0;
	emit_input_header();
	emit_param("STRING", param);
	int result = fullpath(param);
	emit_output_expected_header();
	emit_retval("%d", expected);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(expected != result) {
		FAIL;
	}
	PASS;
}
static bool test_drive_path_backslash() {
	emit_test("Does a path with a drive letter followed by a colon and backslash return true?");
	const char *param = "c:\\";
	int expected = 1;
	emit_input_header();
	emit_param("STRING", param);
	int result = fullpath(param);
	emit_output_expected_header();
	emit_retval("%d", expected);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(expected != result) {
		FAIL;
	}
	PASS;
}

static bool test_colon_backslash() {
	emit_test("Does a path starting with a colon followed by a backslash return false?");
	const char *param = ":\\";
	int expected = 0;
	emit_input_header();
	emit_param("STRING", param);
	int result = fullpath(param);
	emit_output_expected_header();
	emit_retval("%d", expected);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(expected != result) {
		FAIL;
	}
	PASS;
}

static bool test_backslash() {
	emit_test("Does a path with only a backslash return true?");
	const char *param = "\\";
	int expected = 1;
	emit_input_header();
	emit_param("STRING", param);
	int result = fullpath(param);
	emit_output_expected_header();
	emit_retval("%d", expected);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(expected != result) {
		FAIL;
	}
	PASS;
}

static bool test_drive_path_forward_slash() {
	emit_test("Does a path with a drive letter followed by a colon and forward slash return true?");
	const char *param = "x:/";
	int expected = 1;
	emit_input_header();
	emit_param("STRING", param);
	int result = fullpath(param);
	emit_output_expected_header();
	emit_retval("%d", expected);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(expected != result) {
		FAIL;
	}
	PASS;
}

static bool test_colon_forward_slash() {
	emit_test("Does a path with a colon followed by a forward slash return false?");
	const char *param = ":/";
	int expected = 0;
	emit_input_header();
	emit_param("STRING", param);
	int result = fullpath(param);
	emit_output_expected_header();
	emit_retval("%d", expected);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(expected != result) {
		FAIL;
	}
	PASS;
}
