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
#include "condor_ipaddress.h"
#include "condor_ipv6.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"

static bool test_condor_inet_pton(void);
static bool test_condor_inet_pton_ipv4(void);

bool FTEST_ipv6(void) {
		// beginning junk for getPortFromAddr()
	emit_function("various ipv6 functions");
	emit_comment("Testing ipv6 functions");
	emit_problem("None");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_condor_inet_pton);
	driver.register_function(test_condor_inet_pton_ipv4);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	return test_result;
}

static bool test_condor_inet_pton() {
	emit_test("converting ipv6 address");
	char* input = "fe80::21e:4fff:fef0:90c7";
	ipaddr addr;
	int ret = condor_inet_pton(input, &addr);

	emit_input_header();
	emit_param("IP", input);
	emit_output_expected_header();
	emit_param("IP", input);
	emit_retval("%s", tfstr(1));
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret));
	if (!ret) {
		emit_result_failure(__LINE__);
		return false;
	}
	
	printf("Is IPV4 addr? %s\n", tfstr(addr.is_ipv4()));
	sockaddr_in6 sin6 = addr.to_sin6();
	char buf[256];
	const char* ptr = inet_ntop( AF_INET6, &sin6.sin6_addr, buf, 256);
	printf("org ip: %s\n", input);
	printf("converted again: %s\n", ptr);
	
	emit_result_success(__LINE__);
	return true;
}


static bool test_condor_inet_pton_ipv4(void) {
	emit_test("converting ipv4 address");
	char* input = "136.0.3.2";
	ipaddr addr;
	int ret = condor_inet_pton(input, &addr);
	
	emit_input_header();
	emit_param("IP", input);
	emit_output_expected_header();
	emit_param("IP", input);
	emit_retval("%s", tfstr(1));
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret));
	if (!ret) {
		emit_result_failure(__LINE__);
		return false;
	}
	
	printf("Is IPV4 addr? %s\n", tfstr(addr.is_ipv4()));
	{
		sockaddr_in6 sin6 = addr.to_sin6();
		char buf[256];
		const char* ptr = inet_ntop( AF_INET6, &sin6.sin6_addr, buf, 256);
		printf("ipv6 format: %s\n", ptr);
	}
	sockaddr_in sin = addr.to_sin();
	char buf[256];
	const char* ptr = inet_ntop( AF_INET, &sin.sin_addr, buf, 256);
	printf("org ip: %s\n", input);
	printf("converted again: %s\n", ptr);
	
	emit_result_success(__LINE__);
	return true;
}
