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
System call stubs which cannot be built automatically by the stub
generator go in here.

This file can be processed with several purposes in mind.
	FILE_TABLE - build switches needed to trap open file table changes
	REMOTE_SYSCALLS - build general-purpose remote syscalls
	SIGNALS_SUPPORT - build switches for signals support
	(currently all signals switches are in signals_support.c)
*/

#include "condor_common.h"
#include "syscall_numbers.h"
#include "condor_syscall_mode.h"
#include "file_table_interf.h"
#include "condor_debug.h"
#include "../condor_ckpt/signals_control.h"
#include "../condor_ckpt/file_state.h"
#include "gtodc.h"

#if defined(DL_EXTRACT)
#   include <dlfcn.h>   /* for dlopen and dlsym */
#endif

extern unsigned int _condor_numrestarts;  /* in image.C */

/* get rid of the remapping of access() to access_euid() since system calls
	generally should be what they actually should be, and not what we think
	they should be. */
#ifdef access
# undef access
#endif

extern "C" {

int GETRUSAGE(...);
int _WAITPID(...);
int _libc_FORK(...);
int SYSCONF(...);
int SYSCALL(...);
int _FORK_sys(...);
void update_rusage( register struct rusage *ru1, register struct rusage *ru2 );

#if defined(LINUX)
int _condor_xstat(int version, const char *path, struct stat *buf);
int _condor_lxstat(int version, const char *path, struct stat *buf);
int _condor_fxstat(int version, int fd, struct stat *buf);

int _xstat(int, const char *, struct stat *);
int _fxstat(int, int, struct stat *);
int _lxstat(int, const char *, struct stat *);
int __xstat(int, const char *, struct stat *);
int __fxstat(int, int, struct stat *);
int __lxstat(int, const char *, struct stat *);

int _condor_xstat64(int version, const char *path, struct stat64 *buf);
int _condor_lxstat64(int version, const char *path, struct stat64 *buf);
int _condor_fxstat64(int versino, int fd, struct stat64 *buf);

int _xstat64(int, const char *, struct stat64 *);
int _fxstat64(int, int, struct stat64 *);
int _lxstat64(int, const char *, struct stat64 *);
int __xstat64(int, const char *, struct stat64 *);
int __fxstat64(int, int, struct stat64 *);
int __lxstat64(int, const char *, struct stat64 *);

#endif /* LINUX */

/*************************************************************
The following switches are for syscalls that affect the open
file table.  These switches are built for _both_ full remote
syscalls and standalone checkpointing.
*************************************************************/

#ifdef FILE_TABLE

#if defined(LINUX)

/*
__getdents has a very special switch on Linux.
There are two distinct problems.

1 - The kernel structure is different fron the user level structure.
This requires that we transform from user space to kernel space
before returning.

2 - The internal libc functions have a completely unnecessary
calling optimization.  We must reproduce the compiler flags that
are used to generate it within libc.

*/

#include <dirent.h>

struct condor_kernel_dirent {
	long            d_ino;
	off_t           d_off;
	unsigned short  d_reclen;
	char            d_name[256]; 
};

/* This structure isn't needed, per se, but I put it here to record what it is
	in case it is needed in the future. */
struct condor_kernel_dirent64
{
	uint64_t			d_ino;
	int64_t				d_off;
	unsigned short int	d_reclen;
	unsigned char		d_type;
	char				d_name[256];
};

/* This is an annoying little variable that exists in glibc 2.2.4(redhat 7.1),
	but not in glibc 2.1.3(redhat 6.2). In glibc 2.2.4, it is labeled as a C
	linker type meaning if no other definition of this variable is found first,
	the use the first C definition as the actual definition, and make all other
	C definitions of this variable in other .o files a U. It turned out 
	that the first definition of this variable was in getdents.o in libc,
	so something somewhere would bring it in and then cause a multiply defined
	linker error concerning the getdents family of functions we define here, 
	and the getdents family of functions in the libc. Even though this is
	defined under redhat 6.2 compilation, it should have no effect there.
	Also, C type variables are supposed to be uninitialized so this appears to
	be a safe thing to do by defining it here. -psilord 01/24/2002 */
int __have_no_getdents64; 

/* This is another C linkage type which brings in fork.o from libc on
	glibc 2.5 machines, and causes conflicts. So here it gets
	defined. */
#if defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
void* __fork_handlers = NULL;
#endif

extern "C" int REMOTE_CONDOR_getdents( int, struct dirent*, size_t);
extern "C" int getdents ( int fd, struct dirent *buf, size_t nbytes )
{
	struct condor_kernel_dirent kbuf;
	int rval,do_local=0;
	errno = 0;
	sigset_t omask;

	omask = _condor_signals_disable();

	if( MappingFileDescriptors() ) {
		do_local = _condor_is_fd_local( fd );
		fd = _condor_get_unmapped_fd( fd );
	}

	if( LocalSysCalls() || do_local ) {
		rval = syscall( SYS_getdents, fd , &kbuf , sizeof(kbuf) );
		if(rval>0) {
			/* Woe is me. :( It turns out that if the user
				application has _LARGEFILE64_SOURCE
				defined to 1, and _FILE_OFFSET_BITS
				defined to 64, at least under linux, then the struct dirent
				member of d_ino and d_off that the
				APPLICATION code will be using is defined
				as 8 byte quanitites. However, this code
				here sees them (at Condor compile time)
				as 4 byte quantities, since we did not
				define those #defines (and probably
				shouldn't) at this level of the code. I
				know of no way to detect the user's size
				of d_ino and d_off, mostly since the
				struct dirent array that is passed in is
				completely opaque. So, I'm just going
				to declare that you better not define
				those two #defines and use getdents()
				in a user program. Use of getdents64() is
				fine in either case, though. -psilord */

			buf[0].d_ino = kbuf.d_ino;
			buf[0].d_off = kbuf.d_off;

			buf[0].d_type = 0;
			buf[0].d_name[0] = '\0';
			strcpy(buf[0].d_name,kbuf.d_name);

			/* Carefully compute the reclen to be cognizant of the predefine
				space allocated to d_name, whether it be one element or 256 */
			buf[0].d_reclen =
							(sizeof(struct dirent) - sizeof(buf[0].d_name)) +
								strlen(buf[0].d_name) - 1;

			/*
			If I was very clever, I would convert all of the data returned.
			Instead, just convert one, and put the seek pointer
			at the beginning of the next one.
			*/

			int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
			lseek(fd,buf[0].d_off,SEEK_SET);
			SetSyscalls(scm);

			rval = buf[0].d_reclen;
		}       
	} else {
		rval = REMOTE_CONDOR_getdents( fd , buf , nbytes );
	}

	_condor_signals_enable(omask);

	return (int  ) rval;
}

/* getdents, for some unknown reason, requires a special register passing
	convention. But only on 32 bit machines */
#ifdef X86_64
extern "C" int __getdents( int fd, struct dirent *dirp, size_t count ); 
#else
extern "C" int __getdents( int fd, struct dirent *dirp, size_t count ) __attribute__ ((regparm (3), stdcall));
#endif

extern "C" int __getdents( int fd, struct dirent *dirp, size_t count ) {
	return getdents(fd,dirp,count);
}

/* It looks like I don't have to specifically create a getdents64()
	switch which does translation like above since the kernel
	structure and user structure are the same. However, I DO have
	to futz with the argument passing style of this function like
	how was done with getdents(). In my inspection of the entire
	getdents/getdents64() functionality in glibc, I've come to
	realize that drooling monkeys designed the interface for
	getdents/getdents64 and the implementation of it was, and is,
	a crime against all that is rational and good. */

#ifdef X86_64
extern "C" int __getdents64( int fd, struct dirent64 *dirp, size_t count );
#else
extern "C" int __getdents64( int fd, struct dirent64 *dirp, size_t count ) __attribute__ ((regparm (3), stdcall));
#endif

extern "C" int getdents64(int, struct dirent64*, size_t);

extern "C" int __getdents64( int fd, struct dirent64 *dirp, size_t count ) {
	return getdents64(fd,dirp,count);
}

/* Now we define the getdirentries() switch, which calls the above
	getdents(). In local mode, getdirentries() ends up calling
	getdents(), for which we also have a switch. So, if
	getdirentries() unmaps the fd in local mode and then calls
	getdents(), getdents() will ALSO unmap the already unmapped fd
	and get a wrong/undefined answer. However, in the remote i/o
	case we need to unmap the fd and give it to the shadow, since
	its call into getdents() in the shadow binary will do the right
	thing (cause it is the straight glibc version of getdents()).
	-psilord 09/16/2011
*/
#undef getdirentries
#if defined(I386)
extern "C" int REMOTE_CONDOR_getdirentries(int, char*, size_t, off_t*);
extern int GETDIRENTRIES(int fd, char *buf, size_t nbytes, off_t *basep);
int getdirentries( int fd, char *buf, size_t nbytes, off_t *basep )
#else
extern "C" ssize_t REMOTE_CONDOR_getdirentries(int, char*, size_t, off_t*);
extern ssize_t GETDIRENTRIES(int fd, char *buf, size_t nbytes, off_t *basep);
ssize_t getdirentries( int fd, char *buf, size_t nbytes, off_t *basep )
#endif
{
    ssize_t   rval;
    int do_local=0;

    errno = 0;

    sigset_t condor_omask = _condor_signals_disable();

	if( MappingFileDescriptors() ) {
		do_local = _condor_is_fd_local( fd );
	}

    if( LocalSysCalls() || do_local ) {
		/* Don't unmap the fd and let the glibc extracted call pass the
			virtual fd to the getdents() call which does the unmapping */
        rval = (ssize_t  ) GETDIRENTRIES( fd , buf , nbytes , basep );
    } else {
		/* The shadow must see the real fd, so unmap it in a remote context */
		if( MappingFileDescriptors() ) {
			fd = _condor_get_unmapped_fd( fd );
		}
        rval =  REMOTE_CONDOR_getdirentries( fd , buf , nbytes , basep );
    }

    _condor_signals_enable( condor_omask );

    return rval;
}

#endif /* LINUX */

#undef ngetdents
#if defined( SYS_ngetdents )
extern "C" int REMOTE_CONDOR_getdents( int, struct dirent*, size_t);
int   ngetdents ( int fd, struct dirent *buf, size_t nbytes, int *eof )
{
	int rval,do_local=0;
	errno = 0;

	sigset_t omask = _condor_signals_disable();

	if( MappingFileDescriptors() ) {
		do_local = _condor_is_fd_local( fd );
		fd = _condor_get_unmapped_fd( fd );
	}

		if( LocalSysCalls() || do_local ) {
			rval = syscall( SYS_getdents, fd , buf , nbytes );
		} else {
			rval = REMOTE_CONDOR_getdents( fd , buf , nbytes );
		}

	_condor_signals_enable(omask);

	if ( rval == 0 ) {
		*eof = 1;
	} else {
		*eof = 0;
	}

	return (int  ) rval;
}
#undef _ngetdents
int   _ngetdents ( int fd, struct dirent *buf, size_t nbytes, int *eof )
{
	return ngetdents(fd,buf,nbytes,eof);
}
#endif

#undef ngetdents64
#if defined( SYS_ngetdents64 )
int   ngetdents64 ( int fd, struct dirent64 *buf, size_t nbytes, int *eof )
{
	int rval,do_local=0;
	errno = 0;

	sigset_t omask = _condor_signals_disable();

	if( MappingFileDescriptors() ) {
		do_local = _condor_is_fd_local( fd );
		fd = _condor_get_unmapped_fd( fd );
	}

		if( LocalSysCalls() || do_local ) {
			rval = syscall( SYS_getdents64, fd , buf , nbytes );
		} else {
			rval = REMOTE_CONDOR_getdents64( fd , buf , nbytes );
		}

	_condor_signals_enable(omask);

	if ( rval == 0 ) {
		*eof = 1;
	} else {
		*eof = 0;
	}

	return (int  ) rval;
}
#undef _ngetdents64
int _ngetdents64(int fd, struct dirent64 *buf, size_t nbytes, int *eof )
{
	return ngetdents64(fd,buf,nbytes,eof );
}
#endif

/*
gcc seems to make pure virtual functions point to this symbol instead
of to zero.  Although we never instantiate an object with pure virtuals,
this symbol remains undefined when linking against C programs, so
we must define it here.
*/

void __pure_virtual()
{
}

/*
Send and recv are special cases of sendfrom and recvto.
On some platforms (LINUX X86_64), send and recv are defined or otherwise
mucked with, so instead of trapping them, convert them into
sendto and recvfrom.
*/

#if defined(LINUX) && defined(X86_64)
ssize_t send( int fd, const void *data, long unsigned int length, int flags )
{
	return sendto(fd,data,length,flags,0,0);
}

ssize_t __send( int fd, const void *data, long unsigned int length, int flags )
{
	return send(fd,data,length,flags);
}

ssize_t __libc_send( int fd, const void *data, long unsigned int length, int flags )
{
	return send(fd,data,length,flags);
}

ssize_t recv( int fd, void *data, long unsigned int length, int flags )
{
	return recvfrom(fd,data,length,flags,0,0);
}

ssize_t __recv( int fd, void *data, long unsigned int length, int flags )
{
	return recv(fd,data,length,flags);
}

ssize_t __libc_recv( int fd, void *data, long unsigned int length, int flags )
{
	return recv(fd,data,length,flags);
}
#endif


/*
On all UNIXes, creat() is a system call, but has the same semantics
as open() with a particular flag.  We will turn creat() into an
open() so that it follows the same code path in Condor.
*/

int creat(const char *path, mode_t mode)
{
	/* This should NOT be the safe_open_wrapper() since it is an entry point
		for the user program and we don't want to change the user programs
		behavior.
	*/
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

#ifdef SYS_open64
int creat64(const char *path, mode_t mode)
{
	return open64(path, O_WRONLY | O_CREAT | O_TRUNC, mode );
}
#endif

/*
On Solaris>251, socket() is usually found in libsocket.so.  However, many
functions (such as DNS lookups) bypass this interface and go directly
to so_socket in libc.  We need to trap so_socket.  I haven't a clue
what each of these parameters do, and I am throwing them to the wind.
Whee!
*/

#if defined(Solaris)

extern "C" int REMOTE_CONDOR_socket( int, int, int);
int so_socket( int a, int b, int c, int d, int e )
{
	int result;
	sigset_t sigs;

	sigset_t omask = _condor_signals_disable();

	if(MappingFileDescriptors()) {
		_condor_file_table_init();
		result = FileTab->socket(a,b,c);
	} else {
		if(LocalSysCalls()) {
			result = syscall( SYS_so_socket, a, b, c, d, e );
		} else {
			result = REMOTE_CONDOR_socket( a, b, c );
		}
	}

	_condor_signals_enable(omask);

	return result;
}
int _so_socket( int a, int b, int c, int d, int e )
{
	return so_socket(a,b,c,d,e);
}

int __so_socket( int a, int b, int c, int d, int e )
{
	return so_socket(a,b,c,d,e);
}

#endif /* Solaris */

/*
On the other hand,
Solaris 251 opens /dev/tcp instead of calling so_socket.
The shadow recognizes /dev/tcp as a special file and instructs
it to open it as special:/dev/tcp to inhibit checkpointing.

But, that's not the point.
Before any of this happens, the creation of a socket results in
the host lookup code to open /etc/.name_service_door and call door_info
on that fd.  A door is something like a named pipe.  So, we must
trap door_info and cause it to return "that is not a door", and all is well.
*/

#if defined(Solaris27) || defined(Solaris28) || defined(Solaris29)

int door_info( int fd, void *info )
{
	errno = EBADF;
	return -1;
}

int _door_info( int fd, void *info )
{
	return door_info( fd, info );
}

int __door_info( int fd, void *info )
{
	return door_info( fd, info );
}

#endif

/*
getmsg and putmsg apply to SVR4 streams, which we really don't support
due to the complexities of ioctl.  However, gethostbyname makes extensive
use of streams on /dev/udp, which is forced to open locally.  So, we will
allow getmsg and putmsg on locally opened files.  If a file is not locally
opened, we will report that it is not a stream.
*/

#if defined(SYS_getmsg)

#include <stropts.h>

int getmsg( int fd, struct strbuf *cptr, struct strbuf *dptr, int *flags )
{
	int	rval;
	int	real_fd;

	sigset_t omask = _condor_signals_disable();

	if( (real_fd=_condor_get_unmapped_fd(fd)) < 0 ) {
		rval = -1;
	} else {
		if( LocalSysCalls() || _condor_is_fd_local(fd) ) {
			rval = syscall( SYS_getmsg, real_fd, cptr, dptr, flags );
		} else {
			errno = ENOSTR;
			rval = -1;
		}
	}

	_condor_signals_enable(omask);
	return rval;
}

int _getmsg( int fd, struct strbuf *cptr, struct strbuf *dptr, int *flags )
{
	return getmsg( fd, cptr, dptr, flags );
}

int __getmsg( int fd, struct strbuf *cptr, struct strbuf *dptr, int *flags )
{
	return getmsg( fd, cptr, dptr, flags );
}


#endif

#if defined(SYS_getpmsg)

int getpmsg( int fd, struct strbuf *cptr, struct strbuf *dptr, int *band, int *flags )
{
	int	rval;
	int	real_fd;

	sigset_t omask = _condor_signals_disable();

	if( (real_fd=_condor_get_unmapped_fd(fd)) < 0 ) {
		rval = -1;
	} else {
		if( LocalSysCalls() || _condor_is_fd_local(fd) ) {
			rval = syscall( SYS_getpmsg, real_fd, cptr, dptr, band, flags );
		} else {
			errno = ENOSTR;
			rval = -1;
		}
	}

	_condor_signals_enable(omask);
	return rval;
}

int _getpmsg( int fd, struct strbuf *cptr, struct strbuf *dptr, int *band, int *flags )
{
	return getpmsg( fd, cptr, dptr, band, flags );
}

int __getpmsg( int fd, struct strbuf *cptr, struct strbuf *dptr, int *band, int *flags )
{
	return getpmsg( fd, cptr, dptr, band, flags );
}

#endif

#if defined( HPUX )
#define STREAM_CONST 
#else
#define STREAM_CONST const
#endif

#if defined(SYS_putmsg)

int putmsg( int fd, STREAM_CONST struct strbuf *cptr, 
			STREAM_CONST struct strbuf *dptr, int flags )
{
	int	rval;
	int	real_fd;

	sigset_t omask = _condor_signals_disable();

	if( (real_fd=_condor_get_unmapped_fd(fd)) < 0 ) {
		rval = -1;
	} else {
		if( LocalSysCalls() || _condor_is_fd_local(fd) ) {
			rval = syscall( SYS_putmsg, real_fd, cptr, dptr, flags );
		} else {
			errno = ENOSTR;
			rval = -1;
		}
	}

	_condor_signals_enable(omask);
	return rval;
}

int _putmsg( int fd, STREAM_CONST struct strbuf *cptr, 
			 STREAM_CONST struct strbuf *dptr, int flags )
{
	return putmsg( fd, cptr, dptr, flags );
}

int __putmsg( int fd, STREAM_CONST struct strbuf *cptr,
			  STREAM_CONST struct strbuf *dptr, int flags )
{
	return putmsg( fd, cptr, dptr, flags );
}


#endif

#if defined(SYS_putpmsg)

int putpmsg( int fd, STREAM_CONST struct strbuf *cptr, 
			 STREAM_CONST struct strbuf *dptr, int band, int flags )
{
	int	rval;
	int	real_fd;

	sigset_t omask = _condor_signals_disable();

	if( (real_fd=_condor_get_unmapped_fd(fd)) < 0 ) {
		rval = -1;
	} else {
		if( LocalSysCalls() || _condor_is_fd_local(fd) ) {
			rval = syscall( SYS_putpmsg, real_fd, cptr, dptr, band, flags );
		} else {
			errno = ENOSTR;
			rval = -1;
		}
	}

	_condor_signals_enable(omask);
	return rval;
}

int _putpmsg( int fd, STREAM_CONST struct strbuf *cptr, 
			  STREAM_CONST struct strbuf *dptr, int band, int flags )
{
	return putpmsg( fd, cptr, dptr, band, flags );
}

int __putpmsg( int fd, STREAM_CONST struct strbuf *cptr, 
			   STREAM_CONST struct strbuf *dptr, int band, int flags )
{
	return putpmsg( fd, cptr, dptr, band, flags );
}


#endif


/*
The Linux C library uses mmap for several purposes.  The I/O
system uses mmap() to get a nicely aligned buffer.  The memory
system also uses mmap() for large allocation requests.  In general,
we don't support mmap(), but we can support calls with particular
flags (MAP_ANON|MAP_PRIVATE) because they translate to "give me
a clean new segment".  In these cases, we turn around and malloc()
some new memory and trust that the caller doesn't really care that
it came from the heap.
*/

#if defined( LINUX  )
#include "condor_mmap.h"
extern "C" int REMOTE_CONDOR_mmap(MMAP_T, size_t, int, int,
	int, off_t);
MMAP_T
mmap( MMAP_T a, size_t l, int p, int f, int fd, off_t o )
{
        MMAP_T rval = NULL;
        static int recursive=0;

        if( (f & MAP_ANONYMOUS) && (f & MAP_PRIVATE) ) {
                if(recursive) {
                        rval = (MMAP_T) MAP_FAILED;
                } else {
                        recursive = 1;
                        rval = (MMAP_T) malloc( l );
                        recursive = 0;
                }
        } else {
                if( LocalSysCalls() ) {
                        rval = (MMAP_T) MAP_FAILED;
                        errno = ENOSYS;
                } else {
                    rval = (MMAP_T) REMOTE_CONDOR_mmap( a, l, p, f, fd, o );
                }
        }

		/* now that we have the memory, zero it out because we _would_ have
			mapped in /dev/zero */
		if (rval != NULL && rval != MAP_FAILED)
		{
			memset(rval, 0, l);
		}

        return rval;
}

int munmap( MMAP_T addr, size_t /* length */ ) 
{
        if(addr) free(addr);
        return 0;
}
#endif /* defined( LINUX ) */

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

  -Derek Wright 6/30/99
*/

/* 
   First, do all the hell to figure out what actual numbers we'll want
   to use with syscall() in the local cases.  Define our own
   CONDOR_SYS_stat* to be whatever pre-processor definition we already
   have.
*/

#if defined(LINUX)
#	if defined(SYS_prev_stat)
#	   define CONDOR_SYS_stat SYS_prev_stat
#	elif defined(SYS_stat)
#	   define CONDOR_SYS_stat SYS_stat
#	elif defined(SYS_old_stat)
#	   define CONDOR_SYS_stat SYS_old_stat
#	else
#	   error "No local version of SYS_*_stat on this platform!"
#	endif
#else
#	error "Do not understand the stat syscall mapping on this platform!"
#endif

#if defined(LINUX)
#	if defined(SYS_prev_fstat)
#	   define CONDOR_SYS_fstat SYS_prev_fstat
#	elif defined(SYS_fstat)
#	   define CONDOR_SYS_fstat SYS_fstat
#	elif defined(SYS_old_fstat)
#	   define CONDOR_SYS_fstat SYS_old_fstat
#	else
#		error "No local version of SYS_*_fstat on this platform!"
#	endif
#else
#	error "Do not understand the stat syscall mapping on this platform!"
#endif

#if defined(LINUX)
#	if defined(SYS_prev_lstat)
#	   define CONDOR_SYS_lstat SYS_prev_lstat
#	elif defined(SYS_lstat)
#	   define CONDOR_SYS_lstat SYS_lstat
#	elif defined(SYS_old_lstat)
#	   define CONDOR_SYS_lstat SYS_old_lstat
#	else
#		error "No local version of SYS_*_lstat on this platform!"
#	endif
#else
#	error "Do not understand the stat syscall mapping on this platform!"
#endif


/* 
   Now, trap all the various [_flx]*stat calls, and convert them all
   to _condor_[xlf]stat().
*/

int _xstat(int version, const char *path, struct stat *buf)
{
	return _condor_xstat( version, path, buf );
}


int __xstat(int version, const char *path, struct stat *buf) __THROW
{
	return _condor_xstat( version, path, buf );
}

int _fxstat(int version, int fd, struct stat *buf)
{
	return _condor_fxstat( version, fd, buf );
}

int __fxstat(int version, int fd, struct stat *buf) __THROW
{
	return _condor_fxstat( version, fd, buf );
}


int _lxstat(int version, const char *path, struct stat *buf)
{
	return _condor_lxstat( version, path, buf );
}


int __lxstat(int version, const char *path, struct stat *buf) __THROW
{
	return _condor_lxstat( version, path, buf );
}

#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
int _xstat64(int version, const char *path, struct stat64 *buf)
{
	return _condor_xstat64( version, path, buf );
}

int __xstat64(int version, const char *path, struct stat64 *buf) __THROW
{
	return _condor_xstat64( version, path, buf );
}

int _fxstat64(int version, int fd, struct stat64 *buf)
{
	return _condor_fxstat64( version, fd, buf );
}

int __fxstat64(int version, int fd, struct stat64 *buf) __THROW
{
	return _condor_fxstat64( version, fd, buf );
}

int _lxstat64(int version, const char *path, struct stat64 *buf)
{
	return _condor_lxstat64( version, path, buf );
}

int __lxstat64(int version, const char *path, struct stat64 *buf) __THROW
{
	return _condor_lxstat64( version, path, buf );
}
#endif /* GLIBC22 || GLIBC23 */

/*
   _condor_k_stat_convert() takes in a version, and two pointers, one
   to a struct kernel_stat (which we define in kernel_stat.h), and one
   to a struct stat (the type of which is determined by the version
   arg).  For all versions of struct stat, copy out the fields we care
   about, and depending on the version, zero out the fields we don't
   care about.  
*/


#if defined(LINUX)
#if defined(I386)
	#include "linux_kernel_stat.h"
#else
	/* for X86_64, it seems that the kernel stat structure and the user stat
		structure are identical */
	#define kernel_stat /*struct*/ stat
#endif
#endif /* LINUX */

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
#if defined(LINUX)
#if defined(I386)
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
#endif
	case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/
#if defined(I386)
		target->__pad1 = 0;
		target->__pad2 = 0;
#endif
		#if !defined(GLIBC23) && !defined(GLIBC24) && !defined(GLIBC25) && !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		#endif
		#if defined (GLIBC21)
			/* is someone *SURE* we don't need this in glibc22, as well? */
		target->__unused4 = 0;
		target->__unused5 = 0;
		#endif
		break;
#endif

	default:
			/* Some other version: blow-up */
		EXCEPT( "_condor_k_stat_convert: unknown version (%d) of struct stat!", version );
		break;
	}
}

