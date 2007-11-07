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



/* These remaps are needed by file_state.c */
#if defined(FILE_TABLE)
REMAP_ONE( close, __close, int , int )
REMAP_ONE( dup, __dup, int, int )
REMAP_TWO( dup2, __dup2, int, int, int )
REMAP_ONE( fchdir, __fchdir, int, int )
REMAP_THREE_VARARGS( fcntl, __fcntl, int , int , int , int)
REMAP_ONE( fsync, __fsync, int , int )
REMAP_ONE( fdatasync, __fdatasync, int , int )
REMAP_TWO( ftruncate, __ftruncate, int, int, off_t )
REMAP_THREE_VARARGS( ioctl, __ioctl, int , int , int , int)
REMAP_THREE( lseek, __lseek, off_t , int , off_t , int )
REMAP_THREE_VARARGS( open, __open, int , const char *, int , int)
REMAP_THREE_VARARGS( open, _open, int , const char *, int , int)
REMAP_THREE( read, __read, ssize_t , int , void *, size_t )
REMAP_THREE( write, __write, ssize_t , int , const void *, size_t )
#endif

/* These remaps are neede by signals_support.c */
#if defined(SAVE_SIGSTATE)
REMAP_THREE( sigprocmask, __sigprocmask, int, int, const sigset_t *, sigset_t * )
/* Multiple defns */
/* REMAP_THREE( sigprocmask, _sigprocmask, int, int, const sigset_t *, sigset_t * ) */
REMAP_ONE( sigsuspend, __sigsuspend, int, const sigset_t * )
REMAP_ONE( sigsuspend, _sigsuspend, int, const sigset_t * )
#endif

/* These remaps fill out the rest of the remote syscalls */
#if defined(REMOTE_SYSCALLS)

