
#include "condor_common.h"
#include <string.h>
#include <errno.h>
#include "condor_event.h"
#include "user_log.c++.h"

#define ESCAPE { errorNumber=(errno==EAGAIN) ? ULOG_NO_EVENT : ULOG_UNK_ERROR;\
					 return 0; }

ULogEvent *
instantiateEvent (ULogEventNumber event)
{
	switch (event)
	{
	  case ULOG_SUBMIT:
		return new SubmitEvent;

	  case ULOG_EXECUTE:
		return new ExecuteEvent;

	  case ULOG_EXECUTABLE_ERROR:
		return new ExecutableErrorEvent;

	  case ULOG_CHECKPOINTED:
		return new CheckpointedEvent;

	  case ULOG_JOB_EVICTED:
		return new JobEvictedEvent;

	  case ULOG_JOB_TERMINATED:
		return new JobTerminatedEvent;

	  case ULOG_IMAGE_SIZE:
		return new JobImageSizeEvent;

	  case ULOG_SHADOW_EXCEPTION:
		return new ShadowExceptionEvent;

#if defined(GENERIC_EVENT)
	case ULOG_GENERIC:
		return new GenericEvent;
#endif

	  default:
		return 0;
	}

    return 0;
}


ULogEvent::
ULogEvent()
{
	struct tm *tm;
	time_t     clock;

	eventNumber = (ULogEventNumber) - 1;
	cluster = proc = subproc = -1;

	(void) time ((time_t *)&clock);
	tm = localtime ((time_t *)&clock);
	eventTime = *tm;
}


ULogEvent::
~ULogEvent ()
{
}


int ULogEvent::
getEvent (FILE *file)
{
	if (!file) return 0;
	
	return (readHeader (file) && readEvent (file));
}


int ULogEvent::
putEvent (FILE *file)
{
	if (!file) return 0;

	return (writeHeader (file) && writeEvent (file));
}

// This function reads in the header of an event from the UserLog and fills
// the fields of the event object.  It does *not* read the event number.  The 
// caller is supposed to read the event number, instantiate the appropriate 
// event type using instantiateEvent(), and then call readEvent() of the 
// returned event.
int ULogEvent::
readHeader (FILE *file)
{
	int retval;
	
	// read from file
	retval = fscanf (file, " (%d.%d.%d) %d/%d %d:%d:%d ", 
					 &cluster, &proc, &subproc,
					 &(eventTime.tm_mon), &(eventTime.tm_mday), 
					 &(eventTime.tm_hour), &(eventTime.tm_min), 
					 &(eventTime.tm_sec));

	// check if all fields were successfully read
	if (retval != 8)
	{
		return 0;
	}

	// recall that tm_mon+1 was written to log; decrement to compensate
	eventTime.tm_mon--;

	return 1;
}


// Write the header for the event to the file
int ULogEvent::
writeHeader (FILE *file)
{
	int       retval;

	// write header
	retval = fprintf (file, "%03d (%03d.%03d.%03d) %02d/%02d %02d:%02d:%02d ",
					  eventNumber, 
					  cluster, proc, subproc,
					  eventTime.tm_mon+1, eventTime.tm_mday, 
					  eventTime.tm_hour, eventTime.tm_min, eventTime.tm_sec);

	// check if all fields were sucessfully written
	if (retval < 0) 
	{
		return 0;
	}

	return 1;
}


// ----- the SubmitEvent class
SubmitEvent::
SubmitEvent()
{	
	submitHost [0] = '\0';
	eventNumber = ULOG_SUBMIT;
}

SubmitEvent::
~SubmitEvent()
{
}

int SubmitEvent::
writeEvent (FILE *file)
{	
	int retval = fprintf (file, "Job submitted from host: %s\n", submitHost);
	if (retval < 0)
	{
		return 0;
	}
	
	return (1);
}

int SubmitEvent::
readEvent (FILE *file)
{
    int retval  = fscanf (file, "Job submitted from host: %s", submitHost);
    if (retval != 1)
    {
	return 0;
    }
    return 1;
}


#if defined(GENERIC_EVENT)
// ----- the GenericEvent class
GenericEvent::
GenericEvent()
{	
	info[0] = '\0';
	eventNumber = ULOG_GENERIC;
}

GenericEvent::
~GenericEvent()
{
}

int GenericEvent::
writeEvent(FILE *file)
{
    int retval = fprintf(file, "%s\n", info);
    if (retval < 0)
    {
	return 0;
    }
    
    return 1;
}

int GenericEvent::
readEvent(FILE *file)
{
    int retval = fscanf(file, "%[^\n]\n", info);
    if (retval < 0)
    {
	return 0;
    }
    return 1;
}
#endif
	

// ----- the ExecuteEvent class
ExecuteEvent::
ExecuteEvent()
{	
	executeHost [0] = '\0';
	eventNumber = ULOG_EXECUTE;
}

ExecuteEvent::
~ExecuteEvent()
{
}


int ExecuteEvent::
writeEvent (FILE *file)
{	
	int retval = fprintf (file, "Job executing on host: %s\n", executeHost);
	if (retval < 0)
	{
		return 0;
	}
	return 1;
}

