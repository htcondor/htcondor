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
#include "emit.h"
#include "unit_test_utils.h"

static bool test_normal_case(void);
static bool test_hostname(void);
static bool test_no_angle_brackets(void);

bool FTEST_is_valid_sinful(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("int is_valid_sinful(char* sinful)");
	e.emit_comment("Determines if a function is a valid sinful string.");
	e.emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_hostname);
	driver.register_function(test_no_angle_brackets);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input identified correctly?");
	char* input = strdup( "<208.122.19.56:47>" );
	e.emit_input_header();
	e.emit_param("SINFUL", input);
	int result = is_valid_sinful( input );
	free( input );
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(TRUE));
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	if(result != TRUE) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_hostname() {
	e.emit_test("Are hostnames instead of IP addresses rejected like they should be?");
	char* input = strdup( "<balthazar.cs.wisc.edu:47>" );
	e.emit_input_header();
	e.emit_param("SINFUL", input);
	int result = is_valid_sinful( input );
	free( input );
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(FALSE));
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	if(result != FALSE) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_no_angle_brackets() {
	e.emit_test("Is the string correctly rejected if there are no angle brackets?");
	char* input = strdup( "209.172.63.167:8080" );
	e.emit_input_header();
	e.emit_param("SINFUL", input);
	int result = is_valid_sinful( input );
	free( input );
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(FALSE));
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	if(result != FALSE) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}
