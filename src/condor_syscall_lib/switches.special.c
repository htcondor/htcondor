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

#if defined(GLIBC21)
int _xstat64(int, const char *, struct stat64 *);
int _fxstat64(int, int, struct stat64 *);
int _lxstat64(int, const char *, struct stat64 *);
int __xstat64(int, const char *, struct stat64 *);
int __fxstat64(int, int, struct stat64 *);
int __lxstat64(int, const char *, struct stat64 *);
#endif

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

char	*getwd( char * );
void    *malloc();     
int	_condor_open( const char *path, int flags, va_list ap );
sigset_t condor_block_signals();
void condor_restore_sigmask(sigset_t);

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
	sigset_t omask;

	omask = condor_block_signals();

		/* Try the system call */
	if( LocalSysCalls() ) {
		rval = syscall( SYS_chdir, path );
	} else {
		rval = REMOTE_syscall( CONDOR_chdir, path );
	}

		/* If it fails we can stop here */
	if( rval < 0 ) {
		condor_restore_sigmask(omask);
		return rval;
	}

	if( MappingFileDescriptors() ) {
		store_working_directory();
	}

	condor_restore_sigmask(omask);
	return rval;
}

#if defined(SYS_fchdir)
int
fchdir( int fd )
{
	int rval, real_fd;
	sigset_t omask;

	omask = condor_block_signals();

	/* set this up in case we aren't mapping file descriptors. */
	real_fd = fd;

	if( MappingFileDescriptors() ) {
		if( (real_fd = MapFd(fd)) < 0 ) {
			condor_restore_sigmask(omask);
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
		condor_restore_sigmask(omask);
		return rval;
	}

	if( MappingFileDescriptors() ) {
		store_working_directory();
	}

	condor_restore_sigmask(omask);
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
	sigset_t omask;

	omask = condor_block_signals();

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
			condor_restore_sigmask(omask);
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

	condor_restore_sigmask(omask);
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
We don't handle readv directly in ANY case.  Split up the read
and pass it through the regular read mechanism to take advantage
of whatever magic is implemented there.
*/

#if defined(HPUX9) || defined(LINUX) 
ssize_t readv( int fd, const struct iovec *iov, size_t iovcnt )
#elif defined(Solaris251)
int readv( int fd, struct iovec *iov, int iovcnt )
#else
/* Confirmed for IRIX, OSF1, HPUX10, Solaris26, Solaris27 */
ssize_t readv( int fd, const struct iovec *iov, int iovcnt )
#endif
{
	int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = read( fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) return cc;
		rval += cc;
		if( cc != iov->iov_len ) return rval;
		iov++;
	}

	return rval;
}

/*
We don't handle writev directly in ANY case.  Split up the write
and pass it through the regular write mechanism to take advantage
of whatever magic is implemented there.
*/

#if defined(HPUX9) || defined(LINUX) 
ssize_t writev( int fd, const struct iovec *iov, size_t iovcnt )
#elif defined(Solaris) || defined(IRIX) || defined(OSF1) || defined(HPUX10)
ssize_t writev( int fd, const struct iovec *iov, int iovcnt )
#else
int writev( int fd, struct iovec *iov, int iovcnt )
#endif
{
	int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = write( fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) return cc;
		rval += cc;
		if( cc != iov->iov_len ) return rval;
		iov++;
	}

	return rval;
}

/* Kernel readv and writev for AIX */

#ifdef AIX32

int kwritev( int fd, struct iovec *iov, int iovcnt, int ext )
{
	return writev(fd,iov,iovcnt);
}

int kreadv( int fd, struct iovec *iov, int iovcnt, int ext )
{
	return readv(fd,iov,iovcnt);
}

#endif


#if defined(HPUX)
#	include <sys/mib.h>
#	include <sys/ioctl.h>
#endif

