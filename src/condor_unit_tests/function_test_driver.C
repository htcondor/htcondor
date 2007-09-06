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
	The function driver code for unit_tests.
 */

#include "function_test_driver.h"

FunctionDriver::FunctionDriver(int size) {
		// sanity checking
	if(size < 1) return;

	pointers = new test_func_ptr[size];
	max_functions = size;
	used_functions = 0;
}

FunctionDriver::~FunctionDriver() {
	delete[] pointers;
}

void FunctionDriver::register_function(test_func_ptr function) {
	if(used_functions >= max_functions) {
		return;
	}
	pointers[used_functions] = function;
	used_functions++;
}

bool FunctionDriver::do_all_functions() {
	bool test_passed = true;
	int i;
	for(i = 0; i < used_functions; i++) {
		if(!(*pointers[i])()) {
			test_passed = false;
		}
	}
	return test_passed;
}
