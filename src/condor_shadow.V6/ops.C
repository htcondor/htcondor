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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "errno.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>
#include "condor_constants.h"

#if defined(AIX32)
#	include  <sys/statfs.h>
#	include  <sys/utsname.h>
#	include "condor_fdset.h"
#endif

#if defined(IRIX331)
#include <sys/statfs.h>
#elif !defined(ULTRIX42) && !defined(ULTRIX43) && !defined(OSF1) && !defined(IRIX53)
#include <sys/vfs.h>
#endif

#if 0
#if !defined(ultrix)
#ifdef IRIX331
#include <sys/statfs.h>
#else
#include <sys/vfs.h>
#endif
#endif !defined(ultrix)
#endif

#if defined(HPUX9)
#include <sys/ustat.h>
#endif

#include <sys/wait.h>
#include <sys/ioctl.h>

#if	defined(SUNOS40) || defined(SUNOS41) || defined(CMOS)
#include <sys/core.h>
#endif defined(SUNOS40) || defined(SUNOS41) || defined(CMOS)

#undef _POSIX_SOURCE
#include "condor_types.h"

#include "condor_sys.h"
#include "condor_rsc.h"
#include "trace.h"
#include "condor_debug.h"
#include "debug.h"
#include "fileno.h"
#include "condor_io.h"
#include "clib.h"
#include "shadow.h"

FILE	*fdopen();

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

static int RSCSock;
static int LogSock;
extern int	PipeToParent;
extern int	LogPipe;
extern int	LockFd;
extern int	ImageSize;
extern int	ClientUid;
extern int	ClientGid;
extern struct rusage AccumRusage;

extern int UsePipes;

extern int free_fs_blocks();

extern V2_PROC *Proc;

/* XDR xdr_RSC, *xdr_syscall = &xdr_RSC; */
ReliSock *syscall_sock;

ReliSock *
RSC_ShadowInit( int rscsock, int errsock )
{
	RSCSock = rscsock;
	syscall_sock = new ReliSock();
	syscall_sock->attach_to_file_desc(RSCSock);

/*	xdrrec_create( xdr_syscall, XDR_BUFSIZE, XDR_BUFSIZE, (caddr_t)&RSCSock,
			(void*)XDR_ShadowRead, (void*)XDR_ShadowWrite ); */
	
	syscall_sock->encode();

	LogSock = errsock;
	// Now set LogSock to be posix-style non-blocking
	fcntl(LogSock,F_SETFL,O_NONBLOCK);

	return( syscall_sock );
}

#ifdef CARMI_OPS
XDR *
RSC_MyShadowInit( int *rscsock, int *errsock )
{
   XDR* xdrs;
/* AT --- have added this line here to make the streams dynamically 
   allocated, rather than being a single statoc stream */
	xdrs = (XDR *) malloc(sizeof(XDR));
	
	xdrrec_create( xdrs, XDR_BUFSIZE, XDR_BUFSIZE, (caddr_t)rscsock,
			XDR_ShadowRead, XDR_ShadowWrite );
/*        xdrrec_skiprecord( xdrs ); */     /* flush input buffer */
	xdrs->x_op = XDR_ENCODE;
	return( xdrs );
}
#endif


#if 0
char MsgBuf[ BUFSIZ ];
HandleChildLog( log_fp )
FILE	*log_fp;
{
	register char *nlp;
	char *strchr();
	char buf[ BUFSIZ ];

	for(;;) {
		memset(buf,0, sizeof(buf));

		if( fgets(buf, sizeof(buf), log_fp) == NULL ) {
			return -1;
		}

		nlp = strchr(buf, '\n');
		if( nlp != NULL ) {
			*nlp = '\0';
		}

		dprintf(D_ALWAYS|D_NOHEADER, "%s\n", buf );
		if( strncmp("ERROR",buf,5) == 0 ) {
			(void)strcpy( MsgBuf, buf );
		}

		/*
		**	If there is more to read, read it, otherwise just return...
		*/
#if defined(LINUX)
		return 0;	/*Don't know how to handle this yet		*/
#else
		if( log_fp->_cnt == 0 ) {
			return 0;
		}
#endif
	}
}
#endif