#if defined(Solaris) || defined(LINUX) || defined(OSF1) || defined(IRIX) || defined(HPUX10)
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
	sigset_t omask;

	omask = condor_block_signals();

	/* The below length check is a hack to patch an f77 problem on
	   OSF1.  - Jim B. */

	if (length < 0)
		return 0;

	if( (user_fd=MapFd(fd)) < 0 ) {
		condor_restore_sigmask(omask);
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

	condor_restore_sigmask(omask);
	return rval;
}

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

#if defined(LINUX) && defined(GLIBC21)

/* Alrighty, in order to keep homogeneity between the various linux platforms,
	I have remapped all of the 64 bit seek calls into the common 32
	bit seek calls. This is so requests can be serviced by the shadow correctly
	on machines with no 64 bit seek calls. -pete 8/16/99 */

int __llseek( int fd, off_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

int _llseek( int fd, off_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

int llseek( int fd, off_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

__off64_t __lseek64( int fd, __off64_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

__off64_t _lseek64( int fd, __off64_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

__off64_t lseek64( int fd, __off64_t offset, int whence )
{
	return lseek(fd,offset,whence);
}

#endif /* defined glibc21 and linux */

#endif /* defined(SYS_lseek) */

#endif /* !defined(HPUX) */


#if defined(LINUX)

/* 
   There's a whole bunch of Linux-specific evilness w/ [_fxl]*stat.
   Basically, we need to trap all these things and send them over the
   wire as CONDOR_stat, CONDOR_fstat, or CONDOR_lstat as appropriate
   and handle them locally with whatever SYS_*stat is defined on this
   host.  While it's all the same actual number, glibc and libc5 do
   this differently, which is why we need all the pre-processor junk.
  
   If that's not bad enough, there are two versions of the stat
   structure (struct stat) on Linux!  One is a version of the stat
   structure the kernel understands, which is the "old" stat.  The
   other is the version of the stat structure that glibc understands,
   which is the "new" stat.  Each of [_fxl]*stat pass in a version
   argument, which specifies which version of the stat structure the
   caller wants.
  
   So, if we want to do a local stat(), we call syscall() and pass in
   a pointer to a struct kernel_stat, which we define.  This is the
   version of the stat structure that the kernel expects.  Then, we
   pass a pointer to this structure, the pointer we were given, and
   the version number, into _condor_k_stat_convert(), which converts a
   struct kernel_stat into the appropriate struct stat, depending on
   the version.

   If we need to do a remote stat(), we use the native struct stat,
   whatever that happens to be, since that's what CEDAR knows about.
   The RSC will get the fields we care about, and CEDAR knows how to
   give those to us.  Again, we have to convert this into the right
   kind of stat structure, depending on what our caller expects, so we
   use _condor_s_stat_convert() which converts a struct stat into the
   right version of the struct stat we were passed.  This might seem
   unneccesary, but CEDAR only knows about certain fields, and if our
   caller is looking for a "new" struct stat, we've got to zero out
   everything else.  "Just memset() it to 0 before you give it to
   CEDAR!", you say?  And what sizeof() do you propose we use?  We
   have no idea what size the struct stat we are being passed is,
   since it depends on the version.  This method just seems safer to
   me.  If someone has a better solution, I'm all ears.

  -Derek Wright 6/30/99 */

/* 
   First, do all the hell to figure out what actual numbers we'll want
   to use with syscall() in the local cases.  Define our own
   CONDOR_SYS_stat* to be whatever pre-processor definition we already
   have.
*/

#if defined(SYS_prev_stat)
#   define CONDOR_SYS_stat SYS_prev_stat
#elif defined(SYS_stat)
#   define CONDOR_SYS_stat SYS_stat
#elif defined(SYS_old_stat)
#   define CONDOR_SYS_stat SYS_old_stat
#else
#   error "No local version of SYS_*_stat on this platform!"
#endif

#if defined(SYS_prev_fstat)
#   define CONDOR_SYS_fstat SYS_prev_fstat
#elif defined(SYS_fstat)
#   define CONDOR_SYS_fstat SYS_fstat
#elif defined(SYS_old_fstat)
#   define CONDOR_SYS_fstat SYS_old_fstat
#else
#error "No local version of SYS_*_fstat on this platform!"
#endif

#if defined(SYS_prev_lstat)
#   define CONDOR_SYS_lstat SYS_prev_lstat
#elif defined(SYS_lstat)
#   define CONDOR_SYS_lstat SYS_lstat
#elif defined(SYS_old_lstat)
#   define CONDOR_SYS_lstat SYS_old_lstat
#else
#error "No local version of SYS_*_lstat on this platform!"
#endif


/* 
   Now, trap all the various [_flx]*stat calls, and convert them all
   to _condor_[xlf]stat().
*/

int _xstat(int version, const char *path, struct stat *buf)
{
	return _condor_xstat( version, path, buf );
}


int __xstat(int version, const char *path, struct stat *buf)
{
	return _condor_xstat( version, path, buf );
}


int _fxstat(int version, int fd, struct stat *buf)
{
	return _condor_fxstat( version, fd, buf );
}


int __fxstat(int version, int fd, struct stat *buf)
{
	return _condor_fxstat( version, fd, buf );
}


int _lxstat(int version, const char *path, struct stat *buf)
{
	return _condor_lxstat( version, path, buf );
}


int __lxstat(int version, const char *path, struct stat *buf)
{
	return _condor_lxstat( version, path, buf );
}

#if defined(GLIBC21)
int _xstat64(int version, const char *path, struct stat64 *buf)
{
	return _condor_xstat64( version, path, buf );
}

int __xstat64(int version, const char *path, struct stat64 *buf)
{
	return _condor_xstat64( version, path, buf );
}

int _fxstat64(int version, int fd, struct stat64 *buf)
{
	return _condor_fxstat64( version, fd, buf );
}

int __fxstat64(int version, int fd, struct stat64 *buf)
{
	return _condor_fxstat64( version, fd, buf );
}

int _lxstat64(int version, const char *path, struct stat64 *buf)
{
	return _condor_lxstat64( version, path, buf );
}

int __lxstat64(int version, const char *path, struct stat64 *buf)
{
	return _condor_lxstat64( version, path, buf );
}
#endif

/*
   _condor_k_stat_convert() takes in a version, and two pointers, one
   to a struct kernel_stat (which we define in kernel_stat.h), and one
   to a struct stat (the type of which is determined by the version
   arg).  For all versions of struct stat, copy out the fields we care
   about, and depending on the version, zero out the fields we don't
   care about.  
*/


#include "linux_kernel_stat.h"

void 
_condor_k_stat_convert( int version, const struct kernel_stat *source, 
						struct stat *target )
{
		/* In all cases, we need to copy the fields we care about. */
	target->st_dev = source->st_dev;
	target->st_ino = source->st_ino;
	target->st_mode = source->st_mode;
	target->st_nlink = source->st_nlink;
	target->st_uid = source->st_uid;
	target->st_gid = source->st_gid;
	target->st_rdev = source->st_rdev;
	target->st_size = source->st_size;
	target->st_blksize = source->st_blksize;
	target->st_blocks = source->st_blocks;
	target->st_atime = source->st_atime;
	target->st_mtime = source->st_mtime;
	target->st_ctime = source->st_ctime;

		/* Now, handle the different versions we might be passed */
	switch( version ) {
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
    case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/
		target->__pad1 = 0;
		target->__pad2 = 0;
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		target->__unused4 = 0;
		target->__unused5 = 0;
		break;
	default:
			/* Some other version: blow-up */
		EXCEPT( "_condor_k_stat_convert: unknown version (%d) of struct stat!", version );
		break;
	}
}

#if defined(GLIBC21)
void 
_condor_k_stat_convert64( int version, const struct kernel_stat *source, 
						struct stat64 *target )
{
		/* In all cases, we need to copy the fields we care about. */
	target->st_dev = source->st_dev;
	target->st_ino = source->st_ino;
	target->st_mode = source->st_mode;
	target->st_nlink = source->st_nlink;
	target->st_uid = source->st_uid;
	target->st_gid = source->st_gid;
	target->st_rdev = source->st_rdev;
	target->st_size = source->st_size;
	target->st_blksize = source->st_blksize;
	target->st_blocks = source->st_blocks;
	target->st_atime = source->st_atime;
	target->st_mtime = source->st_mtime;
	target->st_ctime = source->st_ctime;

		/* Now, handle the different versions we might be passed */
	switch( version ) {
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
    case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/
		target->__pad1 = 0;
		target->__pad2 = 0;
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		target->__unused4 = 0;
		target->__unused5 = 0;
		break;
	default:
			/* Some other version: blow-up */
		EXCEPT( "_condor_k_stat_convert64: unknown version (%d) of struct stat!", version );
		break;
	}
}
#endif

/*
  _condor_s_stat_convert() is just like _condor_k_stat_convert(),
  except that it deals with two struct stat's, instead of a struct
  kernel_stat and a struct_stat 
*/

void 
_condor_s_stat_convert( int version, const struct stat *source, 
						struct stat *target )
{
		/* In all cases, we need to copy the fields we care about. */
	target->st_dev = source->st_dev;
	target->st_ino = source->st_ino;
	target->st_mode = source->st_mode;
	target->st_nlink = source->st_nlink;
	target->st_uid = source->st_uid;
	target->st_gid = source->st_gid;
	target->st_rdev = source->st_rdev;
	target->st_size = source->st_size;
	target->st_blksize = source->st_blksize;
	target->st_blocks = source->st_blocks;
	target->st_atime = source->st_atime;
	target->st_mtime = source->st_mtime;
	target->st_ctime = source->st_ctime;

		/* Now, handle the different versions we might be passed */
	switch( version ) {
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
    case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/
		target->__pad1 = 0;
		target->__pad2 = 0;
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		target->__unused4 = 0;
		target->__unused5 = 0;
		break;
	default:
			/* Some other version: blow-up */
		EXCEPT( "_condor_s_stat_convert: unknown version (%d) of struct stat!", version );
		break;
	}
}

#if defined(GLIBC21)
void 
_condor_s_stat_convert64( int version, const struct stat *source, 
						struct stat64 *target )
{
		/* In all cases, we need to copy the fields we care about. */
		/* here we downcast the stat64 into a stat */
	target->st_dev = source->st_dev;
	target->st_ino = source->st_ino;
	target->st_mode = source->st_mode;
	target->st_nlink = source->st_nlink;
	target->st_uid = source->st_uid;
	target->st_gid = source->st_gid;
	target->st_rdev = source->st_rdev;
	target->st_size = source->st_size;
	target->st_blksize = source->st_blksize;
	target->st_blocks = source->st_blocks;
	target->st_atime = source->st_atime;
	target->st_mtime = source->st_mtime;
	target->st_ctime = source->st_ctime;

		/* Now, handle the different versions we might be passed */
	switch( version ) {
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
    case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/
		target->__pad1 = 0;
		target->__pad2 = 0;
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		target->__unused4 = 0;
		target->__unused5 = 0;
		break;
	default:
			/* Some other version: blow-up */
		EXCEPT( "_condor_s_stat_convert64: unknown version (%d) of struct stat!", version );
		break;
	}
}
#endif

/*
   Finally, implement the _condor_[xlf]stat() functions which do all
   the work of checking if we want local or remote, and handle the
   different versions of the stat struct as appropriate.
*/

int _condor_xstat(int version, const char *path, struct stat *buf)
{
    int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	sigset_t omask;

	omask = condor_block_signals();

    if( LocalSysCalls() ) {
        rval = syscall( CONDOR_SYS_stat, path, &kbuf );
		_condor_k_stat_convert( version, &kbuf, buf );
    } else {
        rval = REMOTE_syscall( CONDOR_stat, path, &sbuf );
		_condor_s_stat_convert( version, &sbuf, buf );
    }
	condor_restore_sigmask(omask);
    return rval;
}

#if defined(GLIBC21)
int _condor_xstat64(int version, const char *path, struct stat64 *buf)
{
    int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	sigset_t omask;

	omask = condor_block_signals();

    if( LocalSysCalls() ) {
        rval = syscall( CONDOR_SYS_stat, path, &kbuf );
		_condor_k_stat_convert64( version, &kbuf, buf );
    } else {
        rval = REMOTE_syscall( CONDOR_stat, path, &sbuf );
		_condor_s_stat_convert64( version, &sbuf, buf );
    }
	condor_restore_sigmask(omask);
    return rval;
}
#endif

int
_condor_fxstat(int version, int fd, struct stat *buf)
{
    int rval;
    int user_fd;
    int use_local_access = FALSE;
	struct kernel_stat kbuf;
	struct stat sbuf;
	sigset_t omask;

	omask = condor_block_signals();

    if( (user_fd=MapFd(fd)) < 0 ) {
        return (int)-1;
    }
    if( LocalAccess(fd) ) {
        use_local_access = TRUE;
    }

    if( LocalSysCalls() || use_local_access ) {
        rval = syscall( CONDOR_SYS_fstat, fd, &kbuf );
		_condor_k_stat_convert( version, &kbuf, buf );
    } else {
        rval = REMOTE_syscall( CONDOR_fstat, user_fd, &sbuf );
		_condor_s_stat_convert( version, &sbuf, buf );
    }
	condor_restore_sigmask(omask);
    return rval;
}

#if defined(GLIBC21)
int
_condor_fxstat64(int version, int fd, struct stat64 *buf)
{
    int rval;
    int user_fd;
    int use_local_access = FALSE;
	struct kernel_stat kbuf;
	struct stat sbuf;
	sigset_t omask;

	omask = condor_block_signals();

    if( (user_fd=MapFd(fd)) < 0 ) {
        return (int)-1;
    }
    if( LocalAccess(fd) ) {
        use_local_access = TRUE;
    }

    if( LocalSysCalls() || use_local_access ) {
        rval = syscall( CONDOR_SYS_fstat, fd, &kbuf );
		_condor_k_stat_convert64( version, &kbuf, buf );
    } else {
        rval = REMOTE_syscall( CONDOR_fstat, user_fd, &sbuf );
		_condor_s_stat_convert64( version, &sbuf, buf );
    }
	condor_restore_sigmask(omask);
    return rval;
}
#endif

int _condor_lxstat(int version, const char *path, struct stat *buf)
{
    int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	sigset_t omask;

	omask = condor_block_signals();

    if( LocalSysCalls() ) {
        rval = syscall( CONDOR_SYS_lstat, path, &kbuf );
		_condor_k_stat_convert( version, &kbuf, buf );
    } else {
        rval = REMOTE_syscall( CONDOR_lstat, path, &sbuf );
		_condor_s_stat_convert( version, &sbuf, buf );
    }
	condor_restore_sigmask(omask);
    return rval;
}

#if defined(GLIBC21)
int _condor_lxstat64(int version, const char *path, struct stat64 *buf)
{
    int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	sigset_t omask;

	omask = condor_block_signals();

    if( LocalSysCalls() ) {
        rval = syscall( CONDOR_SYS_lstat, path, &kbuf );
		_condor_k_stat_convert64( version, &kbuf, buf );
    } else {
        rval = REMOTE_syscall( CONDOR_lstat, path, &sbuf );
		_condor_s_stat_convert64( version, &sbuf, buf );
    }
	condor_restore_sigmask(omask);
    return rval;
}
#endif

#endif /* LINUX stat hell */


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

#if defined(IRIX) || defined(Solaris)

#if defined(Solaris) || defined(IRIX)
int _xstat(const int ver, const char *path, struct stat *buf)
#endif
{
	int	rval;
	sigset_t omask;

	omask = condor_block_signals();

	if( LocalSysCalls() ) {
		rval = syscall( SYS_xstat, ver, path, buf );
	} else {
		rval = REMOTE_syscall( CONDOR_stat, path, buf );
	}

	condor_restore_sigmask(omask);
	return rval;
}

#if defined(Solaris) || defined(IRIX)
int _lxstat(const int ver, const char *path, struct stat *buf)
#endif
{
	int	rval;
	sigset_t omask;

	omask = condor_block_signals();

	if( LocalSysCalls() ) {
		rval = syscall( SYS_lxstat, ver, path, buf );
	} else {
		rval = REMOTE_syscall( CONDOR_lstat, path, buf );
	}

	condor_restore_sigmask(omask);
	return rval;
}

#if defined(Solaris) || defined(IRIX)
int _fxstat(const int ver, int fd, struct stat *buf)
#endif
{
	int	rval;
	int	user_fd;
	int use_local_access = FALSE;
	sigset_t omask;

	omask = condor_block_signals();

	if( (user_fd=MapFd(fd)) < 0 ) {
		condor_restore_sigmask(omask);
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

	condor_restore_sigmask(omask);
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

/* The Linux C library uses mmap for several purposes.  The I/O system uses mmap() to get a nicely aligned buffer.  The memory system also uses mmap() for large allocation requests.  In general, we don't support mmap(), but we can support calls with particular flags (MAP_ANON|MAP_PRIVATE) because they translate to "give me a clean new segment".  In these cases, we turn around and malloc() some new memory and trust that the caller doesn't really care that it came from the heap. */

#if defined( LINUX  )
#include "condor_mmap.h"
MMAP_T
mmap( MMAP_T a, size_t l, int p, int f, int fd, off_t o )
{
        MMAP_T rval;
        static int recursive=0;

        if( (f==(MAP_ANONYMOUS|MAP_PRIVATE)) ) {
                if(recursive) {
                        rval = MAP_FAILED;
                } else {
                        recursive = 1;
                        rval = (MMAP_T) malloc( l );
                        recursive = 0;
                }
        } else {
                if( LocalSysCalls() ) {
                        rval = MAP_FAILED;
                } else {
                        rval = REMOTE_syscall( CONDOR_mmap, a, l, p, f, fd, o );
                }
        }

        return rval;
}

int munmap( MMAP_T addr, size_t length ) 
{
        if(addr) free(addr);
        return 0;
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
	sigset_t omask;

	omask = condor_block_signals();

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
	condor_restore_sigmask(omask);
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
int fdsync( int fd, int flags )
{
	int	rval;
	int	real_fd;
	int use_local_access = FALSE;
	sigset_t omask;

	omask = condor_block_signals();

	if( (real_fd=MapFd(fd)) < 0 ) {
		condor_restore_sigmask(omask);
		return (int)-1;
	}
	if( LocalAccess(fd) ) {
		use_local_access = TRUE;
	}

	if( LocalSysCalls() || use_local_access ) {
		rval = syscall( SYS_fdsync, real_fd, flags );
	} else {
		rval = REMOTE_syscall( CONDOR_fsync, real_fd );
	}

	condor_restore_sigmask(omask);
	return rval;
}

int _fdsync( int fd, int flags )
{
	return fdsync(fd,flags);
}

int __fdsync( int fd, int flags )
{
	return fdsync(fd,flags);
}

int ___fdsync( int fd, int flags )
{
	return fdsync(fd,flags);
}
#endif /* SYS_fdsync */
