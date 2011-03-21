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


 

#include <signal.h>
#include "utilfns.h"

int sig_num_encode( int sig_num )
{
	switch( sig_num ) {
	case SIGHUP:	return 1;
	case SIGINT:	return 2;
	case SIGQUIT:	return 3;
	case SIGILL:	return 4;
	case SIGTRAP:	return 5;
	case SIGABRT:	return 6;
#if defined(SIGEMT)
	case SIGEMT:	return 7;
#endif
	case SIGFPE:	return 8;
	case SIGKILL:	return 9;
	case SIGBUS:	return 10;
	case SIGSEGV:	return 11;
#if defined(SIGSYS)
	case SIGSYS:	return 12;
#endif
	case SIGPIPE:	return 13;
	case SIGALRM:	return 14;
	case SIGTERM:	return 15;
	case SIGURG:	return 16;
	case SIGSTOP:	return 17;
	case SIGTSTP:	return 18;
	case SIGCONT:	return 19;
	case SIGCHLD:	return 20;
	case SIGTTIN:	return 21;
	case SIGTTOU:	return 22;
	case SIGIO:		return 23;
#if defined(SIGXCPU)
	case SIGXCPU:	return 24;
#endif
#if defined(SIGXFSZ)
	case SIGXFSZ:	return 25;
#endif
	case SIGVTALRM:	return 26;
	case SIGPROF:	return 27;
	case SIGWINCH:	return 28;
#if defined(SIGINFO)
	case SIGINFO:	return 29;
#endif
	case SIGUSR1:	return 30;
	case SIGUSR2:	return 31;
	default:		return sig_num;
	}
}

int sig_num_decode( int sig_num )
{
	switch( sig_num ) {
	case 1:		return SIGHUP;
	case 2:		return SIGINT;
	case 3:		return SIGQUIT;
	case 4:		return SIGILL;
	case 5:		return SIGTRAP;
	case 6:		return SIGABRT;
#if defined(SIGEMT)
	case 7:		return SIGEMT;
#endif
	case 8:		return SIGFPE;
	case 9:		return SIGKILL;
	case 10:	return SIGBUS;
	case 11:	return SIGSEGV;
#if defined(SIGSYS)
	case 12:	return SIGSYS;
#endif
	case 13:	return SIGPIPE;
	case 14:	return SIGALRM;
	case 15:	return SIGTERM;
	case 16:	return SIGURG;
	case 17:	return SIGSTOP;
	case 18:	return SIGTSTP;
	case 19:	return SIGCONT;
	case 20:	return SIGCHLD;
	case 21:	return SIGTTIN;
	case 22:	return SIGTTOU;
	case 23:   	return SIGIO;
#if defined(SIGXCPU)
	case 24:	return SIGXCPU;
#endif
#if defined(SIGXFSZ)
	case 25:	return SIGXFSZ;
#endif
	case 26:	return SIGVTALRM;
	case 27:	return SIGPROF;
	case 28:	return SIGWINCH;
#if defined(SIGINFO)
	case 29:	return SIGINFO;
#endif
	case 30:	return SIGUSR1;
	case 31:	return SIGUSR2;
	default:   	return sig_num;
	}
}
