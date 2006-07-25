/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

/* These remaps are needed by file_state.c */
#if defined(FILE_TABLE)
REMAP_ONE( close, _close, int , int )
REMAP_ONE( close, __close, int , int )
REMAP_TWO( creat, _creat, int, const char *, mode_t )
REMAP_ONE( dup, _dup, int, int )
REMAP_ONE( fchdir, _fchdir, int, int )
REMAP_THREE_VARARGS( fcntl, _fcntl, int , int , int , int )
REMAP_ONE( fdatasync, _fdatasync, int , int )
REMAP_ONE( fsync, _fsync, int , int )
REMAP_TWO( ftruncate, _ftruncate, int , int , off_t )
REMAP_TWO( ftruncate64, _ftruncate64, int , int , off64_t )
REMAP_THREE_VARARGS( ioctl, _ioctl, int, int, int, int )
REMAP_THREE( lseek, _lseek, off_t , int , off_t , int )
REMAP_THREE( lseek64, _lseek64, off64_t , int , off64_t , int )
REMAP_THREE_VARARGS( open, _open, int , const char *, int , int )
REMAP_THREE( read, _read, ssize_t , int , void *, size_t )
REMAP_THREE( write, _write, ssize_t , int , const void *, size_t )
#endif

/* These remaps are neede by signals_support.c */
#if defined(SAVE_SIGSTATE)
REMAP_ONE( sigsuspend, _sigsuspend, int, const sigset_t * )
#endif

/* These remaps fill out the rest of the remote syscalls */
#if defined(REMOTE_SYSCALLS)

REMAP_THREE( accept, _accept, int , int , void *, int * );
REMAP_TWO( access, _access, int , const char *, int )
REMAP_ONE( acct, _acct, int , const char *)
REMAP_TWO( adjtime, _adjtime, int , struct timeval *, struct timeval *)
REMAP_ZERO( async_daemon, _async_daemon, int )
REMAP_THREE( bind, _bind, int , int , const void *, int )
REMAP_ONE( chdir, _chdir, int, const char * )
REMAP_TWO( chmod, _chmod, int , const char *, mode_t )
REMAP_THREE( chown, _chown, int , const char *, uid_t , gid_t )
REMAP_ONE( chroot, _chroot, int , const char *)
REMAP_THREE( connect, _connect, int , int , const void *, int )
REMAP_TWO( execv, _execv, int , const char *, char * const *)
REMAP_THREE( execve, _execve, int , const char *, char * const * , char * const * )

REMAP_THREE( exportfs, _exportfs, int , int , int *, struct exportfsdata *)
REMAP_TWO( fchmod, _fchmod, int , int , mode_t )
REMAP_THREE( fchown, _fchown, int , int , uid_t , gid_t )
REMAP_ZERO( fork, _fork, pid_t )
REMAP_FOUR( fstatfs, _fstatfs, int , int , struct statfs *, int , int )

REMAP_TWO( fstat64, _fstat64, int, int, struct stat64 *)
REMAP_TWO( lstat64, _lstat64, int, const char *, struct stat64 *)
REMAP_TWO( stat64, _stat64, int, const char *, struct stat64 *)