#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
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
#if defined(LINUX)
#if defined(I386)
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
#endif
	case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/
#if defined(I386)
		target->__pad1 = 0;
		target->__pad2 = 0;
#endif
		#if !defined(GLIBC23) && !defined(GLIBC24) && !defined(GLIBC25) && !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
		/* glibc23 uses these fields */
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		#endif
		#if defined(GLIBC21)
			/* is someone *SURE* we don't need this in glibc22, as well? */
		target->__unused4 = 0;
		target->__unused5 = 0;
		#endif
		break;
#endif
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
#if defined(LINUX)
#if defined(I386)
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
#endif
	case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/

#if defined(I386)
		target->__pad1 = 0;
		target->__pad2 = 0;
#endif
		#if !defined(GLIBC23) && !defined(GLIBC24) && !defined(GLIBC25) && !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		#endif
#if defined(I386)
		target->__unused4 = 0;
		target->__unused5 = 0;
#endif
		break;
#endif
	default:
			/* Some other version: blow-up */
		EXCEPT( "_condor_s_stat_convert: unknown version (%d) of struct stat!", version );
		break;
	}
}

#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
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
#if defined(I386)
	case _STAT_VER_LINUX_OLD:
			/*
			  This is the old version, which is identical to the
			  kernel version.  We already copied all the fields we
			  care about, so we're done.
			*/
		break;
