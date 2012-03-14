/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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

/* Test condor_sockaddr's functoinality */
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_sockaddr.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"


static bool sockaddr_equivalance(const condor_sockaddr & s,
	MyString expected_ip_string, int expected_port) {

	int port = s.get_port();
	MyString ip_string = s.to_ip_string();

	emit_output_expected_header();
	emit_param("get_port()", "%d", expected_port);
	emit_param("to_ip_string()", "%s", expected_ip_string.Value());
	emit_output_actual_header();
	emit_param("get_port()", "%d", port);
	emit_param("to_ip_string()", "%s", ip_string.Value());

	return (port == expected_port) && (ip_string == expected_ip_string);
}

static bool test_from_sinful_simple_v4() {
	const char * sinful = "<1.2.3.4:567>";
	const int expected_port = 567;
	const MyString expected_ip_string = "1.2.3.4";

	emit_test("Can parse simple IPv4 sinful string");
	emit_input_header();
	emit_param("sinful", "%s", sinful);

	condor_sockaddr s;
	bool parsed = s.from_sinful(sinful);

	emit_output_expected_header();
	emit_param("Was able to parse?", "%s", "yes");
	emit_output_actual_header();
	emit_param("Was able to parse?", "%s", parsed?"yes":"NO");
	if(!parsed) FAIL;

	bool match = sockaddr_equivalance(s, expected_ip_string, expected_port);
	if(!match) FAIL;
	PASS;
}

bool OTEST_condor_sockaddr() {
	emit_object("condor_sockaddr");
	emit_comment("condor_sockaddr provides a single wrapper around sockaddr_in and sockaddr_in6, along with a host of functions for creating sockaddrs from other forms (notably strings) and converting back to those other forms.");

	FunctionDriver driver;
	driver.register_function(test_from_sinful_simple_v4);
	return driver.do_all_functions();
}
