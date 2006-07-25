/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "iso_dates.h"

static void check_match(ISO8601Type type, const struct tm &time1,
						const struct tm  &time2, int line);
static void dump_time(const struct tm &time);


int main(int argc, char **argv)
{
	time_t  current_time;
	struct tm  broken_down_time, parsed_time;
	char *time_string;
	bool is_utc;

	time(&current_time);
	broken_down_time = *(localtime(&current_time));

	// Basic Date
	time_string = time_to_iso8601(broken_down_time,
								  ISO8601_BasicFormat,
								  ISO8601_DateOnly, 
								  false);
	printf("%s\n", time_string);
	iso8601_to_time(time_string, &parsed_time, &is_utc);
	check_match(ISO8601_DateOnly, broken_down_time, parsed_time, __LINE__);


	// Extended Date
	time_string = time_to_iso8601(broken_down_time,
								  ISO8601_ExtendedFormat,
								  ISO8601_DateOnly, 
								  false);

	printf("%s\n", time_string);
	iso8601_to_time(time_string, &parsed_time, &is_utc);
	check_match(ISO8601_DateOnly, broken_down_time, parsed_time, __LINE__);

	// Basic Time
	time_string = time_to_iso8601(broken_down_time,
								  ISO8601_BasicFormat,
								  ISO8601_TimeOnly, 
								  false);

	printf("%s\n", time_string);
	iso8601_to_time(time_string, &parsed_time, &is_utc);
	check_match(ISO8601_TimeOnly, broken_down_time, parsed_time, __LINE__);

	// Extended Time
	time_string = time_to_iso8601(broken_down_time,
								  ISO8601_ExtendedFormat,
								  ISO8601_TimeOnly, 
								  false);

	printf("%s\n", time_string);
	iso8601_to_time(time_string, &parsed_time, &is_utc);
	check_match(ISO8601_TimeOnly, broken_down_time, parsed_time, __LINE__);

	// Basic Time
	time_string = time_to_iso8601(broken_down_time,
								  ISO8601_BasicFormat,
								  ISO8601_DateAndTime, 
								  false);

	printf("%s\n", time_string);
	iso8601_to_time(time_string, &parsed_time, &is_utc);
	check_match(ISO8601_DateAndTime, broken_down_time, parsed_time, __LINE__);

	// Extended Time
	time_string = time_to_iso8601(broken_down_time,
								  ISO8601_ExtendedFormat,
								  ISO8601_DateAndTime, 
								  false);

	printf("%s\n", time_string);
	iso8601_to_time(time_string, &parsed_time, &is_utc);
	check_match(ISO8601_DateAndTime, broken_down_time, parsed_time, __LINE__);

	// Throw some odd things at the parser
	iso8601_to_time("", &parsed_time, &is_utc);
	printf("This should be negatives: ");
	dump_time(parsed_time);

	iso8601_to_time("2002", &parsed_time, &is_utc);
	printf("This should be negatives except the year: ");
	dump_time(parsed_time);

	iso8601_to_time("T", &parsed_time, &is_utc);
	printf("This should be negatives: ");
	dump_time(parsed_time);

	iso8601_to_time("T01:02:03", &parsed_time, &is_utc);
	printf("This one should be 01:02:03 (no date): ");
	dump_time(parsed_time);

	iso8601_to_time("01:02:03", &parsed_time, &is_utc);
	printf("This one should also be 01:02:03 (no date): ");
	dump_time(parsed_time);

	return 0;
}

static void check_match(
	ISO8601Type type,
	const struct tm  &time1,
	const struct tm  &time2, 
	int line)
{
	bool  matches;

	if (type == ISO8601_DateOnly) {
		matches =  (time1.tm_year == time2.tm_year
					&& time1.tm_mon == time2.tm_mon
					&& time1.tm_mday == time2.tm_mday);
	} else if (type == ISO8601_TimeOnly) {
		matches = (time1.tm_hour == time2.tm_hour
				  && time1.tm_min == time2.tm_min
				  && time1.tm_sec == time2.tm_sec);
	} else {
		matches =  (time1.tm_year == time2.tm_year
					&& time1.tm_mon == time2.tm_mon
					&& time1.tm_mday == time2.tm_mday
					&& time1.tm_hour == time2.tm_hour
					&& time1.tm_min == time2.tm_min
					&& time1.tm_sec == time2.tm_sec);
	}
	if (matches) {
		printf("OK: Times match in line %d.\n", line);
	} else {
		printf("FAILED: Times don't match in line %d.\n", line);
		printf("time1: ");
		dump_time(time1);
		printf("time2: ");
		dump_time(time2);
	}
}

static void dump_time(
	const struct tm &time)
{
	printf("%d-%d-%dT%d:%d:%d\n",
		   time.tm_year, time.tm_mon, time.tm_mday,
		   time.tm_hour, time.tm_min, time.tm_sec);

}
