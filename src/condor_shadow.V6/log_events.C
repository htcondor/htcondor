/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_qmgr.h"

#if !defined( WCOREDUMP )
#define  WCOREDUMP(stat)      ((stat)&WCOREFLG)
#endif
extern UserLog		ULog;

extern int JobStatus;
extern "C" PROC  *Proc;
extern ClassAd *JobAd;

extern ReliSock *syscall_sock;

static int WroteExecuteEvent = 0;
extern char* ExecutingHost;

// count of total network bytes previously send and received for this
// job from previous runs (i.e., includes ckpt transfers)
float	TotalBytesSent = 0.0, TotalBytesRecvd = 0.0;

// count of network bytes send and received outside of CEDAR RSC
// socket during this run
float	BytesSent = 0.0, BytesRecvd = 0.0;

/* a stupid hack to amend the in memory job ad with statistics about job
	un/suspensions */
void record_suspension_hack(unsigned int action);

extern "C" void
log_execute (char *host)
{
	if( WroteExecuteEvent ) {
		return;
	}
	// log execute event
	ExecuteEvent event;
	strcpy (event.executeHost, host);
	if( !ULog.writeEvent(&event) ) {
		dprintf (D_ALWAYS, "Unable to log ULOG_EXECUTE event\n");
	} else {
		WroteExecuteEvent = 1;
	}
}
	

extern "C" void
check_execute_event( void )
{
	if( ! WroteExecuteEvent ) {
		log_execute( ExecutingHost );
	}
}


extern "C" void 
initializeUserLog ()
{
	char tmp[_POSIX_PATH_MAX], logfilename[_POSIX_PATH_MAX];
	int use_xml;
	if (JobAd->LookupString(ATTR_ULOG_FILE, tmp) == 1) {
		if (tmp[0] == '/') {
			strcpy(logfilename, tmp);
		} else {
			sprintf(logfilename, "%s/%s", Proc->iwd, tmp);
		}
		if (!ULog.initialize (Proc->owner, NULL, logfilename,
							  Proc->id.cluster, Proc->id.proc, 0)) {
			EXCEPT("Failed to initialize user log!\n");
		}
		if (JobAd->LookupBool(ATTR_ULOG_USE_XML, use_xml)
			&& use_xml) {
			ULog.setUseXML(true);
		} else {
			ULog.setUseXML(false);
		}
		dprintf(D_FULLDEBUG, "%s = %s\n", ATTR_ULOG_FILE, logfilename);
	} else {
		dprintf(D_FULLDEBUG, "no %s found\n", ATTR_ULOG_FILE);
	}
}

extern "C" void
log_termination (struct rusage *localr, struct rusage *remoter)
{
	check_execute_event();

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
			event.total_recvd_bytes = TotalBytesSent + event.recvd_bytes;
			event.total_sent_bytes = TotalBytesRecvd + event.sent_bytes;
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
				char coreFile[_POSIX_PATH_MAX];
				if (strcmp (Proc->rootdir, "/") == 0)
				{
					sprintf( coreFile, "%s/core.%d.%d", coredir, 
							 Proc->id.cluster, Proc->id.proc );
				}
				else
				{
					sprintf( coreFile, "%s%s/core.%d.%d", Proc->rootdir,
							 coredir, Proc->id.cluster, Proc->id.proc );
				}
				event.setCoreFile( coreFile );
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
	check_execute_event();

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
	check_execute_event();

	// log the event
	JobImageSizeEvent event;
	event.size = size;
	if (!ULog.writeEvent (&event))
	{
		dprintf (D_ALWAYS, "Unable to log ULOG_IMAGE_SIZE event\n");
	}
}