REMAP_THREE( _fxstat, fxstat, int , const int , int , struct stat *)
REMAP_THREE( getdents, _getdents, int, int, struct dirent *, unsigned int )
REMAP_THREE( getdents64, _getdents64, int , int , struct dirent64 *, size_t )
REMAP_TWO( getdomainname, _getdomainname, int , char *, int )
REMAP_ZERO( getgid, _getgid, gid_t )
REMAP_ZERO( gethostid, _gethostid, long int )
REMAP_TWO( gethostname, _gethostname, int , char *, size_t )
REMAP_TWO( getitimer, _getitimer, int , int , struct itimerval *)
REMAP_ZERO( getpagesize, _getpagesize, int )
REMAP_THREE( getpeername, _getpeername, int , int , void * , int *)
REMAP_ZERO( getpid, _getpid, pid_t )
REMAP_TWO( getrlimit, _getrlimit, int , int , struct rlimit *)
REMAP_TWO( getrlimit64, _getrlimit64, int , int , struct rlimit64 *)
REMAP_THREE( getsockname, _getsockname, int , int , void *, int *)
REMAP_FIVE( getsockopt, _getsockopt, int , int , int , int , void *, int *)
REMAP_ZERO( getuid, _getuid, uid_t )
REMAP_TWO( kill, _kill, int, pid_t, int )
REMAP_THREE( lchown, _lchown, int , const char *, uid_t , gid_t )
REMAP_TWO( link, _link, int , const char *, const char *)
REMAP_TWO( listen, _listen, int , int , int )
REMAP_THREE( _lxstat, lxstat, int , const int , const char *, struct stat *)
REMAP_THREE( madvise, _madvise, int , void *, size_t , int )
REMAP_TWO( mkdir, _mkdir, int , const char *, long unsigned int )
REMAP_SIX( mmap, _mmap, void *, void *, size_t, int, int, int, off_t )
REMAP_SIX( mmap64, _mmap64, MMAP_T , MMAP_T , size_t , int , int , int , off64_t )
REMAP_SIX( mount, _mount, int , char *, char *, int , int , char *, int )
REMAP_THREE( mprotect, _mprotect, int , const void *, unsigned int , int )
REMAP_THREE( msync, _msync, int , void *, size_t , int )
REMAP_TWO( munmap, _munmap, int , void *, size_t )
REMAP_THREE( nfssvc, _nfssvc, int , int , int , int )
/* REMAP_ONE( pipe, _pipe, int , int []) */
REMAP_ONE( pipe, _pipe, int , int * )
REMAP_ONE( plock, _plock, int , int )
REMAP_THREE( poll, _poll, int, struct pollfd *, unsigned long, int )
REMAP_FOUR( profil, _profil, int , short unsigned int *, unsigned int , unsigned int , unsigned int )
REMAP_FOUR( ptrace, _ptrace, int , int , pid_t , int , int )
REMAP_FOUR( quotactl, _quotactl, int , char *, int , int , char *)
REMAP_THREE( readlink, _readlink, int , const char *, char *, unsigned int )
REMAP_THREE( readv, _readv, ssize_t, int, const struct iovec *, int )
REMAP_FOUR( recv, _recv, int , int , void *, int , int )
REMAP_SIX( recvfrom, _recvfrom, int , int , void *, int , int , void *, int *)
REMAP_THREE( recvmsg, _recvmsg, int , int , struct msghdr *, int )
REMAP_TWO( rename, _rename, int, const char *, const char * )
REMAP_ONE( rmdir, _rmdir, int , const char *)
REMAP_FIVE( select, _select, int , int , fd_set *, fd_set *, fd_set *, struct timeval *)
REMAP_FOUR( send, _send, int, int, const void *, int, int )
REMAP_THREE( sendmsg, _sendmsg, int , int , const struct msghdr *, int )
REMAP_SIX( sendto, _sendto, int , int , const void *, int , int , const void *, int )
REMAP_TWO( setdomainname, _setdomainname, int , const char *, int )
REMAP_ONE( setgid, _setgid, int , gid_t )
REMAP_ONE( sethostid, _sethostid, int , int )
REMAP_TWO( sethostname, _sethostname, int , const char *, int )
REMAP_THREE( setitimer, _setitimer, int , int , const struct itimerval *, struct itimerval *)
REMAP_ZERO( setpgrp, _setpgrp, pid_t )
REMAP_TWO( setregid, _setregid, int , long int, long int )
REMAP_TWO( setreuid, _setreuid, int , long int , long int )
REMAP_TWO( setrlimit, _setrlimit, int , int , const struct rlimit *)
REMAP_TWO( setrlimit64, _setrlimit64, int , int , const struct rlimit64 *)
REMAP_FIVE( setsockopt, _setsockopt, int , int , int , int , const void *, int )
REMAP_ONE( setuid, _setuid, int , uid_t )
REMAP_TWO( shutdown, _shutdown, int , int , int )
REMAP_THREE( socket, _socket, int, int, int, int )
/* REMAP_FOUR( socketpair, _socketpair, int , int , int , int , int []) */
REMAP_FOUR( socketpair, _socketpair, int , int , int , int , int * )
REMAP_TWO( stat, _stat, int , const char *, struct stat *)
REMAP_FOUR( statfs, _statfs, int , const char *, struct statfs *, int , int )
REMAP_TWO( symlink, _symlink, int , const char *, const char *)
REMAP_ZERO_VOID( sync, _sync, void )
REMAP_TWO( truncate, _truncate, int , const char *, off_t )
REMAP_TWO( truncate64, _truncate64, int , const char *, off64_t )
REMAP_ONE( umask, _umask, long unsigned int , long unsigned int )
REMAP_ONE( unlink, _unlink, int , const char *)
REMAP_TWO( utime, _utime, int , const char *, const struct utimbuf *)
REMAP_THREE( writev, _writev, ssize_t, int, const struct iovec *, int )
REMAP_FOUR( xmknod, _xmknod, int , const int , const char *, mode_t , dev_t )
REMAP_THREE( _xstat, xstat, int , const int , const char *, struct stat *)

#endif /* REMOTE_SYSCALLS */ 
