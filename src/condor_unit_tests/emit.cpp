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
#include "MyString.h"
#include "emit.h"

Emitter::Emitter() {
	function_tests = 0;
	object_tests = 0;
	passed_tests = 0;
	failed_tests = 0;
	aborted_tests = 0;
	skipped_tests = 0;
}

Emitter::~Emitter() {
	// placeholder function, for now
}

/* Initializes the Emitter object.
 */
void Emitter::init() {
	Termlog = 1;
	dprintf_config("TOOL");
	set_debug_flags("D_ALWAYS");
	set_debug_flags("D_NOHEADER");
	config();
}

/* Formats and prints a parameter and its value as a sub-point of input,
 * output_expected, or output_actual.  format can use printf formatting.
 */
void Emitter::emit_param(const char* pname, const char* format, ...) {
	va_list args;
	MyString tmp;
	va_start(args, format);
	tmp.sprintf("    %s:  %s\n",pname,format);
	::_condor_dprintf_va(D_ALWAYS, tmp.Value(), args);
	va_end( args );
}

/* A version of emit_param() for return values. */
void Emitter::emit_retval(const char* format, ...) {
	va_list args;
	MyString tmp;
	va_start(args, format);
	tmp.sprintf("    RETURN:  %s\n",format);
	::_condor_dprintf_va(D_ALWAYS, tmp.Value(), args);
	va_end( args );
}

/* Emits a heading and the function string.
 */
void Emitter::emit_function(const char* function) {
	dprintf(D_ALWAYS, "FUNCTION:  %s\n", function);
}

/* Emits a heading and the object string.
 */
void Emitter::emit_object(const char* object) {
	dprintf(D_ALWAYS, "OBJECT:  %s\n", object);
	object_tests++;
}


/* Prints a comment indicating what the function does.
 */
void Emitter::emit_comment(const char* comment) {
	dprintf(D_ALWAYS, "COMMENT:  %s\n", comment);
}

/* Prints a known problem of the function.
 */
void Emitter::emit_problem(const char* problem) {
	dprintf(D_ALWAYS, "KNOWN PROBLEM:  %s\n", problem);
}

/* Shows exactly what is going to be tested.
 */
void Emitter::emit_test(const char* test) {
	function_tests++;
	dprintf(D_ALWAYS, "TEST:  %s\n", test);
}

/* A header saying that the function's input is going to follow.
 */
void Emitter::emit_input_header() {
	dprintf(D_ALWAYS, "INPUT:\n");
}

/* A header saying that the function's expected output is going to follow.
 */
void Emitter::emit_output_expected_header() {
	dprintf(D_ALWAYS, "EXPECTED OUTPUT:\n");
}

/* A header saying that the function's actual output is going to follow.
 */
void Emitter::emit_output_actual_header() {
	dprintf(D_ALWAYS, "ACTUAL OUTPUT:\n");
}

/* Prints out a message saying that the test succeeded.  The function should
 * be called like "emit_result_success(__LINE__);"
 */
void Emitter::emit_result_success(int line) {
	dprintf(D_ALWAYS, "RESULT:  SUCCESS, test passed at line %d\n", line);
	passed_tests++;
	emit_test_break();
}

/* Prints out a message saying that the test failed.  The function should
 * be called like "emit_result_failure(__LINE__);"
 */
void Emitter::emit_result_failure(int line) {
	dprintf(D_ALWAYS, "RESULT:  FAILURE, test failed at line %d\n", line);
	failed_tests++;
	emit_test_break();
}

/* Prints out a message saying that the test was aborted for some unknown
 * reason, probably given by emit_abort() before this function call.  The
 * function should be called like "emit_result_abort(__LINE__);"
 */
void Emitter::emit_result_abort(int line) {
	dprintf(D_ALWAYS, "RESULT:  ABORTED, test failed at line %d\n", line);
	aborted_tests++;
	emit_test_break();
}

/* Prints out a message saying that the test was skipped, for whatever reason.
 * Usually the testing function should return true after calling this.
 */
void Emitter::emit_skipped(const char* skipped) {
	dprintf(D_ALWAYS, "SKIPPED:  %s", skipped);
	skipped_tests++;
	emit_test_break();
}

/* Prints out an alert.  This could be a reason for why the test is being
 * aborted or just a warning that something happened but the test will continue
 * anyway.
 */
void Emitter::emit_alert(const char* alert) {
	dprintf(D_ALWAYS, "ALERT:  %s\n", alert);
}

/* Emits a break between function tests.
 */
void Emitter::emit_function_break() {
	dprintf(D_ALWAYS, "---------------------\n");
}

/* Emits a break between two tests of the same function.
 */
void Emitter::emit_test_break() {
	dprintf(D_ALWAYS, "\n");
}

void Emitter::emit_summary() {
	dprintf(D_ALWAYS, "SUMMARY:\n");
	dprintf(D_ALWAYS, "========\n");
	dprintf(D_ALWAYS, "    Total Tested Objects:  %d\n", object_tests);
	dprintf(D_ALWAYS, "    Total Unit Tests:      %d\n", function_tests);
	dprintf(D_ALWAYS, "    Passed Unit Tests:     %d\n", passed_tests);
	dprintf(D_ALWAYS, "    Failed Unit Tests:     %d\n", failed_tests);
	dprintf(D_ALWAYS, "    Aborted Unit Tests:    %d\n", aborted_tests);
	dprintf(D_ALWAYS, "    Skipped Unit Tests:    %d\n", skipped_tests);
}
