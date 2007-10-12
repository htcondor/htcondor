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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "internet.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

static bool test_normal_case(void);
static bool test_hostname(void);

bool FTEST_getHostFromAddr(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("getHostFromAddr(sockaddr_in)");
	e.emit_comment("Returns the host portion of the sinful string");
	e.emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_hostname);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input parsed correctly?");
	char* inputstring = strdup("<192.168.0.2:496>");
	e.emit_input_header();
	e.emit_param("STRING", inputstring);
	char* sinstring = getHostFromAddr(inputstring);
	free(inputstring);
	e.emit_output_expected_header();
	char expected[30];
	sprintf(expected, "192.168.0.2");
	e.emit_retval(expected);
	e.emit_output_actual_header();
	e.emit_retval(sinstring);
	if(strcmp(expected, sinstring) != 0) {
		free(sinstring);
		e.emit_result_failure(__LINE__);
		return false;
	}
	free(sinstring);
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_hostname() {
	e.emit_test("Is input with a hostname parsed correctly?");
	char* inputstring = strdup("<balthazar.cs.wisc.edu:496>");
	e.emit_input_header();
	e.emit_param("STRING", inputstring);
	char* sinstring = getHostFromAddr(inputstring);
	free(inputstring);
	e.emit_output_expected_header();
	char expected[30];
	sprintf(expected, "balthazar.cs.wisc.edu");
	e.emit_retval(expected);
	e.emit_output_actual_header();
	e.emit_retval(sinstring);
	if(strcmp(expected, sinstring) != 0) {
		free(sinstring);
		e.emit_result_failure(__LINE__);
		return false;
	}
	free(sinstring);
	e.emit_result_success(__LINE__);
	return true;
}
