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

/* These remaps are needed by file_state.c */
#if defined(FILE_TABLE)
REMAP_ONE( close, _close, int , int )
REMAP_TWO( creat, _creat, int , const char *, mode_t )
REMAP_ONE( dup, _dup, int, int )
REMAP_TWO( dup2, _dup2, int, int, int )
REMAP_ONE( fchdir, _fchdir, int , int  )
REMAP_THREE( fcntl, _fcntl, int , int , int , int )
REMAP_ONE( fsync, _fsync, int , int )
REMAP_TWO( ftruncate, _ftruncate, int , int , off_t )
REMAP_TWO( ftruncate64, __ftruncate64, int , int , off64_t )
REMAP_THREE( ioctl, _ioctl, int , int , int , int )
REMAP_THREE( lseek, _lseek, off_t , int , off_t , int )
REMAP_THREE( open, _open, int , const char *, int , int )
REMAP_THREE( read, _read, ssize_t , int , void *, size_t )
REMAP_THREE( write, _write, ssize_t , int , const void *, size_t )
#endif

/* These remaps are neede by signals_support.c */
#if defined(SAVE_SIGSTATE)
REMAP_THREE( sigaction, _sigaction, int , int , const struct sigaction * , struct sigaction * )
REMAP_ONE( sigblock, __sigblock, long , long )
REMAP_ONE( sigpause, __sigpause, long , long )
REMAP_THREE( sigprocmask, _sigprocmask, int , int , const sigset_t * , sigset_t * )
REMAP_ONE( sigsetmask, __sigsetmask, long , long )
REMAP_ONE( sigsuspend, _sigsuspend, int , const sigset_t * )
#endif

/* These remaps fill out the rest of the remote syscalls */
#if defined(REMOTE_SYSCALLS)
REMAP_ONE( acct, _acct, int , char *)
REMAP_THREE( accept, _accept, int , int , struct sockaddr *, unsigned int *)
REMAP_TWO( access, _access, int , const char *, int )
REMAP_TWO( adjtime, _adjtime, int , struct timeval *, struct timeval *)
REMAP_ZERO_VOID( async_daemon, _async_daemon, void )
REMAP_THREE( bind, _bind, int , int , struct sockaddr *, int )
REMAP_ONE( chdir, _chdir, int, const char * )
REMAP_TWO( chmod, _chmod, int , const char *, unsigned int )
REMAP_THREE( chown, _chown, int , const char *, uid_t , gid_t )
REMAP_ONE( chroot, _chroot, int , const char *)
REMAP_THREE( connect, _connect, int , int , struct sockaddr *, int )
REMAP_TWO( execv, _execv, int , const char *, char ** const )
/* REMAP_THREE( execve, _execve, int , const char *, char * const [], char * const []) */
REMAP_THREE( execve, _execve, int , const char *, char ** const , char ** const )
REMAP_TWO( exportfs, _exportfs, int, const char *, const struct export *)
REMAP_TWO( fchmod, _fchmod, int , int , unsigned int )
REMAP_THREE( fchown, _fchown, int , int , uid_t , gid_t )
REMAP_ZERO( fork, _fork, pid_t )
REMAP_TWO( fstat, _fstat, int , int , struct stat *)
REMAP_TWO( fstat64, __fstat64, int , int , struct stat64 *)
REMAP_TWO( fstatfs, _fstatfs, int , int , struct statfs *)
REMAP_FOUR( getdirentries, _getdirentries, int, int ,struct dirent *, size_t, off_t * )
REMAP_TWO( getdomainname, _getdomainname, int , char *, int )
REMAP_TWO( getfh, _getfh, int , int , fhandle_t *)
REMAP_ZERO( getgid, _getgid, gid_t )
/* REMAP_TWO( getgroups, _getgroups, int , int , gid_t []) */
REMAP_TWO( getgroups, _getgroups, int , int , gid_t *)
REMAP_ZERO( gethostid, _gethostid, int )
REMAP_TWO( getitimer, _getitimer, int , int , struct itimerval *)
REMAP_THREE( getpeername, _getpeername, int , int , struct sockaddr *, unsigned int *)
REMAP_ZERO( getpid, _getpid, pid_t )
REMAP_TWO( getpriority, _getpriority, int , int , int )
REMAP_TWO( getrlimit, _getrlimit, int , int , struct rlimit *)
REMAP_TWO( getrlimit64, __getrlimit64, int , int , struct rlimit64 *)
REMAP_TWO( getrusage, _getrusage, int, int, struct rusage *)
REMAP_THREE( getsockname, _getsockname, int , int , struct sockaddr *, unsigned int *)
REMAP_FIVE( getsockopt, _getsockopt, int , int , int , int , char *, unsigned int *)
REMAP_TWO( gettimeofday, _gettimeofday, int , struct timeval *, struct timezone *)
REMAP_ZERO( getuid, _getuid, uid_t )
REMAP_TWO( kill, _kill, int, pid_t, int )
REMAP_THREE( lchown, _lchown, int , const char *, uid_t , gid_t )
REMAP_TWO( link, _link, int , const char *, const char *)
REMAP_TWO( listen, _listen, int , int , int )
REMAP_THREE( lockf, _lockf, int , int , int , off_t )
REMAP_THREE( lockf64, __lockf64, int , int , int , off64_t )
REMAP_TWO( lstat, _lstat, int , const char *, struct stat *)
REMAP_TWO( lstat64, __lstat64, int , const char *, struct stat64 *)
REMAP_THREE( madvise, _madvise, int , char *, size_t , int )
REMAP_TWO( mkdir, _mkdir, int , const char *, unsigned int )
REMAP_THREE( mknod, _mknod, int , const char *, int , dev_t )
REMAP_SIX( mmap, _mmap, void * , void *, size_t , int , int , int , off_t )
REMAP_SIX( mmap64, __mmap64, MMAP_T , MMAP_T , size_t , int , int , int , off64_t )
REMAP_SIX( mount, _mount, int , char *, char *, int , int , char *, int )
REMAP_THREE( mprotect, _mprotect, int , char *, size_t , int )
REMAP_THREE( msgctl, _msgctl, int , int , int , struct msqid_ds *)
REMAP_TWO( msgget, _msgget, int , key_t , int )
REMAP_FIVE( msgrcv, _msgrcv, int , int , void *, size_t , long , int )
REMAP_FOUR( msgsnd, _msgsnd, int , int , void *, size_t , int )
REMAP_THREE( msync, _msync, int , char *, size_t , int )
	 /* REMAP_TWO( munmap, _munmap, int , char *, size_t ) */
