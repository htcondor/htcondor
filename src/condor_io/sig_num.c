/* 
** Copyright 1997 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:   Jim Basney
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <signal.h>

int
sig_num_encode( sig_num )
int		sig_num;
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

int
sig_num_decode( sig_num )
int		sig_num;
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