extern "C" void
log_except (char *msg)
{
	check_execute_event();

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

extern "C" void
log_old_starter_shadow_suspend_event_hack (char *s1, char *s2)
{
	char *magic_suspend = "TISABH Starter: Suspended user job: ";
	char *magic_unsuspend = "TISABH Starter: Unsuspended user job.";

	/* This should be bug enough to hold the two string params */
	char buffer[BUFSIZ * 2 + 2];

	int size_suspend, size_unsuspend;

	size_suspend = strlen(magic_suspend);
	size_unsuspend = strlen(magic_unsuspend);
	sprintf(buffer, "%s%s", s1, s2);

	/* depending on if it is a suspend or unsuspend event, do something
		about it. */

	if (strncmp(buffer, magic_suspend, size_suspend) == 0)
	{
		/* matched a suspend event */
		JobSuspendedEvent event;
		sscanf(buffer,"TISABH Starter: Suspended user job: %d",&event.num_pids);

		if (!ULog.writeEvent (&event))
		{
			dprintf (D_ALWAYS, "Unable to log ULOG_JOB_SUSPENDED event\n");
		}

		record_suspension_hack(ULOG_JOB_SUSPENDED);
		return;
	}

	if (strncmp(buffer, magic_unsuspend, size_unsuspend) == 0)
	{
		/* matched an unsuspend event */

		JobUnsuspendedEvent event;

		if (!ULog.writeEvent (&event))
		{
			dprintf (D_ALWAYS, "Unable to log ULOG_JOB_UNSUSPENDED event\n");
		}
		record_suspension_hack(ULOG_JOB_UNSUSPENDED);
		return;
	}

	/* otherwise, do nothing */
}

/* Mess up the in memory job ad with interesting statistics about suspensions */
void record_suspension_hack(unsigned int action)
{
	char tmp[256];
	int total_suspensions;
	int last_suspension_time;
	int cumulative_suspension_time;
	char *rt = NULL;
	extern char *schedd;

	if (!JobAd)
	{
		EXCEPT("Suspension code: Non-existant JobAd\n");
	}

	switch(action)
	{
		case ULOG_JOB_SUSPENDED:
			/* Add to ad number of suspensions */
			JobAd->LookupInteger(ATTR_TOTAL_SUSPENSIONS, total_suspensions);
			total_suspensions++;
			sprintf(tmp, "%s = %d", ATTR_TOTAL_SUSPENSIONS, total_suspensions);
			JobAd->Insert(tmp);

			/* Add to ad the current suspension time */
			last_suspension_time = time(NULL);
			sprintf(tmp, "%s = %d", ATTR_LAST_SUSPENSION_TIME, 
				last_suspension_time);
			JobAd->Insert(tmp);
			break;
		case ULOG_JOB_UNSUSPENDED:
			/* add in the time I spent suspended to a running total */
			JobAd->LookupInteger(ATTR_CUMULATIVE_SUSPENSION_TIME,
				cumulative_suspension_time);
			JobAd->LookupInteger(ATTR_LAST_SUSPENSION_TIME,
				last_suspension_time);
			cumulative_suspension_time += time(NULL) - last_suspension_time;
			sprintf(tmp, "%s = %d", ATTR_CUMULATIVE_SUSPENSION_TIME,
				cumulative_suspension_time);
			JobAd->Insert(tmp);

			/* set the current suspension time to zero, meaning not suspended */
			last_suspension_time = 0;
			sprintf(tmp, "%s = %d", ATTR_LAST_SUSPENSION_TIME, 
				last_suspension_time);
			JobAd->Insert(tmp);
			break;
		default:
			EXCEPT("record_suspension_hack(): Action event not recognized.\n");
			break;
	}

	/* Sanity output */
	JobAd->LookupInteger(ATTR_TOTAL_SUSPENSIONS, total_suspensions);
	dprintf(D_FULLDEBUG,"%s = %d\n", ATTR_TOTAL_SUSPENSIONS, total_suspensions);
	JobAd->LookupInteger(ATTR_LAST_SUSPENSION_TIME, last_suspension_time);
	dprintf(D_FULLDEBUG, "%s = %d\n", ATTR_LAST_SUSPENSION_TIME,
		last_suspension_time);
	JobAd->LookupInteger(ATTR_CUMULATIVE_SUSPENSION_TIME, 
		cumulative_suspension_time);
	dprintf(D_FULLDEBUG, "%s = %d\n", ATTR_CUMULATIVE_SUSPENSION_TIME,
		cumulative_suspension_time);
	
	/* If we've been asked to perform real time updates of the suspension
		information, then connect to the queue and do it here. */
	rt = param("REAL_TIME_JOB_SUSPEND_UPDATES");
	if (rt != NULL)
	{
		if (strcasecmp(rt, "true") == MATCH)
		{
			dprintf( D_ALWAYS, "Updating suspension info to schedd.\n" );
			if (!ConnectQ(schedd, SHADOW_QMGMT_TIMEOUT)) {
				/* Since these attributes aren't updated periodically, if
					the schedd is busy and a resume event update is lost,
					the the job will be marked suspended when it really isn't.
					The new shadow eventually corrects this via a periodic
					update of various calssad attributes, but I 
					suspect it won't be corrected in the event of a
					bad connect here for this shadow. */
				dprintf( D_ALWAYS, 
					"Timeout connecting to schedd. Suspension update lost.\n");
				free(rt);
				rt = NULL;
				return;
			}

        	SetAttributeInt(Proc->id.cluster, Proc->id.proc, 
	            ATTR_TOTAL_SUSPENSIONS, total_suspensions);
        	SetAttributeInt(Proc->id.cluster, Proc->id.proc, 
	            ATTR_CUMULATIVE_SUSPENSION_TIME, cumulative_suspension_time);
        	SetAttributeInt(Proc->id.cluster, Proc->id.proc, 
	            ATTR_LAST_SUSPENSION_TIME, last_suspension_time);

			DisconnectQ(NULL);
		}

		free(rt);
		rt = NULL;
	}
}