#endif
	case _STAT_VER_LINUX:
			/* 
			  This is the new version, that has some extra fields we
			  need to 0-out.
			*/
#if defined(I386)
		target->__pad1 = 0;
		target->__pad2 = 0;
#endif
		#if !defined(GLIBC23) && !defined(GLIBC24) && !defined(GLIBC25) && !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
		/* glibc23 uses these fields */
		target->__unused1 = 0;
		target->__unused2 = 0;
		target->__unused3 = 0;
		#endif
		#if defined(GLIBC21)
			/* is someone *SURE* we don't need this in glibc22, as well? */
		target->__unused4 = 0;
		target->__unused5 = 0;
		#endif
		break;
	default:
			/* Some other version: blow-up */
		EXCEPT( "_condor_s_stat_convert64: unknown version (%d) of struct stat!", version );
		break;
	}
}
#endif

#if defined(LINUX)
	#define OPT_STAT_VERSION 
#endif

/*
   Finally, implement the _condor_[xlf]stat() functions which do all
   the work of checking if we want local or remote, and handle the
   different versions of the stat struct as appropriate.

   Note: it is important that we only overwrite the buf the caller
   passed in if the syscall succeeded.  For example, Fortran on Irix
   6.5 doesn't check the return value of stat() calls, but instead
   just checks to see if the stat structure has changed.  If we pass
   back uninitialized garbage from our stack on failed calls, we're
   going to confuse our caller.  -Jim B. (5/8/2000)
*/

