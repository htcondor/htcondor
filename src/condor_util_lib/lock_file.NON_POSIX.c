/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

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

#include <stdio.h>
#include <errno.h>
#include "file_lock.h"


#if defined( HPUX9 )
#define USE_FLOCK 0
#endif

#if defined( AIX32 )
#define USE_FLOCK 0
#endif

#if defined( ULTRIX42 ) || defined(ULTRIX43)
#define USE_FLOCK 1
#endif

#if defined( SUNOS41 )
#define USE_FLOCK 1
#endif

#if defined( OSF1 )
#define USE_FLOCK 1
#endif

#if !defined(USE_FLOCK)
ERROR: DONT KNOW WHETHER TO USE FLOCK or FCNTL
#endif

/*
  This is the non POSIX conforming way of locking a file using the
  bsd style flock() system call.  We may have to do it this way
  becuase some NFS file systems don't fully support the POSIX fcntl()
  system call.
*/
#if USE_FLOCK

#include <sys/file.h>

extern int errno;

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
		op = LOCK_UN;
		break;
	}

	if( !do_block ) {
		op |= LOCK_NB;
	}

		/* be signal safe */
	while( flock(fd,op) < 0 ) {
		if( errno != EINTR ) {
			return -1;
		}
	}
	return 0;
}
#endif

#if !USE_FLOCK
/*
  This is the POSIX conforming way of locking a file using the
  fcntl() system call.  Note: even systems which fully support fcntl()
  for local fils these locks may not work across NFS files systems.
*/

#ifndef F_RDLCK
#include <fcntl.h>
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

int
lock_file( int fd, LOCK_TYPE type, int do_block )
{
	struct flock	f;
	int				cmd;
	int				status;

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
		f.l_type = F_UNLCK;
		break;
	}

		/* be signal safe */
	while( fcntl(fd,cmd,&f) < 0 ) {
		if( errno != EINTR ) {
			return -1;
		}
	}
	return 0;
}
#endif
