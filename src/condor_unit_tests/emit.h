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

// Why wrap these in do{}while? http://c-faq.com/cpp/multistmt.html
#define FAIL \
	do { \
		e.emit_result_failure(__LINE__,__FILE__); \
		return false; \
	} while(0)

#define PASS \
	do { \
		e.emit_result_success(__LINE__,__FILE__); \
		return true; \
	} while(0)

#define ABORT \
	do { \
		e.emit_result_abort(__LINE__,__FILE__); \
		return false; \
	} while(0)

#include "condor_common.h"

// Global emitter declaration
extern class Emitter e;

class Emitter {
private:
		// keep track of how many tests are going
	int function_tests;
	int object_tests;
	int passed_tests;
	int failed_tests;
	int aborted_tests;
	int skipped_tests;
	int cur_test_step_failures;

	bool print_failures;
	bool print_successes;

	std::string buf, test_buf;

	std::string cur_test_name;

	time_t start, global_start;

	void print_result_failure(void);
	
	void print_now_if_possible(void);
public:
		// constructor and destructor
	Emitter();
	~Emitter();
	
	/* Initializes the Emitter object.
	 */
	void init(bool failures_printed, bool successes_printed);
	
	/* Formats and prints a parameter and its value as a sub-point of input,
	 * output_expected, or output_actual.  format can use printf formatting.
	 */
	//void emit_param(const char* pname, const char* format, ...);
	void emit_param(const char* pname, const char* format, va_list args);
	
	/* A version of emit_param() for return values. */
	void emit_retval(const char* format, va_list args);

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

	/* Reports a failure of a subtest, but continues on
	 */
	void emit_step_failure(int line, const char * description);

	/* Shows exactly what is going to be tested.
	 */
	void emit_test_at(const char* test, const char *file, int line);
	
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
	 * be called via the PASS macro."
	 */
	void emit_result_success(int line, const char * file);

	/* Prints out a message saying that the test failed.  The function should
	 * be called via the FAIL macro."
	 */
	void emit_result_failure(int line, const char * file);

	/* Prints out a message saying that the test succeeded or failed based on whether
	 * or not the cur_test_stop_failure count is 0. Use only with tests that
	 * call emit_step_failure when a part of the test fails.
	 * returns number of step failures.
	 */
	int emit_result(int line, const char * file);

	/* Prints out a message saying that the test was aborted for some unknown
	 * reason, probably given by emit_abort() before this function call.  The
	 * function should be called via the ABORT macro."
	 */
	void emit_result_abort(int line, const char * file);

	/* Prints out an alert.  This could be a reason for why the test is being
	 * aborted or just a warning that something happened but the test will continue
	 * anyway.
	 */
	void emit_alert(const char* alert);

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

void init(bool failures_printed, bool successes_printed);
	
void emit_param(const char* pname, const char* format, ...);
	
void emit_retval(const char* format, ...);

void emit_function(const char* function);

void emit_object(const char* object);

void emit_comment(const char* comment);

void emit_problem(const char* problem);

void emit_step_failure(int line, const char* description);
#define REQUIRE(condition) if (!(condition)) { emit_step_failure(__LINE__, #condition ); }

void emit_test_at(const char* test, const char * file, int line);
#define emit_test(test) emit_test_at(test, __FILE__, __LINE__)

void emit_skipped(const char* skipped);

void emit_input_header(void);

void emit_output_expected_header(void);

void emit_output_actual_header(void);

void emit_result_success(int line, const char * file);

void emit_result_failure(int line, const char * file);

int emit_result(int line, const char * file);
#define REQUIRED_RESULT() emit_result(__LINE__, __FILE__) == 0

void emit_result_abort(int line, const char * file);

void emit_alert(const char* alert);

void emit_test_break(void);

void emit_summary(void);

#endif
