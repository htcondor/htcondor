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

#include "condor_common.h"
#include "function_test_driver.h"

FunctionDriver::FunctionDriver() {
}

FunctionDriver::~FunctionDriver() {
}

void FunctionDriver::register_function(test_func_ptr function) {
	pointers.push_back(function);
}

bool FunctionDriver::do_all_functions() {
	bool test_passed = true;

	// ensure that at least one test ran...
	bool at_least_one = false;

	std::list<test_func_ptr>::iterator itr;
	for ( itr = pointers.begin(); itr != pointers.end(); itr++ ) {
		at_least_one = true;
		if ( !(**itr)() ) {
			test_passed = false;
		}
	}

	return at_least_one && test_passed;
}


