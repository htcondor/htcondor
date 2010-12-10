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
#include "condor_threads.h"
#include "condor_debug.h"

static mark_thread_func_t start_callback;
static mark_thread_func_t stop_callback;

void _mark_thread_safe_callback(mark_thread_func_t start_routine, 
		mark_thread_func_t stop_routine)
{
	start_callback = start_routine;
	stop_callback = stop_routine;
	return;
}


/*
 *   A basename() implementation.
 *   This is essentially a stripped down version of condor_basename();
 *   we reproduce it here so we don't need to suck all the rest of the stuff
 *   folks littered into basename.c into the standard universe syscall libs.
 */
static const char *
my_local_basename(const char *path)
{
	const char *s, *name;

	if( ! path ) {
		return "";
	}

	name = path;

	for (s = path; s && *s != '\0'; s++) {
		if (*s == '\\' || *s == '/') {
			name = s+1;
		}
	}

	return name;
}


void _mark_thread_safe(int start_or_stop, int dologging, const char* descrip, 
					   const char* func, const char* file, int line)
{
	mark_thread_func_t callback;
	const char* mode;
#ifdef WIN32
	char buf[40];
#endif

	switch (start_or_stop) {
		case 1:
			callback = start_callback;
			mode = "start";
			break;
		case 2:
			callback = stop_callback;
			mode = "stop";
			break;
		default:
			EXCEPT("unexpected mode: %d",start_or_stop);
	}

	if ( !callback ) {
		// nothing to do, we're done.
		return;
	}

	if (descrip == NULL) {
		descrip = "\0";
	}

#ifdef WIN32	
	strcpy(buf,"tid=");
	func = itoa(GetCurrentThreadId(), &buf[4], 10 );	
#endif

	if ( dologging && (DebugFlags & D_FULLDEBUG)) {
		dprintf(D_THREADS,"Entering thread safe %s [%s] in %s:%d %s()\n",
				mode, descrip, my_local_basename(file), line, func);
	}

	(*callback)();

	if ( dologging && (DebugFlags & D_FULLDEBUG)) {
		dprintf(D_THREADS,"Leaving thread safe %s [%s] in %s:%d %s()\n",
				mode, descrip, my_local_basename(file), line, func);
	}

	return;
}

