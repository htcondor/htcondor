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

/**
 * Convert a struct tm into an ISO 8601 standard date
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
char *time_to_iso8601(const struct tm &time, ISO8601Format format, 
					  ISO8601Type type, bool is_utc);

/**
 * This converts a date/time in ISO 8601 format to a "struct tm". 
 * Note that the string may contain a date, a time, 
 * or a date and time. Only the appropriate fields will be
 * filled in the structure. The wday and yday fields are never
 * filled in. Any fields that can't be filled in are set to 
 * -1. Also note that we maintain the goofiness of the 
 * "struct tm": the year 2001 is represented as 101, January
 * is month 0, etc.
 * @param iso_time The date in IS 8601 format
 * @param time This is filled in with the time.
 * @param is_utc This is set to true if it's UTC. Assume localtime 
 *        otherwise. */
void iso8601_to_time(const char *iso_time, struct tm *time, bool *is_utc);

#endif 
