/***************************************************************
 *
 * Copyright (C) 2026, Condor Team, Computer Sciences Department,
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
	This code tests UpdateWastedMemoryAttrs() (HTCONDOR-4062), which sets
	CurrentWastedMemoryGBHours and LastWastedMemoryGBHours in a job ad.
	The end-to-end behavior is tested by test_wasted_memory.py; this tests
	the cases that are hard to produce with real jobs.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "wasted_memory.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"

#include <cmath>

static bool test_running(void);
static bool test_activation_ended(void);
static bool test_clamped_to_zero(void);
static bool test_real_valued_inputs(void);
static bool test_memory_usage_expression(void);
static bool test_no_request_memory_fallback(void);
static bool test_unknown_input_invalidates_current(void);
static bool test_unknown_input_at_end_invalidates_last(void);
static bool test_unknown_input_never_set(void);
static bool test_negative_duration(void);

bool FTEST_wasted_memory(void) {
	emit_function("bool UpdateWastedMemoryAttrs(classad::ClassAd &job_ad, "
	              "time_t activation_start, time_t now, bool activation_ended)");
	emit_comment("Set CurrentWastedMemoryGBHours, and when the activation ends "
	             "LastWastedMemoryGBHours, from ActivationRequestMemory, "
	             "MemoryUsage, and the activation's duration.");

	FunctionDriver driver;
	driver.register_function(test_running);
	driver.register_function(test_activation_ended);
	driver.register_function(test_clamped_to_zero);
	driver.register_function(test_real_valued_inputs);
	driver.register_function(test_memory_usage_expression);
	driver.register_function(test_no_request_memory_fallback);
	driver.register_function(test_unknown_input_invalidates_current);
	driver.register_function(test_unknown_input_at_end_invalidates_last);
	driver.register_function(test_unknown_input_never_set);
	driver.register_function(test_negative_duration);

	return driver.do_all_functions();
}

	// Requesting 512 MiB and using 128 MiB wastes 384 MiB = 0.375 GiB, so
	// an activation of an hour wastes 0.375 GB-hours.
static const time_t START = 1000;
static const time_t ONE_HOUR_LATER = START + 3600;
static const double EXPECTED = 0.375;

static void
make_ad(ClassAd &ad, double request_memory = 512, double memory_usage = 128)
{
	ad.Assign(ATTR_JOB_ACTIVATION_REQUEST_MEMORY, request_memory);
	ad.Assign(ATTR_MEMORY_USAGE, memory_usage);
}

static bool
close_to(double actual, double expected)
{
	return std::fabs(actual - expected) <= 1e-9;
}

	// Is attr in ad, with the given numeric value?
static bool
has_value(ClassAd &ad, const char *attr, double expected)
{
	double actual = 0;
	return ad.LookupFloat(attr, actual) && close_to(actual, expected);
}

	// Is attr in ad, and undefined?
static bool
is_undefined(ClassAd &ad, const char *attr)
{
	classad::Value val;
	return ad.Lookup(attr) && ad.EvaluateAttr(attr, val) && val.IsUndefinedValue();
}

static void
emit_ad(ClassAd &ad)
{
	std::string text;
	sPrintAd(text, ad);
	emit_param("AD", "%s", text.c_str());
}

static bool test_running() {
	emit_test("While the activation runs, is Current set, and Last not?");
	ClassAd ad;
	make_ad(ad);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, false);
	emit_output_expected_header();
	emit_retval("true");
	emit_param(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, "%f", EXPECTED);
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if ( ! rval ||
		! has_value(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, EXPECTED) ||
		ad.Lookup(ATTR_LAST_WASTED_MEMORY_GB_HOURS)) {
		FAIL;
	}
	PASS;
}

static bool test_activation_ended() {
	emit_test("When the activation ends, is Last set to Current's value, "
	          "replacing the previous activation's?");
	ClassAd ad;
	make_ad(ad);
	ad.Assign(ATTR_LAST_WASTED_MEMORY_GB_HOURS, 99.0);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, true);
	emit_output_expected_header();
	emit_retval("true");
	emit_param(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, "%f", EXPECTED);
	emit_param(ATTR_LAST_WASTED_MEMORY_GB_HOURS, "%f", EXPECTED);
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if ( ! rval ||
		! has_value(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, EXPECTED) ||
		! has_value(ad, ATTR_LAST_WASTED_MEMORY_GB_HOURS, EXPECTED)) {
		FAIL;
	}
	PASS;
}

static bool test_clamped_to_zero() {
	emit_test("If the job used more memory than it requested, is the value zero?");
	ClassAd ad;
	make_ad(ad, 128, 512);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, true);
	emit_output_expected_header();
	emit_retval("true");
	emit_param(ATTR_LAST_WASTED_MEMORY_GB_HOURS, "%f", 0.0);
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if ( ! rval ||
		! has_value(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, 0.0) ||
		! has_value(ad, ATTR_LAST_WASTED_MEMORY_GB_HOURS, 0.0)) {
		FAIL;
	}
	PASS;
}

static bool test_real_valued_inputs() {
	emit_test("Are real-valued ActivationRequestMemory and MemoryUsage used "
	          "without truncation?");
	ClassAd ad;
	make_ad(ad, 512.5, 128.5);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, false);
	emit_output_expected_header();
	emit_retval("true");
	emit_param(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, "%f", EXPECTED);
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if ( ! rval || ! has_value(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, EXPECTED)) {
		FAIL;
	}
	PASS;
}

static bool test_memory_usage_expression() {
	emit_test("Is MemoryUsage evaluated when it's an expression "
	          "(as the starter sends it by default)?");
	ClassAd ad;
	make_ad(ad);
	ad.AssignExpr(ATTR_MEMORY_USAGE, "((ResidentSetSize+1023)/1024)");
	ad.Assign(ATTR_RESIDENT_SET_SIZE, 128 * 1024);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, false);
	emit_output_expected_header();
	emit_retval("true");
	emit_param(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, "%f", EXPECTED);
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if ( ! rval || ! has_value(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, EXPECTED)) {
		FAIL;
	}
	PASS;
}

static bool test_no_request_memory_fallback() {
	emit_test("Without ActivationRequestMemory, is nothing set, even though "
	          "RequestMemory is?");
	ClassAd ad;
	ad.Assign(ATTR_REQUEST_MEMORY, 512);
	ad.Assign(ATTR_MEMORY_USAGE, 128);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, false);
	emit_output_expected_header();
	emit_retval("false");
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if (rval || ad.Lookup(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS) ||
		ad.Lookup(ATTR_LAST_WASTED_MEMORY_GB_HOURS)) {
		FAIL;
	}
	PASS;
}

static bool test_unknown_input_invalidates_current() {
	emit_test("If MemoryUsage becomes unknown after Current was set, does "
	          "Current become undefined (and Last stay unset)?");
	ClassAd ad;
	make_ad(ad);
	bool first = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, false);
	ad.Delete(ATTR_MEMORY_USAGE);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER + 60, false);
	emit_output_expected_header();
	emit_retval("false");
	emit_param(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, "undefined");
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if ( ! first || rval ||
		! is_undefined(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS) ||
		ad.Lookup(ATTR_LAST_WASTED_MEMORY_GB_HOURS)) {
		FAIL;
	}
	PASS;
}

static bool test_unknown_input_at_end_invalidates_last() {
	emit_test("If an input is unknown when the activation ends, do Current and "
	          "Last (from the previous activation) become undefined?");
	ClassAd ad;
	ad.Assign(ATTR_MEMORY_USAGE, 128);
	ad.Assign(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, 0.5);
	ad.Assign(ATTR_LAST_WASTED_MEMORY_GB_HOURS, 0.25);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, true);
	emit_output_expected_header();
	emit_retval("false");
	emit_param(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, "undefined");
	emit_param(ATTR_LAST_WASTED_MEMORY_GB_HOURS, "undefined");
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if (rval ||
		! is_undefined(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS) ||
		! is_undefined(ad, ATTR_LAST_WASTED_MEMORY_GB_HOURS)) {
		FAIL;
	}
	PASS;
}

static bool test_unknown_input_never_set() {
	emit_test("If an input is unknown and Current was never set, is Current "
	          "left unset (rather than set to undefined)?");
	ClassAd ad;
	ad.Assign(ATTR_JOB_ACTIVATION_REQUEST_MEMORY, 512);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, START, ONE_HOUR_LATER, false);
	emit_output_expected_header();
	emit_retval("false");
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if (rval || ad.Lookup(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS)) {
		FAIL;
	}
	PASS;
}

static bool test_negative_duration() {
	emit_test("If now is before the activation started (e.g. the clock went "
	          "backwards), is the value zero rather than negative?");
	ClassAd ad;
	make_ad(ad);
	emit_input_header();
	emit_ad(ad);
	bool rval = UpdateWastedMemoryAttrs(ad, ONE_HOUR_LATER, START, false);
	emit_output_expected_header();
	emit_retval("true");
	emit_param(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, "%f", 0.0);
	emit_output_actual_header();
	emit_retval("%s", tfstr(rval));
	emit_ad(ad);
	if ( ! rval || ! has_value(ad, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, 0.0)) {
		FAIL;
	}
	PASS;
}
