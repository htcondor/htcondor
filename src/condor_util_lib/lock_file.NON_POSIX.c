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

 

/*
  Purpose:
	Handle advisory file locking.
  Portability: 
	When USE_FLOCK is defined as TRUE, this file uses the bsd style
	system call flock(), which is non POSIX conforming.  When
	USE_FLOCK is defined as FALSE, the POSIX routine fcntl() is
	used, and in this case the code should be portable.
  Discussion:
	Why not just use the POSIX version everywhere?  Becuase on my
	local ULTRIX 4.2 systems fcntl() locks don't cross NFS file
	system boundaries.  The flock() call does work across these NFS
	systems.  Your mileage may vary...

	Another important point - while ULTRIX implements both fcntl()
	locks and flock() locks, they are not compatible.  It is perfectly
	possible for one process to lock an entire file with an exclusive
	lock using fcntl() and still another process may get an exclusive
	lock on the same file using flock()!  It is therefore extremely
	important that processes wishing to cooperate with advisory file
	locks agree on which kind of locks are being used.
*/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

/**********************************************************************
** condor_common.h includes condor_file_lock.h, which defines
** CONDOR_USE_FLOCK to be either 0 or 1 depending on whether we should
** use the flock system call to implement file locking.
**********************************************************************/

/*
  This is the non POSIX conforming way of locking a file using the
  bsd style flock() system call.  We may have to do it this way
  becuase some NFS file systems don't fully support the POSIX fcntl()
  system call.
*/

#if CONDOR_USE_FLOCK

int
lock_file( fd, type, do_block )
int fd;
LOCK_TYPE type;
int do_block;
{
	int	op;

	switch( type ) {
	  case READ_LOCK:
		op = LOCK_SH;
		break;
	  case WRITE_LOCK:
		op = LOCK_EX;
		break;
	  case UN_LOCK:
	  case LOCK_UN:         /* this shud be the corrct case : dhruba */
		op = LOCK_UN;
		break;
      default:
			  /* unknown lock type, fail immediately */
		return -1;
	}

	if( !do_block ) {
		op |= LOCK_NB;
	}

		/* be signal safe */
	while( flock(fd,op) < 0 ) {
		switch (errno) {
#ifdef ENOLCK
			/* so, flock never sets errno to ENOLCK.  however, our
			   flock is actually implemented in terms of fcntl, so
			   this is a possible result.  this whole locking sub-
			   system could use a reworking.  -zmiller  */
			case ENOLCK:
				{
					char* p = param("IGNORE_NFS_LOCK_ERRORS");
					char  val = 'N';

					if (p) {
						val = p[0];
						free (p);
					}

					if (val == 'Y' || val == 'y' || val == 'T' || val == 't') {
						// pretend there was no error.
						dprintf ( D_FULLDEBUG, "Ignoring error ENOLCK on fd %i\n", fd );
						return 0;
					}
				}
				// properly propagate the error
				return -1;
#endif /* ENOLCK */
			case EINTR:
				// this just means we we interrupted, not
				// necessarily that we failed.
				break;
			default:
				return -1;
		}
	}
	return 0;
}

#else /* (! CONDOR_USE_FLOCK) */

/*
  This is the POSIX conforming way of locking a file using the
  fcntl() system call.  Note: even systems which fully support fcntl()
  for local fils these locks may not work across NFS files systems.
*/

int
lock_file( int fd, LOCK_TYPE type, int do_block )
{
	struct flock	f;
	int				cmd;

	if( do_block ) {
		cmd = F_SETLKW;		/* blocking */
	} else {
		cmd = F_SETLK;		/* non-blocking */
	}

		/* These parameters say we want to lock the whole file */
	f.l_whence = SEEK_SET;	/* l_start refers to beginning of file */
	f.l_start = 0;			/* start locking at byte 0 */
	f.l_len = 0;			/* continue locking to the end of the file */
	f.l_pid = 0;			/* don't care about this one */

	switch( type ) {
	  case READ_LOCK:
		f.l_type = F_RDLCK;
		break;
	  case WRITE_LOCK:
		f.l_type = F_WRLCK;
		break;
	  case UN_LOCK:
	  case LOCK_UN:         /* this shud be the corrct case : dhruba */
		f.l_type = F_UNLCK;
		break;
      default:
			  /* unknown lock type, fail immediately */
		return -1;
	}

	/* now, fcntl should work accross nfs.  but, due to bugs in some
	   implementations (*cough* linux *cough*) it sometimes fails with
	   errno 37 (ENOLCK).  if this happens we check the config to see
	   if we should report this as an error.   -zmiller  07/15/02
	   */

		/* be signal safe */
	while( fcntl(fd,cmd,&f) < 0 ) {
		switch (errno) {
#ifdef ENOLCK
			case ENOLCK:
				{
					char* p = param("IGNORE_NFS_LOCK_ERRORS");
					char  val = 'N';

					if (p) {
						val = p[0];
						free (p);
					}

					if (val == 'Y' || val == 'y' || val == 'T' || val == 't') {
						// pretend there was no error.
						dprintf ( D_FULLDEBUG, "Ignoring error ENOLCK on fd %i\n", fd );
						return 0;
					}
				}
				// properly propagate the error
				return -1;
#endif /* ENOLCK */
			case EINTR:
				// this just means we were interrupted, not
				// necessarily that we failed.
				break;
			default:
				return -1;
		}
	}
	return 0;
}

#endif /* CONDOR_USE_FLOCK */