int ExecuteEvent::
readEvent (FILE *file)
{
	int retval  = fscanf (file, "Job executing on host: %s", executeHost);
	if (retval != 1)
	{
		return 0;
	}
	return 1;
}


// ----- the ExecutableError class
ExecutableErrorEvent::
ExecutableErrorEvent()
{
	errType = (ExecErrorType) -1;
	eventNumber = ULOG_EXECUTABLE_ERROR;
}


ExecutableErrorEvent::
~ExecutableErrorEvent()
{
}

int ExecutableErrorEvent::
writeEvent (FILE *file)
{
	int retval;

	switch (errType)
	{
	  case CONDOR_EVENT_NOT_EXECUTABLE:
		retval = fprintf (file, "(%d) Job file not executable.\n", errType);
		break;

	  case CONDOR_EVENT_BAD_LINK:
		retval=fprintf(file,"(%d) Job not properly linked for V5.\n", errType);
		break;

	  default:
		retval = fprintf (file, "(%d) [Bad error number.]\n", errType);
	}
	if (retval < 0) return 0;

	return 1;
}


int ExecutableErrorEvent::
readEvent (FILE *file)
{
	int  retval;
	char buffer [128];

	// get the error number
	retval = fscanf (file, "(%d)", &errType);
	if (retval != 1) 
	{ 
		return 0;
	}

	// skip over the rest of the line
	if (fgets (buffer, 128, file) == 0)
	{
		return 0;
	}

	return 1;
}


// ----- the CheckpointedEvent class
CheckpointedEvent::
CheckpointedEvent()
{
	eventNumber = ULOG_CHECKPOINTED;
}

CheckpointedEvent::
~CheckpointedEvent()
{
}

