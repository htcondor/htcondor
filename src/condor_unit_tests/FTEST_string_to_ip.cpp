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
static bool test_invalid_ip(void);

bool FTEST_string_to_ip(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("int string_to_ip(char* addr)");
	e.emit_comment("Starts with a sinful string and finds the unsigned int ip address from it.");
	e.emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_invalid_ip);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input identified correctly?");
	char* input = strdup( "<66.230.200.100:1814>" );
	e.emit_input_header();
	e.emit_param("IP", input);
	unsigned int result = string_to_ip(input);
	unsigned char* byte = (unsigned char*) &result;
	free( input );
	e.emit_output_expected_header();
	unsigned int expected;
	char* ptr = (char*) &expected;
	*ptr = 66;
	ptr++;
	*ptr = 230;
	ptr++;
	*ptr = 200;
	ptr++;
	*ptr = 100;
	e.emit_retval("%u (%d.%d.%d.%d)", expected, 66, 230, 200, 100);
	e.emit_output_actual_header();
	e.emit_retval("%u (%d.%d.%d.%d)", result, *byte, *(byte+1), *(byte+2), *(byte+3));
	if(result != expected) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_invalid_ip() {
	e.emit_test("Does it work correctly when a wildcard is passed in?");
	char* input = strdup( "<66.*:1814>" );
	e.emit_input_header();
	e.emit_param("IP", input);
	unsigned int result = string_to_ip(input);
	unsigned char* byte = (unsigned char*) &result;
	free( input );
	e.emit_output_expected_header();
	unsigned int expected;
	char* ptr = (char*) &expected;
	*ptr = 66;
	ptr++;
	*ptr = 255;
	ptr++;
	*ptr = 255;
	ptr++;
	*ptr = 255;
	e.emit_retval("%u (%d.%d.%d.%d)", expected, 66, 255, 255, 255);
	e.emit_output_actual_header();
	e.emit_retval("%u (%d.%d.%d.%d)", result, *byte, *(byte+1), *(byte+2), *(byte+3));
	if(result != expected) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}
