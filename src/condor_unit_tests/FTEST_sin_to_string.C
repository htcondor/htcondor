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
#include "unit_test_utils.h"
#include "emit.h"

static bool test_normal_case(void);

bool FTEST_sin_to_string(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("sin_to_string(sockaddr_in)");
	e.emit_comment("Converts a sockaddr_in to a sinful string.");
	e.emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(&test_normal_case);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input converted correctly?");
	in_addr ip;
	if(inet_aton("192.168.0.2", &ip) == 0) {
		e.emit_alert("inet_aton() returned failure.");
		e.emit_result_abort(__LINE__);
		return false;
	}
	unsigned int port = 47;
	e.emit_input_header();
	e.emit_param("IP", "192.168.0.2");
	e.emit_param("PORT", "47");
	e.emit_output_expected_header();
	char* expected = strdup("<192.168.0.2:47>");
	e.emit_retval(expected);
	e.emit_output_actual_header();
	sockaddr_in sa_in;
	sa_in.sin_addr.s_addr = ip.s_addr;
	sa_in.sin_port = htons(port);
	char* sinstring = sin_to_string(&sa_in);
	e.emit_retval(sinstring);
	if(strcmp(expected, sinstring) != 0) {
		free(expected);
		free(sinstring);
		e.emit_result_failure(__LINE__);
		return false;
	}
	free(expected);
	e.emit_result_success(__LINE__);
	return true;
}
