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
#include <string.h>
#include <errno.h>
#include "condor_event.h"
#include "user_log.c++.h"
#include "condor_string.h"

//--------------------------------------------------------
#include "condor_debug.h"
//--------------------------------------------------------


#define ESCAPE { errorNumber=(errno==EAGAIN) ? ULOG_NO_EVENT : ULOG_UNK_ERROR;\
					 return 0; }

const char * ULogEventNumberNames[] = {
	"ULOG_SUBMIT          ", // Job submitted
	"ULOG_EXECUTE         ", // Job now running
	"ULOG_EXECUTABLE_ERROR", // Error in executable
	"ULOG_CHECKPOINTED    ", // Job was checkpointed
	"ULOG_JOB_EVICTED     ", // Job evicted from machine
	"ULOG_JOB_TERMINATED  ", // Job terminated
	"ULOG_IMAGE_SIZE      ", // Image size of job updated
	"ULOG_SHADOW_EXCEPTION", // Shadow threw an exception
	"ULOG_JOB_SUSPENDED   ", // Job was suspended
	"ULOG_JOB_UNSUSPENDED "  // Job was unsuspended
#if defined(GENERIC_EVENT)
	,"ULOG_GENERIC        "
#endif	    
	,"ULOG_JOB_ABORTED    "  // Job aborted
	,"ULOG_NODE_EXECUTE   "  // Node executing
	,"ULOG_NODE_TERMINATED"  // Node terminated
,
};

const char * ULogEventOutcomeNames[] = {
  "ULOG_OK       ",
  "ULOG_NO_EVENT ",
  "ULOG_RD_ERROR ",
  "ULOG_UNK_ERROR"
};


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

	  case ULOG_JOB_ABORTED:
		return new JobAbortedEvent;

	case ULOG_NODE_EXECUTE:
		return new NodeExecuteEvent;

	case ULOG_NODE_TERMINATED:
		return new NodeTerminatedEvent;

	  default:
        EXCEPT( "Invalid ULogEventNumber" );

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
	submitEventLogNotes = NULL;
	eventNumber = ULOG_SUBMIT;
}

SubmitEvent::
~SubmitEvent()
{
    if( submitEventLogNotes ) {
        delete[] submitEventLogNotes;
    }
}

int SubmitEvent::
writeEvent (FILE *file)
{	
	int retval = fprintf (file, "Job submitted from host: %s\n", submitHost);
	if (retval < 0)
	{
		return 0;
	}
	if( submitEventLogNotes ) {
		retval = fprintf( file, "    %.8191s\n", submitEventLogNotes );
		if( retval < 0 ) {
			return 0;
		}
	}
	return (1);
}

int SubmitEvent::
readEvent (FILE *file)
{
	char s[8192];
	if( submitEventLogNotes ) {
		delete[] submitEventLogNotes;
	}
	int retval = fscanf( file, "Job submitted from host: %s\n", submitHost );
    if (retval != 1)
    {
	return 0;
    }
	if( ! fscanf( file, "    %8191[^\n]\n", s ) ) {
		return 0;
    }
	submitEventLogNotes = strnewp( s );
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
		retval=fprintf(file,"(%d) Job not properly linked for Condor.\n", errType);
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
	retval = fscanf (file, "(%d)", (int*)&errType);
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
	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;

	eventNumber = ULOG_CHECKPOINTED;
}

CheckpointedEvent::
~CheckpointedEvent()
{
}

