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
REMAP_THREE( accept, _accept, int , int , struct sockaddr *, int *)
REMAP_TWO( access, _access, int , const char *, int )
REMAP_ONE( acct, _acct, int , char *)
REMAP_TWO( adjtime, _adjtime, int , struct timeval *, struct timeval *)
REMAP_ZERO( async_daemon, _async_daemon, int )
REMAP_THREE( bind, _bind, int , int , struct sockaddr *, int )
REMAP_ONE( chdir, _chdir, int, const char * )
REMAP_TWO( chmod, _chmod, int , const char *, unsigned int )
REMAP_THREE( chown, _chown, int , const char *, uid_t , gid_t )
REMAP_ONE( chroot, _chroot, int , const char *)
REMAP_THREE( connect, _connect, int , int , struct sockaddr *, int )
REMAP_TWO( creat, _creat, int, const char *, mode_t )
REMAP_ONE( dup, _dup, int, int )
/* REMAP_TWO( execv, _execv, int , const char *, char * const []) */
REMAP_TWO( execv, _execv, int , const char *, char **)
/* REMAP_THREE( execve, _execve, int , const char *, char * const [], char * const []) */
REMAP_THREE( execve, _execve, int , const char *, char ** , char ** )
REMAP_THREE( exportfs, _exportfs, int , int , int *, struct exportfsdata *)
REMAP_ONE( fchdir, _fchdir, int, int )
REMAP_TWO( fchmod, _fchmod, int , int , unsigned int )
REMAP_THREE( fchown, _fchown, int , int , uid_t , gid_t )
REMAP_ZERO( fork, _fork, pid_t )
REMAP_FOUR( fstatfs, _fstatfs, int , int , struct statfs *, int , int )
REMAP_ONE( fsync, _fsync, int , int )
REMAP_TWO( ftruncate, _ftruncate, int , int , off_t )
REMAP_THREE( getdents, _getdents, int, int, char*, unsigned int )
REMAP_TWO( getdomainname, _getdomainname, int , char *, int )
REMAP_ZERO( getgid, _getgid, gid_t )
REMAP_ZERO( gethostid, _gethostid, int )
REMAP_TWO( gethostname, _gethostname, int , char *, int )
REMAP_TWO( getitimer, _getitimer, int , int , struct itimerval *)
REMAP_ZERO( getpagesize, _getpagesize, int )
REMAP_THREE( getpeername, _getpeername, int , int , struct sockaddr *, int *)
REMAP_ZERO( getpid, _getpid, pid_t )
REMAP_TWO( getrlimit, _getrlimit, int , int , struct rlimit *)
REMAP_THREE( getsockname, _getsockname, int , int , struct sockaddr *, int *)
REMAP_FIVE( getsockopt, _getsockopt, int , int , int , int , char *, int *)
REMAP_ZERO( getuid, _getuid, uid_t )
REMAP_THREE( ioctl, _ioctl, int, int, int, int )
REMAP_TWO( kill, _kill, int, pid_t, int )
REMAP_THREE( lchown, _lchown, int , const char *, uid_t , gid_t )
REMAP_TWO( link, _link, int , const char *, const char *)
REMAP_TWO( listen, _listen, int , int , int )
REMAP_THREE( madvise, _madvise, int , void *, size_t , int )
REMAP_TWO( mkdir, _mkdir, int , const char *, unsigned int )
REMAP_SIX( mmap, _mmap, char *, char *, size_t, int, int, int, off_t )
REMAP_SIX( mount, _mount, int , char *, char *, int , int , char *, int )
REMAP_THREE( mprotect, _mprotect, int , void *, size_t , int )
REMAP_THREE( msync, _msync, int , void *, size_t , int )
REMAP_TWO( munmap, _munmap, int , void *, size_t )
REMAP_THREE( nfssvc, _nfssvc, int , int , int , int )
/* REMAP_ONE( pipe, _pipe, int , int []) */
REMAP_ONE( pipe, _pipe, int , int * )
REMAP_ONE( plock, _plock, int , int )
REMAP_ZERO( poll, _poll, int )
REMAP_FOUR( profil, _profil, void , short *, unsigned int , unsigned int , unsigned int )
REMAP_FOUR( ptrace, _ptrace, int , long , long int , unsigned long *, unsigned long )
REMAP_FOUR( quotactl, _quotactl, int , char *, int , int , char *)
REMAP_THREE( readlink, _readlink, int , const char *, char *, int )
REMAP_THREE( readv, _readv, ssize_t, int, const struct iovec *, int )
REMAP_FOUR( recv, _recv, int , int , char *, int , int )
REMAP_SIX( recvfrom, _recvfrom, int , int , char *, int , int , struct sockaddr *, int *)
REMAP_THREE( recvmsg, _recvmsg, int , int , struct msghdr *, int )
REMAP_TWO( rename, _rename, int, const char *, const char * )
REMAP_ONE( rmdir, _rmdir, int , const char *)
REMAP_FIVE( select, _select, int , int , fd_set *, fd_set *, fd_set *, struct timeval *)
REMAP_FOUR( send, _send, int, int, const void *, int, int )
REMAP_THREE( sendmsg, _sendmsg, int , int , struct msghdr *, int )
REMAP_SIX( sendto, _sendto, int , int , char *, int , int , struct sockaddr *, int )
REMAP_TWO( setdomainname, _setdomainname, int , char *, int )
REMAP_ONE( setgid, _setgid, int , gid_t )
REMAP_ONE( sethostid, _sethostid, int , int )
REMAP_TWO( sethostname, _sethostname, int , char *, int )
REMAP_THREE( setitimer, _setitimer, int , int , struct itimerval *, struct itimerval *)
REMAP_ZERO( setpgrp, _setpgrp, pid_t )
REMAP_TWO( setregid, _setregid, int , int , int )
REMAP_TWO( setreuid, _setreuid, int , int , int )
REMAP_TWO( setrlimit, _setrlimit, int , int , const struct rlimit *)
REMAP_FIVE( setsockopt, _setsockopt, int , int , int , int , char *, int )
REMAP_ONE( setuid, _setuid, int , uid_t )
REMAP_TWO( shutdown, _shutdown, int , int , int )
REMAP_ONE( sigsuspend, _sigsuspend, int, sigset_t * )
REMAP_THREE( socket, _socket, int, int, int, int )
/* REMAP_FOUR( socketpair, _socketpair, int , int , int , int , int []) */
REMAP_FOUR( socketpair, _socketpair, int , int , int , int , int * )
REMAP_TWO( stat, _stat, int , const char *, struct stat *)
REMAP_FOUR( statfs, _statfs, int , const char *, struct statfs *, int , int )
REMAP_TWO( symlink, _symlink, int , const char *, const char *)
REMAP_ZERO( sync, _sync, void )
REMAP_TWO( truncate, _truncate, int , const char *, off_t )
REMAP_ONE( umask, _umask, unsigned int , unsigned int )
REMAP_ONE( unlink, _unlink, int , const char *)
REMAP_THREE( writev, _writev, ssize_t, int, const struct iovec *, int )
