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

 

/*******************************************************************
  System call stubs which need special treatment and cannot be generated
  automatically go here...
*******************************************************************/
#include "condor_common.h"
#include "syscall_sysdep.h"

/* Since we've included condor_common.h, we know off64_t has been
   defined, so we want to define this macro so the syscall_64bit.h
   header file doesn't try to redefine off64_t.  In other places, we 
   include syscall_64bit.h w/o condor_common.h, so it needs to define
   off64_t in that case. -Derek W. 8/20/98 */
#define _CONDOR_OFF64_T
#include "syscall_64bit.h"
#undef _CONDOR_OFF64_T

#if defined(LINUX)
int _xstat(int, const char *, struct stat *);
int _fxstat(int, int, struct stat *);
int _lxstat(int, const char *, struct stat *);
int __xstat(int, const char *, struct stat *);
int __fxstat(int, int, struct stat *);
int __lxstat(int, const char *, struct stat *);
#endif /* LINUX */

#include "syscall_numbers.h"
#include "condor_syscall_mode.h"
#include "file_table_interf.h"

#if defined(HPUX)
#	ifdef _PROTOTYPES   /* to remove compilation errors unistd.h */
#	undef _PROTOTYPES
#	endif
#endif

#include "debug.h"

#if defined(DL_EXTRACT)
#   include <dlfcn.h>   /* for dlopen and dlsym */
#endif

extern unsigned int _condor_numrestarts;  /* in image.C */

static int fake_readv( int fd, const struct iovec *iov, int iovcnt );
static int fake_writev( int fd, const struct iovec *iov, int iovcnt );
char	*getwd( char * );
void    *malloc();     
int	_condor_open( const char *path, int flags, va_list ap );

/*
  The process should exit making the status value available to its parent
  (the starter) - can only be a local operation.
*/
void
_exit( status )
int status;
{
	(void) syscall( SYS_exit, status );
}

/*
  The Open Files Table module needs to keep the current working
  directory in memory so that it can remember pathnames of files that
  are opened, even if they are opened by simple file names.  We first
  try to do the indicated change of directory, and then if that works,
  we record the new current working directory.
*/
int
chdir( const char *path )
{
	int rval, status;
	char	tbuf[ _POSIX_PATH_MAX ];

		/* Try the system call */
	if( LocalSysCalls() ) {
		rval = syscall( SYS_chdir, path );
	} else {
		rval = REMOTE_syscall( CONDOR_chdir, path );
	}

		/* If it fails we can stop here */
	if( rval < 0 ) {
		return rval;
	}

	if( MappingFileDescriptors ) {
		store_working_directory();
	}

	return rval;
}

#if defined(SYS_fchdir)
int
fchdir( int fd )
{
	int rval, real_fd;

	if( MappingFileDescriptors() ) {
		if( (real_fd = MapFd(fd)) < 0 ) {
			return -1;
		}
	}

		/* Try the system call */
	if( LocalSysCalls() ) {
		rval = syscall( SYS_fchdir, real_fd );
	} else {
		rval = REMOTE_syscall( CONDOR_fchdir, real_fd );
	}

		/* If it fails we can stop here */
	if( rval < 0 ) {
		return rval;
	}

	if( MappingFileDescriptors ) {
		store_working_directory();
	}

	return rval;
}
#endif

/*
Keep Condor's version of the CWD up to date
*/
store_working_directory()
{
	char	tbuf[ _POSIX_PATH_MAX ];
	char	*status;

#if defined(HPUX)
	/* avoid infinite recursion in HPUX9, where getwd calls chdir, which
	   calls store_working_directory, which calls getwd...  - Jim B. */
	static int inside_call = 0;

	if (inside_call)
		return;
	inside_call = 1;
#endif

		/* Get the information */
	status = getcwd( tbuf, sizeof(tbuf) );

		/* This routine returns 0 on error! */
	if( !status ) {
		dprintf( D_ALWAYS, "getcwd failed in store_working_directory()!\n" );
		Suicide();
	}

		/* Ok - everything worked */
	Set_CWD( tbuf );

#if defined(HPUX)
	inside_call = 0;
#endif
}

