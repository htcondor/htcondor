/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
