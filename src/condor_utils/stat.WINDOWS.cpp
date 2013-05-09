/***************************************************************
 *
 * Copyright (C) 1990-2013, Condor Team, Computer Sciences Department,
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

int _fixed_windows_stat(const char *file, struct _fixed_windows_stat *sbuf)
{

	// TJ: April 2013 - in vs2008
	// if we don't call this before we call stat, and daylight savings time
	// changes while our process is running, we start getting file times that
	// are off by 1 hour.
	_tzset();

	return _stat(file, (struct _stat*) sbuf);
}

int _fixed_windows_fstat(int file, struct _fixed_windows_stat *sbuf)
{

	// TJ: April 2013 - in vs2008
	// if we don't call this before we call fstat and daylight savings time
	// changes while our process is running, we start getting file times that
	// are off by 1 hour.
	_tzset();

	return _fstat(file, (struct _stat*) sbuf);
}
