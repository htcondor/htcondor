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
static bool test_alpha_input(void);

bool FTEST_string_to_sin(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("string_to_sin(const char*, struct sockaddr_in*)");
	e.emit_comment("Converts a sinful string to a sockaddr_in.");
	e.emit_problem("Port numbers must be passed through htons as expected, but IP addresses should not be passed through htonl.");
	e.emit_problem("Function assumes a well-formatted string if it sees a : anywhere.");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	driver.register_function(test_alpha_input);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input converted correctly?");
	struct sockaddr_in sa_in;
	char* input = strdup("<192.168.0.2:80?param1=value1&param2=value2>");
	int result = string_to_sin(input, &sa_in);
	e.emit_input_header();
	e.emit_param("STRING", input);
	free(input);
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(TRUE));
	e.emit_param("sockaddr_in.in_addr", "192.168.0.2");
	e.emit_param("sockaddr_in.port", "80");
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	unsigned char* byte = (unsigned char*) &sa_in.sin_addr;
	int port = ntohs(sa_in.sin_port);
	e.emit_param("sockaddr_in.in_addr", "%d.%d.%d.%d", *byte, *(byte+1), *(byte+2), *(byte+3));
	e.emit_param("sockaddr_in.port", "%d", port);
	if(result != 1 || port != 80 || !utest_sock_eq_octet(&(sa_in.sin_addr), 192, 168, 0, 2) ){
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool test_alpha_input() {
	e.emit_test("Does an error occur on alpha-only input?");
	struct sockaddr_in sa_in;
	char* input = strdup("Iamafish");
	int result = string_to_sin(input, &sa_in);
	e.emit_input_header();
	e.emit_param("STRING", input);
	free(input);
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(FALSE));
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(result));
	if(result != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}
