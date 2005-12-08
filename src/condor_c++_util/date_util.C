/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
