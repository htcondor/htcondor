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
#include "utc_time.h"

#if defined(HAVE__FTIME)
# include <sys/timeb.h>
#endif

#ifdef WIN32
time_t condor_gettimestamp(long & usec)
{
	time_t sec;

	// Windows8 has GetSystemTimePreciseAsFileTime which returns sub-microsecond system times.
	static bool check_for_precise = false;
	static void (WINAPI*get_precise_time)(unsigned long long * ft) = NULL;
	static BOOLEAN(WINAPI* time_to_1970)(unsigned long long * ft, unsigned long * epoch_time);
	if (! check_for_precise) {
		HMODULE hmod = GetModuleHandle("Kernel32.dll");
		if (hmod) { *(FARPROC*)&get_precise_time = GetProcAddress(hmod, "GetSystemTimePreciseAsFileTime"); }
		hmod = GetModuleHandle("ntdll.dll");
		if (hmod) { *(FARPROC*)&time_to_1970 = GetProcAddress(hmod, "RtlTimeToSecondsSince1970"); }
		check_for_precise = true;
	}
	unsigned long long nanos = 0;
	if (get_precise_time) {
		get_precise_time(&nanos);
		unsigned long now = 0;
		time_to_1970(&nanos, &now);
		sec = now;
		usec = (long)((nanos / 10) % 1000000);
	} else {
		struct _timeb tb;
		_ftime(&tb);
		sec = tb.time;
		usec = tb.millitm * 1000;
	}
	return sec;
}
#else
void condor_gettimestamp( struct timeval &tv )
{
#ifdef HAVE_GETTIMEOFDAY
	gettimeofday( &tv, NULL );
#elif defined(HAVE__FTIME) 
	struct _timeb tb;
	_ftime(&tb);
	tv.tv_sec = tb.time;
	tv.tv_usec = tb.millitm * 1000;
#else
#error Neither _ftime() nor gettimeofday() are available!
#endif
}
#endif

