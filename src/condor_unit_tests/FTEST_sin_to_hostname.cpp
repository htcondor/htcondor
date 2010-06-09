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
#include "unit_test_utils.h"
#include "emit.h"

static bool test_normal_case(void);

bool FTEST_sin_to_hostname(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("sin_to_hostname(const char*, char*** aliases)");
	e.emit_comment("Converts a sinful string to a hostname.");
	e.emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_normal_case);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool test_normal_case() {
	e.emit_test("Is normal input converted correctly?");
	struct sockaddr_in ip;
	char* host_to_test = strdup( "www.google.com" );
		// it's okay that host_to_test will be freed when we use aliases,
		// because it just matters that this isn't null
	char** aliases = &host_to_test;
	struct hostent *h;
	h = gethostbyname(host_to_test);
	ip.sin_addr = *((in_addr*) h->h_addr);
	e.emit_input_header();
	e.emit_param("IP", inet_ntoa(ip.sin_addr));
	e.emit_output_expected_header();
	char expected[30];
	sprintf(expected, h->h_name);
	free(host_to_test);
	e.emit_retval(expected);
	e.emit_output_actual_header();
	char* hostname = sin_to_hostname(&ip, &aliases);
	e.emit_retval(hostname);
	if(*aliases == NULL) {
		dprintf(D_ALWAYS, "I am a fish!\n");
	}
	if(strcmp(&expected[0], hostname) != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}
