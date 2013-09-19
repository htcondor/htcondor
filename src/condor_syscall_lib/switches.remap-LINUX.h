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


extern "C" {
/* These remaps are needed by file_state.c */
#if defined(FILE_TABLE)

REMAP_ONE( close, __close, int , int )
REMAP_TWO( creat, __creat, int, const char*, mode_t )
REMAP_ONE( dup, __dup, int, int )
REMAP_TWO( dup2, __dup2, int, int, int )
REMAP_ONE( fchdir, __fchdir, int, int )
REMAP_THREE_VARARGS( fcntl, __fcntl, int , int , int , int)
REMAP_ONE( fsync, __fsync, int , int )

#if defined(I386)
REMAP_TWO( ftruncate, __ftruncate, int , int , off_t )
#endif

REMAP_THREE( poll, __libc_poll, int , struct pollfd *, unsigned long , int )
REMAP_THREE( poll, __poll, int , struct pollfd *, unsigned long , int )

#if defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27)|| defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)

	REMAP_TWO( ftruncate64, __ftruncate64, int , int , off64_t )
#endif

#if defined(I386)
REMAP_THREE_VARARGS( ioctl, __ioctl, int , int , unsigned long , int)
#else
REMAP_THREE_VARARGS( ioctl, __ioctl, int , unsigned long , unsigned long , int)
#endif

REMAP_THREE( lseek, __lseek, off_t, int, off_t, int )

#if defined(GLIBC21) || defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
REMAP_THREE( llseek, __llseek, off64_t, int, off64_t, int )
REMAP_THREE( llseek, _llseek, off64_t, int, off64_t, int )
REMAP_THREE( llseek, __lseek64, off64_t, int, off64_t, int )
REMAP_THREE( llseek, lseek64, off64_t, int, off64_t, int )
#elif defined(GLIBC20)
REMAP_THREE( llseek, __llseek, unsigned long, int, unsigned long, int )
REMAP_THREE( llseek, __lseek64, unsigned long, int, unsigned long, int )
REMAP_THREE( llseek, lseek64, unsigned long, int, unsigned long, int )
#else
REMAP_THREE( llseek, __llseek, long long int, int, long long int, int )
REMAP_THREE( llseek, __lseek64, long long int, int, long long int, int )
REMAP_THREE( llseek, lseek64, long long int, int, long long int, int )
#endif


REMAP_THREE_VARARGS( open, __open, int, const char *, int, int )

#if defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
REMAP_THREE_VARARGS( open64, __open64, int, const char *, int, int )
REMAP_THREE_VARARGS( open64, __libc_open64, int, const char *, int, int )
#endif

REMAP_THREE( read, __read, ssize_t, int, __ptr_t, size_t )

REMAP_THREE( write, __write, ssize_t, int, const __ptr_t, size_t )
#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
REMAP_THREE( write, __libc_write, ssize_t, int, const __ptr_t, size_t )
#endif

/* make a bunch of __libc remaps for the things that have fd's or paths.
	These were new entry points in glibc22 */
#if defined(GLIBC22) || defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
REMAP_ONE( close, __libc_close, int , int )
REMAP_TWO( creat, __libc_creat, int, const char*, mode_t )
REMAP_ONE( dup, __libc_dup, int, int )
REMAP_TWO( dup2, __libc_dup2, int, int, int )
REMAP_ONE( fchdir, __libc_fchdir, int, int )
REMAP_THREE_VARARGS( fcntl, __libc_fcntl, int , int , int , int)

#if defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
/* According to the implementation of these function in glibc 2.5 they appear
to be simple wrappers around their specific system call */
REMAP_THREE_VARARGS( fcntl, __fcntl_nocancel, int , int , int , int )

REMAP_THREE( write, __write_nocancel, ssize_t , int , const void* , size_t )

REMAP_THREE( read, __read_nocancel, ssize_t , int , void* , size_t )

REMAP_TWO_VARARGS( open, __open_nocancel, int, const char * , int )
REMAP_ONE( close, __close_nocancel, int, int )
#endif

REMAP_ONE( fsync, __libc_fsync, int , int )
REMAP_TWO( ftruncate, __libc_ftruncate, int , int , off_t )
REMAP_THREE_VARARGS( ioctl, __libc_ioctl, int , int , unsigned long , int)
REMAP_THREE( llseek, __libc_llseek, off64_t, int, off64_t, int )
REMAP_THREE( llseek, __libc_lseek64, off64_t, int, off64_t, int )
REMAP_THREE_VARARGS( open, __libc_open, int, const char *, int, int )
REMAP_THREE( read, __libc_read, ssize_t, int, __ptr_t, size_t )
#endif

#if defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
REMAP_THREE( lseek, __libc_lseek, off_t, int, off_t, int )
REMAP_FIVE( select, __libc_select, int , int , fd_set *, fd_set *, fd_set *, struct timeval *)
#endif

#endif /* if defined(FILE_TABLE) */

/* These remaps are neede by signals_support.c */
#if defined(SAVE_SIGSTATE)

#if defined(I386)
REMAP_THREE( sigprocmask, __sigprocmask, int, int, const sigset_t *, sigset_t * )
REMAP_ONE( sigsetmask, __sigsetmask, int, int )
REMAP_ONE( sigsuspend, __sigsuspend, int, const sigset_t * )
REMAP_ONE( sigsuspend, __libc_sigsuspend, int, const sigset_t * )
REMAP_THREE( sigaction, __sigaction, int, int, const struct sigaction *, struct sigaction * )
REMAP_THREE( sigaction, __libc_sigaction, int, int, const struct sigaction *, struct sigaction * )
#endif

#endif /* SAVE_SIGSTATE */

/* These remaps fill out the rest of the remote syscalls */
#if defined(REMOTE_SYSCALLS)

REMAP_TWO( access, __access, int , const char *, int )
REMAP_ONE( acct, __acct, int , const char *)
REMAP_ONE( chdir, __chdir, int, const char* )
REMAP_TWO( chmod, __chmod, int , const char *, mode_t )
REMAP_THREE( chown, __chown, int , const char *, uid_t , gid_t )
REMAP_ONE( chroot, __chroot, int , const char *)
REMAP_THREE( execve, __execve, int , const char *, char * const *, char * const *)
REMAP_TWO( fchmod, __fchmod, int , int , mode_t )
REMAP_THREE( fchown, __fchown, int , int , uid_t , gid_t )
REMAP_TWO( flock, __flock, int , int , int )
REMAP_ZERO( fork, __fork, pid_t )
REMAP_ZERO( fork, __libc_fork, pid_t )
REMAP_ZERO( vfork, __vfork, pid_t )
REMAP_TWO( fstatfs, __fstatfs, int , int , struct statfs * )
REMAP_ZERO( getegid, __getegid, gid_t )
REMAP_ZERO( geteuid, __geteuid, uid_t )
REMAP_ZERO( getgid, __getgid, gid_t )
REMAP_TWO( getgroups, __getgroups, int , int , gid_t*)
REMAP_TWO( getitimer, __getitimer, int , int , struct itimerval *)
REMAP_ONE( getpgid, __getpgid, pid_t , pid_t )
REMAP_ZERO( getpgrp, __getpgrp, pid_t )
REMAP_ZERO( getpid, __getpid, pid_t )
REMAP_ZERO( getppid, __getppid, pid_t )
REMAP_TWO( getrlimit, __getrlimit, int , int , struct rlimit *)
REMAP_TWO( gettimeofday, __gettimeofday, int , struct timeval *, struct timezone *)

/* getuid is now a weak alias to __getuid */
#if !defined(GLIBC22) && !defined(GLIBC23) && !defined(GLIBC24) && !defined(GLIBC25) && !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
REMAP_ZERO( getuid, __getuid, uid_t )
#endif

REMAP_TWO( kill, __kill, int, pid_t, int )
REMAP_TWO( link, __link, int , const char *, const char *)
REMAP_TWO( mkdir, __mkdir, int , const char *, mode_t )

REMAP_SIX( mmap, __mmap, void *, void *, size_t, int, int, int, off_t )

#if defined(X86_64)
REMAP_SIX( mmap, __mmap64, void *, void *, size_t, int, int, int, off_t )
#endif

REMAP_FIVE( mount, __mount, int , const char *, const char *, const char *, unsigned long , const void *)
REMAP_THREE( mprotect, __mprotect, int , MMAP_T, size_t , int )
REMAP_THREE( msync, __msync, int , void *, size_t , int )
REMAP_THREE( msync, __libc_msync, int , void *, size_t , int )
REMAP_TWO( munmap, __munmap, int, void *, size_t )
REMAP_ONE( pipe, __pipe, int , int *)

#if defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
REMAP_THREE( readlink, __readlink, ssize_t , const char *, char *, size_t )
#else
REMAP_THREE( readlink, __readlink, int , const char *, char *, size_t )
#endif

REMAP_THREE( readv, __readv, ssize_t, int, const struct iovec *, int )
REMAP_THREE( readv, __libc_readv, ssize_t, int, const struct iovec *, int )

REMAP_THREE( reboot, __reboot, int , int , int , int )
REMAP_TWO( rename, __rename, int, const char*, const char* )
REMAP_ONE( rmdir, __rmdir, int , const char *)
REMAP_FIVE( select, __select, int , int , fd_set *, fd_set *, fd_set *, struct timeval *)
REMAP_TWO( setdomainname, __setdomainname, int , const char *, size_t )
REMAP_ONE( setgid, __setgid, int , gid_t )
REMAP_TWO( setgroups, __setgroups, int , int , gid_t *)
REMAP_TWO( sethostname, __sethostname, int , const char *, size_t )
REMAP_THREE( setitimer, __setitimer, int , int , const struct itimerval *, struct itimerval *)
REMAP_TWO( setpgid, __setpgid, int , pid_t , pid_t )
     /* REMAP_THREE( setpriority, __setpriority, int, int, int, int ) */
REMAP_TWO( setregid, __setregid, int , uid_t , uid_t )
REMAP_TWO( setreuid, __setreuid, int , uid_t , uid_t )
REMAP_TWO( setrlimit, __setrlimit, int , int , const struct rlimit *)
REMAP_ZERO( setsid, __setsid, pid_t )
REMAP_TWO( settimeofday, __settimeofday, int , const struct timeval *, const struct timezone *)
REMAP_ONE( setuid, __setuid, int , uid_t )
REMAP_TWO( statfs, __statfs, int , const char *, struct statfs *)
REMAP_ONE( swapoff, __swapoff, int , const char *)
REMAP_TWO( swapon, __swapon, int , const char *, int )
REMAP_TWO( symlink, __symlink, int , const char *, const char *)
REMAP_ONE( umask, __umask, mode_t , mode_t )

#if defined(I386)
REMAP_ONE( umount, __umount, int , const char *)
#endif

/* uname is now a weak alias to __uname */
#if !defined(GLIBC22) && !defined(GLIBC23) && !defined(GLIBC24) && !defined(GLIBC25) && !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
REMAP_ONE( uname, __uname, int , struct utsname *)
#endif

REMAP_ONE( unlink, __unlink, int , const char *)
REMAP_TWO( utime, __utime, int, const char *, const struct utimbuf * )
REMAP_FOUR( wait4, __wait4, pid_t , pid_t , void *, int , struct rusage *)

#if defined(I386)
REMAP_THREE( waitpid, __waitpid, pid_t , pid_t , int *, int )
REMAP_THREE( waitpid, __libc_waitpid, pid_t , pid_t , int *, int )
#endif

REMAP_THREE( writev, __writev, ssize_t, int, const struct iovec *, int )
REMAP_THREE( writev, __libc_writev, ssize_t, int, const struct iovec *, int )

#if defined(I386)
REMAP_FOUR( profil, __profil, int , char *, int , int , int );
#endif


/* Differences */

#if defined(GLIBC)

#if !defined(GLIBC22) && !defined(GLIBC23) && !defined(GLIBC24) && !defined(GLIBC25) && !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
/* clone has a much different prototype not supported by the stub generator
	under glibc22 and later machines */
REMAP_TWO( clone, __clone, pid_t , void *, unsigned long )
#endif


#if !defined(GLIBC27) && !defined(GLIBC211) && !defined(GLIBC212) && !defined(GLIBC213)
REMAP_TWO( fstat, __fstat, int , int , struct stat *)
#endif

REMAP_TWO( getrusage, __getrusage, int, int, struct rusage *)
REMAP_THREE( mknod, __mknod, int , const char *, mode_t , dev_t )
REMAP_TWO( truncate, __truncate, int , const char *, off_t )

#else 

REMAP_ONE( fdatasync, __fdatasync, int , int )
REMAP_TWO( getrusage, __getrusage, int, int , struct rusage *)
REMAP_ZERO_VOID( idle, __idle, void )
REMAP_THREE( ioperm, __ioperm, int , unsigned long , unsigned long , int )
REMAP_ONE( iopl, __iopl, int , int )
REMAP_TWO( prev_fstat, __prev_fstat, int , int , struct stat *)
REMAP_TWO( prev_lstat, __prev_lstat, int , const char *, struct stat *)
REMAP_THREE( prev_mknod, __prev_mknod, int , const char *, mode_t , dev_t )
REMAP_TWO( prev_stat, __prev_stat, int , const char *, struct stat *)
REMAP_THREE( setpriority, __setpriority, int, int, int, int )
REMAP_ZERO( sync, __sync, int )
REMAP_TWO( truncate, __truncate, int , const char *, size_t )

#endif

#include "condor_socket_types.h"

REMAP_THREE( accept, __accept, int, int, SOCKET_ADDR_CONST_ACCEPT SOCKET_ADDR_TYPE, SOCKET_ALTERNATE_LENGTH_TYPE *)

REMAP_THREE( bind, __bind, int, int, SOCKET_ADDR_CONST_BIND SOCKET_ADDR_TYPE, SOCKET_LENGTH_TYPE )
REMAP_THREE( connect, __connect, int, int, SOCKET_ADDR_CONST_CONNECT SOCKET_ADDR_TYPE, SOCKET_LENGTH_TYPE)
REMAP_THREE( getpeername, __getpeername, int, int, SOCKET_ADDR_TYPE, SOCKET_ALTERNATE_LENGTH_TYPE *)
REMAP_THREE( getsockname, __getsockname, int, int, SOCKET_ADDR_TYPE, SOCKET_ALTERNATE_LENGTH_TYPE *)
REMAP_FIVE( getsockopt, __getsockopt, int, int, int, int, SOCKET_DATA_TYPE, SOCKET_ALTERNATE_LENGTH_TYPE * )
REMAP_TWO( listen, __listen, int, int, SOCKET_COUNT_TYPE )

#if defined(I386)
REMAP_FOUR( send, __send, SOCKET_SENDRECV_TYPE, int, SOCKET_DATA_CONST SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE )
REMAP_FOUR( recv, __recv, SOCKET_SENDRECV_TYPE, int, SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE )
#endif

REMAP_SIX( recvfrom, __recvfrom, SOCKET_RECVFROM_TYPE, int, SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE, SOCKET_ADDR_TYPE, SOCKET_ALTERNATE_LENGTH_TYPE * )

REMAP_THREE( recvmsg, __recvmsg, SOCKET_SENDRECV_TYPE, int, struct msghdr *, SOCKET_FLAGS_TYPE )
REMAP_THREE( recvmsg, __libc_recvmsg, SOCKET_SENDRECV_TYPE, int, struct msghdr *, SOCKET_FLAGS_TYPE )

REMAP_THREE( sendmsg, __sendmsg, SOCKET_SENDRECV_TYPE, int, SOCKET_MSG_CONST struct msghdr *, SOCKET_FLAGS_TYPE )
REMAP_THREE( sendmsg, __libc_sendmsg, SOCKET_SENDRECV_TYPE, int, SOCKET_MSG_CONST struct msghdr *, SOCKET_FLAGS_TYPE )

REMAP_SIX( sendto, __sendto, SOCKET_SENDRECV_TYPE, int, SOCKET_DATA_CONST SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE, const SOCKET_ADDR_TYPE, SOCKET_LENGTH_TYPE )
REMAP_SIX( sendto, __libc_sendto, SOCKET_SENDRECV_TYPE, int, SOCKET_DATA_CONST SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE, const SOCKET_ADDR_TYPE, SOCKET_LENGTH_TYPE )

REMAP_FIVE( setsockopt, __setsockopt, int, int, int, int, SOCKET_DATA_CONST SOCKET_DATA_TYPE, SOCKET_LENGTH_TYPE )

REMAP_TWO( shutdown, __shutdown, int, int, int )

REMAP_THREE( socket, __socket, int, int, int, int )

REMAP_FOUR( socketpair, __socketpair, int, int, int, int, int * )

#if defined(GLIBC23)

/* This next set of remap function calls don't so much as remap things into
	condor syscall lib entrance points, but provide a more current interface
	against newer glibc header files and libraries that use glibc's and gcc's
	FORTIFY feature (and subsequent API). I have to the best of my ability,
	and without knowing the FORTIFY API (which I couldn't reliably find online),
	copied the interface properly. The only confusion that really arises is
	when there are two or more length values and it was hard to determine 
	which was the correct one. */

/* If I were using glibc 2.4 here, I'd have to undefine everything I'm
	calling. */

int __printf_chk(int /* flag */ , const char *format, ...)
{
	va_list ap;
	int done;

	va_start(ap, format);
	done = vfprintf(stdout, format, ap);
	va_end(ap);

	return done;
}

int __fprintf_chk(FILE *fp, int /* flag */, const char *format, ...)
{
	va_list ap;
	int done;

	va_start(ap, format);
	done = vfprintf(fp, format, ap);
	va_end(ap);

	return done;
}

int __sprintf_chk(char *s, int /* flags */, size_t /* slen */, const char *format, ...)
{
	va_list arg;
	int done;
 
	va_start(arg, format);
	done = vsprintf(s, format, arg);
	va_end(arg);
 
	return done;
}

char * __strcpy_chk(char *dest, const char *src, size_t destlen)
{
	return strncpy(dest, src, destlen);
}

char * __fgets_chk(char *buf, size_t /* size */, int n, FILE *fp)
{
	/* it looks like n is the right parameter */
	return fgets(buf, n, fp); 
}

char * __fgets_unlocked_chk(char *buf, size_t /* size */, int n, FILE *fp)
{
	/* it looks like n is the right parameter */
	return fgets(buf, n, fp);
}


wchar_t * __fgetws_chk(wchar_t *buf, size_t /* size */, int n, _IO_FILE *fp) 
{
	/* it looks like n is the right parameter */
	return fgetws(buf, n, fp);
}

wchar_t * __fgetws_unlocked_chk(wchar_t *buf, size_t /* size */, int n, _IO_FILE *fp) 
{
	/* it looks like n is the right parameter */
	return fgetws_unlocked(buf, n, fp);
}

int __fwprintf_chk(FILE *fp, int /* flag */, const wchar_t *format, ...)
{
	va_list ap;
	int done;

	va_start(ap, format);
	done = vfwprintf(fp, format, ap);
	va_end(ap);

	return done;
}

char * __getcwd_chk(char *buf, size_t size, size_t /* buflen */)
{
	return getcwd(buf, size);
}

int __getdomainname_chk(char *buf, size_t buflen, size_t /* nreal */)
{
	return getdomainname(buf, buflen);
}

int __getgroups_chk(int size, __gid_t list[], size_t /* listlen */)
{
	return getgroups(size, list);
}

int __gethostname_chk(char *buf, size_t buflen, size_t /* nreal */)
{
	return gethostname(buf, buflen);
}

int __getlogin_r_chk(char *buf, size_t buflen, size_t /* nreal */)
{
	return getlogin_r(buf, buflen);
}

char * __gets_chk(char *buf, size_t /* size */)
{
	return gets(buf);
}

char * __getwd_chk(char *buf, size_t buflen)
{
	return getcwd(buf, buflen);
}

size_t __mbsnrtowcs_chk(wchar_t *dst, const char **src, size_t nmc, 
	size_t len, mbstate_t *ps, size_t /* dstlen */)
{
	/* I think len is the right one */
	return mbsnrtowcs(dst, src, nmc, len, ps);
}

size_t __mbsrtowcs_chk(wchar_t *dst, const char **src, size_t len,
         mbstate_t *ps, size_t /* dstlen */)
{
	/* I think len is the right one */
	return mbsrtowcs(dst, src, len, ps);
}

size_t __mbstowcs_chk(wchar_t *dst, const char *src, size_t len, size_t /* dstlen */)
{
	mbstate_t state;

	/* I think len is the right one */
	memset(&state, '\0', sizeof state);

	return mbsrtowcs(dst, &src, len, &state);
}

void * __memcpy_chk(void *dstpp, const void *srcpp, size_t len, size_t /* dstlen */ )
{
	/* I think len is the right one */
	return memcpy(dstpp, srcpp, len);
}

void * __memmove_chk(void *dest, const void *src, size_t len, size_t /* dstlen */)
{
	/* I think len is the right one */
	return memmove(dest, src, len);
}

void * __mempcpy_chk(void *dstpp, const void *srcpp, size_t len, size_t /* dstlen */)
{
	/* I think len is the right one */
	return memcpy(dstpp, srcpp, len);
}

void * __memset_chk(void *dstpp, int c, size_t len, size_t /* dstlen */)
{
	/* I think len is the right one */
	return memset(dstpp, c, len);
}


ssize_t __pread64_chk(int fd, void *buf, size_t nbytes, off64_t offset, 
	size_t /* buflen */)
{
	return pread64(fd, buf, nbytes, offset);
}

ssize_t __pread_chk(int fd, void *buf, size_t nbytes, off_t offset, 
	size_t /* buflen */)
{
	return pread(fd, buf, nbytes, offset);
}

int __ptsname_r_chk(int fd, char *buf, size_t buflen, size_t /* nreal */)
{
	return ptsname_r(fd, buf, buflen);
}

ssize_t __read_chk(int fd, void *buf, size_t nbytes, size_t /* buflen */)
{
	return read (fd, buf, nbytes);
}

ssize_t __readlink_chk(const char *path, char *buf, size_t len, 
	size_t /* buflen */)
{
	return readlink(path, buf, len);
}

char * __realpath_chk(const char *buf, char *resolved, size_t /* resolvedlen */)
{
	return realpath(buf, resolved);
}

ssize_t __recv_chk(int fd, void *buf, size_t n, size_t /* buflen */, int flags)
{
	return recv(fd, buf, n, flags);
}

SOCKET_RECVFROM_TYPE __recvfrom_chk(int fd, SOCKET_DATA_TYPE buf, 
	SOCKET_SENDRECV_LENGTH_TYPE n,  size_t /* buflen */, 
	SOCKET_FLAGS_TYPE flags, SOCKET_ADDR_TYPE addr, 
	SOCKET_ALTERNATE_LENGTH_TYPE *addr_len)
{
	return recvfrom(fd, buf, n, flags, addr, addr_len);
}

int ___snprintf_chk (char *s, size_t maxlen, int /* flags */, size_t /* slen */,
         const char *format, ...)
{
	va_list arg;
	int done;

	va_start(arg, format);
	/* XXX I think I picked the right length variable... */
	done = vsnprintf(s, maxlen, format, arg);
	va_end(arg);

	return done;
}

int __snprintf_chk (char *s, size_t maxlen, int /* flags */, size_t /* slen */,
         const char *format, ...)
{
	va_list arg;
	int done;

	va_start(arg, format);
	/* XXX I think I picked the right length variable... */
	done = vsnprintf(s, maxlen, format, arg);
	va_end(arg);

	return done;
}

char * __stpcpy_chk(char *dest, const char *src, size_t /* destlen */)
{
	return stpcpy(dest, src);
}

char * __stpncpy_chk(char *dest, const char *src, size_t n, size_t /* dstlen */)
{
	return stpncpy(dest, src, n);
}

char * __strcat_chk(char *dest, const char *src, size_t /* dstlen */)
{
	return strcat(dest, src);
}

char * __strncat_chk(char *s1, const char *s2, size_t n, size_t /* s1len */)
{
	return strncat(s1, s2, n);
}

char * __strncpy_chk(char *s1, const char *s2, size_t n, size_t /* s1len */)
{
	return strncpy(s1, s2, n);
}

int __swprintf_chk(wchar_t *s, size_t n, int /* flag */, size_t /* s_len */,
        const wchar_t *format, ...)
{
	va_list arg;
	int done;

	va_start(arg, format);
	done = vswprintf(s, n, format, arg);
	va_end(arg);

	return done;
}

int __ttyname_r_chk(int fd, char *buf, size_t buflen, size_t /* nreal */)
{
	return ttyname_r(fd, buf, buflen);
}

int ___vfprintf_chk(FILE *fp, int /* flag */, const char *format, va_list ap)
{
	return vfprintf(fp, format, ap);
}

/* remap for the above */
int __vfprintf_chk(FILE *fp, int flag, const char *format, va_list ap)
{
	return ___vfprintf_chk(fp, flag, format, ap);
}

int __vfwprintf_chk(FILE *fp, int /* flag */, const wchar_t *format, va_list ap)
{
	return vfwprintf(fp, format, ap);
}

int ___vprintf_chk(int /* flag */, const char *format, va_list ap)
{
	return vfprintf(stdout, format, ap);
}

/* remap for above */
int __vprintf_chk(int flag, const char *format, va_list ap)
{
	return ___vprintf_chk(flag, format, ap);
}

int ___vsnprintf_chk(char *s, size_t maxlen, int /* flags */, size_t /* slen */,
          const char *format, va_list args)
{
	return vsnprintf(s, maxlen, format, args);
}

/* remap for above */
int __vsnprintf_chk(char *s, size_t maxlen, int flags, size_t slen,
          const char *format, va_list args)
{
	return ___vsnprintf_chk(s, maxlen, flags, slen, format, args);
}

int ___vsprintf_chk(char *s, int /* flags */, size_t /* slen */, const char *format,
         va_list args)
{
	return vsprintf(s, format, args);
}

/* remap for above */
int __vsprintf_chk(char *s, int flags, size_t slen, const char *format,
         va_list args)
{
	return ___vsprintf_chk(s, flags, slen, format, args);
}

int __vswprintf_chk(wchar_t *s, size_t maxlen, int /* flags */, size_t /* slen */,
         const wchar_t *format, va_list args)
{
	return vswprintf(s, maxlen, format, args);
}

int __vwprintf_chk(int /* flag */, const wchar_t *format, va_list ap)
{
	return vwprintf(format, ap);
}

wchar_t * __wcpcpy_chk(wchar_t *dest, const wchar_t *src, size_t destlen)
{
	return wcpncpy(dest, src, destlen);
}

wchar_t * __wcpncpy_chk(wchar_t *dest, const wchar_t *src, size_t n, 
	size_t /* destlen */)
{
	return wcpncpy(dest, src, n);
}

size_t __wcrtomb_chk(char *s, wchar_t wchar, mbstate_t *ps, size_t /* buflen */)
{
	return wcrtomb(s, wchar, ps);
}

wchar_t * __wcscat_chk(wchar_t *dest, const wchar_t *src, size_t destlen)
{
	return wcsncat(dest, src, destlen);
}

wchar_t * __wcscpy_chk(wchar_t *dest, const wchar_t *src, size_t /* n */)
{
	return wcscpy(dest, src);
}

wchar_t * __wcsncat_chk(wchar_t *dest, const wchar_t *src, size_t n, 
	size_t /* destlen */)
{
	/* XXX I think n is right in this case */
	return wcsncat(dest, src, n);
}

wchar_t * __wcsncpy_chk(wchar_t *dest, const wchar_t *src, size_t n, 
	size_t /* destlen */)
{
	return wcsncpy(dest, src, n);
}

size_t __wcsnrtombs_chk(char *dst, __const wchar_t **src, size_t nwc, 
	size_t len, mbstate_t *ps, size_t /* dstlen */)
{
	return wcsnrtombs(dst, src, nwc, len, ps);
}

size_t __wcsrtombs_chk(char *dst, __const wchar_t **src, size_t len,
         mbstate_t *ps, size_t /* dstlen */)
{
	return wcsrtombs(dst, src, len, ps);
}

size_t __wcstombs_chk(char *dst, __const wchar_t *src, size_t len, 
	size_t /* dstlen */)
{
	mbstate_t state;

	memset (&state, '\0', sizeof state);

	return wcsrtombs(dst, &src, len, &state);
}

int __wctomb_chk(char *s, wchar_t wchar, size_t /* buflen */)
{
	/* Defined in glibc's mbtowc.c.  */
	extern mbstate_t __no_r_state;

	return wcrtomb(s, wchar, &__no_r_state);
}

wchar_t * __wmemcpy_chk(wchar_t *s1, const wchar_t *s2, size_t n, size_t /* ns1 */)
{
	return (wchar_t *) memcpy ((char *) s1, (char *) s2, n * sizeof (wchar_t));
}

wchar_t * __wmemmove_chk(wchar_t *s1, const wchar_t *s2, size_t n, size_t /* ns1 */)
{
	return (wchar_t *) memmove ((char *) s1, (char *) s2, n * sizeof (wchar_t));
}

wchar_t * __wmempcpy_chk(wchar_t *s1, const wchar_t *s2, size_t n, size_t /* ns1 */)
{
	return (wchar_t *) __mempcpy ((char *) s1, (char *) s2, 
			n * sizeof (wchar_t));
}

wchar_t * __wmemset_chk(wchar_t *s, wchar_t c, size_t n, size_t /* dstlen */)
{
	return wmemset(s, c, n);
}

int __wprintf_chk(int /* flag */, const wchar_t *format, ...)
{
	va_list ap;
	int done;

	va_start(ap, format);
	done = vfwprintf(stdout, format, ap);
	va_end(ap);

	return done;
}


#endif /* GLIBC23 */

#if defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
/* This forces malloc.o from libc.a to not be brought in, and allows stduniv
	code to use the malloc-user.c API instead */
REMAP_TWO( memalign, __libc_memalign, void*, size_t, size_t )
#endif

#endif /* REMOTE_SYSCALLS */

}











