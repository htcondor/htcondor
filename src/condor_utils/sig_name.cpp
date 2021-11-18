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

#if !defined(WIN32)

struct SigTable { int num; char name [9]; };

/*
  The order in this array doesn't really matter.  it's not trying to
  map to signal numbers at all.  however, when we're looking up a
  signal (either by name or number), we start at the begining and
  search through it, so we might as well put the more commonly used
  signals closer to the front...
*/

// WARNING!  SigTable is a FIXED SIZE CHAR ARRAY!!!!!
// DO NOT ADD SIGNAL NAMES LONGER THAN 8 CHARS
// This is done so the thing is marked read-only in libcondor_utils.
static const struct SigTable SigNameArray_local[] = {
	{ SIGKILL, "SIGKILL" },
	{ SIGCONT, "SIGCONT" },
	{ SIGTERM, "SIGTERM" },
	{ SIGQUIT, "SIGQUIT" },
	{ SIGHUP, "SIGHUP" },
	{ SIGSTOP, "SIGSTOP" },
	{ SIGTSTP, "SIGTSTP" },
	{ SIGUSR1, "SIGUSR1" },
	{ SIGUSR2, "SIGUSR2" },
	{ SIGCHLD, "SIGCHLD" },
	{ SIGABRT, "SIGABRT" },
	{ SIGALRM, "SIGALRM" },
	{ SIGFPE, "SIGFPE" },
	{ SIGILL, "SIGILL" },
	{ SIGINT, "SIGINT" },
	{ SIGPIPE, "SIGPIPE" },
	{ SIGSEGV, "SIGSEGV" },
	{ SIGTRAP, "SIGTRAP" },
	{ SIGTTIN, "SIGTTIN" },
	{ SIGTTOU, "SIGTTOU" },
	{ SIGFPE, "SIGFPE" },
	{ SIGIO, "SIGIO" },
	{ SIGBUS, "SIGBUS" },
	{ SIGURG, "SIGURG" },
	{ SIGPROF, "SIGPROF" },
	{ SIGWINCH, "SIGWINCH" },
#if defined(SIGEMT)
	{ SIGEMT, "SIGEMT" },
#endif
#if defined(SIGSYS)
	{ SIGSYS, "SIGSYS" },
#endif
#if defined(SIGXCPU)
	{ SIGXCPU, "SIGXCPU" },
#endif
#if defined(SIGXFSZ)
	{ SIGXFSZ, "SIGXFSZ" },
#endif
#if defined(SIGINFO)
	{ SIGINFO, "SIGINFO" },
#endif
	{ -1, "\0" }
};

int
signalNumber( const char* signame )
{
	if( ! signame ) {
		return -1;
	}
	for( int i = 0; SigNameArray_local[i].name[0] != '\0'; i++ ) {
		if( strcasecmp(SigNameArray_local[i].name, signame) == 0) {
			return SigNameArray_local[i].num;
		}
	}
	return -1;
}


const char*
signalName( int sig )
{
	for( int i = 0; SigNameArray_local[i].name[0] != '\0'; i++ ) {
		if( SigNameArray_local[i].num == sig ) {
			return SigNameArray_local[i].name;
		}
	}
	return NULL;
}

#else /* WIN32 */


int
signalNumber( const char* )
{
	return -1;
}


const char*
signalName( int )
{
	return NULL;
}

#endif /* WIN32 */
