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
  This file implements a facility for providing Condor users with
  history and progress information for their jobs.  The information
  is intended to go into a file named by the user at job submission
  time.  All output is prepended with a timestamp, and the Condor
  job ID, for easy access with "grep".  (Multiple processes may log
  to the same file.)  Two forms of logging are provided, single
  line output, and block output.  In single line output, each line
  is prepended with both a timestamp, and the Condor process id.  In
  block output, the block is labeled with a single timestamp, but
  each line still gets the condor process id.  Both forms are
  implemented by the PutUserLog() routine, but for block output
  the routines BeginUserLogBlock() and EndUserLogBlock() should
  surround the block of output.  In either case, file locking is
  provided to prevent scrambled output.
*/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "user_log.h"


int seteuid( uid_t );
int setegid( gid_t );

static void release_lock( USER_LOG *LP );
static void get_lock( USER_LOG *LP );
static void output_header( USER_LOG *LP, int msg_num );
static void restore_id( USER_LOG *LP );
static void set_user_id( USER_LOG *LP );
static void switch_ids( uid_t new_euid, gid_t new_egid );
static void display_ids();
static char *find_fmt( int msg_num );

static int ULogLockFd = -1;
static int ULogLockCount = 0;

/*
  Initialize a new user log "instance".
*/
USER_LOG *
OpenUserLog( const char *owner, const char *file, int c, int p, int s )
{
	struct passwd	*pwd;
	USER_LOG	*LP;
	char *file_name;

	LP = (USER_LOG *)malloc( sizeof(USER_LOG) );

		/* Save parameter info */
	LP->path = malloc( strlen(file) + 1 );
	strcpy( LP->path, file );
	LP->cluster = c;
	LP->proc = p;
	LP->subproc = s;
	LP->in_block = FALSE;
	LP->locked = FALSE;

		/* Open a zero length file on the local filesystem (hopefully) to use
		 * as a lock file, since the log file itself may be over NFS/AFS, and
		 * file locking over NFS is often problematic at best -Todd 7/95 */
	if ( ULogLockFd < 0 ) {
    	file_name = param( "USERLOG_LOCK" );
    	if( !file_name ) {
        	EXCEPT( "No USERLOG_LOCK file specified in config file\n" );
    	}
    	if( (ULogLockFd = open(file_name,O_RDWR|O_CREAT,0666)) < 0 ) {
        	EXCEPT( "Cannot open User Log lock file \"%s\"", file_name );
    	}
		free( file_name );
	}

		/* Look up process owner's UID and GID */
	if( (pwd = getpwnam(owner)) == NULL ) {
		EXCEPT( "Can't find \"%s\" in passwd file", owner );
	}
	LP->user_uid = pwd->pw_uid;
	LP->user_gid = pwd->pw_gid;

		/* create the file */
	set_user_id( LP );
	if( (LP->fd = open( LP->path, O_CREAT | O_WRONLY, 0644 )) < 0 ) {
		EXCEPT( "%s", LP->path );
	}

		/* attach it to stdio stream */
	if( (LP->fp = fdopen(LP->fd,"w")) == NULL ) {
		EXCEPT( "fdopen(%d)", LP->fd );
	}

		/* set the stdio stream for line buffering */
	if( setvbuf(LP->fp,NULL,_IOLBF,BUFSIZ) < 0 ) {
		EXCEPT( "setvbuf" );
	}

	ULogLockCount++;	/* counter so we know when we can close the lock file */

		/* get back to whatever UID and GID we started with */
	restore_id( LP );
	return LP;
}


/*
  Delete a user log "instance".
*/
void
CloseUserLog( USER_LOG *LP )
{
	if( !LP ) {
		return;
	}
	release_lock( LP );
	fclose( LP->fp );
	free( LP->path );

	/* If we are closing the last user log instance, then close lock file as well */
	ULogLockCount--;
	if ( ULogLockCount == 0 ) {
		close( ULogLockFd );
		ULogLockFd = -1;
	}
}

/*
  Display content of a user log "instance" for debugging purposes.
*/
void
DisplayUserLog( USER_LOG *LP )
{
	if( !LP ) {
		return;
	}
	dprintf( D_ALWAYS, "Path = \"%s\"\n", LP->path );
	dprintf( D_ALWAYS,
		"user_uid = %d, saved_uid = %d\n", LP->user_uid, LP->saved_uid );
	dprintf( D_ALWAYS,
		"user_gid = %d, saved_gid = %d\n", LP->user_gid, LP->saved_gid );
	dprintf( D_ALWAYS, "Job = %d.%d.%d\n", LP->proc, LP->cluster, LP->subproc );
	dprintf( D_ALWAYS, "fp = 0x%x\n", LP->fp );
	dprintf( D_ALWAYS, "fd = %d\n", LP->fd );
	dprintf( D_ALWAYS, "in_block = %s\n", LP->in_block ? "TRUE" : "FALSE" );
	dprintf( D_ALWAYS, "locked = %s\n", LP->locked ? "TRUE" : "FALSE" );
}