extern "C" int REMOTE_CONDOR_stat( const char *, struct stat * );
int _condor_xstat(int version, const char *path, struct stat *buf)
{
	int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	int do_local = 0;

	sigset_t omask = _condor_signals_disable();

	char *newname = NULL;
	do_local = _condor_is_file_name_local( path, &newname );
	path = newname;

	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->stat( path, &sbuf );	
		if (rval == 0 ) {
			_condor_s_stat_convert( version, &sbuf, buf );
		}		
	} else {
		if( LocalSysCalls() || do_local ) {
			rval = syscall( CONDOR_SYS_stat, OPT_STAT_VERSION path, &kbuf );
			if (rval >= 0) {
				_condor_k_stat_convert( version, &kbuf, buf );
			}
		} else {
			rval = REMOTE_CONDOR_stat( path, &sbuf );
			if (rval >= 0) {
				_condor_s_stat_convert( version, &sbuf, buf );
			}
		}
	}
	free( newname );
	_condor_signals_enable(omask);
	return rval;
}

#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
extern "C" int REMOTE_CONDOR_stat( const char *, struct stat * );
int _condor_xstat64(int version, const char *path, struct stat64 *buf)
{
	int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	int do_local = 0;

	sigset_t omask = _condor_signals_disable();

	char *newname = NULL;
	do_local = _condor_is_file_name_local( path, &newname );
	path = newname;

	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->stat( path, &sbuf );	
		if (rval == 0 ) {
			_condor_s_stat_convert64( version, &sbuf, buf );
		}		
	} else {
		if( LocalSysCalls() || do_local ) {
			rval = syscall( CONDOR_SYS_stat, path, &kbuf );
			if (rval >= 0) {
				_condor_k_stat_convert64( version, &kbuf, buf );
			}
		} else {
			rval = REMOTE_CONDOR_stat( path, &sbuf );
			if (rval >= 0) {
				_condor_s_stat_convert64( version, &sbuf, buf );
			}
		}
	}
	free( newname );
	_condor_signals_enable(omask);
	return rval;
}
#endif

