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
	This code tests the sin_to_string() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "internet.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

static bool test_normal_case(void);
static bool test_no_brackets(void);
static bool test_no_port(void);
static bool test_negative_port(void);
static bool test_large_port_number(void);
static bool test_alpha_port(void);

bool FTEST_getPortFromAddr(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("getPortFromAddr(const char*)");
	e.emit_comment("Returns the port part of a sinful string as an int.");
	e.emit_problem("Valid port numbers only go up to 65,535, but this function accepts anything that fits in an int.");

		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_no_brackets);
	driver.register_function(test_no_port);
	driver.register_function(test_negative_port);
	driver.register_function(test_large_port_number);
	driver.register_function(test_alpha_port);

		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal output parsed correctly?");
	char* input = strdup( "<192.168.0.2:790>" );
	e.emit_input_header();
	e.emit_param("STRING", input);
	e.emit_output_expected_header();
	int expected = 790;
	e.emit_retval("%d", expected);
	e.emit_output_actual_header();
	int result = getPortFromAddr(input);
	free(input);
	e.emit_retval("%d", result);
	if(expected != result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_no_brackets() {
	e.emit_test("Is input parsed correctly without angle brackets?");
	char* input = strdup( "37.94.3.21:4" );
	e.emit_input_header();
	e.emit_param("STRING", input);
	e.emit_output_expected_header();
	int expected = 4;
	e.emit_retval("%d", expected);
	e.emit_output_actual_header();
	int result = getPortFromAddr(input);
	free(input);
	e.emit_retval("%d", result);
	if(expected != result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_no_port() {
	e.emit_test("Is an error thrown if no port number is supplied?");
	char* input = strdup( "37.94.3.21" );
	e.emit_input_header();
	e.emit_param("STRING", input);
	e.emit_output_expected_header();
	int expected = -1;
	e.emit_retval("%d", expected);
	e.emit_output_actual_header();
	int result = getPortFromAddr(input);
	free(input);
	e.emit_retval("%d", result);
	if(expected != result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_negative_port() {
	e.emit_test("Is an error thrown if the port number is negative?");
	char* input = strdup( "37.94.3.21:-497" );
	e.emit_input_header();
	e.emit_param("STRING", input);
	e.emit_output_expected_header();
	int expected = -1;
	e.emit_retval("%d", expected);
	e.emit_output_actual_header();
	int result = getPortFromAddr(input);
	free(input);
	e.emit_retval("%d", result);
	if(expected != result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_large_port_number() {
	e.emit_test("Is an error thrown if the port number is crazy big (bigger than an int)?");
	char* input = strdup( "37.94.3.21:7569872365" );
	e.emit_input_header();
	e.emit_param("STRING", input);
	e.emit_output_expected_header();
	int expected = -1;
	e.emit_retval("%d", expected);
	e.emit_output_actual_header();
	int result = getPortFromAddr(input);
	free(input);
	e.emit_retval("%d", result);
	if(expected != result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_alpha_port() {
	e.emit_test("Is an error thrown if text is used instead of a port number?");
	char* input = strdup( "37.94.3.21:penguin" );
	e.emit_input_header();
	e.emit_param("STRING", input);
	e.emit_output_expected_header();
	int expected = -1;
	e.emit_retval("%d", expected);
	e.emit_output_actual_header();
	int result = getPortFromAddr(input);
	free(input);
	e.emit_retval("%d", result);
	if(expected != result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}
