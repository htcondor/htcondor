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

#include "except.h"
#include "debug.h"

extern int	errno;

void display_flock( struct flock *f );
void display_cmd( int cmd );

/*
** Compatibility routine for systems which utilize various forms of the
** fcntl() call for this purpose.  Note that semantics are a bit different
** then the bsd type flock() in that a write lock (exclusive lock) can
** only be applied if the file is open for writing, and a read lock
** (shared lock) can only be applied if the file is open for reading.
*/
int
flock( int fd, int op )
{
	struct flock	f;
	int				cmd;
	int				status;

	if( (op & LOCK_NB) == LOCK_NB ) {
		cmd = F_SETLK;		/* non-blocking */
	} else {
		cmd = F_SETLKW;		/* blocking */
	}

	f.l_start = 0;			/* flock only supports locking whole files */
	f.l_len = 0;
	f.l_whence = 0;
	f.l_pid = 0;

	if( op & LOCK_SH ) {			/* shared */
		f.l_type = F_RDLCK;
	} else if (op & LOCK_EX) {		/* exclusive */
		f.l_type = F_WRLCK;
	} else if (op & LOCK_UN ) {		/* unlock */
		f.l_type = F_UNLCK;
	} else {
		errno = EINVAL;
		return -1;
	}

	display_flock( &f );
	display_cmd( cmd );

	status =  fcntl( fd, cmd, &f );
	/* dprintf( D_ALWAYS, "return value is %d\n", status ); */
	return status;
}

#define CASE(vble,val) \
	case val: \
	/* dprintf( D_ALWAYS, "vble = val\n" ) */; \
	break

#define DEBUG(name,fmt) dprintf( D_ALWAYS, "name = fmt\n", name );
void
display_flock( struct flock	*f )
{
	switch( f->l_type ) {
		CASE( l_type, F_RDLCK );
		CASE( l_type, F_WRLCK );
		CASE( l_type, F_UNLCK );
		default:
			dprintf( D_ALWAYS, "l_type is unknown (%d)\n", f->l_type );
			break;
	}

	/*
	DEBUG( f->l_whence, %d );
	DEBUG( f->l_start, %d );
	DEBUG( f->l_len, %d );
	DEBUG( f->l_pid, %d );
	*/
}


void
display_cmd( int cmd )
{
	switch( cmd ) {
		CASE( cmd, F_DUPFD );
		CASE( cmd, F_GETFD );
		CASE( cmd, F_SETFD );
		CASE( cmd, F_GETLK );
		CASE( cmd, F_SETLK );
		CASE( cmd, F_SETLKW );
		default:
			dprintf( D_ALWAYS, "cmd = UNKNOWN (%d)\n" );
			break;
	}
}
