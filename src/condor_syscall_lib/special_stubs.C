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

/* 
   This file contains special stubs that are needed to make certain
   things work within a user job that can't use their normal
   functionality.  
*/

#include "condor_common.h"
#include "condor_io.h"
#include "condor_sys.h"
#include "condor_debug.h"
#include "exit.h"
#include "shared_utils.h"
#include "get_port_range.h"

extern	ReliSock* syscall_sock;

extern "C" {

extern	int		DebugFlags;
int		_condor_DebugFD = 0;

extern int SetSyscalls(int);
extern int SYSCALL(int, ...);


/*
  We need our own definition of my_ip_addr(), which is used by
  Sock::bind() to support Condor on machines with multiple network
  interfaces.  This version, instead of looking in a config file for
  magic parameters, looks at the existing syscall_sock and grabs the
  IP address off of there.
*/
unsigned int
my_ip_addr()
{
	return syscall_sock->get_ip_int();
}


/*
  _condor_dprintf_va is the real meat of dprintf().  We have different
  concerns inside the user job.  All we need to care about in here is
  making sure we have the right syscall mode.  Otherwise, we don't
  need to do anything complex with priv states, locking, etc.
*/
void
_condor_dprintf_va( int flags, char* fmt, va_list args )
{
	int scm;
	int no_fd = FALSE;

		/* See if this is one of the messages we are logging */
	if( !(flags&DebugFlags) ) {
		return;
	}

		/* If dprintf() isn't initialized, don't seg fault */
	if( ! _condor_DebugFD ) {
		_condor_DebugFD = 2; /* stderr */
		no_fd = TRUE;
	}

		/*
		  When talking to the debug fd, you are talking to an fd that
		  is not visible to the user.  It is not entered in the
		  virtual file table, and, hence, you should be in unmapped
		  mode.
		*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		/* Actually print the message */
	_condor_vfprintf_va( _condor_DebugFD, fmt, args );
	
		/* Restore our syscall mode */
	(void) SetSyscalls( scm );

	if( no_fd ) {
		_condor_DebugFD = 0;
	}
}


int get_port_range(int *low_port, int *high_port)
{
	char *low = NULL, *high = NULL;

	if ( (low = getenv("_condor_LOWPORT")) == NULL ) {
        dprintf(D_NETWORK, "_condor_LOWPORT undefined\n");
		return FALSE;
    }
	if ( (high = getenv("_condor_HIGHPORT")) == NULL ) {
		free(low);
        dprintf(D_ALWAYS, "_condor_LOWPORT is defined but _condor_HIGHPORT undefined!\n");
		return FALSE;
	}

	*low_port = atoi(low);
	*high_port = atoi(high);

	if(*low_port < 1024 || *high_port < 1024 || *low_port > *high_port) {
		dprintf(D_ALWAYS, "get_port_range - invalid LOWPORT(%d) \
		                   and/or HIGHPORT(%d)\n",
		                   *low_port, *high_port);
		free(low);
		free(high);
		return FALSE;
	}

	free(low);
	free(high);
	return TRUE;
}


#if !defined(WIN32)
/*
  Can't open something because we are out of fd's.  Try to let
  somebody know what happened.  In the user job, we don't want to
  close a bunch of fds, since we'll loose our debug socket, which is
  always open.  In fact, we shouldn't have to close anything to be
  able to dprintf().  So, just dprintf(), flush the fd for good
  measure, and call Suicide() (so we don't leave the job queue).  
*/
void
_condor_fd_panic( int line, char* file )
{
	dprintf( D_ALWAYS,
			 "**** PANIC -- OUT OF FILE DESCRIPTORS at line %d in %s\n", 
			 line, file );
	Suicide();
}
#endif /* ! LOOSE32 */

} /* extern "C" */

