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


/********************************************************************
**
**	Description:

**	Provides a means for system processes to block condor jobs from running
**	regardless of other machine availability parameters.  Primarily intended
**	for dumps.
**
**	Exports:
**
** 		void do_block_condor()
**		void unblock_condor()
**
**		int condor_block_requested()
**
**  Current implementation:
**		Reader's/Writer's locks on a well know file are used.  The name
** 		of the file is read from the config files.  Processes wishing
**		to block the running of Condor jobs should obtain a shared lock
**		on the file.  When the process no longer wants to block condor
**		jobs, it should release the lock.  The use of a shared lock
**		allows for the possibility of multiple simultaneous blockers.
**
********************************************************************/


#include <stdio.h>
#include <sys/file.h>
#include <errno.h>
#include "sched.h"
#include "debug.h"
#include "except.h"
#include "file_lock.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

static int	BlockFd = -1;

#if defined( __STDC__ )
char *param( const char *);
static int init_block_file( int flags, int mode );
#else
char *param();
static int init_block_file();
#endif

/*
  Make sure the file exists, and is open.  Then get a shared lock on it.
*/
void
do_block_condor()
{
	if( BlockFd < 0 ) {
		BlockFd = init_block_file( O_RDWR|O_CREAT, 0644 );
		if( BlockFd < 0 ) {
			EXCEPT( "Can't create lock file" );
		}
	}

		/* Get a shared lock on the file */
	if( lock_file(BlockFd,READ_LOCK,TRUE) < 0 ) {
		EXCEPT( "Can't lock file" );
	}
}

/*
  Just unlock the file.  Should already exist and be locked.
*/
void
unblock_condor()
{
	if( BlockFd < 0 ) {
		EXCEPT( "Routine unblock_condor() called, but not blocked\n" );
	}

	if( lock_file(BlockFd,UN_LOCK,FALSE) < 0 ) {
		EXCEPT( "Can't unlock file" );
	}
}

/*
  If the file exists and somebody has a lock on it, return TRUE,
  otherwise return false.
*/
int
condor_block_requested()
{
	if( BlockFd < 0 ) {
		BlockFd = init_block_file(O_RDWR,0644);
		if( BlockFd < 0 ) {
			return FALSE;		/* NO file - not blocked */
		}
	}

		/* Non blocking attempt to get exclusive lock */
	if( lock_file(BlockFd,WRITE_LOCK,FALSE) < 0 ) {
		return TRUE;			/* Can't obtain lock - blocked */
	}

		/* We got the lock, therefore nobody else had it - not blocked */
		/* don't forget to clear the lock now */
	if( lock_file(BlockFd,UN_LOCK,FALSE) < 0 ) {
		EXCEPT( "Can't unlock condor block file" );
	}
	return FALSE;

}

static int
init_block_file( flags, mode )
int flags;
int mode;
{
	char	tmp [ 512 ];
	static char	*name = NULL;
	char	*dir;
	char	*log;
	int		fd;

	if( name == NULL ) {
		name = param( "BLOCK_FILE" );

		if( name == NULL ) {
			log = param( "LOG" );
			if( log == NULL ) {
				EXCEPT( "No LOG directory specified in config file" );
			}
			sprintf( tmp, "%s/%s", log, "BlockCondorJobs" );
			free( log );
			name = strdup( tmp );
		}
	}

	fd = open( name, flags, mode );
	if( fd < 0 &&  (flags & O_CREAT) == O_CREAT ) {
		EXCEPT( "open(%s,0%o,0%o)", name, flags, mode );
	}

	return fd;
}
