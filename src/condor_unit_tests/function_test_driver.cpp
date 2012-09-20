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

static int num_tests_to_run;
static int num_tests_run;

FunctionDriver::FunctionDriver(int num_tests) {
	num_funcs_or_objs = num_tests;
}

FunctionDriver::~FunctionDriver() {
}

void FunctionDriver::init(int num_tests) {
	num_tests_to_run = num_tests;
	num_tests_run = 0;
}

void FunctionDriver::register_function(test_func_ptr function) {

	pointers.push_back(function);
}

bool FunctionDriver::do_all_functions(bool increment_tests_run) {
	bool test_passed = true;

	int i;
	std::list<test_func_ptr>::iterator itr;
	for ( itr = pointers.begin(), i = 0; itr != pointers.end() && 
		i <	num_funcs_or_objs && num_tests_run < num_tests_to_run; itr++, i++ ) 
	{
		if ( !(**itr)() ) {
			test_passed = false;
		}
		if(increment_tests_run)
			num_tests_run++;
	}

	return test_passed;
}


