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
#include "condor_common.h"

#if defined(AIX32)
#	include <sys/id.h>
#   include <sys/wait.h>
#   include <sys/m_wait.h>
#	include "condor_fdset.h"
#endif

#include "user_log.c++.h"
#include "proc.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_io.h"

extern UserLog		ULog;

extern int JobStatus;
extern "C" PROC  *Proc;
extern ClassAd *JobAd;

extern ReliSock *syscall_sock;

// count of network bytes send and received for this job so far
float	TotalBytesSent = 0.0, TotalBytesRecvd = 0.0;

// count of network bytes send and received outside of CEDAR RSC socket
float	BytesSent = 0.0, BytesRecvd = 0.0;

extern "C" void 
initializeUserLog ()
{
	char tmp[_POSIX_PATH_MAX], logfilename[_POSIX_PATH_MAX];
	if (JobAd->LookupString(ATTR_ULOG_FILE, tmp) == 1) {
		if (tmp[0] == '/') {
			strcpy(logfilename, tmp);
		} else {
			sprintf(logfilename, "%s/%s", Proc->iwd, tmp);
		}
		ULog.initialize (Proc->owner, logfilename,
						 Proc->id.cluster, Proc->id.proc, 0);
		dprintf(D_FULLDEBUG, "%s = %s\n", ATTR_ULOG_FILE, logfilename);
	} else {
		dprintf(D_FULLDEBUG, "no %s found\n", ATTR_ULOG_FILE);
	}
}