/*
  Put a single line of output into a user log.  The "fmt" and subsequent
  arguments are as provided by  printf().
*/
void
PutUserLog( USER_LOG *LP, int msg_num, ... )
{
	va_list		ap;
	char		*fmt;

	va_start( ap, msg_num );


	if( !LP ) {
		return;
	}

	fmt = find_fmt( msg_num );
	if( !fmt ) {
		EXCEPT( "Unknown msg number (%d)\n", msg_num );
	}

	if( !LP->in_block ) {
		get_lock( LP );
		fseek( LP->fp, 0, SEEK_END );
	}

	output_header( LP, msg_num );
	vfprintf( LP->fp, fmt, ap );
	putc( '\n', LP->fp );

	if( !LP->in_block ) {
		release_lock( LP );
	}
}

/*
  Begin a block of output to a user log.  A lock is maintained during
  the output of the entire block.
*/
void
BeginUserLogBlock( USER_LOG *LP )
{
	struct tm	*tm;
	time_t		clock;

	if( !LP ) {
		return;
	}

	get_lock( LP );
	fseek( LP->fp, 0, SEEK_END );

	LP->in_block = TRUE;
}

/*
  End a block of output (releases the lock ).
*/
void
EndUserLogBlock( USER_LOG *LP )
{
	if( !LP ) {
		return;
	}

	LP->in_block = FALSE;
	release_lock( LP );
}

/*
  Output a timestamp and Condor process id.
*/
static void
output_header( USER_LOG *LP, int msg_num )
{
	struct tm	*tm;
	time_t		clock;

	if( !LP ){
		return;
	}

	(void)time(  (time_t *)&clock );
	tm = localtime( (time_t *)&clock );
	fprintf( LP->fp, MSG_HDR, msg_num,
		LP->cluster, LP->proc, LP->subproc,
		tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec
	);
}

/*
  Set effective uid to that of the owner of the file.
*/
static void
set_user_id( USER_LOG *LP )
{
	LP->saved_uid = geteuid();
	LP->saved_gid = getegid();

#if 0
	if( LP->saved_uid != LP->user_uid || LP->saved_gid != LP->user_gid ) {
#else
	if( LP->saved_uid != LP->user_uid ) {
#endif
		switch_ids( LP->user_uid, LP->user_gid );
	}
}

/*
  Reset effective uid to whatever it was when we started.
*/
static void
restore_id( USER_LOG *LP )
{
	if( LP->saved_uid != LP->user_uid || LP->saved_gid != LP->saved_gid ) {
		switch_ids( LP->saved_uid, LP->saved_gid );
	}
}

/*
  Switch process's effective user and group ids as desired.
*/
static void
switch_ids( uid_t new_euid, gid_t new_egid )
{
		/* First set euid to root so we have privilege to do this */
	if( seteuid(0) < 0 ) {
		fprintf( stderr, "Can't set euid to root\n" );
	/*	exit( errno ); don't want to exit here in case of non-root condor */
	}

#if 0
		/* Now set the egid as desired */
	if( setegid(new_egid) < 0 ) {
		fprintf( stderr, "Can't set egid to %d\n", new_egid );
		exit( errno );
	}
#endif

		/* Now set the euid as desired */
	if( seteuid(new_euid) < 0 ) {
		fprintf( stderr, "Can't set euid to %d\n", new_euid );
		/*exit( errno ); don't want to exit here in case of non-root Condor*/
	}
}

/*
  Display user id's for debugging purposes.
*/
static void
display_ids()
{
	fprintf( stderr, "ruid = %d, euid = %d\n", getuid(), geteuid() );
	fprintf( stderr, "rgid = %d, egid = %d\n", getgid(), getegid() );
}

/*
  Obtain a write lock on the file (blocks).
*/
static void
get_lock( USER_LOG *LP )
{
	if( LP->locked ) {
		return;
	}
	lock_file( ULogLockFd, WRITE_LOCK, TRUE );
	LP->locked = TRUE;
}

/*
  Release any lock on the file.
*/
static void
release_lock( USER_LOG *LP )
{
	if( !LP->locked ) {
		return;
	}
	lock_file( ULogLockFd, UN_LOCK, TRUE );
	LP->locked = FALSE;
}

static char *
find_fmt( int msg_num )
{
	USR_MSG	*ptr;

	for( ptr=MsgCatalog; ptr->num >= 0; ptr++ ) {
		if( ptr->num == msg_num ) {
			return ptr->fmt;
		}
	}
	return NULL;
}
