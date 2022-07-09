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
	This is unit_test code to print out well-formatted text.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "emit.h"

Emitter::Emitter() {
	function_tests = 0;
	object_tests = 0;
	passed_tests = 0;
	failed_tests = 0;
	aborted_tests = 0;
	skipped_tests = 0;
	cur_test_step_failures = 0;
}

Emitter::~Emitter() {
}

/* Initializes the Emitter object.
 */
void Emitter::init(bool failures_printed, bool successes_printed) {
	print_failures = failures_printed;
	print_successes = successes_printed;
	dprintf_set_tool_debug("TOOL", 0);
	set_debug_flags(NULL, D_ALWAYS | D_NOHEADER);
	config();
	global_start = time(0);
}

/* Formats and prints a parameter and its value as a sub-point of input,
 * output_expected, or output_actual.  format can use printf formatting.
 */
void Emitter::emit_param(const char* pname, const char* format, va_list args) {
	formatstr_cat(buf, "    %s:  ",pname);
	vformatstr_cat(buf, format, args);
	formatstr_cat(buf, "\n");
	print_now_if_possible();
}

/* A version of emit_param() for return values. */
void Emitter::emit_retval(const char* format, va_list args) {
	formatstr_cat(buf, "    RETURN:  ");
	vformatstr_cat(buf, format, args);
	formatstr_cat(buf, "\n");
	print_now_if_possible();
}

/* Emits a heading and the function string.
 */
void Emitter::emit_function(const char* function) {
	formatstr(test_buf, "\n\n------------------------------------------------------"
		"--------------------------\nFUNCTION:  %s\n---------------------------"
		"-----------------------------------------------------\n", function);
	if(print_failures && print_successes) {
		dprintf(D_ALWAYS, "%s", test_buf.c_str());
		test_buf.erase();
	}
}

/* Emits a heading and the object string.
 */
void Emitter::emit_object(const char* object) {
	formatstr(test_buf, "\n------------------------------------------------------"
		"--------------------------\nOBJECT:  %s\n-----------------------------"
		"---------------------------------------------------\n", object);
	if(print_failures && print_successes) {
		dprintf(D_ALWAYS, "%s", test_buf.c_str());
		test_buf.erase();
	}
	object_tests++;
}


/* Prints a comment indicating what the function does.
 */
void Emitter::emit_comment(const char* comment) {
	formatstr_cat(buf, "COMMENT:  %s\n", comment);
	print_now_if_possible();
}

/* Prints a known problem of the function.
 */
void Emitter::emit_problem(const char* problem) {
	formatstr_cat(buf, "KNOWN PROBLEM:  %s\n", problem);
	print_now_if_possible();
}

/* Reports a failure of a subtest, but continues on
 */
void Emitter::emit_step_failure(int line, const char * description) {
	formatstr_cat(buf, "FAILED %5d:  %s\n", line, description);
	++cur_test_step_failures;
	print_now_if_possible();
}

/* Shows exactly what is going to be tested.
 */
void Emitter::emit_test_at(const char* test, const char * file, int line) {
	if(!cur_test_name.empty()) {
		std::string s = "Starting new test \"";
		s += test;
		s += "\" but test \"";
		s += cur_test_name;
		s += "\" never finished!";
		emit_alert(s.c_str());
		emit_result_failure(0, "Unknown");
		cur_test_name.erase();
	}
	emit_test_break();
	function_tests++;
	if (file) {
		formatstr_cat(buf, "TEST:  %s begins at %s:%d\n", test, filename_from_path(file), line);
	} else {
		formatstr_cat(buf, "TEST:  %s\n", test);
	}
	print_now_if_possible();
	cur_test_step_failures = 0;
	cur_test_name = test;
	start = time(0);
}

/* A header saying that the function's input is going to follow.
 */
void Emitter::emit_input_header() {
	formatstr_cat(buf, "INPUT:\n");
	print_now_if_possible();
}

/* A header saying that the function's expected output is going to follow.
 */
void Emitter::emit_output_expected_header() {
	formatstr_cat(buf, "EXPECTED OUTPUT:\n");
	print_now_if_possible();
}

/* A header saying that the function's actual output is going to follow.
 */
void Emitter::emit_output_actual_header() {
	formatstr_cat(buf, "ACTUAL OUTPUT:\n");
	print_now_if_possible();
}

/* Prints out a message saying that the test succeeded. The function should
 * be called via the PASS macro."
 */
void Emitter::emit_result_success(int line, const char * file) {
	if( cur_test_name.empty()) {
		emit_alert("A test succeeded, but we're not in a test!");
	}
	formatstr_cat(buf, "RESULT:  SUCCESS, test passed at %s:%d (%ld seconds)\n",
		filename_from_path(file), line, time(0) - start);
	print_now_if_possible();
	if(print_successes && !print_failures) {
		if(!test_buf.empty()) {
			dprintf(D_ALWAYS, "%s", test_buf.c_str());
			test_buf.erase();
		}
		dprintf(D_ALWAYS, "%s", buf.c_str());
	}
	buf.erase();
	passed_tests++;
	cur_test_name.erase();
	cur_test_step_failures = 0;
}

/* Prints out a message saying that the test failed. The function should
 * be called via the PASS macro."
 */
