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
** Authors:  Anthony B. Dayao
** 	         University of Wisconsin, Computer Sciences Dept.
** Modified for Solaris by : Dhaval N. Shah
**							 University of Wisconsin, Madison
**                           Computer Sciences Department 
** 
*/ 


#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#if 0
#include <rpc/types.h>
#include <rpc/xdr.h>
#endif

#if defined(Solaris)
#include <fcntl.h>
#include "fake_flock.h"
#endif

#include "condor_types.h"
#include "debug.h"
#include "trace.h"
#include "except.h"
#include "sched.h"
#if 0 /* now included in condor_types.h */
#include "expr.h"
#include "proc.h"
#endif
#include "clib.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
extern char *History;

extern int ClientTimeout;				/* timeout value for get_history */

void *calloc( size_t, size_t );

/*
 *		Called when schedd receives a GET_HISTORY command from
 *		condor_get_history.
 *
 *				( get_history )							( schedd )
 *
 *				GET_HISTORY---->
 *													<-------send history records 1 at a time
 *													<-------send end marker
 *				UNLINK_HISTORY_FILE---->
 *													<-------UNLINK_HISTORY_FILE_DONE
 *
 *
 *
 *    WARNING:  there is small probability that the history records
 *              may be duplicated in both local (where schedd is running)
 *					 and remote (where condor_get_history is running) machines.
 *
 */

getHistory(xdrs)
XDR	*xdrs;
{
	int      fd, op;
	XDR      xdr, *H = &xdr, *OpenHistory();

	if( !xdrrec_skiprecord(xdrs) ) {
		dprintf( D_ALWAYS, "Can't xdrrec_skiprecord(xdrs) [1.0]\n" );
		return;
	}
	dprintf( D_ALWAYS, "Received GET_HISTORY from condor_getHistory\n" );

   (void) alarm( (unsigned) ClientTimeout );    /* don't hang here forever */

	/* transmit the contents of history file */
	H = OpenHistory( History, H, &fd );
	(void)LockHistory( H, READER );
	if (SendAllHistoryRecs(xdrs, H) < 0) {
		CloseHistory( H );
		return;
	}
	CloseHistory( H );

	

	/* wait longer bec. client is copying/appending files and may take time */
   (void) alarm( (unsigned) (ClientTimeout * 4));  /* don't hang here forever */

	/* wait for command to unlink history file */
	if( !rcv_int(xdrs, &op, TRUE) ) {
		dprintf( D_ALWAYS,
				"Can't receive UNLINK_HISTORY_FILE msg from condor_getHistory\n" );
		return;
		}
	if (op == UNLINK_HISTORY_FILE) {
		dprintf( D_ALWAYS,
			 "Received UNLINK_HISTORY_FILE msg from condor_getHistory\n");

		EmptyHistoryLog(); /* History log should now contain 0 records */

		(void) alarm( (unsigned) ClientTimeout );    /* don't hang here forever */

		/* inform client that we've unlinked the history file and created */
		/* an empty one */
		if( !snd_int(xdrs, UNLINK_HISTORY_FILE_DONE, TRUE) ) {
			dprintf( D_ALWAYS,
					"Can't send UNLINK_HISTORY_FILE_DONE to condor_getHistory\n" );
			return;
			}
		dprintf(D_ALWAYS, "Sent UNLINK_HISTORY_FILE_DONE to condor_getHistory\n");
   	(void) alarm( (unsigned) 0 );                /* cancel alarm */
		}
	else if (op == DO_NOT_UNLINK_HISTORY_FILE) {
		dprintf( D_ALWAYS,
			 "Received DO_NOT_UNLINK_HISTORY_FILE msg from condor_getHistory\n");
		}
	else {
		dprintf( D_ALWAYS, "Received unrecognized msg from schedd\n");
		}
}

PROC EndMarker;					/* empty PROC data structure */
										/* make sure the version number = PROC_VERSION */

/* UNSURE of what to do on error */
/* EndMarker marks end of history records */

