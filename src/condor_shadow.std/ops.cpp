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


#include "condor_common.h"
#include "condor_debug.h"
#include "fileno.h"
#include "condor_io.h"
#include "structproc.h"

FILE	*fdopen();

static int RSCSock;
static int LogSock;
extern int	PipeToParent;
extern int	LogPipe;
extern int	LockFd;
extern int	ImageSize;
extern int	ClientUid;
extern int	ClientGid;

extern int UsePipes;

extern V2_PROC *Proc;

ReliSock *syscall_sock = NULL;

/* This is a nasty hack that allows the shadow to record into the user log when 
the starter suspends its job. It is such a bad hack because the starter
writes to the CLIENT_LOG socket in the starter after it suspends
the job, and just before it unsuspends the job. This is a bad idea
because the starter should NEVER write to the CLIENT_LOG or RSC_LOG
sockets(only the client should), but someone wanted the functionality
in the old shadow/starter instead of the new one, so here it is. The long
function name denotates my dislike for this feature to be here.
-psilord 2/1/2001 */
extern "C" void log_old_starter_shadow_suspend_event_hack(char *s1, char *s2);

ReliSock *
RSC_ShadowInit( int rscsock, int errsock )
{
	RSCSock = rscsock;
	syscall_sock = new ReliSock();
	syscall_sock->attach_to_file_desc(RSCSock);

	syscall_sock->encode();

	LogSock = errsock;
	// Now set LogSock to be posix-style non-blocking
	fcntl(LogSock,F_SETFL,O_NONBLOCK);

	return( syscall_sock );
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

/*
  This handles log messages coming in on the Log Socket.  A message
  is defined as terminating with a newline or a NULL.  HandleLog is
  called whenever there is something to read on the Log Sock, which
  might not include a full message.  We handle this with a static
  buffer, oldbuf.  We read from the LogSock into buf.  We then walk
  through buf.  Everytime we get to the end of a valid message, we
  print it out.  When we get to the end of the bytes we've read, if we
  haven't just printed out a message, we're left with an incomplete
  message.  We store this into oldbuf to be printed out after the next
  read (or the next time HandleLog is called if there's nothing else
  on the wire yet).   Author: Todd Tannenbaum.  Cleaned up on 12/22/97
  by Derek and Todd.
*/
int
HandleLog()
{
	int nli;			// New Line Index
	char buf[ BUFSIZ ];
	int len,i,done;
	static char oldbuf[ BUFSIZ + 1 ];
	static int first_time = 1;
	if( first_time ) {
		first_time = 0;
		memset( oldbuf, 0, sizeof(oldbuf) );
	}
	memset( buf, 0, sizeof(buf) );

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
		buf[len]='\0';
		nli = 0;
		done = 1;
		for (i=0;i<len;i++) {
			switch ( buf[i] ) {
				case '\n' :
					buf[i] = '\0';
					// NOTICE: no break here, we fall thru....
				case '\0' :
					if ( buf[nli] != '\0' ) {

						/* if this is a suspend/unsuspend event, deal with it */
						log_old_starter_shadow_suspend_event_hack(oldbuf,
							&(buf[nli]));

						/* print the final message into the userlog */
						dprintf(D_ALWAYS,"Read: %s%s\n",oldbuf,&(buf[nli]));
					}
					nli = i+1;
					oldbuf[0] = '\0';
					done = 1;
					break;
				default :
					if( buf[nli] == '\0' ) {
						nli = i;
					}
					done = 0;
			}
		}
		if ( !done ) {
				/* we did not receive the entire line - store the first part */
			memcpy( oldbuf, &buf[nli], len-nli );
			oldbuf[ (len-nli) ] = '\0';
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

/*
**	getppid normally returns the parents id, right?  Well in
**	the CONDOR case, this job may be started and checkpointed many
**	times.  The parent will change each time.  Instead we will
**	treat it as though the parent process died and this process
**	was inherited by init.
*/
int
condor_getppid()
{
	return( 1 );
}

/*
**	getpid should return the same number each time.  As indicated
**	above, this CONDOR job may be started and checkpointed several times
**	during its lifetime, we return the condor "process id".
*/
int
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
int
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