extern "C" int REMOTE_CONDOR_fstat( int, struct stat * );
int
_condor_fxstat(int version, int fd, struct stat *buf)
{
	int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;

	sigset_t omask = _condor_signals_disable();

	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->fstat( fd, &sbuf );	
		if (rval == 0 ) {
			_condor_s_stat_convert( version, &sbuf, buf );
		}		
	} else {
		if( LocalSysCalls() ) {
			rval = syscall( CONDOR_SYS_fstat, OPT_STAT_VERSION fd, &kbuf );
			if (rval >= 0) {
				_condor_k_stat_convert( version, &kbuf, buf );
			}
		} else {
			rval = REMOTE_CONDOR_fstat( fd, &sbuf );
			if (rval >= 0) {
				_condor_s_stat_convert( version, &sbuf, buf );
			}
		}
	}

	_condor_signals_enable(omask);
	return rval;
}

#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
extern "C" int REMOTE_CONDOR_fstat( int, struct stat * );
int
_condor_fxstat64(int version, int fd, struct stat64 *buf)
{
	int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;

	sigset_t omask = _condor_signals_disable();

	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->fstat( fd, &sbuf );	
		if (rval == 0 ) {
			_condor_s_stat_convert64( version, &sbuf, buf );
		}		
	} else {
		if( LocalSysCalls() ) {
			rval = syscall( CONDOR_SYS_fstat, fd, &kbuf );
			if (rval >= 0) {
				_condor_k_stat_convert64( version, &kbuf, buf );
			}
		} else {
			rval = REMOTE_CONDOR_fstat( fd, &sbuf );
			if (rval >= 0) {
				_condor_s_stat_convert64( version, &sbuf, buf );
			}
		}
	}

	_condor_signals_enable(omask);
	return rval;
}
#endif

