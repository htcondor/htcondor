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
	A header file for the function driver code for unit_tests.
 */
#ifndef _FUNCTION_TEST_DRIVER_H_
#define  _FUNCTION_TEST_DRIVER_H_

#include <list>

typedef bool (*test_func_ptr)(void);

class FunctionDriver {
public:
		// ctor/detor
	FunctionDriver(int num_tests = INT_MAX);
	~FunctionDriver();

	void init(int num_tests);
	
		// register a function with the driver
	void register_function(test_func_ptr);

		// perform all the functions.  Returns true if every function
		// called returns true, false otherwise.
	bool do_all_functions(bool increment_tests_run = true);
	
private:
	std::list<test_func_ptr> pointers;
	int num_funcs_or_objs;
};


#endif
