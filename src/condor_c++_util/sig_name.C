/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
 
#include "condor_common.h"

#if !defined(WIN32)

struct SigTable { int num; const char *name; };

/*
  The order in this array doesn't really matter.  it's not trying to
  map to signal numbers at all.  however, when we're looking up a
  signal (either by name or number), we start at the begining and
  search through it, so we might as well put the more commonly used
  signals closer to the front...
*/
static struct SigTable SigNameArray[] = {
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
	{ -1, 0 }
};

extern "C" {

int
signalNumber( const char* signame )
{
	if( ! signame ) {
		return -1;
	}
	for( int i = 0; SigNameArray[i].name; i++ ) {
		if( stricmp(SigNameArray[i].name, signame) == 0) {
			return SigNameArray[i].num;
		}
	}
	return -1;
}


const char*
signalName( int sig )
{
	for( int i = 0; SigNameArray[i].name != 0; i++ ) {
		if( SigNameArray[i].num == sig ) {
			return SigNameArray[i].name;
		}
	}
	return NULL;
}

} // end of extern "C"

#else /* WIN32 */


extern "C" {

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

} // end of extern "C"

#endif /* WIN32 */
