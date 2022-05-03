/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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
	This code tests the functions within condor_utils/condor_timslice.h.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "condor_timeslice.h"

static bool test_always_runs_first_time(void);

//Global variables

bool OTEST_Timeslice(void) {
	emit_object("Timeslice");
	emit_comment("This tests the timeslice object");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_always_runs_first_time);
	
		// run the tests
	return driver.do_all_functions();
}

static bool test_always_runs_first_time() {
	emit_test("Test that timeslice object always runs when initialized");

	emit_input_header();
	time_t startTime = time(0);

	// This code use to fail when gettimeofday returns
	// tv_usec close to 0. Give it 20 seconds to try 
	// to hit this race condition
	while (time(0) - startTime < 20) {
		Timeslice ts;
		ts.setTimeslice(0.01);
        int avoid_time = 3600;
        ts.setMaxInterval(avoid_time);
        ts.setInitialInterval(0);
		if (!ts.isTimeToRun()) {
			FAIL;
		}
	}
	PASS;
}