int CheckpointedEvent::
writeEvent (FILE *file)
{
#if defined(WIN32)
	if (fprintf (file, "Job was checkpointed.\n") < 0)
		return 0;
#else
	if (fprintf (file, "Job was checkpointed.\n") < 0  		||
		(!writeRusage (file, run_remote_rusage)) 			||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n") < 0))
		return 0;
#endif
	return 1;
}

int CheckpointedEvent::
readEvent (FILE *file)
{
	int retval = fscanf (file, "Job was checkpointed.");
#if !defined(WIN32)
	char buffer[128];
	if (retval == EOF ||
		!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;
#endif

	return 1;
}
		
// ----- the JobEvictedEvent class
JobEvictedEvent::
JobEvictedEvent ()
{
	eventNumber = ULOG_JOB_EVICTED;
	checkpointed = false;
#if !defined(WIN32)
	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;
#endif
}

JobEvictedEvent::
~JobEvictedEvent ()
{
}

int JobEvictedEvent::
readEvent (FILE *file)
{
	int  retval1, retval2;
	int  ckpt;
	char buffer [128];

	if (((retval1 = fscanf (file, "Job was evicted.")) == EOF)  ||
		((retval2 = fscanf (file, "\n\t(%d) ", &ckpt)) != 1))
		return 0;
	checkpointed = (bool) ckpt;
	if (fgets (buffer, 128, file) == 0) return 0;
	
#if !defined(WIN32)
	if (!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;
#endif
	
	return 1;
}

int JobEvictedEvent::
writeEvent (FILE *file)
{
	int retval;

	if (fprintf (file, "Job was evicted.\n\t(%d) ", (int) checkpointed) < 0)
		return 0;

	if (checkpointed)
		retval = fprintf (file, "Job was checkpointed.\n\t");
	else
		retval = fprintf (file, "Job was not checkpointed.\n\t");

#if !defined(WIN32)
	if ((retval < 0)										||
		(!writeRusage (file, run_remote_rusage)) 			||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n") < 0))
		return 0;
#endif

	return 1;
}


// ----- JobTerminatedEvent class
JobTerminatedEvent::
JobTerminatedEvent ()
{
	eventNumber = ULOG_JOB_TERMINATED;
	coreFile[0] = '\0';
	returnValue = signalNumber = -1;
#if !defined(WIN32)
	(void)memset((void*)&run_local_rusage,0,(size_t)sizeof(run_local_rusage));
	run_remote_rusage=total_local_rusage=total_remote_rusage=run_local_rusage;
#endif
}

JobTerminatedEvent::
~JobTerminatedEvent()
{
}

int JobTerminatedEvent::
writeEvent (FILE *file)
{
	int retval;

	if (fprintf (file, "Job terminated.\n") < 0) return 0;
	if (normal)
	{
		if (fprintf (file,"\t(1) Normal termination (return value %d)\n\t", 
						  returnValue) < 0)
			return 0;
	}
	else
	{
		if (fprintf (file,"\t(0) Abnormal termination (signal %d)\n",
						  signalNumber) < 0)
			return 0;

		if (coreFile [0])
			retval = fprintf (file, "\t(1) Corefile in: %s\n\t", coreFile);
		else
			retval = fprintf (file, "\t(0) No core file\n\t");
	}

#if !defined(WIN32)
	if ((retval < 0)										||
		(!writeRusage (file, run_remote_rusage))			||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n\t") < 0)   	||
		(!writeRusage (file, total_remote_rusage))			||
		(fprintf (file, "  -  Total Remote Usage\n\t") < 0)	||
		(!writeRusage (file,  total_local_rusage))			||
		(fprintf (file, "  -  Total Local Usage\n") < 0))
		return 0;
#endif
	
	return 1;
}


int JobTerminatedEvent::
readEvent (FILE *file)
{
	char buffer[128];
	int  normalTerm;
	int  gotCore;
	int  retval, retval1, retval2;

	if ((retval1 = (fscanf (file, "Job terminated.") == EOF)) 	||
		(retval2 = fscanf (file, "\n\t(%d) ", &normalTerm)) != 1)
		return 0;

	if (normalTerm)
	{
		normal = true;
		if (fscanf(file,"Normal termination (return value %d)",&retval)!=1)
			return 0;
	}
	else
	{
		normal = false;
		if((fscanf(file,"Abnormal termination (signal %d)",&signalNumber)!=1)||
		   (fscanf(file,"\n\t(%d) ", &gotCore) != 1))
			return 0;

		if (gotCore)
		{
			if (fscanf (file, "Corefile in: %s", coreFile) != 1) 
				return 0;
		}
		else
		{
			if (fgets (buffer, 128, file) == 0) 
				return 0;
		}
	}

#if !defined(WIN32)
	// read in rusage values
	if (readRusage(file,run_remote_rusage)  && fgets(buffer, 128, file) &&
		readRusage(file,run_local_rusage)   && fgets(buffer, 128, file) &&
		readRusage(file,total_remote_rusage)&& fgets(buffer, 128, file) &&
		readRusage(file,total_local_rusage) && fgets(buffer, 128, file))
		return 1;
#endif
	
	return 0;
}


JobImageSizeEvent::
JobImageSizeEvent()
{
	eventNumber = ULOG_IMAGE_SIZE;
	size = -1;
}


JobImageSizeEvent::
~JobImageSizeEvent()
{
}


int JobImageSizeEvent::
writeEvent (FILE *file)
{
	if (fprintf (file, "Image size of job updated: %d\n", size) < 0)
		return 0;

	return 1;
}


int JobImageSizeEvent::
readEvent (FILE *file)
{
	int retval;
	if ((retval=fscanf(file,"Image size of job updated: %d", &size)) != 1)
		return 0;

	return 1;
}

ShadowExceptionEvent::
ShadowExceptionEvent ()
{
	eventNumber = ULOG_SHADOW_EXCEPTION;
}

ShadowExceptionEvent::
~ShadowExceptionEvent ()
{
}

int ShadowExceptionEvent::
readEvent (FILE *file)
{
	if (fscanf (file, "Shadow exception!") == EOF)
		return 0;

	return 1;
}

int ShadowExceptionEvent::
writeEvent (FILE *file)
{
	if (fprintf (file, "Shadow exception!\n") < 0)
		return 0;

	return 1;
}

static const int seconds = 1;
static const int minutes = 60 * seconds;
static const int hours = 60 * minutes;
static const int days = 24 * hours;

#if !defined(WIN32)
int ULogEvent::
writeRusage (FILE *file, rusage &usage)
{
	int usr_secs = usage.ru_utime.tv_sec;
	int sys_secs = usage.ru_stime.tv_sec;

	int usr_days, usr_hours, usr_minutes;
	int sys_days, sys_hours, sys_minutes;

	usr_days = usr_secs/days;  			usr_secs %= days;
	usr_hours = usr_secs/hours;			usr_secs %= hours;
	usr_minutes = usr_secs/minutes;		usr_secs %= minutes;
 	
	sys_days = sys_secs/days;  			sys_secs %= days;
	sys_hours = sys_secs/hours;			sys_secs %= hours;
	sys_minutes = sys_secs/minutes;		sys_secs %= minutes;
 	
	int retval;
	retval = fprintf (file, "\tUsr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
					  usr_days, usr_hours, usr_minutes, usr_secs,
					  sys_days, sys_hours, sys_minutes, sys_secs);

	return (retval > 0);
}


int ULogEvent::
readRusage (FILE *file, rusage &usage)
{
	int usr_secs, usr_minutes, usr_hours, usr_days;
	int sys_secs, sys_minutes, sys_hours, sys_days;
	int retval;

	retval = fscanf (file, "\tUsr %d %d:%d:%d, Sys %d %d:%d:%d",
					  &usr_days, &usr_hours, &usr_minutes, &usr_secs,
					  &sys_days, &sys_hours, &sys_minutes, &sys_secs);

	if (retval < 8)
	{
		return 0;
	}

	usage.ru_utime.tv_sec = usr_secs + usr_minutes*minutes + usr_hours*hours +
		usr_days*days;

	usage.ru_stime.tv_sec = sys_secs + sys_minutes*minutes + sys_hours*hours +
		sys_days*days;

	return (1);
}
#endif