int
SendAllHistoryRecs(xdrs, H)
XDR *xdrs, *H;
{
	PROC  p;
	int      *fd = ((int **)H->x_private)[0];

	if( lseek(*fd,0,L_XTND) < 0 ) {
		return -1;
	}

	if( lseek(*fd,0,L_SET) < 0 ) {
		return -1;
	}

	for(;;) {
   	(void) alarm( (unsigned) ClientTimeout );    /* don't hang here forever */

		if (!rcv_proc( H, &p, TRUE )) {
			break; /* no more records, break to send empty PROC structure */
			}

		if (!snd_proc(xdrs, &p, FALSE )) {
			dprintf( D_ALWAYS, "Can't send job history record to client\n");
			return -1;
			}

		H->x_op = XDR_FREE;
		if( !xdr_proc(H,&p) ) {
			dprintf( D_ALWAYS, "Can't free proc struct\n");
			return -1;
			}
		}

	/*
	 *  Send a blank proc structure to signal last PROC record sent.
	 *  Since this structure has all 0's, the version_num field has
	 *  to be initialized to the correct version #; otherwise
	 *  xdr_proc() will complain!
	 */
	make_null_proc( &EndMarker );
	if (!snd_proc(xdrs, &EndMarker, TRUE )) {
		dprintf( D_ALWAYS, "Can't send EndMarker to client\n");
		return -1;
	}
	dprintf( D_ALWAYS, "Sent EndMarker to condor_getHistory\n" );

	return 0;
}

snd_proc( xdrs, val, end_of_record )
XDR      *xdrs;
PROC     *val;
int      end_of_record;
{
	xdrs->x_op = XDR_ENCODE;
	if( !xdr_proc(xdrs,val) ) {
		return FALSE;
	}

	if( end_of_record ) {
		if( !xdrrec_endofrecord(xdrs,TRUE) ) {
			return FALSE;
		}
	}

	return TRUE;
}

rcv_proc( xdrs, val, end_of_record )
XDR      *xdrs;
PROC      *val;
int      end_of_record;
{
	memset( (char *)val,0, sizeof(PROC) );
	xdrs->x_op = XDR_DECODE;
	if( !xdr_proc(xdrs,val) ) {
		return FALSE;
	}
	if( end_of_record ) {
		if( !xdrrec_skiprecord(xdrs) ) {
			dprintf( D_ALWAYS, "Can't xdrrec_skiprecord(xdrs) [2.1]\n" );
			return FALSE;
		}
	}
	return TRUE;
	}


/*
 *   Now that the new history records have been transmitted,
 *   update the history log file to contain 0 records.
 */

EmptyHistoryLog()
{
   int fd;

	if ((fd=open(History, O_CREAT | O_TRUNC, 0660)) < 0) {
		EXCEPT("open(History, O_CREAT | O_TRUNC, 0660)");
		}

   if (fsync(fd) < 0) {
		EXCEPT("fsync(History)");
		}

   if (close(fd) < 0) {
		EXCEPT("close(History)");
		}
}

static char *EmptyStr = "";
static struct rusage ZeroUsage;
static int ZeroStatus;
static P_DESC ZeroPipe = { 0, 0, "" };

make_null_proc( proc )
PROC  *proc;
{
	int		i;

	proc->version_num = PROC_VERSION;
	proc->n_cmds = 1;

	memset( &proc->local_usage, 0, sizeof(struct rusage) ); /* dhaval 9/25 */
	proc->owner = EmptyStr;
	proc->env = EmptyStr;

	proc->cmd = calloc( proc->n_cmds, sizeof(char *) );
	proc->args = calloc( proc->n_cmds, sizeof(char *) );
	proc->in = calloc( proc->n_cmds, sizeof(char *) );
	proc->out = calloc( proc->n_cmds, sizeof(char *) );
	proc->err = calloc( proc->n_cmds, sizeof(char *) );
	proc->remote_usage =
		(struct rusage *)calloc( proc->n_cmds, sizeof(struct rusage) );
	proc->exit_status = (int *)calloc( proc->n_cmds, sizeof(int) );

	for( i=0; i<proc->n_cmds; i++ ) {
		proc->cmd[i] = EmptyStr;
		proc->args[i] = EmptyStr;
		proc->in[i] = EmptyStr;
		proc->out[i] = EmptyStr;
		proc->err[i] = EmptyStr;
	}

	proc->pipe = calloc( proc->n_pipes, sizeof(P_DESC) );
	for( i=0; i<proc->n_pipes; i++ ) {
		proc->pipe[i] = ZeroPipe;
	}

	proc->rootdir = EmptyStr;
	proc->iwd = EmptyStr;
	proc->requirements = EmptyStr;
	proc->preferences = EmptyStr;
}
