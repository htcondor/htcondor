/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <stdio.h>
#include <varargs.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/statfs.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include "syscall.aix.h"

int		CondorErrno;

#define UNDOC_SYSCALL 78
#define UNKNOWN_SYSCALL 79


#define ZERO(call) \
	case SYS_ ## call: { \
	va_end(ap); \
	rval = call(); \
	CondorErrno = errno; \
	return rval; \
	}

#define ONE(call,type_1) \
	case SYS_ ## call: { \
	type_1	arg_1; \
	arg_1 = va_arg(ap,type_1); \
	va_end(ap); \
	rval = call(arg_1); \
	CondorErrno = errno; \
	return rval; \
	}

#define TWO(call,type_1,type_2) \
	case SYS_ ## call: { \
	type_1	arg_1; \
	type_2	arg_2; \
	arg_1 = va_arg(ap,type_1); \
	arg_2 = va_arg(ap,type_2); \
	va_end(ap); \
	rval = call(arg_1,arg_2); \
	CondorErrno = errno; \
	return rval; \
	}

#define THREE(call,type_1,type_2,type_3) \
	case SYS_ ## call: { \
	type_1	arg_1; \
	type_2	arg_2; \
	type_3	arg_3; \
	arg_1 = va_arg(ap,type_1); \
	arg_2 = va_arg(ap,type_2); \
	arg_3 = va_arg(ap,type_3); \
	va_end(ap); \
	rval = call(arg_1,arg_2,arg_3); \
	CondorErrno = errno; \
	return rval; \
	}

#define FOUR(call,type_1,type_2,type_3,type_4) \
	case SYS_ ## call: { \
	type_1	arg_1; \
	type_2	arg_2; \
	type_3	arg_3; \
	type_4	arg_4; \
	arg_1 = va_arg(ap,type_1); \
	arg_2 = va_arg(ap,type_2); \
	arg_3 = va_arg(ap,type_3); \
	arg_4 = va_arg(ap,type_4); \
	va_end(ap); \
	rval = call(arg_1,arg_2,arg_3,arg_4); \
	if( rval == -1 && errno == EAGAIN ) { \
		_exit( 379 ); \
	} \
	CondorErrno = errno; \
	return rval; \
	}

#define FIVE(call,type_1,type_2,type_3,type_4,type_5) \
	case SYS_ ## call: { \
	type_1	arg_1; \
	type_2	arg_2; \
	type_3	arg_3; \
	type_4	arg_4; \
	type_5	arg_5; \
	arg_1 = va_arg(ap,type_1); \
	arg_2 = va_arg(ap,type_2); \
	arg_3 = va_arg(ap,type_3); \
	arg_4 = va_arg(ap,type_4); \
	arg_5 = va_arg(ap,type_5); \
	va_end(ap); \
	rval = call(arg_1,arg_2,arg_3,arg_4,arg_5); \
	CondorErrno = errno; \
	return rval; \
	}

#define SIX(call,type_1,type_2,type_3,type_4,type_5,type_6) \
	case SYS_ ## call: { \
	type_1	arg_1; \
	type_2	arg_2; \
	type_3	arg_3; \
	type_4	arg_4; \
	type_5	arg_5; \
	type_6	arg_6; \
	arg_1 = va_arg(ap,type_1); \
	arg_2 = va_arg(ap,type_2); \
	arg_3 = va_arg(ap,type_3); \
	arg_4 = va_arg(ap,type_4); \
	arg_5 = va_arg(ap,type_5); \
	arg_6 = va_arg(ap,type_6); \
	va_end(ap); \
	rval = call(arg_1,arg_2,arg_3,arg_4,arg_5,arg_6); \
	CondorErrno = errno; \
	return rval; \
	}

#define UNDOC(call) \
	case SYS_ ## call: { \
	_exit( SYS_ ## call ); \
	}

#include </usr/include/sys/errno.h>