REMAP_THREE( accept, __accept, int , int , struct sockaddr *, int *)
REMAP_TWO( access, __access, int , const char *, int )
REMAP_ONE( acct, __acct, int , char *)
REMAP_TWO( adjtime, __adjtime, int , struct timeval *, struct timeval *)
REMAP_ZERO( async_daemon, __async_daemon, int )
REMAP_THREE( bind, __bind, int , int , const struct sockaddr *, int )
REMAP_ONE( chdir, __chdir, int, const char * )
REMAP_TWO( chmod, __chmod, int , const char *, unsigned int )
REMAP_THREE( chown, __chown, int , const char *, uid_t , gid_t )
REMAP_ONE( chroot, __chroot, int , const char *)
REMAP_THREE( connect, __connect, int , int , const struct sockaddr *, int )
REMAP_FIVE( exec_with_loader, __exec_with_loader, int , int , const char *, const char *, char * const *, char * const *)
REMAP_TWO( execv, __execv, int , const char *, char * const *)
REMAP_THREE( execve, __execve, int , const char *, char * const *, char * const *)
REMAP_THREE( exportfs, __exportfs, int , int , int *, struct exportfsdata *)
REMAP_TWO( fchmod, __fchmod, int , int , unsigned int )
REMAP_THREE( fchown, __fchown, int , int , uid_t , gid_t )
REMAP_TWO( flock, __flock, int , int , int )
REMAP_ZERO( fork, __fork, pid_t )
REMAP_ZERO( fork, _fork, pid_t )
REMAP_TWO( fstat, __fstat, int , int , struct stat *)
REMAP_TWO_VARARGS( _F64_fstatfs, __fstatfs, int , int , struct statfs * )
REMAP_TWO( getaddressconf, __getaddressconf, int , struct addressconf *, size_t )
REMAP_FOUR( getdirentries, __getdirentries, int , int , struct dirent *, size_t , off_t *)
REMAP_TWO( getdomainname, __getdomainname, int , char *, int )
REMAP_ZERO( getdtablesize, __getdtablesize, int )
REMAP_THREE( getfh, __getfh, int , int , fhandle_t *, int )
REMAP_THREE( getfsstat, __getfsstat, int , struct statfs *, long , int )
REMAP_ZERO( getgid, __getgid, gid_t )
REMAP_TWO( getgroups, __getgroups, int , int , gid_t *)
REMAP_ZERO( gethostid, __gethostid, long )
REMAP_TWO( gethostname, __gethostname, int , char *, size_t )
REMAP_TWO( getitimer, __getitimer, int , int , struct itimerval *)
REMAP_ZERO( getlogin, __getlogin, char* )
REMAP_ZERO( getlogin, _getlogin, char* )
REMAP_ZERO( getpagesize, __getpagesize, int )
REMAP_THREE( getpeername, __getpeername, int , int , struct sockaddr *, int *)
REMAP_ONE( getpgid, __getpgid, pid_t , pid_t )
REMAP_ZERO( getpgrp, __getpgrp, pid_t )
REMAP_ZERO( getpid, __getpid, pid_t )
REMAP_TWO( getpriority, __getpriority, int , int , int )
REMAP_TWO( getrlimit, __getrlimit, int , int , struct rlimit *)
REMAP_TWO( getrusage, __getrusage, int, int, struct rusage * ) 
REMAP_THREE( getsockname, __getsockname, int , int , struct sockaddr *, int *)
REMAP_FIVE( getsockopt, __getsockopt, int , int , int , int , void *, int *)
REMAP_FIVE( getsysinfo, __getsysinfo, int , unsigned long , char *, unsigned long , int *, char *)
REMAP_TWO( gettimeofday, __gettimeofday, int , struct timeval *, void *)
REMAP_ZERO( getuid, __getuid, uid_t )
REMAP_TWO( kill, __kill, int, pid_t, int )
REMAP_THREE( lchown, __lchown, int , const char *, uid_t , gid_t )
REMAP_TWO( link, __link, int , const char *, const char *)
REMAP_TWO( listen, __listen, int , int , int )
REMAP_TWO( lstat, __lstat, int , const char *, struct stat *)
REMAP_THREE( madvise, __madvise, int , void *, size_t , int )
REMAP_TWO( mkdir, __mkdir, int , const char *, unsigned int )
REMAP_THREE( mknod, __mknod, int , const char *, mode_t , dev_t )
REMAP_SIX( mmap, __mmap, void * , void *, size_t , int , int , int , off_t )
REMAP_FOUR( mount, __mount, int , int, char *, u_long, caddr_t )
REMAP_THREE( mprotect, __mprotect, int , const void *, size_t , int )
REMAP_THREE( msgctl, __msgctl, int , int , int , struct msqid_ds *)
REMAP_TWO( msgget, __msgget, int , key_t , int )
REMAP_FIVE( msgrcv, __msgrcv, int , int , void *, size_t , long , int )
REMAP_FOUR( msgsnd, __msgsnd, int , int , const void *, size_t , int )
REMAP_THREE( msync, __msync, int , void *, size_t , int )
REMAP_THREE( mvalid, __mvalid, int , char *, size_t , int )
/* Multiple definitions */
/* REMAP_THREE( nfssvc, __nfssvc, int , int , int , int ) */
REMAP_ONE( pipe, __pipe, int , int *)
REMAP_ONE( plock, __plock, int , int )
REMAP_THREE( poll, __poll, int, struct pollfd *, nfds_t, int )
REMAP_FOUR_VOID( profil, __profil, void , short *, unsigned int , unsigned int , unsigned int )
REMAP_FOUR( ptrace, __ptrace, int , long , long int , unsigned long *, unsigned long )
REMAP_FOUR( quotactl, __quotactl, int , char *, int , int , char *)
REMAP_THREE( readlink, __readlink, int , const char *, char *, size_t )
REMAP_THREE( readv, __readv, ssize_t, int, const struct iovec *, int )
REMAP_ONE_VOID( reboot, __reboot, void , int )
REMAP_SIX( recvfrom, __recvfrom, int , int , void *, int , int , struct sockaddr *, int *)
REMAP_THREE( recvmsg, __recvmsg, int , int , struct msghdr *, int )
REMAP_ONE( revoke, __revoke, int , char *)
REMAP_ONE( rmdir, __rmdir, int , const char *)
REMAP_FIVE( select, __select, int , int , fd_set *, fd_set *, fd_set *, struct timeval *)
REMAP_FOUR( semctl, __semctl, int , int , int , int , void *)
REMAP_THREE( semget, __semget, int , key_t , int , int )
REMAP_THREE( semop, __semop, int , int , struct sembuf *, unsigned int )
REMAP_THREE( sendmsg, __sendmsg, int , int , struct msghdr *, int )
REMAP_SIX( sendto, __sendto, int , int , const void *, int , int , const struct sockaddr *, int )
REMAP_TWO( setdomainname, __setdomainname, int , char *, int )
REMAP_ONE( setgid, __setgid, int , gid_t )
REMAP_TWO( setgroups, __setgroups, int , int , gid_t *)
REMAP_ONE( sethostid, __sethostid, int , int )
REMAP_TWO( sethostname, __sethostname, int , char *, int )
REMAP_THREE( setitimer, __setitimer, int , int , const struct itimerval *, struct itimerval *)
REMAP_ONE( setlogin, __setlogin, int , char *)
REMAP_TWO( setpgid, __setpgid, int , pid_t , pid_t )
REMAP_TWO( setpgrp, __setpgrp, int , pid_t , pid_t )
REMAP_THREE( setpriority, __setpriority, int, int, id_t, int )
REMAP_TWO( setregid, __setregid, int , gid_t , gid_t )
REMAP_TWO( setreuid, __setreuid, int , uid_t , uid_t )
REMAP_TWO( setrlimit, __setrlimit, int , int , struct rlimit *)
REMAP_ZERO( setsid, __setsid, pid_t )
REMAP_FIVE( setsockopt, __setsockopt, int , int , int , int , const void *, int )
REMAP_FIVE( setsysinfo, __setsysinfo, int , unsigned long , char *, unsigned long , char *, unsigned long )
REMAP_TWO( settimeofday, __settimeofday, int , struct timeval *, struct timezone *)
REMAP_ONE( setuid, __setuid, int , uid_t )
REMAP_THREE( shmat, __shmat, void * , int , const void *, int )
REMAP_THREE( shmctl, __shmctl, int , int , int , struct shmid_ds *)
REMAP_ONE( shmdt, __shmdt, int , const void *)
REMAP_THREE( shmget, __shmget, int , key_t , size_t , int )
REMAP_TWO( shutdown, __shutdown, int , int , int )
REMAP_THREE( socket, __socket, int, int, int, int )
REMAP_FOUR( socketpair, __socketpair, int , int , int , int , int *)
REMAP_TWO( stat, __stat, int , const char *, struct stat *)
REMAP_TWO_VARARGS( _F64_statfs, __statfs, int , char *, struct statfs * )
REMAP_TWO( swapon, __swapon, int , const char *, int )
REMAP_TWO( symlink, __symlink, int , const char *, const char *)
REMAP_ZERO_VOID( sync, __sync, void )
REMAP_FIVE( table, __table, int , int , int , char *, int , unsigned int )
REMAP_TWO( truncate, __truncate, int , const char *, off_t )
REMAP_ONE( umask, __umask, unsigned int , unsigned int )
     // REMAP_ONE( uname, __uname, int , struct utsname *)
REMAP_ONE( unlink, __unlink, int , const char *)
REMAP_TWO( uswitch, __uswitch, int , int , int )
REMAP_TWO( utimes, __utimes, int , const char *, const struct timeval *)
REMAP_ZERO( vfork, __vfork, pid_t )
REMAP_FOUR( wait4, __wait4, pid_t , pid_t , union wait *, int , struct rusage *)
REMAP_THREE( writev, __writev, ssize_t, int, const struct iovec *, int )

#endif REMOTE_SYSCALLS