extern "C" void
log_termination (struct rusage *localr, struct rusage *remoter)
{
	switch (WTERMSIG(JobStatus))
	{
	  case 0:
	  case -1: 
		// if core, bad exectuable --- otherwise, a normal exit
		if (WCOREDUMP(JobStatus) && WEXITSTATUS(JobStatus) == ENOEXEC)
		{
			// log the ULOG_EXECUTABLE_ERROR event
			ExecutableErrorEvent event;
			event.errType = CONDOR_EVENT_NOT_EXECUTABLE;
			if (!ULog.writeEvent (&event))
			{
				dprintf (D_ALWAYS, "Unable to log NOT_EXECUTABLE event\n");
			}
		}
		else
		if (WCOREDUMP(JobStatus) && WEXITSTATUS(JobStatus) == 0)
		{		
			// log the ULOG_EXECUTABLE_ERROR event
			ExecutableErrorEvent event;
			event.errType = CONDOR_EVENT_BAD_LINK;
			if (!ULog.writeEvent (&event))
			{
				dprintf (D_ALWAYS, "Unable to log BAD_LINK event\n");
			}
		}
		else
		{
			// log the ULOG_JOB_TERMINATED event
			JobTerminatedEvent event;
			event.normal = true; // normal termination
			event.returnValue = WEXITSTATUS(JobStatus);
			event.total_local_rusage = Proc->local_usage;
			event.total_remote_rusage = Proc->remote_usage[0];
			event.run_local_rusage = *localr;
			event.run_remote_rusage = *remoter;
			// we want to log the events from the perspective of the
			// user job, so if the shadow *sent* the bytes, then that
			// means the user job *received* the bytes
			event.recvd_bytes = BytesSent;
			event.sent_bytes = BytesRecvd;
			if (syscall_sock) {
				event.recvd_bytes += syscall_sock->get_bytes_sent();
				event.sent_bytes += syscall_sock->get_bytes_recvd();
			}
			event.total_recvd_bytes = TotalBytesSent;
			event.total_sent_bytes = TotalBytesRecvd;
			if (!ULog.writeEvent (&event))
			{
				dprintf (D_ALWAYS,"Unable to log ULOG_JOB_TERMINATED event\n");
			}
		}
		break;


	  case SIGKILL:
		// evicted without a checkpoint
		{
			JobEvictedEvent event;
			event.checkpointed = false;
			event.run_local_rusage = *localr;
			event.run_remote_rusage = *remoter;
			// we want to log the events from the perspective of the
			// user job, so if the shadow *sent* the bytes, then that
			// means the user job *received* the bytes
			event.recvd_bytes = BytesSent;
			event.sent_bytes = BytesRecvd;
			if (syscall_sock) {
				event.recvd_bytes += syscall_sock->get_bytes_sent();
				event.sent_bytes += syscall_sock->get_bytes_recvd();
			}
			if (!ULog.writeEvent (&event))
			{
				dprintf (D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n");
			}
		}
		break;

			
	  case SIGQUIT:
		// evicted, but *with* a checkpoint
		{
			JobEvictedEvent event;
			event.checkpointed = true;
			event.run_local_rusage = *localr;
			event.run_remote_rusage = *remoter;
			// we want to log the events from the perspective of the
			// user job, so if the shadow *sent* the bytes, then that
			// means the user job *received* the bytes
			event.recvd_bytes = BytesSent;
			event.sent_bytes = BytesRecvd;
			if (syscall_sock) {
				event.recvd_bytes += syscall_sock->get_bytes_sent();
				event.sent_bytes += syscall_sock->get_bytes_recvd();
			}
			if (!ULog.writeEvent (&event))
			{
				dprintf (D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n");
			}
		}
		break;


	  default:
		// abnormal termination
		{
			char coredir[_POSIX_PATH_MAX];
			JobTerminatedEvent event;

#if defined(Solaris) || defined(HPUX)
			getcwd(coredir,_POSIX_PATH_MAX);
#else	
			getwd( coredir );
#endif

			event.normal = false;
			event.signalNumber = WTERMSIG(JobStatus);
			if (WCOREDUMP(JobStatus))
			{
				if (strcmp (Proc->rootdir, "/") == 0)
				{
					sprintf (event.coreFile, "%s/core.%d.%d", coredir, 
							 Proc->id.cluster, Proc->id.proc);
				}
				else
				{
					sprintf (event.coreFile, "%s%s/core.%d.%d", Proc->rootdir,
							 coredir, Proc->id.cluster, Proc->id.proc);
				}
			}
			else
			{
				// no core file
				event.coreFile[0] = '\0';
			}
			event.run_local_rusage = *localr;
			event.run_remote_rusage = *remoter;
			event.total_local_rusage = Proc->local_usage;
			event.total_remote_rusage = Proc->remote_usage[0];
			// we want to log the events from the perspective of the
			// user job, so if the shadow *sent* the bytes, then that
			// means the user job *received* the bytes
			event.recvd_bytes = BytesSent;
			event.sent_bytes = BytesRecvd;
			if (syscall_sock) {
				event.recvd_bytes += syscall_sock->get_bytes_sent();
				event.sent_bytes += syscall_sock->get_bytes_recvd();
			}
			if (!ULog.writeEvent (&event))
			{
				dprintf (D_ALWAYS,"Unable to log ULOG_JOB_TERMINATED event\n");
			}
		}
	}
}

extern "C" void 
log_checkpoint (struct rusage *localr, struct rusage *remoter)
{
	CheckpointedEvent event;
	event.run_local_rusage = *localr;
	event.run_remote_rusage = *remoter;
	if (!ULog.writeEvent (&event))
	{	
		dprintf (D_ALWAYS, "Could not log ULOG_CHECKPOINTED event\n");
	}
}



extern "C" void 
log_image_size (int size)
{
	// log the event
	JobImageSizeEvent event;
	event.size = size;
	if (!ULog.writeEvent (&event))
	{
		dprintf (D_ALWAYS, "Unable to log ULOG_IMAGE_SIZE event\n");
	}
}


extern "C" void
log_execute (char *host)
{
	// log execute event
	ExecuteEvent event;
	strcpy (event.executeHost, host);
	if (!ULog.writeEvent (&event))
	{
		dprintf (D_ALWAYS, "Unable to log ULOG_EXECUTE event\n");
	}
}
	
extern "C" void
log_except (char *msg)
{
	// log shadow exception event
	ShadowExceptionEvent event;
	sprintf(event.message, msg);
	// we want to log the events from the perspective of the
	// user job, so if the shadow *sent* the bytes, then that
	// means the user job *received* the bytes

	event.recvd_bytes = BytesSent;
	event.sent_bytes = BytesRecvd;
	if (syscall_sock) {
		event.recvd_bytes += syscall_sock->get_bytes_sent();
		event.sent_bytes += syscall_sock->get_bytes_recvd();
	}

	if (!ULog.writeEvent (&event))
	{
		dprintf (D_ALWAYS, "Unable to log ULOG_SHADOW_EXCEPTION event\n");
	}
}
