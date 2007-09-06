/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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

bool FTEST_is_valid_network(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("int is_valid_network(char* network, struct in_addr* ip, struct in_addr* mask)");
	e.emit_comment("Returns true if the given string is a valid network.");
	e.emit_problem("Allows the use of wildcards in addresses and netmasks.");
	e.emit_problem("Doesn't check to see if the netmask is a valid netmask, if it sees something like an IP address it assumes everything works.");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(&test_normal_case);
	driver.register_function(&test_slash_notation);
	driver.register_function(&test_classless_mask);
	
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