HandleLog()
{
	char *nlp;
	char buf[ BUFSIZ ];
	int len,i,done;
	static char oldbuf[ BUFSIZ ];

	for(;;) {
		errno = 0;

		len=read(LogSock,buf,sizeof(buf)-1);
		if ( len < 0 ) {
			if ( errno == EINTR )
				continue;	// interrupted by signal; just continue
			if ( errno == EAGAIN )
				return 0;	// no more data to read 

			dprintf(D_FULLDEBUG,"HandleLog() read FAILED, errno=%d\n",errno );
			return -1; 		// anything other than EINTR or EAGAIN is error
		}
		if ( len == 0 ) {
			// some platforms return 0 instead of -1/EAGAIN like POSIX says...
			return 0;
		}

		nlp = buf;
		done = 1;
		for (i=0;i<len;i++) {
			switch ( buf[i] ) {
				case '\n' :
					buf[i] = '\0';
					// NOTICE: no break here, we fall thru....
				case '\0' :
					if ( nlp[0] != '\0' ) {
						dprintf(D_ALWAYS,"Read: %s%s\n",oldbuf,nlp);
					}
					nlp = &(buf[i+1]);
					oldbuf[0] = '\0';
					done = 1;
					break;
				default :
					done = 0;
			}
		}


		if ( !done ) {
			/* we did not receive the entire line - store the first part */
			buf[ BUFSIZ - 1 ]  = '\0';  /* make certain strcpy does not SEGV */
			strcpy(oldbuf,buf);
		}

	}
}

#if 0
/*
Pseudo system call reporting virtual image size, to be used in calculating
future disk requirements.
*/
condor_image_size(d)
int d;
{
	if( d > ImageSize ) {
		ImageSize = d;
		write_to_parent( SIZE, &ImageSize, sizeof(ImageSize) );
	}
	return 0;
}
#endif

#if 0
/*
**	Make sure that they are not trying to
**	close a reserved descriptor.
*/
condor_close(d)
int d;
{
	if( d == RSC_SOCK || d == CLIENT_LOG ||
					d == PipeToParent || d == LogPipe ) {
		errno = EINVAL;
		return( -1 );
	}

	return( close(d) );
}
#endif

#if 0
/*
**	Make sure that they are not trying to
**	dup to a reserved descriptor.
*/
condor_dup2(oldd, newd)
int oldd, newd;
{
	if( newd == RSC_SOCK || newd == CLIENT_LOG ||
					newd == PipeToParent || newd == LogPipe ) {
		errno = EINVAL;
		return( -1 );
	}

	return( dup2(oldd, newd) );
}
#endif

#if 0
/*
**	We don't really want the shadow to exit here because the 
**	starter may want to make a new checkpoint file.  So, just
**	respond with an error-free return code and hang out; the
**	shadow will (should) go away when (if) the starter closes
**	the LogSock.
*/
/* ARGSUSED */
condor_exit(status)
int status;
{
	return( 0 );
}
#endif

/*
**	getppid normally returns the parents id, right?  Well in
**	the CONDOR case, this job may be started and checkpointed many
**	times.  The parent will change each time.  Instead we will
**	treat it as though the parent process died and this process
**	was inherited by init.
*/
condor_getppid()
{
	return( 1 );
}

/*
**	getpid should return the same number each time.  As indicated
**	above, this CONDOR job may be started and checkpointed several times
**	during its lifetime, we return the condor "process id".
*/
condor_getpid()
{
	extern V2_PROC *Proc;

	return( Proc->id.proc );
}

/*
**	getpgrp should return the same number each time.  As indicated
**	above, this CONDOR job may be started and checkpointed several times
**	during its lifetime, we return the condor "cluster id".
*/
#if defined(AIX31) || defined(AIX32)
condor_getpgrp()
#else
condor_getpgrp( int pid )		/* Ignore it anyway */
#endif
{
	extern V2_PROC *Proc;

	return( Proc->id.cluster );
}

#if defined(AIX31) || defined(AIX32)
/*
**	kgetpgrp should return the same number each time.  As indicated
**	above, this CONDOR job may be started and checkpointed several times
**	during its lifetime, we return the condor "cluster id".  Since we
**	don't handle multi processing, should only be asking about own pgrp.
*/
condor_kgetpgrp( pid )
int		pid;
{
	extern V2_PROC *Proc;

	if( (pid != 0) && (pid != Proc->id.proc) ) {
		errno = EINVAL;
		return -1;
	}
	return( Proc->id.cluster );
}
#endif


#if defined(AIX31) || defined(AIX32)
/*
** Ignore which uid is wanted, just give out the condor client's uid
** in every case.
*/
condor_getuidx( which )
int		which;
{
	return ClientUid;
}
#endif

#if defined(AIX31) || defined(AIX32)
/*
** Ignore which gid is wanted, just give out the condor client's gid
** in every case.
*/
condor_getgidx( which )
int		which;
{
	return ClientGid;
}
#endif
