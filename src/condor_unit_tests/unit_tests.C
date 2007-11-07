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
#include "internet.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

	// prototypes for testing functions, each function here has its own file
bool FTEST_host_in_domain(void);
bool FTEST_getHostFromAddr(void);
bool FTEST_getPortFromAddr(void);
bool FTEST_is_ipaddr(void);
bool FTEST_is_valid_sinful(void);
bool FTEST_is_valid_network(void);
bool FTEST_sin_to_string(void);
bool FTEST_string_to_hostname(void);
bool FTEST_string_to_ip(void);
bool FTEST_string_to_sin(void);
bool FTEST_string_to_port(void);
bool OTEST_HashTable(void);

int main() {
	e.init();
		// set up the function driver
	FunctionDriver driver(15);
	driver.register_function(FTEST_host_in_domain);
	driver.register_function(FTEST_getHostFromAddr);
	driver.register_function(FTEST_getPortFromAddr);
	driver.register_function(FTEST_is_ipaddr);
	driver.register_function(FTEST_is_valid_sinful);
	driver.register_function(FTEST_is_valid_network);
	driver.register_function(FTEST_sin_to_string);
	driver.register_function(FTEST_string_to_hostname);
	driver.register_function(FTEST_string_to_ip);
	driver.register_function(FTEST_string_to_sin);
	driver.register_function(FTEST_string_to_port);
	driver.register_function(OTEST_HashTable);

		// run all the functions and return the result
	bool result = driver.do_all_functions();
	e.emit_summary();
	if(result) {
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}