/*
  getrusage()

  Condor doesn't support the fork() system call, so by definition the
  resource usage of all our child processes is zero.  We must support
  this, since those users in the POSIX world will call utimes() which
  probably checks resource ruage for children - even though there are
  none.

  In the remote case things are a bit more complicated.  The rusage
  should he the sum of what the user process has accumulated on the
  current machine, and the usages it accumulated on all the machines
  where it has run in the past.
*/
int
getrusage( int who, struct rusage *rusage )
{
	int rval = 0;
	int rval1 = 0;
	int scm;
	static struct rusage accum_rusage;
	static int num_restarts = 50;  /* must not initialize to 0 */

	/* Get current rusage for this process accumulated on this machine */

	/* Set syscalls to local, since getrusage() in libc often calls
	 * things like _open(), and we wish to avoid an infinite loop
 	 */
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
		
#if defined( SYS_getrusage )
		rval1 = syscall( SYS_getrusage, who, rusage);
#elif defined( DL_EXTRACT )
		{
        void *handle;
        int (*fptr)(int,struct rusage *);
        if ((handle = dlopen("/usr/lib/libc.so", RTLD_LAZY)) == NULL) {
            rval = -1;
        } else {
        	if ((fptr = (int (*)(int,struct rusage *))dlsym(handle, "getrusage")) == NULL) {
           		 rval = -1;
        	} else {
        		rval1 = (*fptr)(who,rusage);
			}
		}
		}
#else
		rval1 = GETRUSAGE(who,rusage);
#endif 

	/* Set syscalls back to what it was */
	SetSyscalls(scm);

	/* If in remote mode, we need to add in resource usage from previous runs as well */
	if( !LocalSysCalls() ) {

		/* Condor user processes don't have children - yet */
		if( who != RUSAGE_SELF ) {
			memset( (char *)rusage, '\0', sizeof(struct rusage) );
			return 0;
		}

		/* If our local getrusage above was successful, query the shadow 
		 * for past usage */
		if ( rval1 == 0 ) {  
			/*
			 * Get accumulated rusage from previous runs, but only once per
			 * restart instead of doing a REMOTE_syscall every single time
			 * getrusage() is called, which can be very frequently.
			 * Note: _condor_numrestarts is updated in the restart code in
			 * image.C whenver Condor does a restart from a checkpoint.
			 */
			if ( _condor_numrestarts != num_restarts ) {
				num_restarts = _condor_numrestarts;
				rval = REMOTE_syscall( CONDOR_getrusage, who, &accum_rusage );
				/* on failure, clear out accum_rusage so we do not blow up
				 * inside of update_rusage()
				 */
				if ( rval != 0 ) {
					memset( (char *)&accum_rusage, '\0', sizeof(struct rusage) );
				}
			}

			/* Sum up current rusage and past accumulated rusage */
			update_rusage(rusage, &accum_rusage);
		}
	}

	if ( rval == 0  && rval1 == 0 ) {
		return 0;
	} else {
		return -1;
	}
}

/*
  This routine which is normally provided in the C library determines
  whether a given file descriptor refers to a tty.  The underlying
  mechanism is an ioctl, but Condor does not support ioctl.  We
  therefore provide a "quick and dirty" substitute here.  This may
  not always be correct, but it will get you by in most cases.
*/
int _isatty( int filedes )
{
		/* Stdin, stdout, and stderr are redirected to normal
		** files for all condor jobs.
		*/
	if( RemoteSysCalls() ) {
		return FALSE;
	}

		/* Assume stdin, stdout, and stderr are the only ttys */
	switch( filedes ) {
		case 0:
		case 1:
		case 2:
				return TRUE;
		default:
			return FALSE;
	}
}

int isatty( int fd )
{
	return _isatty(fd);
}

/* Same purpose as isatty, but asks if tape.  Present in Fortran library.
   I don't know if this is the real function format, but it's a good guess. */

int __istape( int fd )
{
        return 0;
}

/* Force the dummy functions to be loaded and linked.  Even if the user's code
   does not make use of it, we want to override the C library version.  */

void _condor_force_isatty( int fd )
{
        isatty(fd);
        __istape(fd);
}

