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
	The function driver code for unit_tests.
 */

#include "function_test_driver.h"

FunctionDriver::FunctionDriver(int size) {
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