extern "C" int REMOTE_CONDOR_lstat( const char *, struct stat *);
int _condor_lxstat(int version, const char *path, struct stat *buf)
{
	int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	int do_local = 0;

	sigset_t omask = _condor_signals_disable();

	char *newname = NULL;
	do_local = _condor_is_file_name_local( path, &newname );
	path = newname;

	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->lstat( path, &sbuf );	
		if (rval == 0 ) {
			_condor_s_stat_convert( version, &sbuf, buf );
		}		
	} else {
		if( LocalSysCalls() ) {
			rval = syscall( CONDOR_SYS_lstat, OPT_STAT_VERSION path, &kbuf );
			if (rval >= 0) {
				_condor_k_stat_convert( version, &kbuf, buf );
			}
		} else {
			rval = REMOTE_CONDOR_lstat( path, &sbuf );
			if (rval >= 0) {
				_condor_s_stat_convert( version, &sbuf, buf );
			}
		}
	}
	free( newname );
	_condor_signals_enable(omask);
	return rval;
}

#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
extern "C" int REMOTE_CONDOR_lstat( const char *, struct stat *);
int _condor_lxstat64(int version, const char *path, struct stat64 *buf)
{
	int rval;
	struct kernel_stat kbuf;
	struct stat sbuf;
	int do_local = 0;

	sigset_t omask = _condor_signals_disable();

	char *newname = NULL;
	do_local = _condor_is_file_name_local( path, &newname );
	path = newname;

	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->lstat( path, &sbuf );	
		if (rval == 0 ) {
			_condor_s_stat_convert64( version, &sbuf, buf );
		}		
	} else {
		if( LocalSysCalls() ) {
			rval = syscall( CONDOR_SYS_lstat, path, &kbuf );
			if (rval >= 0) {
				_condor_k_stat_convert64( version, &kbuf, buf );
			}
		} else {
			rval = REMOTE_CONDOR_lstat( path, &sbuf );
			if (rval >= 0) {
				_condor_s_stat_convert64( version, &sbuf, buf );
			}
		}
	}
	free( newname );
	_condor_signals_enable(omask);
	return rval;
}
#endif

#endif /* LINUX stat hell */

/*
On Solaris, stat, lstat, and fstat are statically defined in stat.h,
so we can't override them.  Instead, we override the following three
xstat definitions.

These definitions look wrong to me.  I think they may need to change
to be similar to the Linux definitions.  thain, 28 Dec 1999.
*/

#if defined(Solaris)

extern "C" int REMOTE_CONDOR_stat( const char *, struct stat * );
int _xstat(const int ver, const char *path, struct stat *buf)
{
	int	rval;
	int	do_local = 0;

	sigset_t omask = _condor_signals_disable();

	char *newname = NULL;
	do_local = _condor_is_file_name_local( path, &newname );
	path = newname;


	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->stat( path, buf );
	} else {
		if( LocalSysCalls() || do_local ) {
			rval = syscall( SYS_xstat, ver, path, buf );
		} else {
			rval = REMOTE_CONDOR_stat( path, buf );
		}
	}
	free( newname );
	_condor_signals_enable(omask);
	return rval;
}

extern "C" int REMOTE_CONDOR_lstat( const char *, struct stat *);
int _lxstat(const int ver, const char *path, struct stat *buf)
{
	int	rval;
	int	do_local=0;

	sigset_t omask = _condor_signals_disable();

	char *newname = NULL;
	do_local = _condor_is_file_name_local( path, &newname );
	path = newname;


	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->lstat( path, buf );
	} else {
		if( LocalSysCalls() || do_local ) {
			rval = syscall( SYS_lxstat, ver, path, buf );
		} else {
			rval = REMOTE_CONDOR_lstat( path, buf );
		}
	}
	free( newname );
	_condor_signals_enable(omask);
	return rval;
}

extern "C" int REMOTE_CONDOR_fstat( int, struct stat * );
int _fxstat(const int ver, int fd, struct stat *buf)
{
	int	rval;

	sigset_t omask = _condor_signals_disable();

	if( MappingFileDescriptors() ) {
		_condor_file_table_init();
		rval = FileTab->fstat( fd, buf );
	} else {
		if( LocalSysCalls() ) {
			rval = syscall( SYS_fxstat, ver, fd, buf );
		} else {
			rval = REMOTE_CONDOR_fstat( fd, buf );
		}
	}

	_condor_signals_enable(omask);
	return rval;
}
#endif /* Solaris */

#endif /* FILE_TABLE */


/*************************************************************
The following switches are for general purpose remote syscalls.
Notice that some of these deal with files and fds, but none
consult or change the open file table.
*************************************************************/

#ifdef REMOTE_SYSCALLS

/*
We really don't pretend to support signals or sleeping of any
kind.  However, we are willing to go to a little effort to
see sleep() at least delay for some amount of time if no
checkpoint intervenes.  If a checkpoint interrupts a sleep,
any timers that were set are lost, but sleep() will return
EINTR after a checkpoint anyway, thus preventing an infinite loop.

Except on HPUX.  HPUX relies on setitimer and getitimer to
implement sleep.  We don't really want to support these, but
they need to work correctly to at least prevent sleep() from
looping infinitely.
*/

#if defined(HPUX)

int setitimer( int which, const struct itimerval *value, struct itimerval *oval )
{
	int scm;
	int result;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = syscall( SYS_setitimer, which, value, oval );
	SetSyscalls(scm);

	return result;
}

int getitimer( int which, struct itimerval *value )
{
	int scm;
	int result;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = syscall( SYS_getitimer, which, value );
	SetSyscalls(scm);

	return result;
}

