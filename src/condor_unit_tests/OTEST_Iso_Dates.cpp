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
	This code tests the functions within condor_c++_util/iso_dates.h.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "iso_dates.h"

static bool test_basic_date(void);
static bool test_extended_date(void);
static bool test_basic_time(void);
static bool test_extended_time(void);
static bool test_basic_date_time(void);
static bool test_extended_date_time(void);
static bool test_empty(void);
static bool test_year_only(void);
static bool test_char(void);
static bool test_time(void);
static bool test_time_char(void);

//Global variables
static time_t  current_time;
static struct tm  broken_down_time, parsed_time;
bool is_utc;
char time_string[ISO8601_DateAndTimeBufferMax*2];
unsigned long sub_second;
long microsec;
static std::string iso8601_str, time_str;

bool OTEST_Iso_Dates(void) {
	emit_object("Iso Dates");
	emit_comment("This tests time_to_iso8601() and iso8601_to_time.");
	emit_comment("Note that these tests are not exhaustive, but only test "
		"what was tested in condor_c++_util/test_condor_arglist.cpp.");
	
	time(&current_time);
	broken_down_time = *(localtime(&current_time));

		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_basic_date);
	driver.register_function(test_extended_date);
	driver.register_function(test_basic_time);
	driver.register_function(test_extended_time);
	driver.register_function(test_basic_date_time);
	driver.register_function(test_extended_date_time);
	driver.register_function(test_empty);
	driver.register_function(test_year_only);
	driver.register_function(test_char);
	driver.register_function(test_time);
	driver.register_function(test_time_char);

		// run the tests
	return driver.do_all_functions();
}

