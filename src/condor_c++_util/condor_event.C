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
	"ULOG_SUBMIT",					// Job submitted
	"ULOG_EXECUTE",					// Job now running
	"ULOG_EXECUTABLE_ERROR",		// Error in executable
	"ULOG_CHECKPOINTED",			// Job was checkpointed
	"ULOG_JOB_EVICTED",				// Job evicted from machine
	"ULOG_JOB_TERMINATED",			// Job terminated
	"ULOG_IMAGE_SIZE",				// Image size of job updated
	"ULOG_SHADOW_EXCEPTION",		// Shadow threw an exception
	"ULOG_GENERIC",
	"ULOG_JOB_ABORTED",  			// Job aborted
	"ULOG_JOB_SUSPENDED",			// Job was suspended
	"ULOG_JOB_UNSUSPENDED",			// Job was unsuspended
	"ULOG_JOB_HELD",  				// Job was held
	"ULOG_JOB_RELEASED",  			// Job was released
	"ULOG_NODE_EXECUTE",  			// MPI Node executing
	"ULOG_NODE_TERMINATED",  		// MPI Node terminated
	"ULOG_POST_SCRIPT_TERMINATED",	// POST script terminated
	"ULOG_GLOBUS_SUBMIT",			// Job Submitted to Globus
	"ULOG_GLOBUS_SUBMIT_FAILED",	// Globus Submit Failed 
	"ULOG_GLOBUS_RESOURCE_UP",		// Globus Machine UP 
	"ULOG_GLOBUS_RESOURCE_DOWN",	// Globus Machine Down
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

	case ULOG_GENERIC:
		return new GenericEvent;

	  case ULOG_JOB_ABORTED:
		return new JobAbortedEvent;

	  case ULOG_JOB_SUSPENDED:
		return new JobSuspendedEvent;

	  case ULOG_JOB_UNSUSPENDED:
		return new JobUnsuspendedEvent;

	  case ULOG_JOB_HELD:
		return new JobHeldEvent;

	  case ULOG_JOB_RELEASED:
		return new JobReleasedEvent;

	case ULOG_NODE_EXECUTE:
		return new NodeExecuteEvent;

	case ULOG_NODE_TERMINATED:
		return new NodeTerminatedEvent;

	case ULOG_POST_SCRIPT_TERMINATED:
		return new PostScriptTerminatedEvent;

	case ULOG_GLOBUS_SUBMIT:
		return new GlobusSubmitEvent;

	case ULOG_GLOBUS_SUBMIT_FAILED:
		return new GlobusSubmitFailedEvent;

	case ULOG_GLOBUS_RESOURCE_DOWN:
		return new GlobusResourceDownEvent;

	case ULOG_GLOBUS_RESOURCE_UP:
		return new GlobusResourceUpEvent;

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
	s[0] = '\0';
	if( submitEventLogNotes ) {
		delete[] submitEventLogNotes;
	}
	if( fscanf( file, "Job submitted from host: %s\n", submitHost ) != 1 ) {
		return 0;
	}

	// see if the next line contains an optional event notes string,
	// and, if not, rewind, because that means we slurped in the next
	// event delimiter looking for it...
 
	fpos_t filep;
	fgetpos( file, &filep );
     
	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}
 
	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';

	submitEventLogNotes = strnewp( s );
	return 1;
}

// ----- the GlobusSubmitEvent class
GlobusSubmitEvent::
GlobusSubmitEvent()
{	
	eventNumber = ULOG_GLOBUS_SUBMIT;
	rmContact = NULL;
	jmContact = NULL;
	restartableJM = false;
}

GlobusSubmitEvent::
~GlobusSubmitEvent()
{
    if( rmContact ) {
        delete[] rmContact;
    }
    if( jmContact ) {
        delete[] jmContact;
    }
}

int GlobusSubmitEvent::
writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;
	const char * jm = unknown;

	int retval = fprintf (file, "Job submitted to Globus\n");
	if (retval < 0)
	{
		return 0;
	}
	
	if ( rmContact ) rm = rmContact;
	if ( jmContact ) jm = jmContact;

	retval = fprintf( file, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return 0;
	}

	retval = fprintf( file, "    JM-Contact: %.8191s\n", jm );
	if( retval < 0 ) {
		return 0;
	}

	int newjm = 0;
	if ( restartableJM ) { 
		newjm = 1;
	}
	retval = fprintf( file, "    Can-Restart-JM: %d\n", newjm );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int GlobusSubmitEvent::
