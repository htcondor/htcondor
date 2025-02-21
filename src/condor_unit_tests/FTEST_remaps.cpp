/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
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
	This code tests the fullpath() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "filename_tools.h"

static bool test_remaps_easy(void);
static bool test_remaps_empty(void);

bool FTEST_remaps(void) {
	emit_function("int filename_remap_find( const char *input, const char *filename, std::string &output, int cur_remap_level)");
	emit_comment("   test filename remapping functions");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_remaps_easy);
	driver.register_function(test_remaps_empty);
	
		// run the tests
	return driver.do_all_functions();
}

static bool test_remaps_easy() {
	emit_test("Does a basic remaps work");
	const char *remap = "src_file1=dst_file1;out=_condor_stdout;err=_condor_stderr";
	emit_input_header();
	emit_param("remap rules", remap);
	emit_param("first input", "src_file_1");
	emit_param("second input", "out");
	std::string output;
	bool pass = true;
	int result = filename_remap_find(remap, "src_file1", output);
	emit_retval("result was %d output file was %s\n", result, output.c_str());
	if ((result == 0) || (output != "dst_file1")) {
		pass = false;
	}
	output.clear();
	result = filename_remap_find(remap, "out", output);
	emit_retval("result was %d output file was %s\n", result, output.c_str());
	if ((result == 0) || (output != "_condor_stdout")) {
		pass = false;
	}
	emit_output_expected_header();
	emit_output_actual_header();
	if (pass) {
		PASS;
	} else {
		FAIL;
	}
}

static bool test_remaps_empty() {
	emit_test("Does remap with a empty entry work");
	const char *remap = "src_file1=dst_file1;;out=_condor_stdout;err=_condor_stderr";
	emit_input_header();
	emit_param("remap rules", remap);
	emit_param("first input", "src_file_1");
	emit_param("second input", "out");
	std::string output;
	bool pass = true;
	int result = filename_remap_find(remap, "src_file1", output);
	emit_retval("result was %d output file was %s\n", result, output.c_str());
	if ((result == 0) || (output != "dst_file1")) {
		pass = false;
	}
	output.clear();
	result = filename_remap_find(remap, "out", output);
	emit_retval("result was %d output file was %s\n", result, output.c_str());
	if ((result == 0) || (output != "_condor_stdout")) {
		pass = false;
	}
	output.clear();
	result = filename_remap_find(remap, "err", output);
	emit_retval("result was %d output file was %s\n", result, output.c_str());
	if ((result == 0) || (output != "_condor_stderr")) {
		pass = false;
	}
	emit_output_expected_header();
	emit_output_actual_header();
	if (pass) {
		PASS;
	} else {
		FAIL;
	}
}
