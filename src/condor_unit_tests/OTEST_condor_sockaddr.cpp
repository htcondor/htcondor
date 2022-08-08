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
	std::string expected_ip_string, int expected_port) {

	int port = s.get_port();
	std::string ip_string = s.to_ip_string();

	emit_output_expected_header();
	emit_param("get_port()", "%d", expected_port);
	emit_param("to_ip_string()", "%s", expected_ip_string.c_str());
	emit_output_actual_header();
	emit_param("get_port()", "%d", port);
	emit_param("to_ip_string()", "%s", ip_string.c_str());

	return (port == expected_port) && (ip_string == expected_ip_string);
}

static bool test_from_sinful_simple_v4() {
	const char * sinful = "<1.2.3.4:567>";
	const int expected_port = 567;
	const char * expected_ip_string = "1.2.3.4";

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

static bool test_from_ip_string_simple_v4() {
	const char * in_ip_string = "86.75.30.9";
	const int expected_port = 0;

	emit_test("Can parse simple IPv4 string");
	emit_input_header();
	emit_param("ip_string", "%s", in_ip_string);

	condor_sockaddr s;
	bool parsed = s.from_ip_string(in_ip_string);

	emit_output_expected_header();
	emit_param("Was able to parse?", "%s", "yes");
	emit_output_actual_header();
	emit_param("Was able to parse?", "%s", parsed?"yes":"NO");
	if(!parsed) FAIL;

	bool match = sockaddr_equivalance(s, in_ip_string, expected_port);
	if(!match) FAIL;
	PASS;
}

static bool is_private_network(const char * ip_string, bool expect_private) {
	std::string header;
	formatstr(header, "Is %s a private network?", ip_string);
	emit_test(header.c_str());

	emit_input_header();
	emit_param("ip_string", "%s", ip_string);
	emit_output_expected_header();
	emit_param("Was able to parse?", "%s", "yes");
	emit_param("Is private?", "%s", expect_private?"yes":"no");

	condor_sockaddr s;
	bool parsed = s.from_ip_string(ip_string);
	bool is_private = s.is_private_network();

	emit_output_actual_header();
	emit_param("Was able to parse?", "%s", parsed?"yes":"NO");
	emit_param("Is private?", "%s", is_private?"yes":"no");

	if(!parsed) FAIL;
	if(is_private != expect_private) FAIL;
	PASS;
}

static bool test_private_network_192_167_255_255() { return is_private_network("192.167.255.255", false); }
static bool test_private_network_192_168_0_0() { return is_private_network("192.168.0.0", true); }
static bool test_private_network_192_168_1_0() { return is_private_network("192.168.1.0", true); }
static bool test_private_network_192_168_122_1() { return is_private_network("192.168.122.1", true); }
static bool test_private_network_192_168_255_255() { return is_private_network("192.168.255.255", true); }
static bool test_private_network_192_169_0_0() { return is_private_network("192.169.0.0", false); }


static bool test_private_network_172_15_255_255() { return is_private_network("172.15.255.255", false); }
static bool test_private_network_172_16_0_0() { return is_private_network("172.16.0.0", true); }
static bool test_private_network_172_31_255_255() { return is_private_network("172.31.255.255", true); }
static bool test_private_network_172_32_0_0() { return is_private_network("172.32.0.0", false); }

static bool test_private_network_9_255_255_255() { return is_private_network("9.255.255.255", false); }
static bool test_private_network_10_0_0_0() { return is_private_network("10.0.0.0", true); }
static bool test_private_network_10_0_0_1() { return is_private_network("10.0.0.1", true); }
static bool test_private_network_10_255_255_255() { return is_private_network("10.255.255.255", true); }
static bool test_private_network_11_0_0_0() { return is_private_network("11.0.0.0", false); }

// Test for getting it backward. 
static bool test_private_network_0_0_168_192() { return is_private_network("0.0.168.192", false); }
static bool test_private_network_255_255_168_192() { return is_private_network("255.255.168.192", false); }
static bool test_private_network_0_0_0_10() { return is_private_network("0.0.0.10", false); }
static bool test_private_network_1_0_0_10() { return is_private_network("1.0.0.10", false); }
static bool test_private_network_255_255_255_10() { return is_private_network("255.255.255.10", false); }


bool OTEST_condor_sockaddr() {
	emit_object("condor_sockaddr");
	emit_comment("condor_sockaddr provides a single wrapper around sockaddr_in and sockaddr_in6, along with a host of functions for creating sockaddrs from other forms (notably strings) and converting back to those other forms.");

	FunctionDriver driver;
	driver.register_function(test_from_sinful_simple_v4);
	driver.register_function(test_from_ip_string_simple_v4);

	driver.register_function(test_private_network_192_167_255_255);
	driver.register_function(test_private_network_192_168_0_0);
	driver.register_function(test_private_network_192_168_1_0);
	driver.register_function(test_private_network_192_168_122_1);
	driver.register_function(test_private_network_192_168_255_255);
	driver.register_function(test_private_network_192_169_0_0);

	driver.register_function(test_private_network_172_15_255_255);
	driver.register_function(test_private_network_172_16_0_0);
	driver.register_function(test_private_network_172_31_255_255);
	driver.register_function(test_private_network_172_32_0_0);

	driver.register_function(test_private_network_9_255_255_255);
	driver.register_function(test_private_network_10_0_0_0);
	driver.register_function(test_private_network_10_0_0_1);
	driver.register_function(test_private_network_10_255_255_255);
	driver.register_function(test_private_network_11_0_0_0);

	driver.register_function(test_private_network_0_0_168_192);
	driver.register_function(test_private_network_255_255_168_192);
	driver.register_function(test_private_network_0_0_0_10);
	driver.register_function(test_private_network_1_0_0_10);
	driver.register_function(test_private_network_255_255_255_10);

	return driver.do_all_functions();
}