readEvent (FILE *file)
{
	char s[8192];

	if ( rmContact ) {
		delete [] rmContact;
		rmContact = NULL;
	} 
	if ( jmContact ) {
		delete [] jmContact;
		jmContact = NULL;
	}
	int retval = fscanf (file, "Job submitted to Globus\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    RM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	rmContact = strnewp(s);
	retval = fscanf( file, "    JM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	jmContact = strnewp(s);
	
	int newjm = 0;
	retval = fscanf( file, "    Can-Restart-JM: %d\n", &newjm );
	if ( retval != 1 )
	{
		return 0;
	}
	if ( newjm ) {
		restartableJM = true;
	} else {
		restartableJM = false;
	}
    
	
	return 1;
}

// ----- the GlobusSubmitFailedEvent class
GlobusSubmitFailedEvent::
GlobusSubmitFailedEvent()
{	
	eventNumber = ULOG_GLOBUS_SUBMIT_FAILED;
	reason = NULL;
}

GlobusSubmitFailedEvent::
~GlobusSubmitFailedEvent()
{
    if( reason ) {
        delete [] reason;
    }
}

int GlobusSubmitFailedEvent::
writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * reasonString = unknown;

	int retval = fprintf (file, "Globus job submission failed!\n");
	if (retval < 0)
	{
		return 0;
	}
	
	if ( reason ) reasonString = reason;

	retval = fprintf( file, "    Reason: %.8191s\n", reasonString );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int GlobusSubmitFailedEvent::
readEvent (FILE *file)
{
	char s[8192];

	if ( reason ) {
		delete [] reason;
		reason = NULL;
	} 
	int retval = fscanf (file, "Globus job submission failed!\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';

	fpos_t filep;
	fgetpos( file, &filep );
     
	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}
 
	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';

	// Copy after the "Reason: "
	reason = strnewp( &s[8] );
	return 1;
}

// ----- the GlobusResourceUp class
GlobusResourceUpEvent::
GlobusResourceUpEvent()
{	
	eventNumber = ULOG_GLOBUS_RESOURCE_UP;
	rmContact = NULL;
}

GlobusResourceUpEvent::
~GlobusResourceUpEvent()
{
    if( rmContact ) {
        delete[] rmContact;
    }
}

int GlobusResourceUpEvent::
writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;

	int retval = fprintf (file, "Globus Resource Back Up\n");
	if (retval < 0)
	{
		return 0;
	}
	
	if ( rmContact ) rm = rmContact;

	retval = fprintf( file, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int GlobusResourceUpEvent::
readEvent (FILE *file)
{
	char s[8192];

	if ( rmContact ) {
		delete [] rmContact;
		rmContact = NULL;
	} 
	int retval = fscanf (file, "Globus Resource Back Up\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    RM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	rmContact = strnewp(s);	
	return 1;
}

// ----- the GlobusResourceUp class
GlobusResourceDownEvent::
GlobusResourceDownEvent()
{	
	eventNumber = ULOG_GLOBUS_RESOURCE_DOWN;
	rmContact = NULL;
}

GlobusResourceDownEvent::
~GlobusResourceDownEvent()
{
    if( rmContact ) {
        delete[] rmContact;
    }
}

int GlobusResourceDownEvent::
writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;

	int retval = fprintf (file, "Detected Down Globus Resource\n");
	if (retval < 0)
	{
		return 0;
	}
	
	if ( rmContact ) rm = rmContact;

	retval = fprintf( file, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int GlobusResourceDownEvent::
readEvent (FILE *file)
{
	char s[8192];

	if ( rmContact ) {
		delete [] rmContact;
		rmContact = NULL;
	} 
	int retval = fscanf (file, "Detected Down Globus Resource\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    RM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	rmContact = strnewp(s);	
	return 1;
}

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
JobEvictedEvent::JobEvictedEvent()
{
	eventNumber = ULOG_JOB_EVICTED;
	checkpointed = false;

	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;

	sent_bytes = recvd_bytes = 0.0;

	terminate_and_requeued = false;
	normal = false;
	return_value = -1;
	signal_number = -1;
	reason = NULL;
	core_file = NULL;
}


JobEvictedEvent::~JobEvictedEvent()
{
	if( reason ) {
		delete [] reason;
	}
	if( core_file ) {
		delete [] core_file;
		core_file = NULL;
	}
}


void
JobEvictedEvent::setReason( const char* reason_str )
{
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	if( reason_str ) {
		reason = strnewp( reason_str );
	}
}


const char*
JobEvictedEvent::getReason( void )
{
	return reason;
}


void
JobEvictedEvent::setCoreFile( const char* core_name )
{
	if( core_file ) {
		delete [] core_file;
		core_file = NULL;
	}
	if( core_name ) {
		core_file = strnewp( core_name );
	}
}


const char*
JobEvictedEvent::getCoreFile( void )
{
	return core_file;
}


int
JobEvictedEvent::readEvent( FILE *file )
{
	int  ckpt;
	char buffer [128];

	if( (fscanf(file, "Job was evicted.") == EOF) ||
		(fscanf(file, "\n\t(%d) ", &ckpt) != 1) )
	{
		return 0;
	}
	checkpointed = (bool) ckpt;
	if( fgets(buffer, 128, file) == 0 ) {
		return 0;
	}

		/* 
		   since the old parsing code treated the integer we read as a
		   bool (only to decide between checkpointed or not), we now
		   have to parse the string we just read to figure out if this
		   was a terminate_and_requeued eviction or not.
		*/
	if( ! strncmp(buffer, "Job terminated and was requeued", 31) ) {
		terminate_and_requeued = true;
	} else {
		terminate_and_requeued = false;
	}

	if( !readRusage(file,run_remote_rusage) || !fgets(buffer,128,file) ||
		!readRusage(file,run_local_rusage) || !fgets(buffer,128,file) )
	{
		return 0;
	}

	if( !fscanf(file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) ||
		!fscanf(file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) )
	{
		return 1;				// backwards compatibility
	}

	if( ! terminate_and_requeued ) {
			// nothing more to read
		return 1;
	}

		// now, parse the terminate and requeue specific stuff.

	int  normal_term;
	int  got_core;

	if( fscanf(file, "\n\t(%d) ", &normal_term) != 1 ) {
		return 0;
	}
	if( normal_term ) {
		normal = true;
		if( fscanf(file, "Normal termination (return value %d)\n",
				   &return_value) !=1 ) {
			return 0;
		}
	} else {
		normal = false;
		if( fscanf(file, "Abnormal termination (signal %d)",
				   &signal_number) !=1 ) {
			return 0;
		}
		if( fscanf(file, "\n\t(%d) ", &got_core) != 1 ) {
			return 0;
		}
		if( got_core ) {
			if( fscanf(file, "Corefile in: ") == EOF ) {
				return 0;
			}
			if( !fgets(buffer, 128, file) ) {
				return 0;
			}
			chomp( buffer );
			setCoreFile( buffer );
		} else {
			if( !fgets(buffer, 128, file) ) {
				return 0;
			}
		}
	}
		// finally, see if there's a reason.  this is optional.
	if( !fgets(buffer, 128, file) ) {
		return 1;  // not considered failure
	}
	chomp( buffer );
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( buffer[0] == '\t' && buffer[1] ) {
		setReason( &buffer[1] );
	} else {
		setReason( buffer );
	}
	return 1;
}


int
JobEvictedEvent::writeEvent( FILE *file )
{
	int retval;

	if( fprintf(file, "Job was evicted.\n\t") < 0 ) { 
		return 0;
	}

	if( terminate_and_requeued ) { 
		retval = fprintf( file, "(0) Job terminated and was requeued\n\t" );
	} else if( checkpointed ) {
		retval = fprintf( file, "(1) Job was checkpointed\n\t" );
	} else {
		retval = fprintf( file, "(0) Job was not checkpointed\n\t" );
	}
	if( retval < 0 ) {
		return 0;
	}

	if( (!writeRusage (file, run_remote_rusage)) 			||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n") < 0) )
    {
		return 0;
	}

	if( fprintf(file, "\t%.0f  -  Run Bytes Sent By Job\n", 
				sent_bytes) < 0 ) {
		return 0;
	}
	if( fprintf(file, "\t%.0f  -  Run Bytes Received By Job\n", 
				recvd_bytes) < 0 ) {
		return 0;
	}

	if( ! terminate_and_requeued ) {
			// nothing else to write
		return 1;
	}

	if( normal ) {
		if( fprintf(file, "\t(1) Normal termination (return value %d)\n", 
					return_value) < 0 ) {
			return 0;
		}
	} else {
		if( fprintf(file, "\t(0) Abnormal termination (signal %d)\n",
					signal_number) < 0 ) {
			return 0;
		}
		if( core_file ) {
			retval = fprintf( file, "\t(1) Corefile in: %s\n", core_file );
		} else {
			retval = fprintf( file, "\t(0) No core file\n" );
		}
		if( retval < 0 ) {
			return 0;
		}
	}

	if( reason ) {
		if( fprintf(file, "\t%s\n", reason) < 0 ) {
			return 0;
		}
	}
	return 1;
}


// ----- JobAbortedEvent class
JobAbortedEvent::
JobAbortedEvent ()
{
	eventNumber = ULOG_JOB_ABORTED;
	reason = NULL;
}

JobAbortedEvent::
~JobAbortedEvent()
{
	if( reason ) {
		delete [] reason;
	}
}


void JobAbortedEvent::
setReason( const char* reason_str )
{
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	if( reason_str ) {
		reason = strnewp( reason_str );
	}
}


const char* JobAbortedEvent::
getReason( void )
{
	return reason;
}


int JobAbortedEvent::
writeEvent (FILE *file)
{
	if( fprintf(file, "Job was aborted by the user.\n") < 0 ) {
		return 0;
	}
	if( reason ) {
		if( fprintf(file, "\t%s\n", reason) < 0 ) {
			return 0;
		}
	}
	return 1;
}


int JobAbortedEvent::
readEvent (FILE *file)
{
	if( fscanf(file, "Job was aborted by the user.\n") == EOF ) {
		return 0;
	}
		// also try to read the reason, but for backwards
		// compatibility, don't fail if it's not there.
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	char reason_buf[BUFSIZ];
	if( fgets(reason_buf, BUFSIZ, file) == NULL) {
		return 1;	// backwards compatibility
	}
	chomp( reason_buf );  // strip the newline, if it's there.
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( reason_buf[0] == '\t' && reason_buf[1] ) {
		reason = strnewp( &reason_buf[1] );
	} else {
		reason = strnewp( reason_buf );
	}
	return 1;
}


// ----- TerminatedEvent baseclass
TerminatedEvent::TerminatedEvent()
{
	core_file = NULL;
	returnValue = signalNumber = -1;

	(void)memset((void*)&run_local_rusage,0,(size_t)sizeof(run_local_rusage));
	run_remote_rusage=total_local_rusage=total_remote_rusage=run_local_rusage;

	sent_bytes = recvd_bytes = total_sent_bytes = total_recvd_bytes = 0.0;
}

TerminatedEvent::~TerminatedEvent()
{
	if( core_file ) {
		delete [] core_file;
		core_file = NULL;
	}
}


void
TerminatedEvent::setCoreFile( const char* core_name )
{
	if( core_file ) {
		delete [] core_file;
		core_file = NULL;
	}
	if( core_name ) {
		core_file = strnewp( core_name );
	}
}


const char*
TerminatedEvent::getCoreFile( void )
{
	return core_file;
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
		if( core_file ) {
			retval = fprintf( file, "\t(1) Corefile in: %s\n\t",
							  core_file );
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
	int  retval;

	if( (retval = fscanf (file, "\n\t(%d) ", &normalTerm)) != 1 ) {
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
			if( fscanf(file, "Corefile in: ") == EOF ) {
				return 0;
			}
			if( !fgets(buffer, 128, file) ) {
				return 0;
			}
			chomp( buffer );
			setCoreFile( buffer );
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
	
		// THIS CODE IS TOTALLY BROKEN.  Please fix me.
		// In particular: fscanf() when you don't convert anything to
		// a local variable returns 0, but we think that's failure.
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


JobHeldEvent::JobHeldEvent ()
{
	eventNumber = ULOG_JOB_HELD;
	reason = NULL;
}


JobHeldEvent::~JobHeldEvent ()
{
	if( reason ) {
		delete [] reason;
	}
}


void
JobHeldEvent::setReason( const char* reason_str )
{
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	if( reason_str ) { 
		reason = strnewp( reason_str );
	}
}


const char* 
JobHeldEvent::getReason( void )
{
	return reason;
}


int
JobHeldEvent::readEvent( FILE *file )
{
	if( fscanf(file, "Job was held.\n") == EOF ) { 
		return 0;
	}
		// try to read the reason, but don't fail if it's not there.
		// a user of this event might not set a reason before calling
		// writeEvent()
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	char reason_buf[BUFSIZ];
	if( fgets(reason_buf, BUFSIZ, file) == NULL) {
		return 1;		// fake it :)
	}
	chomp( reason_buf );  // strip the newline
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( reason_buf[0] == '\t' && reason_buf[1] ) {
		reason = strnewp( &reason_buf[1] );
	} else {
		reason = strnewp( reason_buf );
	}
	return 1;
}


int
JobHeldEvent::writeEvent( FILE *file )
{
	if( fprintf(file, "Job was held.\n") < 0 ) {
		return 0;
	}
	if( reason ) {
		if( fprintf(file, "\t%s\n", reason) < 0 ) {
			return 0;
		} else {
			return 1;
		}
	} 
		// do we want to do anything else if there's no reason?
		// should we fail?  EXCEPT()?  
	return 1;
}


JobReleasedEvent::JobReleasedEvent()
{
	eventNumber = ULOG_JOB_RELEASED;
	reason = NULL;
}


JobReleasedEvent::~JobReleasedEvent()
{
	if( reason ) {
		delete [] reason;
	}
}


void
JobReleasedEvent::setReason( const char* reason_str )
{
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	if( reason_str ) { 
		reason = strnewp( reason_str );
	}
}


const char* 
JobReleasedEvent::getReason( void )
{
	return reason;
}


int
JobReleasedEvent::readEvent( FILE *file )
{
	if( fscanf(file, "Job was released.\n") == EOF ) { 
		return 0;
	}
		// try to read the reason, but don't fail if it's not there.
		// a user of this event might not set a reason before calling
		// writeEvent()
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	char reason_buf[BUFSIZ];
	if( fgets(reason_buf, BUFSIZ, file) == NULL) {
		return 1;		// fake it :)
	}
	chomp( reason_buf );  // strip the newline
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( reason_buf[0] == '\t' && reason_buf[1] ) {
		reason = strnewp( &reason_buf[1] );
	} else {
		reason = strnewp( reason_buf );
	}
	return 1;
}


int
JobReleasedEvent::writeEvent( FILE *file )
{
	if( fprintf(file, "Job was released.\n") < 0 ) {
		return 0;
	}
	if( reason ) {
		if( fprintf(file, "\t%s\n", reason) < 0 ) {
			return 0;
		} else {
			return 1;
		}
	} 
		// do we want to do anything else if there's no reason?
		// should we fail?  EXCEPT()?  
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


// ----- PostScriptTerminatedEvent class

PostScriptTerminatedEvent::
PostScriptTerminatedEvent()
{
	eventNumber = ULOG_POST_SCRIPT_TERMINATED;
	normal = false;
	returnValue = -1;
	signalNumber = -1;
}


PostScriptTerminatedEvent::
~PostScriptTerminatedEvent()
{
}


int PostScriptTerminatedEvent::
writeEvent( FILE* file )
{
    if( fprintf( file, "POST Script terminated.\n" ) < 0 ) {
        return 0;
    }

    if( normal ) {
        if( fprintf( file, "\t(1) Normal termination (return value %d)\n", 
					 returnValue ) < 0 ) {
            return 0;
        }
    } else {
        if( fprintf( file, "\t(0) Abnormal termination (signal %d)\n",
					 signalNumber ) < 0 ) {
            return 0;
        }
    }
    return 1;
}


int PostScriptTerminatedEvent::
readEvent( FILE* file )
{
	int tmp;
	if( fscanf( file, "POST Script terminated.\n\t(%d) ", &tmp ) != 1 ) {
		return 0;
	}
	if( tmp == 1 ) {
		normal = true;
	} else {
		normal = false;
	}
    if( normal ) {
        if( fscanf( file, "Normal termination (return value %d)",
					&returnValue ) != 1 ) {
            return 0;
		}
    } else {
        if( fscanf( file, "Abnormal termination (signal %d)",
					&signalNumber ) != 1 ) {
            return 0;
		}
    }
    return 1;
}
