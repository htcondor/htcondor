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


#ifndef __ISO_DATES_H__
#define __ISO_DATES_H__

/**
 * Lets you specify format for printing dates. 
 * The Basic Format doesn't use delimiters (like 200104-50)
 * The Extended format uses delimiters (like 2001-04-05) */
enum ISO8601Format
{
	ISO8601_BasicFormat,
	ISO8601_ExtendedFormat
};

/**
 * Lets you specify what part of the date/time you want to print
 * You can print specify just the date, just the time, or both the
 * date and time.
 */
enum ISO8601Type
{
	ISO8601_DateOnly,
	ISO8601_TimeOnly,
	ISO8601_DateAndTime
};

/* required buffer sizes based on: yyyy-mm-dd hh:mm:ss.ffffff+hh:mm
                                            1         2         3
                                   12345678901234567890123456789012
*/
enum ISO8601BufferMax
{
	ISO8601_DateOnlyBufferMax = 11,
	ISO8601_TimeOnlyBufferMax = 23,
	ISO8601_DateAndTimeBufferMax = 33,
};

/**
 * Convert a struct tm into an ISO 8601 standard date
 * @param buf output buffer, must be large enough to hold the string
 *        use ISO8601BufferMax enum for buffer sizes for the various ISO8601Type options
 * @param time The time you wish to convert. A normal "struct tm",
 *        which has the usual weirdness: the year is off by 1900, etc.
 *        (See ctime(3).)
 * @param format basic format (without delimiters) 
 *        or extended (with delimiters)
 * @param type Just date, just time, or date and time
 * @param is_utc Pass true if it's in UTC, otherwise it is considered
 *        local time. (We don't do timezones.)
 * @return A string allcoated with malloc. You'll need to free() it
 *         when you're done with it. */
char *time_to_iso8601(
	char * outbuf,
	const struct tm &time, ISO8601Format format, 
	ISO8601Type type, bool is_utc,
	unsigned int sub_sec=0, int sub_sec_digits=0);

/**
 * This converts a date/time in ISO 8601 format to a "struct tm". 
 * Note that the string may contain a date, a time, 
 * or a date and time. Only the appropriate fields will be
 * filled in the structure. The wday and yday fields are never
 * filled in. Any fields that can't be filled in are set to 
 * -1. Also note that we maintain the goofiness of the 
 * "struct tm": the year 2001 is represented as 101, January
 * is month 0, etc.
 * @param iso_time The date in ISO 8601 format
 * @param time This is filled in with the time.
 * @param is_utc This is set to true if it's UTC. Assume localtime 
 *        otherwise. */
void iso8601_to_time(const char *iso_time, struct tm *time, long *usec, bool *is_utc);

#endif 
