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

#ifndef EMIT_H
#define EMIT_H

#include "condor_common.h"

class Emitter {
private:
		// keep track of how many tests are going
	int function_tests;
	int object_tests;
	int passed_tests;
	int failed_tests;
	int aborted_tests;
	int skipped_tests;

public:
		// constructor and destructor
	Emitter();
	~Emitter();
	
	/* Initializes the Emitter object.
	 */
	void init(void);
	
	/* Formats and prints a parameter and its value as a sub-point of input,
	 * output_expected, or output_actual.  format can use printf formatting.
	 */
	void emit_param(const char* pname, const char* format, ...);
	
	/* A version of emit_param() for return values. */
	void emit_retval(const char* format, ...);

	/* Emits a heading and the function string.
	 */
	void emit_function(const char* function);

	/* Emits a heading and the object string.
	 */
	void emit_object(const char* object);

	/* Prints a comment indicating what the function does.
	 */
	void emit_comment(const char* comment);

	/* Prints a known problem of the function.
	 */
	void emit_problem(const char* problem);

	/* Shows exactly what is going to be tested.
	 */
	void emit_test(const char* test);

	/* Shows exactly what is going to be tested.
	 */
	void emit_skipped(const char* skipped);

	/* A header saying that the function's input is going to follow.
	 */
	void emit_input_header(void);

	/* A header saying that the function's expected output is going to follow.
	 */
	void emit_output_expected_header(void);

	/* A header saying that the function's actual output is going to follow.
	 */
	void emit_output_actual_header(void);

	/* Prints out a message saying that the test succeeded.  The function should
	 * be called like "emit_result_success(__LINE__);"
	 */
	void emit_result_success(int line);

	/* Prints out a message saying that the test failed.  The function should
	 * be called like "emit_result_failure(__LINE__);"
	 */
	void emit_result_failure(int line);

	/* Prints out a message saying that the test was aborted for some unknown
	 * reason, probably given by emit_abort() before this function call.  The
	 * function should be called like "emit_result_abort(__LINE__);"
	 */
	void emit_result_abort(int line);

	/* Prints out an alert.  This could be a reason for why the test is being
	 * aborted or just a warning that something happened but the test will continue
	 * anyway.
	 */
	void emit_alert(const char* alert);

	/* Emits a break between function tests.
	 */
	void emit_function_break(void);

	/* Emits a line break.  This is taken care of between tests, so you probably
	 *  won't need this.
	 */
	void emit_test_break(void);

	/* Emits a summary of the unit tests thus far.  Will probably only have to
	 * only be called by the unit_tests.C main() function, so you probably
	 * won't have to worry about this.
	 */
	void emit_summary(void);
};
#endif
