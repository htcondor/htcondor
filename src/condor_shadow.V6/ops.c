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
#include "except.h"
#include "debug.h"
#include "fileno.h"
#include "xdr_lib.h"
#include "clib.h"
#include "shadow.h"

#if defined(LINUX) || defined(Solaris)
#define MAX(a,b)	((a)<(b)?(b):(a))
#endif

FILE	*fdopen();
static XDR_ShadowRead( int *iohandle, char *buf, int len );
static XDR_ShadowWrite( int *iohandle, char *buf, int len );

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

static FILE *LogFP;

extern V2_PROC *Proc;

#define XDR_BUFSIZE	(4 * 1024)

XDR xdr_RSC, *xdr_syscall = &xdr_RSC;

XDR *
RSC_ShadowInit( rscsock, errsock )
{
	RSCSock = rscsock;
	xdrrec_create( xdr_syscall, XDR_BUFSIZE, XDR_BUFSIZE, (caddr_t)&RSCSock,
			(void*)XDR_ShadowRead, (void*)XDR_ShadowWrite );
	
	xdr_syscall->x_op = XDR_ENCODE;

	LogSock = errsock;
	LogFP = fdopen(LogSock, "r");

	if( LogFP == NULL ) {
		EXCEPT("Cannot fdopen LogSock");
	}

	return( xdr_syscall );
}


static
XDR_ShadowRead( int *iohandle, char *buf, int len )
{
	register int cnt;
	fd_set readfds;
	int nfds;
	int	req_sock;

	if( UsePipes ) {
		req_sock = REQ_SOCK;
	} else {
		req_sock = *iohandle;
	}


	nfds = MAX(req_sock, LogSock) + 1;

	for(;;) {

		dprintf(D_XDR,
		"Shadow: XDR_ShadowRead: about to select on <%d, %d, and %d>\n",
				req_sock, LogSock, UMBILICAL);

		
		for(;;) {

			FD_ZERO(&readfds);
			FD_SET(req_sock, &readfds);
			FD_SET(LogSock, &readfds);

			unblock_signal(SIGCHLD);
			unblock_signal(SIGUSR1);
#if defined(LINUX)
			cnt = select(nfds, &readfds, (fd_set *)0, (fd_set *)0,
					(struct timeval *)0);
#else
			cnt = select(nfds, &readfds, 0, 0,
					(struct timeval *)0);
#endif
			block_signal(SIGCHLD);
			block_signal(SIGUSR1);

			if( cnt >= 0 ) {
				break;
			}

			if( errno != EINTR ) {
				EXCEPT("XDR_ShadowRead: select, errno=%d, req_sock=%d, LogSock=%d",errno,req_sock,LogSock);
			}
		}

		if( FD_ISSET(LogSock, &readfds) ) {
			if( HandleLog() < 0 ) {
				EXCEPT( "Peer went away" );
			}
			continue;
		}

		if( FD_ISSET(req_sock, &readfds) ) {
			break;
		}

		if( FD_ISSET(UMBILICAL, &readfds) ) {
			dprintf(D_ALWAYS,
				"Shadow: Local scheduler apparently died, so I die too\n");
			exit(1);
		}
	}

	dprintf(D_XDR, "Shadow: XDR_ShadowRead: about to read(%d, 0x%x, %d)\n",
			req_sock, buf, len );
	
	cnt = read( req_sock, buf, len );

	dprintf(D_XDR, "Shadow: XDR_ShadowRead: cnt = %d\n", cnt );

	return( cnt );
}

static
XDR_ShadowWrite( int *iohandle, char *buf, int len )
{
	register int cnt;
	fd_set readfds;
	fd_set writefds;
	int nfds;
	int	rpl_sock;

	if( UsePipes ) {
		rpl_sock = RPL_SOCK;
	} else {
		rpl_sock = *iohandle;
	}

#if 0

	nfds = MAX(rpl_sock, LogSock) + 1;

	for(;;) {
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_SET(rpl_sock, &writefds);
		FD_SET(LogSock, &readfds);


		dprintf(D_XDR,
		"Shadow: XDR_ShadowWrite: about to select on <%d, %d, and %d>\n",
				rpl_sock, LogSock, UMBILICAL);

		unblock_signal(SIGCHLD);
		unblock_signal(SIGUSR1);
		cnt = select(nfds, (int *)&readfds, (int *)&writefds, (int *)0, 
														(struct timeval *)0 );
		block_signal(SIGCHLD);
		block_signal(SIGUSR1);

		if( cnt < 0 ) {
			if ( errno == EINTR )
				continue;
			else {
				EXCEPT("XDR_ShadowWrite: select");
			}
		}

		if( FD_ISSET(LogSock, &readfds) ) {
			if( HandleLog() < 0 ) {
				EXCEPT( "Peer went away" );
			}
			continue;
		}

		if( FD_ISSET(rpl_sock, &writefds) ) {
			break;
		}

		if( FD_ISSET(UMBILICAL, &readfds) ) {
			dprintf(D_ALWAYS,
				"Shadow: Local scheduler apparently died, so I'm dying too\n");
			exit(1);
		}
	}
#endif

	dprintf(D_XDR, "Shadow: XDR_ShadowWrite: about to write(%d, 0x%x, %d)\n",
			rpl_sock, buf, len );
	
	cnt = write( rpl_sock, buf, len );

	dprintf(D_XDR, "Shadow: XDR_ShadowWrite: cnt = %d\n", cnt );

	ASSERT(cnt == len);

	return( cnt );
}



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
	register char *nlp;
	char buf[ BUFSIZ ];
	char *strchr();

	for(;;) {
		memset(buf,0,sizeof(buf));

		errno = 0;
		if( fgets(buf, sizeof(buf), LogFP) == NULL ) {
			dprintf( D_FULLDEBUG, "errno = %d\n", errno );
			dprintf( D_FULLDEBUG, "buf = '%s'\n", buf);
			return -1;
		}


		nlp = strchr(buf, '\n');
		if( nlp != NULL ) {
			*nlp = '\0';
		}

		if ( buf[0] != '\0' ) {
			dprintf(D_ALWAYS, "Read: %s\n", buf );
		}

		/*
		**	If there is more to read, read it, otherwise just return...
		*/
#if defined(LINUX)
		if( LogFP->_IO_read_ptr == LogFP->_IO_read_end) {
			return 0;
		}
#else
		if( LogFP->_cnt == 0 ) {
			return 0;
		}
#endif
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
condor_getpgrp( pid )		/* Ignore it anyway */
int		pid;
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
