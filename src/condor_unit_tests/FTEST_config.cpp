/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include <limits>
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"


bool test_param_integer() {
    emit_test("Test param_integer()");
    clear_config();
    typedef int target_t;
    const target_t expect = std::numeric_limits<target_t>::max();
    const char* pname = "TEST1";
    char pval[64];
    sprintf(pval, "%lld", (long long)(expect));
    config_insert(pname, pval);
    target_t actual = param_integer(pname, 0);
	emit_output_expected_header();
	emit_retval("%lld", (long long)(expect));
	emit_output_actual_header();
	emit_retval("%lld", (long long)(actual));
    if (actual != expect) { FAIL; }
    PASS;
}

bool test_param_long() {
    emit_test("Test param_long()");
    clear_config();
    typedef long target_t;
    const target_t expect = std::numeric_limits<target_t>::max();
    const char* pname = "TEST1";
    char pval[64];
    sprintf(pval, "%lld", (long long)(expect));
    config_insert(pname, pval);
    target_t actual = param_long(pname, 0);
	emit_output_expected_header();
	emit_retval("%lld", (long long)(expect));
	emit_output_actual_header();
	emit_retval("%lld", (long long)(actual));
    if (actual != expect) { FAIL; }
    PASS;
}

bool test_param_long_long() {
    emit_test("Test param_long_long()");
    clear_config();
    typedef long long target_t;
    const target_t expect = std::numeric_limits<target_t>::max();
    const char* pname = "TEST1";
    char pval[64];
    sprintf(pval, "%lld", (long long)(expect));
    config_insert(pname, pval);
    target_t actual = param_long_long(pname, 0);
	emit_output_expected_header();
	emit_retval("%lld", (long long)(expect));
	emit_output_actual_header();
	emit_retval("%lld", (long long)(actual));
    if (actual != expect) { FAIL; }
    PASS;
}


bool test_param_off_t() {
    emit_test("Test param_off_t()");
    clear_config();
    typedef off_t target_t;
    const target_t expect = std::numeric_limits<target_t>::max();
    const char* pname = "TEST1";
    char pval[64];
    sprintf(pval, "%lld", (long long)(expect));
    config_insert(pname, pval);
    target_t actual = param_off_t(pname, 0);
	emit_output_expected_header();
	emit_retval("%lld", (long long)(expect));
	emit_output_actual_header();
	emit_retval("%lld", (long long)(actual));
    if (actual != expect) { FAIL; }
    PASS;
}


bool FTEST_config(void) {
	emit_function("FTEST_config");
	emit_comment("Functions from condor_config.h");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;

    driver.register_function(test_param_integer);
    driver.register_function(test_param_long);
    driver.register_function(test_param_long_long);
    driver.register_function(test_param_off_t);

    return driver.do_all_functions();
}
