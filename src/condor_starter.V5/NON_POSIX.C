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
Portability
	The code in this file is not POSIX.1 conforming, and is
	therefore not generally portable.  However, this code is
	generally portable across UNIX platforms with little or no
	modification.
*/
	

#define _POSIX_SOURCE

#if defined(ULTRIX42) || defined(ULTRIX43)
typedef char *  caddr_t;
#endif

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_jobqueue.h"
#include "condor_sys.h"
#include <signal.h>
#include <sys/stat.h>

extern "C" {
	int SetSyscalls( int );
}

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

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
*/
void limit( int resource, rlim_t new_limit )
{
	int		scm;
	struct	rlimit lim;

	scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );


		/* Find out current limits */
	if( getrlimit(resource,&lim) < 0 ) {
		EXCEPT( "getrlimit(%d,0x%x)", resource, &lim );
	}

		/* Don't try to exceed the max */
	if( new_limit > lim.rlim_max ) {
		new_limit = lim.rlim_max;
	}

		/* Set the new limit */
	lim.rlim_cur = new_limit;
	if( setrlimit(resource,&lim) < 0 ) {
		EXCEPT( "setrlimit(%d,0x%x)", resource, &lim );
	}

	(void)SetSyscalls( scm );
}

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


#if defined(AIX32) || defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41)|| defined(OSF1)

	   /*  On these systems struct stat member st_blocks is
	       defined, and appears to be in 512 byte blocks. */
	return buf.st_blocks / 2;

#elif defined(HPUX9)

	/*  On this systems struct stat member st_blocks is
	   defined, and appears to be in 1024 byte blocks. */

	return buf.st_blocks;

#elif defined(IRIX401)

	/*  On this system struct stat member st_blocks is not defined.
		Also, it appears that gaps take up real blocks here.  We just
		return the number of blocks to store the data.  Probably
		there will be some overhead, but it's not clear how to
		determine what it is, so we ignore it.  */

	return (buf.st_size + 1023 ) / 1024;
#else
#	error "Don't know how to determine file size on this platform."
#endif
}

#if defined(AIX32)
#	undef _POSIX_SOURCE
#	define _BSD
#	include <sys/m_wait.h>
#elif defined(OSF1)
#	undef _POSIX_SOURCE
#	define _BSD
#	define _OSF_SOURCE
#	include <sys/wait.h>
#elif defined(SUNOS41)
#	undef _POSIX_SOURCE
#	include <sys/wait.h>
#elif defined(ULTRIX43)
#	undef _POSIX_SOURCE
#	include <sys/wait.h>
#elif defined(HPUX9)
#	undef _POSIX_SOURCE
#	define _BSD
#	include <sys/wait.h>
#endif


#include <errno.h>
#include "condor_constants.h"
#include "startup.h"
#include "user_proc.h"

/*
Encode the status of a user process in the way the shadow is expecting
it.  The encoding is a BSD style "union wait" structure with the
exit status of the process and a few goodies thrown in for special
cases.  We keep the encoding in a static memory area and return a
pointer to it as a "void *" so that POSIX code doesn't have to know
about it.
*/
void *
bsd_status( int posix_st, PROC_STATE state, int ckpt_trans, int core_trans )
{
	static union wait	bsd_status;

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
		if( ckpt_trans ) {
			bsd_status.w_termsig = SIGQUIT;
			dprintf( D_ALWAYS, "STATUS encoded as CKPT, *IS* TRANSFERRED\n" );
		} else {
			bsd_status.w_termsig = SIGKILL;
			dprintf( D_ALWAYS, "STATUS encoded as CKPT, *NOT* TRANSFERRED\n" );
		}
		break;

	  case ABNORMAL_EXIT:
		if( core_trans ) {
			bsd_status.w_coredump = 1;
			dprintf( D_ALWAYS, "STATUS encoded as ABNORMAL, WITH CORE\n" );
		} else {
			bsd_status.w_coredump = 0;
			dprintf( D_ALWAYS, "STATUS encoded as ABNORMAL, NO CORE\n" );
		}
		break;

	  case BAD_MAGIC:
		bsd_status.w_termsig = 0;
		bsd_status.w_coredump = 1;
		bsd_status.w_retcode = ENOEXEC;
		dprintf( D_ALWAYS, "STATUS encoded as BAD MAGIC\n" );
		break;

	  case BAD_LINK:
		bsd_status.w_termsig = 0;
		bsd_status.w_coredump = 1;
		bsd_status.w_retcode = 0;
		dprintf( D_ALWAYS, "STATUS encoded as BAD LINK\n" );
		break;

	  case CANT_FETCH:
		bsd_status.w_termsig = 0;
		bsd_status.w_coredump = 1;
		bsd_status.w_retcode = posix_st;

	}
	return (void *)&bsd_status;
}



#include <sys/resource.h>
#define _POSIX_SOURCE
#include <time.h>		// to get CLK_TCK
#undef _POSIX_SOURCE

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
	int		sec, usec;

	memset( &bsd_rusage, '\0', sizeof(bsd_rusage) );

	clock_t_to_timeval( user, bsd_rusage.ru_utime );
	clock_t_to_timeval( sys, bsd_rusage.ru_stime );

	dprintf( D_ALWAYS, "User time = %d.%06d seconds\n",
				bsd_rusage.ru_utime.tv_sec, bsd_rusage.ru_utime.tv_usec );
	dprintf( D_ALWAYS, "System time = %d.%06d seconds\n",
				bsd_rusage.ru_stime.tv_sec, bsd_rusage.ru_stime.tv_usec );

	return (void *)&bsd_rusage;
}

/*
  Tell the operating system that we want to operate in "POSIX
  conforming mode".  Isn't it ironic that such a request is itself
  non-POSIX conforming?
*/
#if defined(ULTRIX42) || defined(ULTRIX43)
#include <sys/sysinfo.h>
#define A_POSIX 2		// from exec.h, which I prefer not to include here...
extern "C" {
	int setsysinfo( unsigned, char *, unsigned, unsigned, unsigned );
}
void
set_posix_environment()
{
	int		name_value[2];

	name_value[0] = SSIN_PROG_ENV;
	name_value[1] = A_POSIX;

	if( setsysinfo(SSI_NVPAIRS,(char *)name_value,1,0,0) < 0 ) {
		EXCEPT( "setsysinfo" );
	}
}
#else
void
set_posix_environment() {}
#endif