/*
  We don't handle readv directly in remote system calls.  Instead we
  break the readv up into a series of individual reads.
*/
#if defined(HPUX9) || defined(LINUX) 
ssize_t
readv( int fd, const struct iovec *iov, size_t iovcnt )
#elif defined(IRIX62) || defined(OSF1)|| defined(HPUX10) || defined(Solaris26)
ssize_t
readv( int fd, const struct iovec *iov, int iovcnt )
#else
int
readv( int fd, struct iovec *iov, int iovcnt )
#endif
{
	int rval;
	int user_fd;

	if( (user_fd=MapFd(fd)) < 0 ) {
		return -1;
	}

	if( LocalSysCalls() ) {
		rval = syscall( SYS_readv, user_fd, iov, iovcnt );
	} else {
		rval = fake_readv( user_fd, iov, iovcnt );
	}

	return rval;
}

/*
  Handle a call to readv which must be done remotely, (not NFS).  Note
  the user file descriptor has already been mapped by the calling routine,
  and we already know this is a remote system call.  We handle the call
  by breaking it up into a series of remote reads.
*/
static int
fake_readv( int fd, const struct iovec *iov, int iovcnt )
{
	register int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = REMOTE_syscall( CONDOR_read, fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) {
			return cc;
		}

		rval += cc;
		if( cc != iov->iov_len ) {
			return rval;
		}

		iov++;
	}

	return rval;
}

#if 0
/*Fake local readv for Linux						*/
static int
linux_fake_readv( int fd, const struct iovec *iov, int iovcnt )
{
	register int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = syscall( SYS_read, fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) {
			return cc;
		}

		rval += cc;
		if( cc != iov->iov_len ) {
			return rval;
		}

		iov++;
	}

	return rval;
}
#endif


/*
  We don't handle writev directly in remote system calls.  Instead we
  break the writev up into a series of individual writes.
*/
#if defined(HPUX9) || defined(LINUX) 
ssize_t
writev( int fd, const struct iovec *iov, size_t iovcnt )
#elif defined(Solaris) || defined(IRIX62) || defined(OSF1) || defined(HPUX10)
ssize_t
writev( int fd, const struct iovec *iov, int iovcnt )
#else
int
writev( int fd, struct iovec *iov, int iovcnt )
#endif
{
	int rval;
	int user_fd;

	if( (user_fd=MapFd(fd)) < 0 ) {
		return -1;
	}

	if( LocalSysCalls() ) {
		rval = syscall( SYS_writev, user_fd, iov, iovcnt );
	} else {
		rval = fake_writev( user_fd, iov, iovcnt );
	}

	return rval;
}

/*
  Handle a call to writev which must be done remotely, (not NFS).  Note
  the user file descriptor has already been mapped by the calling routine,
  and we already know this is a remote system call.  We handle the call
  by breaking it up into a series of remote writes.
*/
static int
fake_writev( int fd, const struct iovec *iov, int iovcnt )
{
	register int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = REMOTE_syscall( CONDOR_write, fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) {
			return cc;
		}

		rval += cc;
		if( cc != iov->iov_len ) {
			return rval;
		}

		iov++;
	}

	return rval;
}

#if 0
/*Fake local writev for Linux						*/
static int
linux_fake_writev( int fd, const struct iovec *iov, int iovcnt )
{
	register int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = syscall( SYS_write, fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) {
			return cc;
		}

		rval += cc;
		if( cc != iov->iov_len ) {
			return rval;
		}

		iov++;
	}

	return rval;
}
#endif


#if defined(HPUX)
#	include <sys/mib.h>
#	include <sys/ioctl.h>
#endif

