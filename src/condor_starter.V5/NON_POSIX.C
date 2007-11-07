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


/*
Portability
	The code in this file is not POSIX.1 conforming, and is
	therefore not generally portable.  However, this code is
	generally portable across UNIX platforms with little or no
	modification.
*/
	
#include "condor_common.h"
#include "condor_debug.h"
#include "startup.h"
#include "user_proc.h"


/*
Controlling any of these limits is problematic because the system calls
setrlimit() and getrlimit() do not appear in POSIX.1, but leaving them
to chance in environments where they appear is unacceptable.  Defaults
limits will frequently be handed down by some shell whose
initialization file has been set up by the system administrator.  These
will often not be appropriate for condor operation.  For example the
administrator may have set the limit for COREDUMPSIZE to 0.

Additional complication results from the fact that various platforms
define different sets of limits which may be set.  Here we define a
routine for setting an individual limit.  Code in platform specific
files will call this routine to set up limits as appropriate for that
platform.

Note: the "limit" function that is used to set an individual limit has
been moved to the c++ util lib since it is needed by daemonCore.
-Derek Wright <wright@cs.wisc.edu> 5/19/98
*/


/*
** Return physical file size in 1024 byte units.  This is the number of
** 1024 byte blocks required to store the file.  This could be more than the
** number of blocks of data due to overhead imposed by the file system
** structure (indirect blocks).  It could also be less than the
** number of data blocks if the filesystem implements "gaps" in the file
** without allocating real disk blocks to them.  This is a traditional
** behavior for UNIX file systems, but is optional in POSIX.
**
** The POSIX definition of "struct stat" does not define any member
** which describes the physical storage requirements of the file, but
** many UNIX systems do have such a member in their stat structure.
** Generally the name of this member is "st_blocks", but the size of
** the blocks is not often documented.  I have seen some code that uses
** the "st_blocksize" member for this value, but that turns out not to
** be generally correct.  The following interpretations come from comparing
** the stat structures associated with actual files against the storage
** requirements of those files as reported by "ls -ls".
*/
off_t
physical_file_size( char *name )
{
	struct stat	buf;

	if( stat(name,&buf) < 0 ) {
		return (off_t)-1;
	}


#if defined(AIX) || defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41)|| defined(OSF1) || defined(Solaris) || defined(IRIX) || defined(Darwin) || defined(CONDOR_FREEBSD)

	   /*  On these systems struct stat member st_blocks is
	       defined, and appears to be in 512 byte blocks. */
	return buf.st_blocks / 2;

#elif defined(HPUX) || defined(LINUX)

	/*  On this systems struct stat member st_blocks is
	   defined, and appears to be in 1024 byte blocks. */

	return buf.st_blocks;

#else
#	error "Don't know how to determine file size on this platform."
#endif
}

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * macros for setting stat
 */
#define SETCOREDUMP(stat, flag)         ((int)((flag) ? ((stat) | WCOREFLG) \
                                                : ((stat) & (~(WCOREFLG)))))
#define SETEXITSTATUS(stat, flag)       ((int)(((stat) & 0xFFFF00FF) | \
                                                ((flag) << 8)))
#define SETTERMSIG(stat, flag)          ((int)(((stat) & 0xFFFFFF80) | \
                                                ((flag) & 0x7F)))

#ifdef  __cplusplus
}
#endif

