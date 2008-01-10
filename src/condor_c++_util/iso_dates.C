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


#include "condor_common.h"
#include "iso_dates.h"

static bool get_next_bit(const char **current, int count,	char *workspace);

/***********************************************************************
 *
 * Function: time_to_iso8601
 * Purpose:  This converts a "struct tm" into a string containing a
 *           representation of the date that conforms to the ISO8601
 *           standard. 
 * Returns:  A string allcoated with malloc. You'll need to free() it
 *           when you're done with it. 
 * Note:     We expect a normal struct tm: the year you give is 1900 
 *           less than the actual year, months are 0-11, etc. 
 *
 ***********************************************************************/
char *time_to_iso8601(
	const struct tm  &time, 
	ISO8601Format    format, 
	ISO8601Type      type, 
	bool             is_utc)
{
	char  buffer[128];
	char  *iso_representation;
	char  *utc_note;
	int   year, month, day;
	int   hour, minute, second;

	if (type != ISO8601_TimeOnly) {
		year = 1900 + time.tm_year; // struct tm has year-1900
		if (year < 0)
			year = 0;
		else if (year > 9999)
			year = 9999;
		
		month = time.tm_mon + 1; // struct tm has 0-11
		if (month < 1)
			month = 1;
		else if (month > 12)
			month = 12;

		day = time.tm_mday;
		if (day < 1)
			day = 1;
		else if (day > 31)
			day = 31;
	}
	if (type != ISO8601_DateOnly) {
		hour = time.tm_hour;
		if (hour < 0) hour = 0;
		else if (hour > 24) hour = 24; // yes, I really mean 24

		minute = time.tm_min;
		if (minute < 0) minute = 0;
		else if (minute > 60) minute = 60; // yes, I really mean 60

		second = time.tm_sec;
		if (second < 0) second = 0;
		else if (second > 60) second = 60; // yes, I really mean 60

		if (is_utc)
			utc_note = "Z";
		else 
			utc_note = "";
	}

	if (type == ISO8601_DateOnly) {
		if (format == ISO8601_BasicFormat) {
			sprintf(buffer, "%04d%02d%02d", year, month, day);
		} else {
			sprintf(buffer, "%04d-%02d-%02d", year, month, day);
		}
	} else if (type == ISO8601_TimeOnly) {
		if (format == ISO8601_BasicFormat) {
			sprintf(buffer, "T%02d%02d%02d%s", 
					hour, minute, second, utc_note);
		} else {
			sprintf(buffer, "T%02d:%02d:%02d%s", 
					hour, minute, second, utc_note);
		}
	} else {
		if (format == ISO8601_BasicFormat) {
			sprintf(buffer, "%04d%02d%02dT%02d%02d%02d%s", 
					year, month, day, hour, minute, second, utc_note);
		} else {
			sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d%s", 
					year, month, day, hour, minute, second, utc_note);
		}
	}

	iso_representation = strdup(buffer);
	return iso_representation;
}

/***********************************************************************
 *
 * Function: iso8601_to_time
 * Purpose:  This converts a date/time in ISO 8601 format to a "struct
 *           tm". Note that the string may contain a date, a time, 
 *           or a date and time. Only the appropriate fields will be
 *           filled in the structure. The wday and yday fields are never
 *           filled in. Any fields that can't be filled in are set to 
 *           -1. Also note that we maintain the goofiness of the 
 *           "struct tm": the year 2001 is represented as 101, January
 *           is month 0, etc.
 *
 ***********************************************************************/
void iso8601_to_time(
	const char     *iso_time,
	struct tm      *time, 
	bool           *is_utc)
{
	bool begins_with_time;

	// Preset to -1, to indicate nothing was parsed.
	if (time != NULL) {
		time->tm_year = -1;
		time->tm_wday = -1;
		time->tm_yday = -1;
		time->tm_mon  = -1;
		time->tm_mday = -1;
		time->tm_hour = -1;
		time->tm_min  = -1;
		time->tm_sec  = -1;
		time->tm_isdst = -1;
	}

	// Only do something if we got valid parameters
	if (iso_time != NULL && time != NULL) {
		if (iso_time[0] == 'T' || iso_time[2] == ':')
			begins_with_time = true;
		else
			begins_with_time = false;

		const char *current;
		char workspace[6];

		current = iso_time;
		/* ----- Parse the date ----- */
		if (!begins_with_time) { 
			if (get_next_bit(&current, 4, workspace)) {
				time->tm_year = atoi(workspace);
				time->tm_year -= 1900; 
			}

			if (get_next_bit(&current, 2, workspace)) {
				time->tm_mon = atoi(workspace);
				time->tm_mon -= 1;
			}

			if (get_next_bit(&current, 2, workspace)) {
				time->tm_mday = atoi(workspace);
			}
		}

		/* ----- Parse the time ----- */
		if (get_next_bit(&current, 2, workspace)) {
			time->tm_hour = atoi(workspace);
		}

		if (get_next_bit(&current, 2, workspace)) {
			time->tm_min = atoi(workspace);
		}

		if (get_next_bit(&current, 2, workspace)) {
			time->tm_sec = atoi(workspace);
		}

		if (is_utc != NULL) {
			if (toupper(*current) == 'Z') {
				*is_utc = true;
			} else {
				*is_utc = false;
			}
		}
	}

	return;
}

/***********************************************************************
 *
 * Function: get_next_bit
 * Purpose:  This fills in the workspacce with the next "count" characters
 *           from current, and updates current to point just past the
 *           characters is got. 
 *           If there aren't enough characters in current to copy, we don't
 *           copy them. We only return true if exactly "count" characters
 *           were copied. 
 *           We skip delimiters that may exist in an ISO8601 string. We
 *           are somewhat leninent: for example, you could have a string
 *           like 2001T-T03-03, and it would grab out the correct digits. 
 *           This could be good or bad. On the other hand, you couldn't
 *           have 2001*03*03 and have it work.
 *
 ***********************************************************************/
static bool get_next_bit(
	const char **current,
	int        count,
	char       *workspace)
{
	int i;
	const char *p;
	bool  got_characters;

	p = *current;

	// Skip delimiters
	while (*p == ':' || *p == '-' || *p == 'T') {
		p++;
	}

	// Copy the requested number of characters into the workspace,
	// assuming that we actually have them.
	i = 0;
	while (i < count && *p != 0) {
		workspace[i++] = *p;
		p++;
	}
   
	// Null terminate the workspace, to make a valid string.
	workspace[i] = 0;
	*current = p;

	if (i == count) 
		got_characters = true;
	else
		got_characters = false;

	return got_characters;
}