#if defined(Solaris) || defined(LINUX) || defined(OSF1) || defined(IRIX53) || defined(HPUX10)
/* int
ioctl( int fd, int request, ...) */
#else
int
ioctl( int fd, int request, caddr_t arg )
{
#if defined(HPUX9) && 0  /* leave this out cuz it breaks Fortran on HPUX9 */
	static int first_time = 1;
	static int MaxOpenFiles;
	static struct nmparms *parmset;
	if (first_time) {
		first_time = 0;
		MaxOpenFiles = sysconf(_SC_OPEN_MAX);
		parmset = (struct nmparms *)calloc(sizeof(struct nmparms),
										   MaxOpenFiles);
	}
	if (request == 0x40206d02) {
		struct nmparms *ptr = (struct nmparms *)arg;
		dprintf( D_ALWAYS, "got NMIOSET request 0x%x on fd %d\n",
				request, fd );
		dprintf( D_ALWAYS, "objid = %d, buffer = 0x%x, len = 0x%x\n",
				ptr->objid, ptr->buffer, ptr->len );
		parmset[fd].objid = ptr->objid;
		parmset[fd].buffer = ptr->buffer;
		parmset[fd].len = ptr->len;
		return 0;
	}
	else if (request == 0x80086d01) {
		struct nmparms *ptr = (struct nmparms *)arg;
		dprintf( D_ALWAYS, "got NMIOGET request 0x%x on fd %d\n",
				request, fd);
		ptr->objid = parmset[fd].objid;
		ptr->buffer = parmset[fd].buffer;
		ptr->len = parmset[fd].len;
		dprintf( D_ALWAYS, "objid = %d, buffer = 0x%x, len = 0x%x\n",
				ptr->objid, ptr->buffer, ptr->len );
		return 0;
	}		
#endif   /* HPUX9 */
	switch( request ) {
#if defined(SUNOS41)
	  case MTIOCGET:
		fprintf( stderr, "Got mag tape request - reply -1\n" );
		return -1;
	  case TCGETA:
		fprintf( stderr, "Got terminal IO request - reply -1\n" );
		return -1;
#endif
	  default:
		fprintf( stderr, "Got (unknown) ioctl 0x%x on fd %d - reply -1\n",
															request, fd );
		return -1;
	}
}
#endif

int
#if defined(LINUX)
ftruncate( int fd, size_t length )
#else
ftruncate( int fd, off_t length )
#endif
{
	int	rval;
	int	user_fd;
	int use_local_access = FALSE;

	/* The below length check is a hack to patch an f77 problem on
	   OSF1.  - Jim B. */

	if (length < 0)
		return 0;

	if( (user_fd=MapFd(fd)) < 0 ) {
		return (int)-1;
	}
	if( LocalAccess(fd) ) {
		use_local_access = TRUE;
	}

	if( LocalSysCalls() || use_local_access ) {
#if defined( SYS_ftruncate )
		rval = syscall( SYS_ftruncate, user_fd, length );
#else 
		rval = FTRUNCATE( user_fd, length );
#endif
	} else {
		rval = REMOTE_syscall( CONDOR_ftruncate, user_fd, length );
	}

	return rval;
}

#if defined(AIX32)
	int
	kwritev( int fd, struct iovec *iov, int iovcnt, int ext )
	{
		int rval;
		int user_fd;

		if( ext != 0 ) {
			errno = ENOSYS;
			return -1;
		}

		if( (user_fd=MapFd(fd)) < 0 ) {
			return -1;
		}

		if( LocalSysCalls() ) {
			rval = syscall( SYS_kwritev, user_fd, iov, iovcnt );
		} else {
			rval = fake_writev( user_fd, iov, iovcnt );
		}

		return rval;
	}
#endif

#if defined(AIX32)
	int
	kreadv( int fd, struct iovec *iov, int iovcnt, int ext )
	{
		int rval;
		int user_fd;

		if( ext != 0 ) {
			errno = ENOSYS;
			return -1;
		}

		if( (user_fd=MapFd(fd)) < 0 ) {
			return -1;
		}

		if( LocalSysCalls() ) {
			rval = syscall( SYS_kreadv, user_fd, iov, iovcnt );
		} else {
			rval = fake_readv( user_fd, iov, iovcnt );
		}

		return rval;
	}
#endif

#if defined OSF1
/*
  This is some kind of cleanup routine for dynamically linked programs which
  is called by exit.  For some reason it occasionally cuases a SEGV
  when mixed with the condor checkpointing code.  Since condor programs
  are always statically linked, we just make a dummy here to avoid
  the problem.
*/
void ldr_atexit() {}
#endif /* defined OSF1 */

/* This has been added to solve the problem with the printf and fprintf 
statements on alphas but have not enclosed it in ifdefs since it will not
harm to other platforms */

#if !defined(LINUX)
__write(int fd, char *buf, int size)
{
	return write(fd,buf,size);
}
#endif

/* These are similar additions as above.  This problem cropped up for 
   FORTRAN programs on Solaris 2.4 and OSF1.  -Jim B. */

#if !defined(HPUX)
#if defined( SYS_write )
_write( int fd, const void *buf, size_t len )
{
	return write(fd,buf,len);
}
#endif

