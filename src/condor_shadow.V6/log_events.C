#include "condor_common.h"

#if defined(AIX32)
#	include <sys/id.h>
#   include <sys/wait.h>
#   include <sys/m_wait.h>
#	include "condor_fdset.h"
#endif

#if defined(HPUX9)
#	include <sys/wait.h>
#else
#	include "condor_fix_wait.h"
#endif

#include <sys/signal.h>
#include "user_log.c++.h"
#include "proc.h"
#include "_condor_fix_resource.h"
#include "condor_debug.h"

extern UserLog		ULog;

extern "C" union wait JobStatus;
extern "C" PROC  *Proc;

extern "C" void 
initializeUserLog (PROC *p)
{
	ULog.initialize (p);
}

extern "C" void
log_termination (struct rusage *localr, struct rusage *remoter)
{
	switch (JobStatus.w_termsig)
	{
	  case 0:
		// if core, bad exectuable --- otherwise, a normal exit
		if (JobStatus.w_coredump && JobStatus.w_retcode == ENOEXEC)
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
		if (JobStatus.w_coredump && JobStatus.w_retcode == 0)
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
			event.returnValue = JobStatus.w_retcode;
			event.total_local_rusage = Proc->local_usage;
			event.total_remote_rusage = Proc->remote_usage[0];
			event.run_local_rusage = *localr;
			event.run_remote_rusage = *remoter;
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

#if defined(Solaris) || defined(HPUX9)
			getcwd(coredir,_POSIX_PATH_MAX);
#else	
			getwd( coredir );
#endif

			event.normal = false;
			event.signalNumber = JobStatus.w_termsig;
			if (JobStatus.w_coredump)
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
log_except (void)
{
	// log shadow exception event
	ShadowExceptionEvent event;
	if (!ULog.writeEvent (&event))
	{
		dprintf (D_ALWAYS, "Unable to log ULOG_SHADOW_EXCEPTION event\n");
	}
}