/*
Encode the status of a user process in the way the shadow is expecting
it.  The encoding is a BSD style "union wait" structure with the
exit status of the process and a few goodies thrown in for special
cases.  We keep the encoding in a static memory area and return a
pointer to it as a "void *" so that POSIX code doesn't have to know
about it.
Changed the union wait to int to be POSIX conformant - 12/01/97
*/
void *
bsd_status( int posix_st, PROC_STATE state, int ckpt_trans, int core_trans )
{
	static int	bsd_status;

	memcpy( &bsd_status, &posix_st, sizeof(bsd_status) );

	switch( state ) {

	  case EXECUTING:
		EXCEPT( "Tried to generate BSD status, but state is EXECUTING\n" );
		break;

	  case SUSPENDED:
		EXCEPT( "Tried to generate BSD status, but state is SUSPENDED\n" );
		break;

	  case NORMAL_EXIT:
		dprintf( D_ALWAYS, "STATUS encoded as NORMAL\n" );
		break;

	  case NEW:
	  case RUNNABLE:
	  case NON_RUNNABLE:
	  case CHECKPOINTING:
	  case CANT_FETCH:
		if( ckpt_trans ) {
			bsd_status = SETTERMSIG(bsd_status, SIGQUIT);
			dprintf( D_ALWAYS, "STATUS encoded as CKPT, *IS* TRANSFERRED\n" );
		} else {
			bsd_status = SETTERMSIG(bsd_status, SIGKILL);
			dprintf( D_ALWAYS, "STATUS encoded as CKPT, *NOT* TRANSFERRED\n" );
		}
		break;

	  case ABNORMAL_EXIT:
		if( core_trans ) {
			bsd_status = SETCOREDUMP(bsd_status, 1);
			dprintf( D_ALWAYS, "STATUS encoded as ABNORMAL, WITH CORE\n" );
		} else {
			bsd_status = SETCOREDUMP(bsd_status, 0);
			dprintf( D_ALWAYS, "STATUS encoded as ABNORMAL, NO CORE\n" );
		}
		break;

	  case BAD_MAGIC:
		bsd_status = SETTERMSIG(bsd_status, 0);
		bsd_status = SETCOREDUMP(bsd_status, 1);
		bsd_status = SETEXITSTATUS(bsd_status, ENOEXEC);
		dprintf( D_ALWAYS, "STATUS encoded as BAD MAGIC\n" );
		break;

	  case BAD_LINK:
		bsd_status = SETTERMSIG(bsd_status, 0);
		bsd_status = SETCOREDUMP(bsd_status, 1);
		bsd_status = SETEXITSTATUS(bsd_status, 0);
		dprintf( D_ALWAYS, "STATUS encoded as BAD LINK\n" );
		break;

/*
	  case CANT_FETCH:
		bsd_status = SETTERMSIG(bsd_status, 0);
		bsd_status = SETCOREDUMP(bsd_status, 1);
		bsd_status = SETEXITSTATUS(bsd_status, posix_st);
*/

	}
	return (void *)&bsd_status;
}



/*
Convert a time value from the POSIX style "clock_t" to a BSD style
"struct timeval".
*/
void
clock_t_to_timeval( clock_t ticks, struct timeval &tv )
{
	static long clock_tick;

	if( !clock_tick ) {
		clock_tick = sysconf( _SC_CLK_TCK );
	}

	tv.tv_sec = ticks / clock_tick;
	tv.tv_usec = ticks % clock_tick;
	tv.tv_usec *= 1000000 / clock_tick;
}

/*
Encode the CPU usage statistics in a BSD style "rusage" structure.  The
structure contains much more information than just CPU times, but we
zero everything except the CPU information because POSIX doesn't provide
it.  We return a pointer to a static data area as a "void *" so
POSIX code doesn't have to know about it.
*/
void *
bsd_rusage( clock_t user, clock_t sys )
{
	static struct rusage bsd_rusage;

	memset( &bsd_rusage, '\0', sizeof(bsd_rusage) );

	clock_t_to_timeval( user, bsd_rusage.ru_utime );
	clock_t_to_timeval( sys, bsd_rusage.ru_stime );

	dprintf( D_ALWAYS, "User time = %ld.%06ld seconds\n",
				(long)bsd_rusage.ru_utime.tv_sec, 
				(long)bsd_rusage.ru_utime.tv_usec );
	dprintf( D_ALWAYS, "System time = %ld.%06ld seconds\n",
				(long)bsd_rusage.ru_stime.tv_sec, 
				(long)bsd_rusage.ru_stime.tv_usec );

	return (void *)&bsd_rusage;
}

/*
  Tell the operating system that we want to operate in "POSIX
  conforming mode".  Isn't it ironic that such a request is itself
  non-POSIX conforming?
*/

void
set_posix_environment() {}

