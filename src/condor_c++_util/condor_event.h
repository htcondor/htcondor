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

#define GENERIC_EVENT 1

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */

#include <stdio.h>				/* for FILE type */
#if !defined(WIN32)
#include <sys/resource.h>		/* for struct rusage */
#endif
#include <limits.h>				/* for _POSIX_PATH_MAX */

// If you modify this enum, you must also modify ULogEventNumberNames array
enum ULogEventNumber
{
	ULOG_SUBMIT,				// Job submitted
	ULOG_EXECUTE,				// Job now running
	ULOG_EXECUTABLE_ERROR,			// Error in executable
	ULOG_CHECKPOINTED,				// Job was checkpointed
	ULOG_JOB_EVICTED,				// Job evicted from machine
	ULOG_JOB_TERMINATED,			// Job terminated
	ULOG_IMAGE_SIZE,				// Image size of job updated
	ULOG_SHADOW_EXCEPTION			// Shadow threw an exception
#if defined(GENERIC_EVENT)
	,ULOG_GENERIC			        
#endif	    
	,ULOG_JOB_ABORTED				// Job terminated
};

/// For printing the enum value.  cout << ULogEventNumberNames[outcome];
extern const char * ULogEventNumberNames[];

// If you modify this enum, you must also modify ULogEventOutcomeNames array
enum ULogEventOutcome
{
	ULOG_OK,
	ULOG_NO_EVENT,
	ULOG_RD_ERROR,
	ULOG_UNK_ERROR
};

/// For printing the enum value.  cout << ULogEventOutcomeNames[outcome];
extern const char * ULogEventOutcomeNames[];

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
#if !defined(WIN32)
	int readRusage (FILE *, rusage &);
	int writeRusage (FILE *, rusage &);
#endif
	virtual int readEvent (FILE *) = 0;	// read the event from a file
	virtual int writeEvent (FILE *) = 0;   	// write the event to a file

	int readHeader (FILE *);	// read the event header from the file
	int writeHeader (FILE *);	// write the wvwnt header to the file

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

#if defined(GENERIC_EVENT)
// this is a generic event described by a single string
class GenericEvent : public ULogEvent
{
  public:
	GenericEvent();
	~GenericEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

	char info[128];
};
#endif	    

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

#if !defined(WIN32)
	rusage	run_local_rusage;
	rusage  run_remote_rusage;
#endif
};


// this event occurs when a job is removed from a machine
class JobAbortedEvent : public ULogEvent
{
  public:
	JobAbortedEvent();
	~JobAbortedEvent();

	virtual int readEvent (FILE *);
	virtual int writeEvent (FILE *);

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
#if !defined(WIN32)
	rusage	run_local_rusage;	// local and remote usage for the run
	rusage  run_remote_rusage;
#endif
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
#if !defined(WIN32)
	rusage	run_local_rusage;	// local and remote usage for the run
	rusage  run_remote_rusage;
	rusage	total_local_rusage;	// total local and remote rusage
	rusage  total_remote_rusage;
#endif
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

