/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "MyString.h"
#include "condor_config.h"
#include <time.h>

/* 
   This converts the given int into a string, and tacks on "st", "nd",
   "rd", or "th", as appropriate.  The value is returned in a static
   buffer, so beware, and use w/ caution.
*/
const unsigned BUF_SIZE = 32;
const char*
num_string( int num )
{
	int i;
	static char buf[BUF_SIZE];

	i = num % 100; 	// i is just last two digits.
	
		// teens are a special case... they're always "th"
	if( i > 10 && i < 20 ) {
		snprintf( buf, BUF_SIZE, "%dth", num );
		return buf;
	}

	i = i % 10;		// Now, we have just the last digit.
	switch( i ) {
	case 1:
		snprintf( buf, BUF_SIZE, "%dst", num );
		return buf;
		break;
	case 2:
		snprintf( buf, BUF_SIZE, "%dnd", num );
		return buf;
		break;
	case 3:
		snprintf( buf, BUF_SIZE, "%drd", num );
		return buf;
		break;
	default:
		snprintf( buf, BUF_SIZE, "%dth", num );
		return buf;
		break;
	}
	return "";
}


std::string
startdClaimIdFile( int slot_id )
{
	std::string filename;

	char* tmp;
	tmp = param( "STARTD_CLAIM_ID_FILE" );
	if( tmp ) {
		filename = tmp;
		free( tmp );
		tmp = NULL;
	} else {
			// otherwise, we must create our own default version...
		tmp = param( "LOG" );
		if( ! tmp ) {
			dprintf( D_ALWAYS,
					 "ERROR: startdClaimIdFile: LOG is not defined!\n" );
			return "";
		}
		filename = tmp;
		free( tmp );
		tmp = NULL;
		filename += DIR_DELIM_CHAR;
		filename += ".startd_claim_id";
	}

	if( slot_id ) {
		filename += ".slot";
		filename += std::to_string( slot_id );
	}			
	return filename;
}

const char*
my_timezone(int isdst) 
{
  tzset();

#ifndef WIN32
	// On Unix, return tzname which is CST, CDT, PST, etc.
  
	  // if daylight is in effect (isdst is positive)
  if (isdst > 0) {
	  return tzname[1];
  }
  else {
	  return tzname[0];
  }
#else
	// On Win32, tzname is useless.  It return a string like
	// "Central Standard Time", which doesn't follow any standard,
	// and thus cannot be used in things like valid SQL statements.
	// So instead on Win32, return a string like "-6:00" which is
	// a timezone that at least is in common use, and won't cause
	// systems like PostgreSQL to barf.
  	static char buf[15];
	int answer;
	char c = '+';

	long tzv = -6 * 3600;
	_get_timezone(&tzv);

	answer = -1 * (tzv / 3600);	// note: timezone is global
	if ( answer < 0 ) {
		c = '-';
		answer *= -1;
	}

	sprintf(buf,"%c%d:00",c,answer);
	return buf;
#endif

}
