#ifndef __CONDOR_EVENT_H__
#define __CONDOR_EVENT_H__

#if defined(IRIX62)
#	ifdef _NO_ANSIMODE
#		define _TMP_NO_ANSIMODE
#	endif
#	undef _NO_ANSIMODE
#	include <time.h>
#	ifdef _TMP_NO_ANSIMODE
#		define _NO_ANSIMODE
#		undef _TMP_NO_ANSIMODE
#	endif
#endif   /* IRIX62 */

#include "condor_common.h"
#include "_condor_fix_resource.h"

enum ULogEventNumber
{
	ULOG_SUBMIT,					// Job submitted
	ULOG_EXECUTE,					// Job now running
	ULOG_EXECUTABLE_ERROR,			// Error in executable
	ULOG_CHECKPOINTED,				// Job was checkpointed
	ULOG_JOB_EVICTED,				// Job evicted from machine
	ULOG_JOB_TERMINATED,			// Job terminated
	ULOG_IMAGE_SIZE,				// Image size of job updated
	ULOG_SHADOW_EXCEPTION			// Shadow threw an exception
};


enum ULogEventOutcome
{
	ULOG_OK,
	ULOG_NO_EVENT,
	ULOG_RD_ERROR,
	ULOG_UNK_ERROR
};


class ULogEvent
{
  public:
	ULogEvent();					// ctor
	virtual ~ULogEvent();			// dtor

	int getEvent (FILE *);			// get an event from the file
	int putEvent (FILE *);			// put an event to the file
	
	ULogEventNumber	 eventNumber;   // which event
	struct tm		 eventTime;	   	// time of occurrance
	int 			 cluster;		// to which job
	int				 proc;			  
	int				 subproc;

  protected:
	int readRusage (FILE *, rusage &);
	int writeRusage (FILE *, rusage &);
	virtual int readEvent (FILE *) = 0;		// read the event from a file
	virtual int writeEvent (FILE *) = 0;   	// write the event to a file

	int readHeader (FILE *);		// read the event header from the file
	int writeHeader (FILE *);		// write the wvwnt header to the file

};


// function to instantiate an event of the given number
ULogEvent *instantiateEvent (ULogEventNumber);

// this event occurs when a job is submitted
class SubmitEvent : public ULogEvent
{
  public:
	SubmitEvent();
	~SubmitEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	char submitHost[128];
};

// this event occurs when a job begins running on a machine
class ExecuteEvent : public ULogEvent
{
  public:
	ExecuteEvent();
	~ExecuteEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	char executeHost[128];
};


// this error is issued if the submitted file is:
//  + not an executable
//  + was not properly linked
enum ExecErrorType
{
	CONDOR_EVENT_NOT_EXECUTABLE,
	CONDOR_EVENT_BAD_LINK
};

class ExecutableErrorEvent : public ULogEvent
{
  public:
	ExecutableErrorEvent();
	~ExecutableErrorEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	ExecErrorType	errType;
};
	
// this event occurs when a job is checkpointed
class CheckpointedEvent : public ULogEvent
{
  public:
	CheckpointedEvent();
	~CheckpointedEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	rusage	run_local_rusage;
	rusage  run_remote_rusage;
};


// this event occurs when a job is evicted from a machine
class JobEvictedEvent : public ULogEvent
{
  public:
	JobEvictedEvent();
	~JobEvictedEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	bool	checkpointed;	 	// was it checkpointed on eviction?
	rusage	run_local_rusage;	// local and remote usage for the run
	rusage  run_remote_rusage;
};


// this event occurs when a job terminates
class JobTerminatedEvent : public ULogEvent
{
  public:
	JobTerminatedEvent();
	~JobTerminatedEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	bool	normal;	 			// did it terminate normally?
	int		returnValue;		// return value (valid only on normal exit)
	int		signalNumber;		// signal number (valid only on abnormal exit)
	char    coreFile[_POSIX_PATH_MAX]; // name of core file
	rusage	run_local_rusage;	// local and remote usage for the run
	rusage  run_remote_rusage;
	rusage	total_local_rusage;	// total local and remote rusage
	rusage  total_remote_rusage;
};


// this event occurs when the image size of a job is updated
class JobImageSizeEvent : public ULogEvent
{
  public:
	JobImageSizeEvent();
	~JobImageSizeEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	int size;
};

// if the shadow throws an exception and quits, this event is logged
class ShadowExceptionEvent : public ULogEvent
{
  public:
	ShadowExceptionEvent ();
	~ShadowExceptionEvent ();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);
};
	
#endif // __CONDOR_EVENT_H__
