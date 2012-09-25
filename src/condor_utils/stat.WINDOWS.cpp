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
#include "stat.WINDOWS.h"

void 
resolve_daylight_saving_problem (struct stat *sbuf) {

	struct tm	*newtime;
    time_t		mytime;

	time( &mytime );					/* get time as long integer. */
    newtime = localtime( &mytime );		/* convert to local time. */


	/*
		It appearers that the _stat function under Windows NT 4.0 and, by all 
		reports, many other Windows systems, will return file times 3600 seconds 
		(1 hour) too big when daylight savings time is in effect (this also 
		includes st_atime and st_ctime).  This seems surprising to me: since, in 
		general, the MSDN is an accurate source of information, but that the stat 
		section leaves out is a glaring omission. This is a to be considered a bug since 
		it is defined as returning the time in UTC. The following fix is kinda gross, 
		but it seems to resolve the problem (the MS code does += so we are just
		reversing it). Also, since the problem has been around since NT 4.0, it's 
		sure to survive at least another 10 years... if not forever.

		-B [11/6/2007]
	*/

	
    if ( 1 == newtime->tm_isdst ) {
		sbuf->st_atime -= 3600; 
		sbuf->st_ctime -= 3600; 
		sbuf->st_mtime -= 3600;
    }

}

/* on success returns the file's *real* UTC time; otherwise, -1 */
int
_fixed_windows_stat(const char *file, struct _fixed_windows_stat *sbuf) { 
	
	int ret = _stat(file, (struct _stat*) sbuf);

	if ( ret != 0 ) {
		return ret;
	}
	
	resolve_daylight_saving_problem ( sbuf );

	return ret;

}


/* on success returns the file's *real* UTC time; otherwise, -1 */
int
_fixed_windows_fstat(int file, struct _fixed_windows_stat *sbuf) { 
	
	int ret = _fstat(file, (struct _stat*) sbuf);

	if ( ret != 0 ) {
		return ret;
	}
	
	resolve_daylight_saving_problem ( sbuf );

	return ret;

}
