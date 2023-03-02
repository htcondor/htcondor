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
	This code tests the strupr() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "strupr.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"

static bool test_normal_case(void);
static bool test_return_value(void);

bool FTEST_strupr(void) {
	emit_function("char* strupr(char* src)");
	emit_comment("Convert a string, in place, to the uppercase version of it.");
	emit_problem("Function does no error checking on the size of string");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_return_value);
	
		// run the tests
	return driver.do_all_functions();
}

static bool test_normal_case() {
	emit_test("Does a lowercase string get converted to upper in place?");
	char input[1024];
	snprintf(input, sizeof(input), "%s", "lower");
	emit_input_header();
	emit_param("STRING", input);
	strupr(input);
	emit_output_expected_header();
	emit_retval("LOWER");
	emit_output_actual_header();
	emit_retval("%s", input);
	if(strcmp(input, "LOWER") != 0) {
		FAIL;
	}
	PASS;
}

static bool test_return_value() {
	char *up = NULL;
	emit_test("Does the function return correct pointer to input parameter?");
	char input[1024];
	snprintf(input, sizeof(input), "%s", "lower");
	emit_input_header();
	emit_param("STRING", input);
	up = strupr(input);
	emit_output_expected_header();
	emit_retval("%p", input);
	emit_output_actual_header();
	emit_retval("%p", up);

	if(up != input) {
		FAIL;
	}
	PASS;
}