void Emitter::emit_result_failure(int line, const char * file) {
	if( cur_test_name.empty()) {
		emit_alert("A test failed, but we're not in a test!");
	}
	formatstr_cat(buf, "RESULT:  FAILURE, test failed at %s:%d (%ld seconds)\n", 
		filename_from_path(file), line, time(0) - start);
	print_now_if_possible();
	print_result_failure();
	failed_tests++;
	cur_test_name.erase();
	cur_test_step_failures = 0;
}

int Emitter::emit_result(int line, const char * file) {
	int rval = cur_test_step_failures;
	if (cur_test_step_failures) {
		emit_result_failure(line, file);
	} else {
		emit_result_success(line, file);
	}
	return rval;
}

/* Prints out a message saying that the test was aborted for some unknown
 * reason, probably given by emit_abort() before this function call.  The
 * function should be called via the ABORT macro."
 */
void Emitter::emit_result_abort(int line, const char * file) {
	if( cur_test_name.empty()) {
		emit_alert("A test was aborted, but we're not in a test!");
	}
	formatstr_cat(buf, "RESULT:  ABORTED, test failed at %s:%d (%ld seconds)\n", 
		file, line, time(0) - start);
	print_now_if_possible();
	print_result_failure();
	aborted_tests++;
	cur_test_name.erase();
	cur_test_step_failures = 0;
}

/* Prints out a message saying that the test was skipped, for whatever reason.
 * Usually the testing function should return true after calling this.
 */
void Emitter::emit_skipped(const char* skipped) {
	formatstr_cat(buf, "SKIPPED:  %s", skipped);
	print_now_if_possible();
	print_result_failure();
	skipped_tests++;
}

/* Prints out an alert.  This could be a reason for why the test is being
 * aborted or just a warning that something happened but the test will continue
 * anyway.
 */
void Emitter::emit_alert(const char* alert) {
	formatstr_cat(buf, "ALERT:  %s\n", alert);
	print_now_if_possible();
}

/* Emits a break between two tests of the same function.
 */
void Emitter::emit_test_break() {
	formatstr_cat(buf, "\n");
	print_now_if_possible();
}

void Emitter::emit_summary() {
	dprintf(D_ALWAYS, "\n========\nSUMMARY:\n========\n");
	dprintf(D_ALWAYS, "    Total Tested Objects:  %d\n", object_tests);
	dprintf(D_ALWAYS, "    Total Unit Tests:      %d\n", function_tests);
	dprintf(D_ALWAYS, "    Passed Unit Tests:     %d\n", passed_tests);
	dprintf(D_ALWAYS, "    Failed Unit Tests:     %d\n", failed_tests);
	dprintf(D_ALWAYS, "    Aborted Unit Tests:    %d\n", aborted_tests);
	dprintf(D_ALWAYS, "    Skipped Unit Tests:    %d\n", skipped_tests);
	dprintf(D_ALWAYS, "    Total Time Taken:      %ld seconds\n",
			time(0) - global_start);
}

void Emitter::print_result_failure() {
	if(print_failures && !print_successes) {
		if(!test_buf.empty()) {
			dprintf(D_ALWAYS, "%s", test_buf.c_str());
			test_buf.erase();
		}
		dprintf(D_ALWAYS, "%s", buf.c_str());
	}
	buf.erase();
}

void Emitter::print_now_if_possible() {
	if(print_failures && print_successes) {
		dprintf(D_ALWAYS, "%s", buf.c_str());
		buf.erase();
	}
}

void init(bool failures_printed, bool successes_printed) {
	e.init(failures_printed, successes_printed);
}
	
void emit_param(const char* pname, const char* format, ...) {
	va_list args;
	va_start(args, format);
	e.emit_param(pname, format, args);
	va_end(args);
}
	
void emit_retval(const char* format, ...) {
	va_list args;
	va_start(args, format);
	e.emit_retval(format, args);
	va_end(args);
}

void emit_function(const char* function) {
	e.emit_function(function);
}

void emit_object(const char* object) {
	e.emit_object(object);
}

void emit_comment(const char* comment) {
	e.emit_comment(comment);
}

void emit_problem(const char* problem) {
	e.emit_problem(problem);
}

void emit_step_failure(int line, const char* description) {
	e.emit_step_failure(line, description);
}

void emit_test_at(const char* test, const char * file, int line) {
	e.emit_test_at(test, file, line);
}

void emit_skipped(const char* skipped) {
	e.emit_skipped(skipped);
}

void emit_input_header(void) {
	e.emit_input_header();
}

void emit_output_expected_header(void) {
	e.emit_output_expected_header();
}

void emit_output_actual_header(void) {
	e.emit_output_actual_header();
}

void emit_result_success(int line, const char * file) {
	e.emit_result_success(line, file);
}

void emit_result_failure(int line, const char * file) {
	e.emit_result_failure(line, file);
}

int emit_result(int line, const char * file) {
	return e.emit_result(line, file);
}

void emit_result_abort(int line, const char * file) {
	e.emit_result_abort(line, file);
}

void emit_alert(const char* alert) {
	e.emit_alert(alert);
}

void emit_test_break(void) {
	e.emit_test_break();
}

void emit_summary(void) {
	e.emit_summary();
}
