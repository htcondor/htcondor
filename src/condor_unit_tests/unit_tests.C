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
