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
#include "internet.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

static bool test_normal_case(void);
static bool test_hostname(void);
static bool test_ipv6_sinful();
static bool test_complex_sinful();

bool FTEST_getHostFromAddr(void) {
		// beginning junk for getPortFromAddr(() {
	emit_function("getHostFromAddr(sockaddr_in)");
	emit_comment("Returns the host portion of the sinful string");
	emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_hostname);
	driver.register_function(test_ipv6_sinful);
	driver.register_function(test_complex_sinful);
	
		// run the tests
	return driver.do_all_functions();
}

static bool test_normal_case() {
	emit_test("Is normal input parsed correctly?");
	char* inputstring = strdup("<192.168.0.2:496>");
	emit_input_header();
	emit_param("STRING", inputstring);
	char* sinstring = getHostFromAddr(inputstring);
	free(inputstring);
	emit_output_expected_header();
	char expected[30];
	snprintf(expected, sizeof(expected), "192.168.0.2");
	emit_retval(expected);
	emit_output_actual_header();
	emit_retval(sinstring);
	if(strcmp(expected, sinstring) != 0) {
		free(sinstring);
		FAIL;
	}
	free(sinstring);
	PASS;
}

static bool test_hostname() {
	emit_test("Is input with a hostname parsed correctly?");
	char* inputstring = strdup("<balthazar.cs.wisc.edu:496>");
	emit_input_header();
	emit_param("STRING", inputstring);
	char* sinstring = getHostFromAddr(inputstring);
	free(inputstring);
	emit_output_expected_header();
	char expected[30];
	snprintf(expected, sizeof(expected), "balthazar.cs.wisc.edu");
	emit_retval(expected);
	emit_output_actual_header();
	emit_retval(sinstring);
	if(strcmp(expected, sinstring) != 0) {
		free(sinstring);
		FAIL;
	}
	free(sinstring);
	PASS;
}

static bool test_ipv6_sinful() {
	emit_test("Is IPv6 input parsed correctly?");
	const char* inputstring = "<[2607:f388:107c:501:90e2:baff:fe2c:2724]:496>";
	emit_input_header();
	emit_param("STRING", inputstring);
	char* sinstring = getHostFromAddr(inputstring);
	emit_output_expected_header();
	const char *expected = "2607:f388:107c:501:90e2:baff:fe2c:2724";
	emit_retval(expected);
	emit_output_actual_header();
	emit_retval(sinstring);
	if(strcmp(expected, sinstring) != 0) {
		free(sinstring);
		FAIL;
	}
	free(sinstring);
	PASS;
}

static bool test_complex_sinful() {
	emit_test("Is complex sinful input parsed correctly?");
	const char* inputstring = "<192.168.0.2:51450?addrs=192.168.0.2-51450+[--1]-51450>";
	emit_input_header();
	emit_param("STRING", inputstring);
	char* sinstring = getHostFromAddr(inputstring);
	emit_output_expected_header();
	const char *expected = "192.168.0.2";
	emit_retval(expected);
	emit_output_actual_header();
	emit_retval(sinstring);
	if(strcmp(expected, sinstring) != 0) {
		free(sinstring);
		FAIL;
	}
	free(sinstring);
	PASS;
}