REMAP_ONE( nfssvc, _nfssvc, int , int)
REMAP_ONE( pipe, _pipe, int , int *)
REMAP_ONE( plock, _plock, int , int )
REMAP_THREE( poll, _poll, int, struct pollfd *, long unsigned int, int)
REMAP_FOUR_VOID( profil, _profil, void , short *, unsigned int , unsigned int , unsigned int )
REMAP_FOUR( ptrace, _ptrace, int , long , long int , unsigned long *, unsigned long )
REMAP_FOUR( quotactl, _quotactl, int , char *, int , int , char *)
REMAP_THREE( readlink, _readlink, int , const char *, char *, int )
REMAP_THREE( readv, _readv, size_t , int , const struct iovec * , size_t )
REMAP_ONE_VOID( reboot, _reboot, void , int )
REMAP_FOUR( recv, _recv, int , int , char *, int , int )
REMAP_SIX( recvfrom, _recvfrom, int , int , char *, int , int , struct sockaddr *, unsigned int *)
REMAP_THREE( recvmsg, _recvmsg, int , int , struct msghdr *, int )
REMAP_TWO( rename, _rename, int , const char * , const char * )
REMAP_ONE( rmdir, _rmdir, int , const char *)
REMAP_FIVE( select, _select, int, int, struct fd_set *, struct fd_set *, struct fd_set *, struct timeval *)
REMAP_FOUR( semctl, _semctl, int , int , int , int , void *)
REMAP_THREE( semget, _semget, int , key_t , int , int )
REMAP_THREE( semop, _semop, int , int , struct sembuf *, unsigned int )
REMAP_FOUR( send, _send, int , int , const void * , int , int )
REMAP_THREE( sendmsg, _sendmsg, int , int , struct msghdr *, int )
REMAP_SIX( sendto, _sendto, int , int , char *, int , int , struct sockaddr *, int )
REMAP_TWO( setdomainname, _setdomainname, int , char *, int )
REMAP_ONE( setgid, _setgid, int , gid_t )
/* REMAP_TWO( setgroups, _setgroups, int , int , gid_t []) */
REMAP_TWO( setgroups, _setgroups, int , int , gid_t * )
REMAP_ONE( sethostid, _sethostid, int , int )
REMAP_THREE( setitimer, _setitimer, int , int , const struct itimerval *, struct itimerval *)
REMAP_TWO( setpgid, _setpgid, int , pid_t , pid_t )
REMAP_ZERO( setpgrp, _setpgrp, int )
REMAP_THREE( setpriority, _setpriority, int , int , int , int )
REMAP_TWO( setregid, _setregid, int , uid_t , uid_t )
REMAP_TWO( setrlimit, _setrlimit, int , int , const struct rlimit *)
REMAP_TWO( setrlimit64, __setrlimit64, int , int , const struct rlimit64 *)
REMAP_FIVE( setsockopt, _setsockopt, int , int , int , int , char *, int )
REMAP_TWO( settimeofday, _settimeofday, int , struct timeval *, struct timezone *)
REMAP_ONE( setuid, _setuid, int , uid_t )
REMAP_THREE( shmat, _shmat, char * , int , char *, int )
REMAP_THREE( shmctl, _shmctl, int , int , int , struct shmid_ds *)
REMAP_ONE( shmdt, _shmdt, int , char *)
REMAP_THREE( shmget, _shmget, int , key_t , size_t , int )
REMAP_TWO( shutdown, _shutdown, int , int , int )
REMAP_THREE( socket, _socket, int, int, int, int )
/* REMAP_FOUR( socketpair, _socketpair, int , int , int , int , int []) */
REMAP_FOUR( socketpair, _socketpair, int , int , int , int , int * )
REMAP_TWO( stat, _stat, int , const char *, struct stat *)
REMAP_TWO( stat64, __stat64, int , const char *, struct stat64 *)
REMAP_TWO( statfs, _statfs, int , char *, struct statfs *)
REMAP_FOUR( swapon, _swapon, int , char *, int , int , int )
REMAP_TWO( symlink, _symlink, int , const char *, const char *)
REMAP_ZERO_VOID( sync, _sync, void )
REMAP_TWO( truncate, _truncate, int , const char *, off_t )
REMAP_TWO( truncate64, __truncate64, int , const char *, off64_t )
REMAP_ONE( umask, _umask, unsigned int , unsigned int )
REMAP_ONE( unlink, _unlink, int , const char *)
REMAP_ZERO( vfork, _vfork, pid_t )
REMAP_ONE( wait, _wait, pid_t , int *)
REMAP_THREE( writev, _writev, ssize_t , int , const struct iovec * , size_t )
#endif /* REMOTE_SYSCALLS */ 








