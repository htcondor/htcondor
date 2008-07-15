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
static bool test_one_octet_wildcard(void);
static bool test_upper_bound(void);
static bool test_lower_bound(void);
static bool test_only_wildcard(void);
static bool test_start_wildcard(void);

bool FTEST_is_ipaddr(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("int is_ipaddr(char* addr)");
	e.emit_comment("Determines if a function is an ip address (with wildcards allowed).");
	e.emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_one_octet_wildcard);
	driver.register_function(test_upper_bound);
	driver.register_function(test_lower_bound);
	driver.register_function(test_only_wildcard);
	driver.register_function(test_start_wildcard);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input identified correctly?");
	char* input = strdup( "66.184.142.51" );
	e.emit_input_header();
	e.emit_param("IP", input);
	struct in_addr ip;
	unsigned char* byte = (unsigned char*) &ip;
	int result = is_ipaddr( input, &ip );
	free( input );
	e.emit_output_expected_header();
	e.emit_param("IP", "%d.%d.%d.%d", 66, 184, 142, 51);
	e.emit_retval("%s", tfstr(TRUE));
	e.emit_output_actual_header();
	e.emit_param("IP", "%d.%d.%d.%d", *byte, *(byte+1), *(byte+2), *(byte+3));
	e.emit_retval("%s", tfstr(result));
	if(result != TRUE || *byte != 66 || *(byte+1) != 184 || *(byte+2) != 142 || *(byte+3) != 51) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_one_octet_wildcard() {
	e.emit_test("Is it identified as an IP address with one octet and a wildcard?");
	char* input = strdup( "66.*" );
	e.emit_input_header();
	e.emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
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

static bool test_upper_bound() {
	e.emit_test("Does it work with one octet >255?");
	char* input = strdup( "3.14.159.265" );
	e.emit_input_header();
	e.emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
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

static bool test_lower_bound() {
	e.emit_test("Does it work with one octet <0?");
	char* input = strdup( "-2.71.82.81" );
	e.emit_input_header();
	e.emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
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

static bool test_only_wildcard() {
	e.emit_test("Does it work with nothing but a single wildcard?"); 
	char* input = strdup( "*" );
	e.emit_input_header();
	e.emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
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

static bool test_start_wildcard() {
	e.emit_test("Does it fail correctly with a wildcard and then some octets?");
	char* input = strdup( "*.0.42.1" );
	e.emit_input_header();
	e.emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
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