#if defined( SYS_read )
_read( int fd, void *buf, size_t len )
{
	return read(fd,buf,len);
}

#if !defined(LINUX)
__read( int fd, void *buf, size_t len )
{
	return read(fd,buf,len);
}
#endif /* !defined(LINUX) */
#endif /* defined(SYS_read) */

#if defined( SYS_lseek )
off_t
_lseek( int fd, off_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

#if !defined(LINUX)
__lseek( int fd, off_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

#endif /* !defined(LINUX) */
#endif /* defined(SYS_lseek) */

#endif /* !defined(HPUX) */


#if defined(LINUX)
/* 
   Whole bunch of Linux-specific evilness w/ [_fxl]*stat.  Basically,
   we need to trap all these things and send them over the wire as
   CONDOR_stat, CONDOR_fstat, or CONDOR_lstat as appropriate, and
   handle them locally with whatever SYS_*stat is defined on this
   host.  glibc and libc5 do this differently, which is why we need
   the pre-processor junk in the local case.  -Derek Wright 3/30/99 
*/

int _xstat(int version, const char *path, struct stat *buf)
{
	return _condor_xstat( version, path, buf );
}


int __xstat(int version, const char *path, struct stat *buf)
{
	return _condor_xstat( version, path, buf );
}


int _condor_xstat(int version, const char *path, struct stat *buf)
{
    int rval;

    if( LocalSysCalls() ) {
#if defined(SYS_prev_stat)
        rval = syscall( SYS_prev_stat, path, buf );
#elif defined(SYS_stat)
        rval = syscall( SYS_stat, path, buf );
#elif defined(SYS_old_stat)
        rval = syscall( SYS_old_stat, path, buf );
#else
#error "No local version of SYS_*stat on this platform!"
#endif
    } else {
        rval = REMOTE_syscall( CONDOR_stat, path, buf );
    }

    return rval;
}


int _fxstat(int version, int fd, struct stat *buf)
{
	return _condor_fxstat( version, fd, buf );
}


int __fxstat(int version, int fd, struct stat *buf)
{
	return _condor_fxstat( version, fd, buf );
}


int
_condor_fxstat(int version, int fd, struct stat *buf)
{
    int rval;
    int user_fd;
    int use_local_access = FALSE;

    if( (user_fd=MapFd(fd)) < 0 ) {
        return (int)-1;
    }
    if( LocalAccess(fd) ) {
        use_local_access = TRUE;
    }

    if( LocalSysCalls() || use_local_access ) {
#if defined(SYS_prev_fstat)
        rval = syscall( SYS_prev_fstat, fd, buf );
#elif defined(SYS_fstat)
        rval = syscall( SYS_fstat, fd, buf );
#elif defined(SYS_old_fstat)
        rval = syscall( SYS_old_fstat, fd, buf );
#else
#error "No local version of SYS_*fstat on this platform!"
#endif
    } else {
        rval = REMOTE_syscall( CONDOR_fstat, user_fd, buf );
    }

    return rval;
}


int _lxstat(int version, const char *path, struct stat *buf)
{
	return _condor_lxstat( version, path, buf );
}


int __lxstat(int version, const char *path, struct stat *buf)
{
	return _condor_lxstat( version, path, buf );
}


int _condor_lxstat(int version, const char *path, struct stat *buf)
{
    int rval;

    if( LocalSysCalls() ) {
#if defined(SYS_prev_lstat)
        rval = syscall( SYS_prev_lstat, path, buf );
#elif defined(SYS_lstat)
        rval = syscall( SYS_lstat, path, buf );
#elif defined(SYS_old_lstat)
        rval = syscall( SYS_old_lstat, path, buf );
#else
#error "No local version of SYS_*_lstat on this platform!"
#endif
    } else {
        rval = REMOTE_syscall( CONDOR_lstat, path, buf );
    }

    return rval;
}

#endif /* LINUX */


/* fork() and sigaction() are not in fork.o or sigaction.o on Solaris 2.5
   but instead are only in the threads libraries.  We access the old
   versions through their new names. */

#if defined(Solaris)
pid_t
FORK()
{
	return _libc_FORK();
}

#endif

/* On Solaris, if an application uses sysconf() to query whether mmap()
   support is enabled, override and return 0 (FALSE).  Condor does not
   currently support mmap().  */

#if defined(Solaris)
long
sysconf(int name)
{
	if (name == _SC_MAPPED_FILES) return 0;
	return SYSCONF(name);
}

long
_sysconf(int name)
{
	if (name == _SC_MAPPED_FILES) return 0;
	return SYSCONF(name);
}
#endif

/* On Solaris and IRIX, stat, lstat, and fstat are statically defined in stat.h,
   so we can't override them.  Instead, we override the following three
   xstat definitions.  */

#if defined(IRIX53) || defined(Solaris)

#if defined(IRIX53) && !defined(IRIX62)
int _xstat(int ver, char *path, struct stat *buf)
#elif defined(Solaris) || defined(IRIX62)
int _xstat(const int ver, const char *path, struct stat *buf)
#endif
{
	int	rval;

	if( LocalSysCalls() ) {
		rval = syscall( SYS_xstat, ver, path, buf );
	} else {
		rval = REMOTE_syscall( CONDOR_stat, path, buf );
	}

	return rval;
}

#if defined(IRIX53) && !defined(IRIX62)
int _lxstat(int ver, char *path, struct stat *buf)
#elif defined(Solaris) || defined(IRIX62)
int _lxstat(const int ver, const char *path, struct stat *buf)
#endif
{
	int	rval;

	if( LocalSysCalls() ) {
		rval = syscall( SYS_lxstat, ver, path, buf );
	} else {
		rval = REMOTE_syscall( CONDOR_lstat, path, buf );
	}

	return rval;
}

#if defined(IRIX53) && !defined(IRIX62)
int _fxstat(int ver, int fd, struct stat *buf)
#elif defined(Solaris) || defined(IRIX62)
int _fxstat(const int ver, int fd, struct stat *buf)
#endif
{
	int	rval;
	int	user_fd;
	int use_local_access = FALSE;

	if( (user_fd=MapFd(fd)) < 0 ) {
		return (int)-1;
	}
	if( LocalAccess(fd) ) {
		use_local_access = TRUE;
	}

	if( LocalSysCalls() || use_local_access ) {
		rval = syscall( SYS_fxstat, ver, user_fd, buf );
	} else {
		rval = REMOTE_syscall( CONDOR_fstat, user_fd, buf );
	}

	return rval;
}
#endif /* IRIX || Solaris */


/* getlogin needs to be special because it returns a char*, and until
 * we make stub_gen return longs instead of ints, casting char* to an int
 * causes problems on some platforms...  also, stubgen does not deal
 * well with a function like getlogin which returns a NULL on error */
char *
getlogin()
{
	int rval;
	static char *loginbuf = NULL;
	char *loc_rval;

	if( LocalSysCalls() ) {
#if defined( SYS_getlogin )
		loc_rval = (char *) syscall( SYS_getlogin );
		return loc_rval;
#elif defined( DL_EXTRACT ) 
		{
        void *handle;
        char * (*fptr)();
        if ((handle = dlopen("/usr/lib/libc.so", RTLD_LAZY)) == NULL) {
            return NULL;
        }

        if ((fptr = (char * (*)())dlsym(handle, "getlogin")) == NULL) {
            return NULL;
        }

        return (*fptr)();
		}
#else
		extern char *GETLOGIN();
		return (  GETLOGIN() );
#endif
	} else {
		if (loginbuf == NULL) {
			loginbuf = (char *)malloc(35);
			memset( loginbuf, 0, 35 );
		}
		rval = REMOTE_syscall( CONDOR_getlogin, loginbuf );
	}

	if ( rval >= 0 )
		return loginbuf;
	else
		return NULL;  
}


#if defined( LINUX  )
/* 
   Linux's mmap can be passed a special flag, MAP_ANONYMOUS, which
   means that you don't want to use a file, in which case, mmap
   behaves a lot like malloc().  So, we need to check for that flag
   here, and if it's set, don't do anything with the fd we were passed
   and always do the mmap locally.  The C library uses mmap with
   MAP_ANONYMOUS to allocate I/O buffers.  -Derek Wright 3/12/98 

   Plus, glibc's mmap returns and takes as it's first arg a caddr_t,
   which is really a char*

*/

#include "condor_mmap.h"
MMAP_T
mmap( MMAP_T a, size_t l, int p, int f, int fd, off_t o )
{
	MMAP_T	rval;
	int		user_fd;
	int		use_local_access = FALSE;

	if( f & MAP_ANONYMOUS ) {
			/* If the MAP_ANONYMOUS flag is set, ignore the fd we were
			   passed and do the mmap locally */
		use_local_access = TRUE;
	} else {
		if( (user_fd=MapFd(fd)) < 0 ) {
			return MAP_FAILED;
		}
		if( LocalAccess(fd) ) {
			use_local_access = TRUE;
		}
	}
	if( use_local_access || LocalSysCalls() ) {
		rval = MMAP( a, l, p, f, user_fd, o );
	} else {
		rval = (MMAP_T)REMOTE_syscall( CONDOR_mmap, a, l, p, f, user_fd, o );
	}
	
	return rval;
}
#endif /* defined( LINUX ) */


#if defined( Solaris26 )
/*
  There's an open64() on Solaris 2.6 that we want to trap.  We could
  just remap it, except open() takes a variable number of args.  So
  we just put in a definition here that deals with variable args and
  calls the real open, which we'll trap and do our magic with.
  -Derek Wright, 6/24/98

  We also want _open64() and __open64().  -Derek 6/25/98

  We really need to use the va_list differently.  We now setup our
  va_list in (_*)open64() and always call _condor_open() which expects
  a va_list as its 3rd arg.  -Derek 8/20/98
*/

int
open64( const char* path, int oflag, ... ) {
	va_list ap;
	int rval;
	va_start( ap, oflag );
	rval = _condor_open( path, oflag, ap );
	va_end( ap );
	return rval;
}

int
_open64( const char* path, int oflag, ... ) {
	va_list ap;
	int rval;
	va_start( ap, oflag );
	rval = _condor_open( path, oflag, ap );
	va_end( ap );
	return rval;
}

int
__open64( const char* path, int oflag, ... ) {
	va_list ap;
	int rval;
	va_start( ap, oflag );
	rval = _condor_open( path, oflag, ap );
	va_end( ap );
	return rval;
}

#endif /* defined( Solaris26 ) */


/* Special kill that allows us to send signals to ourself, but not any
   other pids.  Written on 7/8 by Derek Wrigh <wright@cs.wisc.edu> */
int
kill( pid_t pid, int sig )
{
	int rval;
	pid_t my_pid;	

	if( LocalSysCalls() ) {
			/* We're in local mode, do exactly what we were told. */
		rval = SYSCALL( SYS_kill, pid, sig );
	} else {
			/* Remote mode.  Only allow signals to be sent to ourself.
			   Call the same getpid() the user job is calling to see
			   if it is trying to send a signal to itself */
		my_pid = getpid();   
		if( pid == my_pid ) {
				/* The user job thinks it's sending a signal to
				   itself... let that work by getting our real pid. */
			my_pid = SYSCALL( SYS_getpid );
			rval = SYSCALL( SYS_kill, my_pid, sig );			
		} else {
				/* We don't allow you to send signals to anyone else */
			rval = -1;
		}
	}	
	return rval;
}


#if SYNC_RETURNS_VOID
void
#else
int
#endif
sync( void )
{
	int rval;
	pid_t my_pid;	

	/* Always want to do a local sync() */
	SYSCALL( SYS_sync );

	/* If we're in remote mode, also want to send a sync() to the shadow */
	if( RemoteSysCalls() ) {
		REMOTE_syscall( CONDOR_sync );
	}
#if ! SYNC_RETURNS_VOID
	return 0;
#endif
}

/* Solaris has a library entry fsync() which calls the system call fdsync. */

#ifdef SYS_fdsync
int fdsync( int fd )
{
	int	rval;
	int	user_fd;
	int use_local_access = FALSE;

	if( (user_fd=MapFd(fd)) < 0 ) {
		return (int)-1;
	}
	if( LocalAccess(fd) ) {
		use_local_access = TRUE;
	}

	if( LocalSysCalls() || use_local_access ) {
		rval = syscall( SYS_fdsync, user_fd );
	} else {
		rval = REMOTE_syscall( CONDOR_fsync, user_fd );
	}

	return rval;
}

int _fdsync( int fd )
{
	fdsync(fd);
}

int __fdsync( int fd )
{
	fdsync(fd);
}

int ___fdsync( int fd )
{
	fdsync(fd);
}
#endif
