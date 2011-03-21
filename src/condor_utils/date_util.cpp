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
#include "date_util.h"
#include <math.h>
  
//
// Returns the # of days for a month in a given year
//
int
daysInMonth( int month, int year ) 
{
	const unsigned char daysInMonth[13] = {	0, 31, 28, 31, 30, 31,
  											30, 31, 31, 30, 31, 30, 31};
  		//
  		// Check if it's a leap year
  		//
	bool leap = ( ( !(year % 4) && (year % 100) ) || (!(year % 400)) );
	return ( month > 0 && month <= 12 ? 
  				daysInMonth[month] + (month == 2 && leap ) :
  				0 );
}

//
// Returns the day of week for a given date 
// Sunday (0) to Saturday (6)
// Assuming Gregorian Calendar
//
int
dayOfWeek( int month, int day, int year )
{
	if ( month < 3 ) {
		month += 12;
		year--;
	}
		//
		// I have no idea what this really does, I got
		// it off of the Internet. It doesn't seem to be 
		// anything too special because it's posted around
		// in similiar forms
		//
	int temp = (int)(	day + 1
				+ ( 2 * month )
				+ rint( 6 * ( month + 1) / 10 )
				+ year
				+ rint( year / 4 )
				- rint( year / 100)
				+ rint( year / 400));
	
	return ( temp % 7 );
}