int CheckpointedEvent::
writeEvent (FILE *file)
{
	if (fprintf (file, "Job was checkpointed.\n") < 0  		||
		(!writeRusage (file, run_remote_rusage)) 			||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n") < 0))
		return 0;

	return 1;
}

int CheckpointedEvent::
readEvent (FILE *file)
{
	int retval = fscanf (file, "Job was checkpointed.");

	char buffer[128];
	if (retval == EOF ||
		!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;

	return 1;
}
		
// ----- the JobEvictedEvent class
JobEvictedEvent::
JobEvictedEvent ()
{
	eventNumber = ULOG_JOB_EVICTED;
	checkpointed = false;

	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;

	sent_bytes = recvd_bytes = 0.0;
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
	

	if (!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;


	if (fscanf (file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) == 0)
		return 1;				// backwards compatibility

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


	if ((retval < 0)										||
		(!writeRusage (file, run_remote_rusage)) 			||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n") < 0))
		return 0;


	if (fprintf (file, "\t%.0f  -  Run Bytes Sent By Job\n", sent_bytes) < 0 ||
		fprintf (file, "\t%.0f  -  Run Bytes Received By Job\n",
				 recvd_bytes) < 0)
		return 1;				// backwards compatibility
	
	return 1;
}


// ----- JobAbortedEvent class
JobAbortedEvent::
JobAbortedEvent ()
{
	eventNumber = ULOG_JOB_ABORTED;
}

JobAbortedEvent::
~JobAbortedEvent()
{
}

int JobAbortedEvent::
writeEvent (FILE *file)
{

	if (fprintf (file, "Job was aborted by the user.\n") < 0) return 0;

	return 1;
}


int JobAbortedEvent::
readEvent (FILE *file)
{
	if (fscanf (file, "Job was aborted by the user.") == EOF)
		return 0;

	return 1;
}


// ----- TerminatedEvent baseclass
TerminatedEvent::TerminatedEvent()
{
	coreFile[0] = '\0';
	returnValue = signalNumber = -1;

	(void)memset((void*)&run_local_rusage,0,(size_t)sizeof(run_local_rusage));
	run_remote_rusage=total_local_rusage=total_remote_rusage=run_local_rusage;

	sent_bytes = recvd_bytes = total_sent_bytes = total_recvd_bytes = 0.0;
}

TerminatedEvent::~TerminatedEvent()
{
}


int
TerminatedEvent::writeEvent( FILE *file, const char* header )
{
	int retval=0;

	if( normal ) {
		if( fprintf(file, "\t(1) Normal termination (return value %d)\n\t", 
					returnValue) < 0 ) {
			return 0;
		}
	} else {
		if( fprintf(file, "\t(0) Abnormal termination (signal %d)\n",
					signalNumber) < 0 ) {
			return 0;
		}
		if( coreFile[0] ) {
			retval = fprintf( file, "\t(1) Corefile in: %s\n\t", coreFile );
		} else {
			retval = fprintf( file, "\t(0) No core file\n\t" );
		}
	}

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


	if (fprintf(file, "\t%.0f  -  Run Bytes Sent By %s\n", 
				sent_bytes, header) < 0 ||
		fprintf(file, "\t%.0f  -  Run Bytes Received By %s\n",
				recvd_bytes, header) < 0 ||
		fprintf(file, "\t%.0f  -  Total Bytes Sent By %s\n",
				total_sent_bytes, header) < 0 ||
		fprintf(file, "\t%.0f  -  Total Bytes Received By %s\n",
				total_recvd_bytes, header) < 0)
		return 1;				// backwards compatibility

	return 1;
}


int
TerminatedEvent::readEvent( FILE *file, const char* header )
{
	char buffer[128];
	int  normalTerm;
	int  gotCore;
	int  retval1, retval2;

	if( (retval2 = fscanf (file, "\n\t(%d) ", &normalTerm)) != 1 ) {
		return 0;
	}

	if( normalTerm ) {
		normal = true;
		if(fscanf(file,"Normal termination (return value %d)",&returnValue)!=1)
			return 0;
	} else {
		normal = false;
		if((fscanf(file,"Abnormal termination (signal %d)",&signalNumber)!=1)||
		   (fscanf(file,"\n\t(%d) ", &gotCore) != 1))
			return 0;

		if( gotCore ) {
			if (fscanf (file, "Corefile in: %s", coreFile) != 1) 
				return 0;
		} else {
			if (fgets (buffer, 128, file) == 0) 
				return 0;
		}
	}

		// read in rusage values
	if (!readRusage(file,run_remote_rusage) || !fgets(buffer, 128, file) ||
		!readRusage(file,run_local_rusage)   || !fgets(buffer, 128, file) ||
		!readRusage(file,total_remote_rusage)|| !fgets(buffer, 128, file) ||
		!readRusage(file,total_local_rusage) || !fgets(buffer, 128, file))
		return 0;
	
	if( fscanf (file, "\t%f  -  Run Bytes Sent By ", &sent_bytes) == 0 ||
		fscanf (file, header) == 0 ||
		fscanf (file, "\n") == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By ",
				&recvd_bytes) == 0 ||
		fscanf (file, header) == 0 || 
		fscanf (file, "\n") == 0 ||
		fscanf (file, "\t%f  -  Total Bytes Sent By ",
				&total_sent_bytes) == 0 ||
		fscanf (file, header) == 0 ||
		fscanf (file, "\n") == 0 ||
		fscanf (file, "\t%f  -  Total Bytes Received By ",
				&total_recvd_bytes) == 0 ||
		fscanf (file, header) == 0 ||
		fscanf (file, "\n") == 0 ) {
		return 1;		// backwards compatibility
	}
	return 1;
}


// ----- JobTerminatedEvent class
JobTerminatedEvent::JobTerminatedEvent() : TerminatedEvent()
{
	eventNumber = ULOG_JOB_TERMINATED;
}


JobTerminatedEvent::~JobTerminatedEvent()
{
}


int
JobTerminatedEvent::writeEvent (FILE *file)
{
	if( fprintf(file, "Job terminated.\n") < 0 ) {
		return 0;
	}
	return TerminatedEvent::writeEvent( file, "Job" );
}


int
JobTerminatedEvent::readEvent (FILE *file)
{
	if( fscanf(file, "Job terminated.") == EOF ) {
		return 0;
	}
	return TerminatedEvent::readEvent( file, "Job" );
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
	message[0] = '\0';
	sent_bytes = recvd_bytes = 0.0;
}

ShadowExceptionEvent::
~ShadowExceptionEvent ()
{
}

int ShadowExceptionEvent::
readEvent (FILE *file)
{
	if (fscanf (file, "Shadow exception!\n\t") == EOF)
		return 0;
	if (fgets(message, BUFSIZ, file) == NULL) {
		message[0] = '\0';
		return 1;				// backwards compatibility
	}

	// remove '\n' from message
	message[strlen(message)-1] = '\0';

	if (fscanf (file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) == 0)
		return 1;				// backwards compatibility

	return 1;
}

int ShadowExceptionEvent::
writeEvent (FILE *file)
{
	if (fprintf (file, "Shadow exception!\n\t") < 0)
		return 0;
	if (fprintf (file, "%s\n", message) < 0)
		return 0;

	if (fprintf (file, "\t%.0f  -  Run Bytes Sent By Job\n", sent_bytes) < 0 ||
		fprintf (file, "\t%.0f  -  Run Bytes Received By Job\n",
				 recvd_bytes) < 0)
		return 1;				// backwards compatibility
	
	return 1;
}

JobSuspendedEvent::
JobSuspendedEvent ()
{
	eventNumber = ULOG_JOB_SUSPENDED;
}

JobSuspendedEvent::
~JobSuspendedEvent ()
{
}

int JobSuspendedEvent::
readEvent (FILE *file)
{
	if (fscanf (file, "Job was suspended.\n\t") == EOF)
		return 0;
	if (fscanf (file, "Number of processes actually suspended: %d\n",
			&num_pids) == EOF)
		return 1;				// backwards compatibility

	return 1;
}


int JobSuspendedEvent::
writeEvent (FILE *file)
{
	if (fprintf (file, "Job was suspended.\n\t") < 0)
		return 0;
	if (fprintf (file, "Number of processes actually suspended: %d\n", 
			num_pids) < 0)
		return 0;

	return 1;
}

JobUnsuspendedEvent::
JobUnsuspendedEvent ()
{
	eventNumber = ULOG_JOB_UNSUSPENDED;
}

JobUnsuspendedEvent::
~JobUnsuspendedEvent ()
{
}

int JobUnsuspendedEvent::
readEvent (FILE *file)
{
	if (fscanf (file, "Job was unsuspended.\n") == EOF)
		return 0;

	return 1;
}


int JobUnsuspendedEvent::
writeEvent (FILE *file)
{
	if (fprintf (file, "Job was unsuspended.\n") < 0)
		return 0;

	return 1;
}

static const int seconds = 1;
static const int minutes = 60 * seconds;
static const int hours = 60 * minutes;
static const int days = 24 * hours;

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


// ----- the NodeExecuteEvent class
NodeExecuteEvent::NodeExecuteEvent()
{	
	executeHost [0] = '\0';
	eventNumber = ULOG_NODE_EXECUTE;
}


NodeExecuteEvent::~NodeExecuteEvent()
{
}


int
NodeExecuteEvent::writeEvent (FILE *file)
{	
	return( fprintf(file, "Node %d executing on host: %s\n",
					node, executeHost) >= 0 );
}


int
NodeExecuteEvent::readEvent (FILE *file)
{
	return( fscanf(file, "Node %d executing on host: %s", 
				   &node, executeHost) != EOF );
}


// ----- NodeTerminatedEvent class
NodeTerminatedEvent::NodeTerminatedEvent() : TerminatedEvent()
{
	eventNumber = ULOG_NODE_TERMINATED;
	node = -1;
}


NodeTerminatedEvent::
~NodeTerminatedEvent()
{
}


int
NodeTerminatedEvent::writeEvent( FILE *file )
{
	if( fprintf(file, "Node %d terminated.\n", node) < 0 ) {
		return 0;
	}
	return TerminatedEvent::writeEvent( file, "Node" );
}


int
NodeTerminatedEvent::readEvent( FILE *file )
{
	if( fscanf(file, "Node %d terminated.", &node) == EOF ) {
		return 0;
	}
	return TerminatedEvent::readEvent( file, "Node" );
}

