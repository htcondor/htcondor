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
static bool test_slash_notation(void);
static bool test_classless_mask(void);
static bool test_wildcard(void);
static bool test_plain_ip(void);

bool FTEST_is_valid_network(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("int is_valid_network(char* network, struct in_addr* ip, struct in_addr* mask)");
	e.emit_comment("Returns true if the given string is a valid network.");
	e.emit_problem("Allows the use of wildcards in addresses and netmasks.");
	e.emit_problem("Doesn't check to see if the netmask is a valid netmask, if it sees something like an IP address it assumes everything works.");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_slash_notation);
	driver.register_function(test_classless_mask);
	driver.register_function(test_wildcard);
	driver.register_function(test_plain_ip);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input parsed correctly?");
	char* inputstring = strdup("192.168.3.4/255.255.255.0");
	e.emit_input_header();
	e.emit_param("STRING", inputstring);
	struct in_addr ip;
	unsigned char* ipbyte = (unsigned char*) &ip;
	struct in_addr mask;
	unsigned char* maskbyte = (unsigned char*) &mask;
	int result = is_valid_network(inputstring, &ip, &mask);
	free(inputstring);
	e.emit_output_expected_header();
	int expected_return = TRUE;
	e.emit_retval("%s", tfstr(expected_return));
	e.emit_param("IP", "192.168.3.4");
	e.emit_param("MASK", "255.255.255.0");
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	e.emit_param("IP", "%d.%d.%d.%d", *ipbyte, *(ipbyte + 1), *(ipbyte + 2), *(ipbyte + 3));
	e.emit_param("MASK", "%d.%d.%d.%d", *maskbyte, *(maskbyte + 1), *(maskbyte + 2), *(maskbyte + 3));
	if( expected_return != result ||
		192 != *ipbyte || 168 != *(ipbyte + 1) || 3 != *(ipbyte + 2) || 4 != *(ipbyte + 3) ||
		255 != *maskbyte || 255 != *(maskbyte + 1) || 255 != *(maskbyte + 2) || 0 != *(maskbyte + 3)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_slash_notation() {
	e.emit_test("Is normal input parsed correctly with slash notation for the mask?");
	char* inputstring = strdup("192.168.3.4/8");
	e.emit_input_header();
	e.emit_param("STRING", inputstring);
	struct in_addr ip;
	unsigned char* ipbyte = (unsigned char*) &ip;
	struct in_addr mask;
	unsigned char* maskbyte = (unsigned char*) &mask;
	int result = is_valid_network(inputstring, &ip, &mask);
	free(inputstring);
	e.emit_output_expected_header();
	int expected_return = TRUE;
	e.emit_retval("%s", tfstr(expected_return));
	e.emit_param("IP", "192.168.3.4");
	e.emit_param("MASK", "255.0.0.0");
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	e.emit_param("IP", "%d.%d.%d.%d", *ipbyte, *(ipbyte + 1), *(ipbyte + 2), *(ipbyte + 3));
	e.emit_param("MASK", "%d.%d.%d.%d", *maskbyte, *(maskbyte + 1), *(maskbyte + 2), *(maskbyte + 3));
	if( expected_return != result ||
		192 != *ipbyte || 168 != *(ipbyte + 1) || 3 != *(ipbyte + 2) || 4 != *(ipbyte + 3) ||
		255 != *maskbyte || 0 != *(maskbyte + 1) || 0 != *(maskbyte + 2) || 0 != *(maskbyte + 3)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_classless_mask() {
	e.emit_test("Is normal input parsed correctly when one octet of the mask is partially filled?");
	char* inputstring = strdup("192.168.3.4/255.255.252.0");
	e.emit_input_header();
	e.emit_param("STRING", inputstring);
	struct in_addr ip;
	unsigned char* ipbyte = (unsigned char*) &ip;
	struct in_addr mask;
	unsigned char* maskbyte = (unsigned char*) &mask;
	int result = is_valid_network(inputstring, &ip, &mask);
	free(inputstring);
	e.emit_output_expected_header();
	int expected_return = TRUE;
	e.emit_retval("%s", tfstr(expected_return));
	e.emit_param("IP", "192.168.3.4");
	e.emit_param("MASK", "255.255.252.0");
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	e.emit_param("IP", "%d.%d.%d.%d", *ipbyte, *(ipbyte + 1), *(ipbyte + 2), *(ipbyte + 3));
	e.emit_param("MASK", "%d.%d.%d.%d", *maskbyte, *(maskbyte + 1), *(maskbyte + 2), *(maskbyte + 3));
	if( expected_return != result ||
		192 != *ipbyte || 168 != *(ipbyte + 1) || 3 != *(ipbyte + 2) || 4 != *(ipbyte + 3) ||
		255 != *maskbyte || 255 != *(maskbyte + 1) || 252 != *(maskbyte + 2) || 0 != *(maskbyte + 3)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_wildcard() {
	e.emit_test("Is wildcard parsed correctly?");
	char* inputstring = strdup("192.168.*");
	e.emit_input_header();
	e.emit_param("STRING", inputstring);
	struct in_addr ip;
	unsigned char* ipbyte = (unsigned char*) &ip;
	struct in_addr mask;
	unsigned char* maskbyte = (unsigned char*) &mask;
	int result = is_valid_network(inputstring, &ip, &mask);
	free(inputstring);
	e.emit_output_expected_header();
	int expected_return = TRUE;
	e.emit_retval("%s", tfstr(expected_return));
	e.emit_param("IP", "192.168.255.255");
	e.emit_param("MASK", "255.255.0.0");
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	e.emit_param("IP", "%d.%d.%d.%d", *ipbyte, *(ipbyte + 1), *(ipbyte + 2), *(ipbyte + 3));
	e.emit_param("MASK", "%d.%d.%d.%d", *maskbyte, *(maskbyte + 1), *(maskbyte + 2), *(maskbyte + 3));
	if( expected_return != result ||
		192 != *ipbyte || 168 != *(ipbyte + 1) || 255 != *(ipbyte + 2) || 255 != *(ipbyte + 3) ||
		255 != *maskbyte || 255 != *(maskbyte + 1) || 0 != *(maskbyte + 2) || 0 != *(maskbyte + 3)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_plain_ip() {
	e.emit_test("Is a plain IP parsed correctly?");
	char* inputstring = strdup("192.168.4.3");
	e.emit_input_header();
	e.emit_param("STRING", inputstring);
	struct in_addr ip;
	unsigned char* ipbyte = (unsigned char*) &ip;
	struct in_addr mask;
	unsigned char* maskbyte = (unsigned char*) &mask;
	int result = is_valid_network(inputstring, &ip, &mask);
	free(inputstring);
	e.emit_output_expected_header();
	int expected_return = TRUE;
	e.emit_retval("%s", tfstr(expected_return));
	e.emit_param("IP", "192.168.4.3");
	e.emit_param("MASK", "255.255.255.255");
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	e.emit_param("IP", "%d.%d.%d.%d", *ipbyte, *(ipbyte + 1), *(ipbyte + 2), *(ipbyte + 3));
	e.emit_param("MASK", "%d.%d.%d.%d", *maskbyte, *(maskbyte + 1), *(maskbyte + 2), *(maskbyte + 3));
	if( expected_return != result ||
		192 != *ipbyte || 168 != *(ipbyte + 1) || 4 != *(ipbyte + 2) || 3 != *(ipbyte + 3) ||
		255 != *maskbyte || 255 != *(maskbyte + 1) || 255 != *(maskbyte + 2) || 255 != *(maskbyte + 3)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}
