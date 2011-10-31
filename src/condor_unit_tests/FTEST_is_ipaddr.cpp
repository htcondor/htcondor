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

// IPV6_REMOVED - IPv6 changes obsoleted these interfaces.  However, 
// these tests probably should be updated to the new interfaces,
// so they are being kept here for nwo.
#ifdef IPV6_REMOVED
static bool test_normal_case(void);
static bool test_one_octet_wildcard(void);
static bool test_upper_bound(void);
static bool test_lower_bound(void);
static bool test_only_wildcard(void);
static bool test_start_wildcard(void);
#endif

bool FTEST_is_ipaddr(void) {
		// beginning junk for getPortFromAddr(() {
	emit_function("int is_ipaddr(char* addr)");
	emit_comment("Determines if a function is an ip address (with wildcards allowed).");
	emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
#ifdef IPV6_REMOVED
	driver.register_function(test_normal_case);
	driver.register_function(test_one_octet_wildcard);
	driver.register_function(test_upper_bound);
	driver.register_function(test_lower_bound);
	driver.register_function(test_only_wildcard);
	driver.register_function(test_start_wildcard);
#endif
	
		// run the tests
	return driver.do_all_functions();
}

#ifdef IPV6_REMOVED
static bool test_normal_case() {
	emit_test("Is normal input identified correctly?");
	char* input = strdup( "66.184.142.51" );
	emit_input_header();
	emit_param("IP", input);
	struct in_addr ip;
	unsigned char* byte = (unsigned char*) &ip;
	int result = is_ipaddr( input, &ip );
	free( input );
	emit_output_expected_header();
	emit_param("IP", "%d.%d.%d.%d", 66, 184, 142, 51);
	emit_retval("%s", tfstr(TRUE));
	emit_output_actual_header();
	emit_param("IP", "%d.%d.%d.%d", *byte, *(byte+1), *(byte+2), *(byte+3));
	emit_retval("%s", tfstr(result));
	if(result != TRUE || *byte != 66 || *(byte+1) != 184 || *(byte+2) != 142 || *(byte+3) != 51) {
		FAIL;
	}
	PASS;
}

static bool test_one_octet_wildcard() {
	emit_test("Is it identified as an IP address with one octet and a wildcard?");
	char* input = strdup( "66.*" );
	emit_input_header();
	emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
	free( input );
	emit_output_expected_header();
	emit_retval("%s", tfstr(TRUE));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != TRUE) {
		FAIL;
	}
	PASS;
}

static bool test_upper_bound() {
	emit_test("Does it work with one octet >255?");
	char* input = strdup( "3.14.159.265" );
	emit_input_header();
	emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
	free( input );
	emit_output_expected_header();
	emit_retval("%s", tfstr(FALSE));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != FALSE) {
		FAIL;
	}
	PASS;
}

static bool test_lower_bound() {
	emit_test("Does it work with one octet <0?");
	char* input = strdup( "-2.71.82.81" );
	emit_input_header();
	emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
	free( input );
	emit_output_expected_header();
	emit_retval("%s", tfstr(FALSE));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != FALSE) {
		FAIL;
	}
	PASS;
}

static bool test_only_wildcard() {
	emit_test("Does it work with nothing but a single wildcard?"); 
	char* input = strdup( "*" );
	emit_input_header();
	emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
	free( input );
	emit_output_expected_header();
	emit_retval("%s", tfstr(TRUE));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != TRUE) {
		FAIL;
	}
	PASS;
}

static bool test_start_wildcard() {
	emit_test("Does it fail correctly with a wildcard and then some octets?");
	char* input = strdup( "*.0.42.1" );
	emit_input_header();
	emit_param("IP", input);
	int result = is_ipaddr( input, NULL );
	free( input );
	emit_output_expected_header();
	emit_retval("%s", tfstr(FALSE));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != FALSE) {
		FAIL;
	}
	PASS;
}
#endif
