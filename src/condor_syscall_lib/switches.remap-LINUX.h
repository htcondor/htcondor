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
REMAP_TWO( ftruncate, __ftruncate, int , int , off_t )
REMAP_THREE( poll, __libc_poll, int , struct pollfd *, unsigned long , int )
REMAP_THREE( poll, __poll, int , struct pollfd *, unsigned long , int )

#if defined(GLIBC23)
	REMAP_TWO( ftruncate64, __ftruncate64, int , int , off64_t )
#endif

REMAP_THREE_VARARGS( ioctl, __ioctl, int , int , unsigned long , int)

REMAP_THREE( lseek, __lseek, off_t, int, off_t, int )

#if defined(GLIBC21) || defined(GLIBC22) || defined(GLIBC23)
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

#if defined(GLIBC23)
REMAP_THREE_VARARGS( open64, __open64, int, const char *, int, int )
REMAP_THREE_VARARGS( open64, __libc_open64, int, const char *, int, int )
#endif

REMAP_THREE( read, __read, ssize_t, int, __ptr_t, size_t )

REMAP_THREE( write, __write, ssize_t, int, const __ptr_t, size_t )
#if defined(GLIBC22) || defined(GLIBC23)
REMAP_THREE( write, __libc_write, ssize_t, int, const __ptr_t, size_t )
#endif

/* make a bunch of __libc remaps for the things that have fd's or paths.
	These were new entry points in glibc22 */
#if defined(GLIBC22) || defined(GLIBC23)
REMAP_ONE( close, __libc_close, int , int )
REMAP_TWO( creat, __libc_creat, int, const char*, mode_t )
REMAP_ONE( dup, __libc_dup, int, int )
REMAP_TWO( dup2, __libc_dup2, int, int, int )
REMAP_ONE( fchdir, __libc_fchdir, int, int )
REMAP_THREE_VARARGS( fcntl, __libc_fcntl, int , int , int , int)
REMAP_ONE( fsync, __libc_fsync, int , int )
REMAP_TWO( ftruncate, __libc_ftruncate, int , int , off_t )
REMAP_THREE_VARARGS( ioctl, __libc_ioctl, int , int , unsigned long , int)
REMAP_THREE( llseek, __libc_llseek, off64_t, int, off64_t, int )
REMAP_THREE( llseek, __libc_lseek64, off64_t, int, off64_t, int )
REMAP_THREE_VARARGS( open, __libc_open, int, const char *, int, int )
REMAP_THREE( read, __libc_read, ssize_t, int, __ptr_t, size_t )
#endif

#if defined(GLIBC23)
REMAP_THREE( lseek, __libc_lseek, off_t, int, off_t, int )
REMAP_FIVE( select, __libc_select, int , int , fd_set *, fd_set *, fd_set *, struct timeval *)
#endif

#endif /* if defined(FILE_TABLE) */

/* These remaps are neede by signals_support.c */
#if defined(SAVE_SIGSTATE)

REMAP_THREE( sigprocmask, __sigprocmask, int, int, const sigset_t *, sigset_t * )
REMAP_ONE( sigsetmask, __sigsetmask, int, int )
REMAP_ONE( sigsuspend, __sigsuspend, int, const sigset_t * )
REMAP_ONE( sigsuspend, __libc_sigsuspend, int, const sigset_t * )
REMAP_THREE( sigaction, __sigaction, int, int, const struct sigaction *, struct sigaction * )
REMAP_THREE( sigaction, __libc_sigaction, int, int, const struct sigaction *, struct sigaction * )


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
#if !defined(GLIBC22) && !defined(GLIBC23)
REMAP_ZERO( getuid, __getuid, uid_t )
#endif

REMAP_TWO( kill, __kill, int, pid_t, int )
REMAP_TWO( link, __link, int , const char *, const char *)
REMAP_TWO( mkdir, __mkdir, int , const char *, mode_t )
REMAP_SIX( mmap, __mmap, void *, void *, size_t, int, int, int, off_t )
REMAP_FIVE( mount, __mount, int , const char *, const char *, const char *, unsigned long , const void *)
REMAP_THREE( mprotect, __mprotect, int , MMAP_T, size_t , int )
REMAP_THREE( msync, __msync, int , void *, size_t , int )
REMAP_THREE( msync, __libc_msync, int , void *, size_t , int )
REMAP_TWO( munmap, __munmap, int, void *, size_t )
REMAP_ONE( pipe, __pipe, int , int *)
REMAP_THREE( readlink, __readlink, int , const char *, char *, size_t )
REMAP_THREE( readv, __readv, int, int, const struct iovec *, size_t )

#if defined(GLIBC23)
REMAP_THREE( readv, __libc_readv, int, int, const struct iovec *, size_t )
#endif

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
REMAP_ONE( umount, __umount, int , const char *)

/* uname is now a weak alias to __uname */
#if !defined(GLIBC22) && !defined(GLIBC23)
REMAP_ONE( uname, __uname, int , struct utsname *)
#endif

REMAP_ONE( unlink, __unlink, int , const char *)
REMAP_TWO( utime, __utime, int, const char *, const struct utimbuf * )
REMAP_FOUR( wait4, __wait4, pid_t , pid_t , void *, int , struct rusage *)
REMAP_THREE( waitpid, __waitpid, pid_t , pid_t , int *, int )
REMAP_THREE( waitpid, __libc_waitpid, pid_t , pid_t , int *, int )

REMAP_THREE( writev, __writev, int, int, const struct iovec *, size_t )

#if defined(GLIBC23)
REMAP_THREE( writev, __libc_writev, int, int, const struct iovec *, size_t )
#endif

REMAP_FOUR( profil, __profil, int , char *, int , int , int );


/* Differences */

#if defined(GLIBC)

#if !defined(GLIBC22) && !defined(GLIBC23)
/* clone has a much different prototype not supported by the stub generator
	under glibc22 and later machines */
REMAP_TWO( clone, __clone, pid_t , void *, unsigned long )
#endif

REMAP_TWO( fstat, __fstat, int , int , struct stat *)
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
REMAP_FOUR( send, __send, int, int, SOCKET_DATA_CONST SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE )
REMAP_FOUR( recv, __recv, int, int, SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE )
REMAP_SIX( recvfrom, __recvfrom, int, int, SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE, SOCKET_ADDR_TYPE, SOCKET_ALTERNATE_LENGTH_TYPE * )
REMAP_THREE( recvmsg, __recvmsg, int, int, struct msghdr *, SOCKET_FLAGS_TYPE )
REMAP_THREE( sendmsg, __sendmsg, int, int, SOCKET_MSG_CONST struct msghdr *, SOCKET_FLAGS_TYPE )
REMAP_SIX( sendto, __sendto, int, int, SOCKET_DATA_CONST SOCKET_DATA_TYPE, SOCKET_SENDRECV_LENGTH_TYPE, SOCKET_FLAGS_TYPE, const SOCKET_ADDR_TYPE, SOCKET_LENGTH_TYPE )
REMAP_FIVE( setsockopt, __setsockopt, int, int, int, int, SOCKET_DATA_CONST SOCKET_DATA_TYPE, SOCKET_LENGTH_TYPE )
REMAP_TWO( shutdown, __shutdown, int, int, int )
REMAP_THREE( socket, __socket, int, int, int, int )
REMAP_FOUR( socketpair, __socketpair, int, int, int, int, int * )


#endif /* REMOTE_SYSCALLS */

}