static bool test_basic_date() {
	emit_test("Test that iso8601_to_time() correctly parses the string returned"
		" from time_to_iso8601() on a basic date tm struct.");
	time_to_iso8601(time_string, broken_down_time,
								  ISO8601_BasicFormat,
								  ISO8601_DateOnly, 
								  false);
	iso8601_to_time(time_string, &parsed_time, &microsec, &is_utc);
	get_tm(ISO8601_DateOnly, broken_down_time, iso8601_str);
	get_tm(ISO8601_DateOnly, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", time_string);
	emit_param("ISO8601Type", "ISO8601_DateOnly");
	emit_output_expected_header();
	emit_param("Broken down time", time_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	emit_output_actual_header();
	emit_param("Broken down time", "%s", iso8601_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	if(iso8601_str != time_str) {
		FAIL;
	}
	PASS;
}

static bool test_extended_date() {
	emit_test("Test that iso8601_to_time() correctly parses the string returned"
		" from time_to_iso8601() on an extended date tm struct.");
	time_to_iso8601(time_string, broken_down_time,
								  ISO8601_ExtendedFormat,
								  ISO8601_DateOnly, 
								  false);
	iso8601_to_time(time_string, &parsed_time, NULL, &is_utc);
	get_tm(ISO8601_DateOnly, broken_down_time, iso8601_str);
	get_tm(ISO8601_DateOnly, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", time_string);
	emit_param("ISO8601Type", "ISO8601_DateOnly");
	emit_output_expected_header();
	emit_param("Broken down time", time_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	emit_output_actual_header();
	emit_param("Broken down time", "%s", iso8601_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	if(iso8601_str != time_str) {
		FAIL;
	}
	PASS;
}

static bool test_basic_time() {
	emit_test("Test that iso8601_to_time() correctly parses the string returned"
		" from time_to_iso8601() on a basic time tm struct.");
	time_to_iso8601(time_string, broken_down_time,
								  ISO8601_BasicFormat,
								  ISO8601_TimeOnly, 
								  false);
	iso8601_to_time(time_string, &parsed_time, &microsec, &is_utc);
	get_tm(ISO8601_TimeOnly, broken_down_time, iso8601_str);
	get_tm(ISO8601_TimeOnly, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", time_string);
	emit_param("ISO8601Type", "ISO8601_TimeOnly");
	emit_output_expected_header();
	emit_param("Broken down time", "%s", time_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	emit_output_actual_header();
	emit_param("Broken down time", "%s", iso8601_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	if(iso8601_str != time_str) {
		FAIL;
	}
	PASS;
}

static bool test_extended_time() {
	emit_test("Test that iso8601_to_time() correctly parses the string returned"
		" from time_to_iso8601() on an extended date tm struct.");
	time_to_iso8601(time_string, broken_down_time,
								  ISO8601_ExtendedFormat,
								  ISO8601_TimeOnly, 
								  false);
	iso8601_to_time(time_string, &parsed_time, NULL, &is_utc);
	get_tm(ISO8601_TimeOnly, broken_down_time, iso8601_str);
	get_tm(ISO8601_TimeOnly, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", time_string);
	emit_param("ISO8601Type", "ISO8601_TimeOnly");
	emit_output_expected_header();
	emit_param("Broken down time", time_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	emit_output_actual_header();
	emit_param("Broken down time", "%s", iso8601_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	if(iso8601_str != time_str) {
		FAIL;
	}
	PASS;
}

static bool test_basic_date_time() {
	emit_test("Test that iso8601_to_time() correctly parses the string returned"
		" from time_to_iso8601() on a basic date/time tm struct.");
	time_to_iso8601(time_string, broken_down_time,
								  ISO8601_BasicFormat,
								  ISO8601_DateAndTime, 
								  false);
	iso8601_to_time(time_string, &parsed_time, &microsec, &is_utc);
	get_tm(ISO8601_DateAndTime, broken_down_time, iso8601_str);
	get_tm(ISO8601_DateAndTime, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", time_string);
	emit_param("ISO8601Type", "ISO8601_DateAndTime");
	emit_output_expected_header();
	emit_param("Broken down time", time_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	emit_output_actual_header();
	emit_param("Broken down time", "%s", iso8601_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	if(iso8601_str != time_str) {
		FAIL;
	}
	PASS;
}

static bool test_extended_date_time() {
	emit_test("Test that iso8601_to_time() correctly parses the string returned"
		" from time_to_iso8601() on an extended date/time tm struct.");
	time_to_iso8601(time_string, broken_down_time,
								  ISO8601_ExtendedFormat,
								  ISO8601_DateAndTime, 
								  false);
	iso8601_to_time(time_string, &parsed_time, NULL, &is_utc);
	get_tm(ISO8601_DateAndTime, broken_down_time, iso8601_str);
	get_tm(ISO8601_DateAndTime, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", time_string);
	emit_param("ISO8601Type", "ISO8601_DateAndTime");
	emit_output_expected_header();
	emit_param("Broken down time", time_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	emit_output_actual_header();
	emit_param("Broken down time", "%s", iso8601_str.c_str());
	emit_param("Parsed time", "%s", time_str.c_str());
	if(iso8601_str != time_str) {
		FAIL;
	}
	PASS;
}

static bool test_empty() {
	emit_test("Test that iso8601_to_time() correctly parses the empty string.");
	std::string expect("-1--1--1T-1:-1:-1");
	iso8601_to_time("", &parsed_time, NULL, &is_utc);
	get_tm(ISO8601_DateAndTime, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", "");
	emit_param("ISO8601Type", "ISO8601_DateAndTime");
	emit_output_expected_header();
	emit_param("Parsed time", "%s", expect.c_str());
	emit_output_actual_header();
	emit_param("Parsed time", "%s", time_str.c_str());
	if(time_str != expect) {
		FAIL;
	}
	PASS;
}

static bool test_year_only() {
	emit_test("Test that iso8601_to_time() correctly parses a string that only "
		"contains the year.");
	std::string expect("110--1--1T-1:-1:-1");
	iso8601_to_time("2010", &parsed_time, &microsec, &is_utc);
	get_tm(ISO8601_DateAndTime, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", "2010");
	emit_param("ISO8601Type", "ISO8601_DateAndTime");
	emit_output_expected_header();
	emit_param("Parsed time", "%s", expect.c_str());
	emit_output_actual_header();
	emit_param("Parsed time", "%s", time_str.c_str());
	if(time_str != expect) {
		FAIL;
	}
	PASS;
}

static bool test_char() {
	emit_test("Test that iso8601_to_time() correctly parses a string that only "
		"contains the character 'T'.");
	std::string expect("-1--1--1T-1:-1:-1");
	iso8601_to_time("T", &parsed_time, NULL, &is_utc);
	get_tm(ISO8601_DateAndTime, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", "T");
	emit_param("ISO8601Type", "ISO8601_DateAndTime");
	emit_output_expected_header();
	emit_param("Parsed time", "%s", expect.c_str());
	emit_output_actual_header();
	emit_param("Parsed time", "%s", time_str.c_str());
	if(time_str != expect) {
		FAIL;
	}
	PASS;
}

static bool test_time() {
	emit_test("Test that iso8601_to_time() correctly parses a string that only "
		"contains a time.");
	std::string expect("-1--1--1T1:2:3");
	iso8601_to_time("01:02:03", &parsed_time, &microsec, &is_utc);
	get_tm(ISO8601_DateAndTime, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", "01:02:03");
	emit_param("ISO8601Type", "ISO8601_DateAndTime");
	emit_output_expected_header();
	emit_param("Parsed time", "%s", expect.c_str());
	emit_output_actual_header();
	emit_param("Parsed time", "%s", time_str.c_str());
	if(time_str != expect) {
		FAIL;
	}
	PASS;
}

static bool test_time_char() {
	emit_test("Test that iso8601_to_time() correctly parses a string that only "
		"contains a time with a 'T' preceding it.");
	std::string expect("-1--1--1T1:2:3");
	iso8601_to_time("T01:02:03", &parsed_time, &microsec, &is_utc);
	get_tm(ISO8601_DateAndTime, parsed_time, time_str);
	emit_input_header();
	emit_param("Time String", "%s", "T01:02:03");
	emit_param("ISO8601Type", "ISO8601_DateAndTime");
	emit_output_expected_header();
	emit_param("Parsed time", "%s", expect.c_str());
	emit_output_actual_header();
	emit_param("Parsed time", "%s", time_str.c_str());
	if(time_str != expect) {
		FAIL;
	}
	PASS;
}