#endif

/*
We don't handle readv directly in ANY case.  Split up the read
and pass it through the regular read mechanism to take advantage
of whatever magic is implemented there.  Notice that this works for
both remote and local system calls.
*/

ssize_t readv( int fd, const struct iovec *iov, int iovcnt )
{
	int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = read( fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) return cc;
		rval += cc;
		if( ((unsigned) cc) != iov->iov_len ) return rval;
		iov++;
	}

	return rval;
}

/*
We don't handle writev directly in ANY case.  Split up the write
and pass it through the regular write mechanism to take advantage
of whatever magic is implemented there.  Notice that this works for
both remote and local system calls.
*/

ssize_t writev( int fd, const struct iovec *iov, int iovcnt )
{
	int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = write( fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) return cc;
		rval += cc;
		if( ((unsigned) cc) != iov->iov_len ) return rval;
		iov++;
	}

	return rval;
}

/*
  The process should exit making the status value available to its parent
  (the starter) - can only be a local operation.
*/
void _exit( int status )
{
	/* XXX this breaks atexit() and global C++ destructors */
	(void) syscall( SYS_exit, status );

	exit(status); // shouldn't get here, but supresses "_exit didn't exit" warning
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
extern "C" int REMOTE_CONDOR_getrusage( int, struct rusage * );
int
getrusage( int who, struct rusage *rusage )
{
	int rval = 0;
	int rval1 = 0;
	int scm;
	static struct rusage accum_rusage;
	static unsigned int num_restarts = 50;  /* must not initialize to 0 */

	sigset_t omask = _condor_signals_disable();

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

		if ((handle = dlopen("libc.so", RTLD_LAZY)) == NULL) {
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
			_condor_signals_enable(omask);
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
				rval = REMOTE_CONDOR_getrusage( who, &accum_rusage );
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

	_condor_signals_enable(omask);
	if ( rval == 0  && rval1 == 0 ) {
		return 0;
	} else {
		return -1;
	}
}

/* Programs which call gettimeofday() in tight loops trying to calculate
	timing statistics will really slow down as each gettimeofday()
	goes across the network to the shadow. So, we're going to
	implement a cache where the first time gettimeofday() is called
	it will contact the shadow, but afterwards it'll use a cache. The
	execution machine time is gotten and the difference from the last
	execution machine time is added the the submission machine time.
	This should fix time skew since _instances_ in time are relative
	to the submission machine, but the passing of time is relative
	to the execution machine.

	We have to be careful to reinitialize the cache after we resume
	from a checkpoint to keep eveything as correct as possible. This
	reinitalization happens elsewhere.
*/

#undef gettimeofday
#if defined( SYS_gettimeofday )
#if defined(LINUX)
extern "C" int REMOTE_CONDOR_gettimeofday(struct timeval *, struct timezone *);
int gettimeofday (struct timeval *tp, struct timezone *tzp)
#else
extern "C" int REMOTE_CONDOR_gettimeofday(struct timeval *, void *);
int gettimeofday (struct timeval *tp, void *tzp)
#endif
{
	struct timeval exe_machine_now;
	struct timeval sub_machine_now;

	int rval, do_local=0;
	errno = 0;

	sigset_t condor_omask = _condor_signals_disable();

	if( LocalSysCalls() || do_local ) {

		rval = syscall( SYS_gettimeofday, tp , tzp );

	} else {

		/* initialize the in memory cache, if needed */
		if (_condor_gtodc_is_initialized(_condor_global_gtodc) == FALSE) {

			/* get "now" from the submit machine */
			rval = -1;
			rval = REMOTE_CONDOR_gettimeofday( &sub_machine_now, tzp );
			if (rval != 0) {
				goto finished;
			}

			/* get "now" from the execute machine */
			rval = syscall( SYS_gettimeofday, &exe_machine_now, tzp );
			if (rval != 0) {
				goto finished;
			}
			
			_condor_gtodc_set_now(_condor_global_gtodc, &exe_machine_now, 
							&sub_machine_now);

			_condor_gtodc_set_initialized(_condor_global_gtodc, TRUE);

		}

		/* get "now" on the local machine, the differential of this and
			when I initially filled the cache will be added to the 
			submit machine's original "now" and returned to the user. */
		rval = syscall( SYS_gettimeofday, &exe_machine_now, tzp );
		if (rval != 0) {
			goto finished;
		}

		/* determine how much time has passed since the last gettimeofday()
			call has happened (in terms of the submit machine), but without 
			going to the submit machine. */
		_condor_gtodc_synchronize(_condor_global_gtodc, tp, &exe_machine_now);

	}

	finished:
	_condor_signals_enable( condor_omask );

	return rval;
}
#endif


/* This function call performs a true system() call in local mode, and in
	remote mode calls back to a pseudo call to the shadow, which
	performs the actual system() invocation.  The reason it is a pseudo call
	in the shadow is we have to write a process wrapper around the invocation
	if the system() because it can block indefinitely, and if the job ends up
	getting killed or must be evicted, the shadow can still have control
	over the situation on the remote end.

	Obviously, there are serious issues with system(), checkpointing, and
	hetergeneous submission...
	like the fact it is probably not idempotent across checkpointins...
*/
extern "C" int REMOTE_CONDOR_shell( char *, int );
static int local_system_posix(const char *command);
static int remote_system_posix(const char *command, int len);
static int shell ( const char *command, int len )
{
	int rval, do_local=0;
	errno = 0;

	sigset_t condor_omask = _condor_signals_disable();

	if( LocalSysCalls() || do_local ) {
		rval = local_system_posix( command );
	} else {
		rval = remote_system_posix( command, len );
	}

	_condor_signals_enable( condor_omask );

	return rval;
}

/* here is the actual system() libc entrance call */
int system( const char *command )
{
	/* include space for a terminating nul character on the receiver side */
	return shell( command, strlen(command) + 1 );
}

/* this props up the environment string for the job and passes the entire thing
	to the shadow for execution. */
static int remote_system_posix(const char *command, int len)
{
	extern char **environ;
	char **ev;
	int rval;
	int envsize = 0;
	char *str;

	/* if this process has an environment, then preserve it when the shadow
		executes this subprocess by prepending the job's environment before the
		command the user wanted to execute. XXX This function does not perform
		any escaping of any environment variables, so things like spaces and
		other things could be interpreted badly if they are in the environment.
	*/
	if (*environ != NULL) {

		/* count up how much environment I need */
		for (ev = environ; *ev != NULL; ev++) {
			/* add three for ' ; ' at the end of each env var */
			envsize += strlen(*ev) + 3;
		}

		/* add the size of the executing command */
		envsize += len;

		/* add one for the nul character at the end of it all */
		envsize += 1;
	
		/* get some memory */
		str = (char*)malloc(sizeof(char*) * envsize);
		if (str == NULL) {
			EXCEPT( "remote_system_posix(): Out of memory!");
		}
		memset(str, 0, envsize);

		/* append everything together */
		strcpy(str, *environ);
		strcat(str, " ; ");
		for (ev = environ + 1; *ev != NULL; ev++) {
			strcat(str, *ev);
			strcat(str, " ; ");
		}

		/* add the command at the end */
		strcat(str, command);

		/* execute the entire ball of wax with the remote shell */
		rval = REMOTE_CONDOR_shell( str, envsize );

		free(str);

		return rval;
	}

	/* else just do what the user wanted of me */
	rval = REMOTE_CONDOR_shell( const_cast<char*>(command), len );
	return rval;
}

/* here is the standalone implementation of system() */
static int local_system_posix(const char *command)
{
	pid_t pid;
	int status = -1;
	struct sigaction ignore, saveintr, savequit;
	sigset_t chldmask, savemask;
	const char *argv[4];
	extern char **environ;

	if (command == NULL) {
		/* always a command processor under unix */
		return 1;
	}

	/* ignore sigint and sigquit. Do this BEFORE the fork() */
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	ignore.sa_flags = 0;
	if (sigaction(SIGINT, &ignore, &saveintr) < 0) {
		return -1;
	}
	if (sigaction(SIGQUIT, &ignore, &savequit) < 0) {
		return -1;
	}

	/* Now block SIGCHLD */
	sigemptyset(&chldmask);
	sigaddset(&chldmask, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0) {
		return -1;
	}

#if defined(LINUX)
	if ((pid = SYSCALL(SYS_fork)) < 0) {
#elif defined(Solaris)
	if ((pid = _libc_FORK()) < 0) {
#elif defined(HPUX)
	if ((pid = _FORK_sys()) < 0) {
#else
#error "Please port which 'fork' I need on this platform"
#endif
		status = -1;
	} else if (pid == 0) { 
		/* child */
		
		SetSyscalls(SYS_LOCAL | SYS_UNMAPPED);

		sigaction(SIGINT, &saveintr, NULL);
		sigaction(SIGQUIT, &savequit, NULL);
		sigprocmask(SIG_SETMASK, &savemask, NULL);

		argv[0] = "sh";
		argv[1] = "-c";
		argv[2] = command;
		argv[3] = NULL;
		syscall(SYS_execve, "/bin/sh", argv, environ);

		_exit(127);

	} else {
		/* parent */
#if (defined(LINUX) && defined(I386)) || defined(HPUX)
		while(SYSCALL(SYS_waitpid, pid, &status, 0) < 0) {
#elif defined(LINUX) && defined(X86_64)
		while(SYSCALL(SYS_wait4, pid, &status, 0) < 0) {
#elif defined(Solaris)
		while(_WAITPID(pid, &status, 0) < 0) {
#else
#error "Please port which 'waitpid' I need on this platform"
#endif
			if (errno != EINTR) {
				status = -1;
				break;
			}
		}
	}
	
	/* restore previous signal actions and reset signal mask */
	if (sigaction(SIGINT, &saveintr, NULL) < 0) {
		return -1;
	}
	if (sigaction(SIGQUIT, &savequit, NULL) < 0) {
		return -1;
	}
	if (sigprocmask(SIG_SETMASK, &savemask, NULL) < 0) {
		return -1;
	}

	return status;
}



/* This routine implements an entry point into the stack protection codes
in later glibcs */

#if defined(LINUX) && defined(GLIBC24)

void __stack_chk_fail(void)
{
	write(1, "*** stack smashing detected *** terminated\n", 43);
	exit(1);
}
void __stack_chk_fail_local(void)
{
	__stack_chk_fail ();
}
#endif


/*
  This routine which is normally provided in the C library determines
  whether a given file descriptor refers to a tty.  The underlying
  mechanism is an ioctl, but Condor does not support ioctl.  We
  therefore provide a "quick and dirty" substitute here.  This may
  not always be correct, but it will get you by in most cases.
*/
int __isatty( int filedes )
{
	if( RemoteSysCalls() ) {
		return FALSE;
	}

	switch( filedes ) {
		case 0:
		case 1:
		case 2:
			return TRUE;
		default:
			return FALSE;
	}
}

int _isatty( int fd )
{
	return __isatty(fd);
}

int isatty( int fd )
{
	return __isatty(fd);
}

/* Same purpose as isatty, but asks if tape.  Present in Fortran library.
   I don't know if this is the real function format, but it's a good guess. */

int __istape( int /*fd*/ )
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
long sysconf(int name)
{
	if (name == _SC_MAPPED_FILES) return 0;
	return SYSCONF(name);
}

long _sysconf(int name)
{
	return sysconf(name);
}
#endif

/* getlogin needs to be special because it returns a char*, and until
 * we make stub_gen return longs instead of ints, casting char* to an int
 * causes problems on some platforms...  also, stubgen does not deal
 * well with a function like getlogin which returns a NULL on error */
extern "C" int REMOTE_CONDOR_getlogin( char ** );
char *
getlogin()
{
	int rval;
	static char *loginbuf = NULL;

	if( LocalSysCalls() ) {
#if defined( SYS_getlogin )
		char* rval_str = (char *) syscall( SYS_getlogin );
		return rval_str;
#elif defined( DL_EXTRACT ) 
		{
        void *handle;
        char * (*fptr)();
        if ((handle = dlopen("libc.so", RTLD_LAZY)) == NULL) {
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
		free( loginbuf );
		loginbuf = NULL;
		rval = REMOTE_CONDOR_getlogin( &loginbuf );
	}

	if( rval >= 0 ) {
		return loginbuf;
	} else {
		return NULL;
	}
}

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

extern "C" int REMOTE_CONDOR_sync(void);
#if SYNC_RETURNS_VOID
void
#else
int
#endif
sync( void )
{
	/* Always flush buffered data from the file table */
	_condor_file_table_init();
	FileTab->flush();

	/* Always want to do a local sync() */
	SYSCALL( SYS_sync );

	/* If we're in remote mode, also want to send a sync() to the shadow */
	if( RemoteSysCalls() ) {
		REMOTE_CONDOR_sync( );
	}
#if ! SYNC_RETURNS_VOID
	return 0;
#endif
}

#endif /* REMOTE_SYSCALLS */


} // end extern "C"