syscall(va_alist)
	va_dcl
{	va_list ap;
	int		number;
	int		rval;

	va_start( ap );
	number = va_arg( ap, int );

	switch( number ) {
		TWO(access,char *,int)
		THREE(accessx,char *,int,int)
		ONE(acct,char *)
		TWO(adjtime,struct timeval *,struct timeval *)
		TWO(audit,int,int)
		FOUR(auditbin,int,int,int,int)
		THREE(auditevents,int,struct audit_class *,int)
		FOUR(auditlog,char *,int,char *,int)
		THREE(auditobj,int,struct o_event *,int)
		FOUR(auditproc,int,int,int,int)
		ONE(brk,char *)
		ONE(chdir,char *)
		THREE(chacl,char *,struct acl *,int)
		TWO(chmod,char *,mode_t)
		THREE(chown,char *,uid_t,gid_t)
		FOUR(chownx,char *,uid_t,gid_t,int)
		THREE(chpriv,char *,struct pcl *,int)
		ONE(chroot,char *)
		ONE(close,int)
		TWO(creat,char *,int)
		THREE(disclaim,char *,unsigned int,unsigned int)
		THREE(execve,char *,char**,char**)
		ONE(_exit,int)
		THREE(faccessx,int,int,int)
		THREE(fchacl,int,struct acl *,int)
		TWO(fchmod,char *,int)
		THREE(fchown,int,uid_t,gid_t)
		FOUR(fchownx,int,uid_t,gid_t,int)
		THREE(fchpriv,int,struct pcl *,int)
		TWO(fclear,int,unsigned long)
		ZERO(fork)
		ONE(frevoke,int)
		FOUR(fscntl,int,int,char *,int)
		FOUR(fstatacl,char *,int,struct acl *,int)
		TWO(fstatfs,int,struct statfs *)
		FOUR(fstatpriv,int,int,struct pcl *,int)
		FOUR(fstatx,int,struct stat *,int,int)
		ONE(fsync,int)
		TWO(ftruncate,int,unsigned long)
		FOUR(getargs,struct procinfo *,int,char *,int)
		THREE(getdirent,int,char *,unsigned)
		FOUR(getevars,struct procinfo *,int,char *,int)
		ONE(getgidx,int)
		TWO(getgroups,int,gid_t *)
		ZERO(getpgrp)
		ONE(kgetpgrp,pid_t)
		ZERO(getpid)
		ZERO(getppid)
		TWO(getpriority,int,int)
		ONE(getpri,pid_t)
		UNDOC(getpriv)
		THREE(getproc,struct procinfo *,int,int)
		TWO(getrlimit,int,struct rlimit *)
		TWO(getrusage,int,struct rusage *)
		ONE(getuidx,int)
		FOUR(getuser,struct procinfo *,int,void *,int)
		THREE(kfcntl,int,int,int)
		TWO(kill,pid_t,int)
		FOUR(kioctl,int,int,int,char *)
		THREE(knlist,struct nlist *,int,int)
		FOUR(kreadv,int,struct iovec *,int,int)
#ifdef NOTDEF
		case SYS_kreadv: {
		int				arg_1;
		struct iovec	*arg_2;
		int				arg_3;
		int				arg_4;
		int				temp_errno;
		int				file_flags;
		arg_1 = va_arg(ap,int);
		arg_2 = va_arg(ap,struct iovec *);
		arg_3 = va_arg(ap,int);
		arg_4 = va_arg(ap,int);
		va_end(ap);
		rval = kreadv(arg_1,arg_2,arg_3,arg_4);
		if( rval < 0 && errno == EAGAIN ) {
			file_flags = -1;
			temp_errno = errno;
			file_flags = kfcntl( arg_1, F_GETFL, 0 );
			abort();
		}
		CondorErrno = errno;
		return rval;
		}
#endif NOTDEF


		FOUR(kwaitpid,int,pid_t,int,struct rusage *)
		FOUR(kwritev,int,struct iovec *,int,int)
		TWO(link,char *,char *)
		THREE(_load,char *,int,char *)
		THREE(loadbind,int,void *,void *)
		THREE(loadquery,int,void *,unsigned int)
		THREE(lockf,int,int,off_t)
		THREE(lseek,int,int,off_t)
		TWO(mkdir,char *, int)
		THREE(mknod,char *,int,dev_t)
		THREE(mntctl,int,int,char *)
		THREE(open,char *,int,int)
		FOUR(openx,char *,int,int,int)
		ZERO(pause)
		ONE(pipe,int *)
		ONE(plock,int)
		THREE(poll,struct pollfd *,unsigned long,long)
		ONE(privcheck,int)
		FOUR(profil,short *,unsigned,unsigned,unsigned)
		ONE(psdanger,int)
		FIVE(ptrace,int,int,int,int *,int *)
		THREE(readlink,char *,char *,int)
		ONE(reboot,char *)
		TWO(rename,char *,char *)
		ONE(revoke,char *)
		ONE(rmdir,char *)
		THREE(absinterval,timer_t,struct itimerstruc_t *,struct itimerstruc_t *)
		TWO(getinterval,timer_t,struct itimerstruc_t *)
		TWO(gettimer,int,struct timestruc_t *)
		TWO(gettimerid,int,int)
		THREE(incinterval,timer_t,struct itimerstruc_t *,struct itimerstruc_t *)
		ONE(reltimerid,timer_t)
		THREE(resabs,timer_t,struct timestruc_t *,struct timestruc_t *)
		THREE(resinc,timer_t,struct timestruc_t *,struct timestruc_t *)
		THREE(restimer,int,struct timestruc_t *,struct timestruc_t *)
		TWO(settimer,int,struct timestruc_t *)
		TWO(nsleep,struct timestruc_t *,struct timestruc_t *)
		ONE(sbrk,int)
		FIVE(select,int,struct sellist *,struct sellist *,struct sellist *,
															struct timeval *)
		ONE(seteuid,uid_t)
		ONE(setgid,gid_t)
		TWO(setgidx,int,gid_t)
		TWO(setgroups,int,gid_t *)
		TWO(setpgid,pid_t,pid_t)
		ZERO(setpgrp)
		TWO(setpri,pid_t,int)
		THREE(setpriority,int,int,int)
		UNDOC(setpriv)
		TWO(setreuid,uid_t,uid_t)
		TWO(setrlimit,int,struct rlimit *)
		ZERO(setsid)
		ONE(setuid,uid_t)
		TWO(setuidx,int,uid_t)
		THREE(shmctl,int,int,struct shmid_ds *)
		THREE(shmget,key_t,int,int)
		THREE(shmat,int,int,char *)
		ONE(shmdt,char *)
		THREE(msgctl,int,int,struct msqid_ds *)
		TWO(msgget,key_t,int)
		FOUR(msgsnd,int,void *,size_t,size_t)
		FIVE(msgrcv,int,void *,size_t,size_t,long)
		FIVE(msgxrcv,int,int,int,struct msgxbuf *,long)
		FOUR(semctl,int,int,int,int)
		THREE(semget,key_t,int,int)
		THREE(semop,int,struct sembuf *,unsigned)
		THREE(sigaction,int,struct sigaction *,struct sigaction *)
		ONE(sigcleanup,sigset_t *)
		THREE(sigprocmask,int,sigset_t *,sigset_t *)
		ONE(sigreturn,struct sigcontext *)
		TWO(sigstack,struct sigstack *,struct sigstack *)
		ONE(sigsuspend,sigset_t *)
		ONE(sigpending,sigset_t *)
		FOUR(statacl,char *,int,struct acl *,int)
		TWO(statfs,char *,struct statfs *)
		FOUR(statpriv,char *,int,struct pcl *,int)
		FOUR(statx,char *,struct stat *,int,int)
		ONE(swapoff,char *)
		ONE(swapon,char *)
		TWO(swapqry,char *,struct pginfo *)
		TWO(symlink,char *,char *)
		ZERO(sync)
		THREE(sysconfig,int,void *,int)
		ONE(times,struct tms *)
		TWO(truncate,char *,off_t)
		TWO(ulimit,int,off_t)
		ONE(umask,mode_t)
		ONE(umount,char *)
		ONE(uname,struct utsname *)
		ONE(unamex,struct xutsname *)
		ONE(unlink,char *)
		ONE(unload,int *)
		THREE(usrinfo,int,int,char *)
		TWO(ustat,dev_t,struct ustat *)
		TWO(utimes,char *,struct timeval *)
		TWO(uvmount,int,int)
		TWO(vmount,struct vmount *,int)
		FIVE(trcgen,int,int,int,int,char)
		THREE(socket,int,int,int)
		THREE(bind,int,struct sockaddr *,int)
		TWO(listen,int,int)
		THREE(accept,int,struct sockaddr *,int *)
		THREE(connect,int,struct sockaddr *,int)
		FOUR(socketpair,int,int,int,int *)
		SIX(sendto,int,char *,int,int,struct sockaddr *,int)
		FOUR(send,int,char *,int,int)
		THREE(sendmsg,int,struct msghdr *,int)
		SIX(recvfrom,int,char *,int,int,struct sockaddr *,int)
		FOUR(recv,int,char *,int,int)
		THREE(recvmsg,int,struct msghdr *,int)
		TWO(shutdown,int,int)
		FIVE(setsockopt,int,int,int,char *,int)
		FIVE(getsockopt,int,int,int,char *,int)
		THREE(getsockname,int,struct sockaddr *,int)
		THREE(getpeername,int,struct sockaddr *,int)
		TWO(setdomainname,char *,int)
		TWO(getdomainname,char *,int)
		TWO(sethostname,char *,int)
		TWO(gethostname,char *,int)
		ONE(sethostid,int)
		ZERO(gethostid)
/*
** The kernel exports these, but even Marc Auslander doesn't know
** what they are...
*/
		UNDOC(unameu)
		UNDOC(Trconflag)
		UNDOC(trchook)
		UNDOC(trchk)
		UNDOC(trchkt)
		UNDOC(trchkl)
		UNDOC(trchklt)
		UNDOC(trchkg)
		UNDOC(trchkgt)
		UNDOC(trcgent)

		default:
			_exit( UNKNOWN_SYSCALL );
	}
}


#ifdef NOTDEF
abort()
{
	struct sigaction    action;
	struct rlimit lim;

		/* Set up SIGIOT to produce a full core dump. */
	if( sigaction(SIGIOT,NULL,&action) < 0 ) {
		_exit( 1 );
	}
	action.sa_flags |= SA_FULLDUMP;
	action.sa_handler = SIG_DFL;
	if( sigaction(SIGIOT,&action,NULL) < 0 ) {
		_exit( 1 );
	}

		/* Make sure our core size limit allows for a full dump */
	lim.rlim_cur = RLIM_INFINITY;
	if( setrlimit(RLIMIT_CORE,&lim) < 0 ) {
		_exit( 1 );
	}

		/* Send ourself the signal and wait for it */
	kill( getpid(), SIGIOT );
}
#endif NOTDEF
