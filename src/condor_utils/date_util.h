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

  
#ifndef DATE_UTIL_H
#define DATE_UTIL_H

// ---------------------------------------------------
// This file contains some helper functions for working
// with the calendar.
// ---------------------------------------------------

//
// Returns the # of days for a month in a given year
//
int daysInMonth( int month, int year );

//
// Returns the day of week for a given date 
// Sunday (0) to Saturday (6)
// Assuming Gregorian Calendar
//
int dayOfWeek( int month, int day, int year );

#endif // DATE_UTIL_H
