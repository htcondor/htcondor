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

#if defined(IRIX)
#   ifdef _NO_ANSIMODE
#       define _TMP_NO_ANSIMODE
#   endif
#   undef _NO_ANSIMODE
#   include <time.h>
#   ifdef _TMP_NO_ANSIMODE
#       define _NO_ANSIMODE
#       undef _TMP_NO_ANSIMODE
#   endif
#endif   /* IRIX */

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */

#include <stdio.h>              /* for FILE type */
#if !defined(WIN32)
#include <sys/resource.h>       /* for struct rusage */
#endif
#include <limits.h>             /* for _POSIX_PATH_MAX */

//----------------------------------------------------------------------------
/** Enumeration of all possible events.
    If you modify this enum, you must also modify ULogEventNumberNames array
	WARNING: DO NOT CHANGE THE NUMBERS OF EXISTING EVENTS !!!
	         ^^^^^^           
*/
enum ULogEventNumber {
    /** Job submitted             */  ULOG_SUBMIT 					= 0,
    /** Job now running           */  ULOG_EXECUTE 					= 1,
    /** Error in executable       */  ULOG_EXECUTABLE_ERROR 		= 2,
    /** Job was checkpointed      */  ULOG_CHECKPOINTED 			= 3,
    /** Job evicted from machine  */  ULOG_JOB_EVICTED 				= 4,
    /** Job terminated            */  ULOG_JOB_TERMINATED 			= 5,
    /** Image size of job updated */  ULOG_IMAGE_SIZE 				= 6,
    /** Shadow threw an exception */  ULOG_SHADOW_EXCEPTION 		= 7,
    /** Generic Log Event         */  ULOG_GENERIC 					= 8,
    /** Job Aborted               */  ULOG_JOB_ABORTED 				= 9,
	/** Job was suspended         */  ULOG_JOB_SUSPENDED 			= 10,
	/** Job was unsuspended       */  ULOG_JOB_UNSUSPENDED 			= 11,
	/** Job was held              */  ULOG_JOB_HELD 				= 12,
	/** Job was released          */  ULOG_JOB_RELEASED 			= 13,
    /** Parallel Node executed    */  ULOG_NODE_EXECUTE 			= 14,
    /** Parallel Node terminated  */  ULOG_NODE_TERMINATED 			= 15,
    /** POST script terminated    */  ULOG_POST_SCRIPT_TERMINATED 	= 16,
	/** Job Submitted to Globus   */  ULOG_GLOBUS_SUBMIT 			= 17,
	/** Globus Submit failed      */  ULOG_GLOBUS_SUBMIT_FAILED 	= 18,
	/** Globus Resource Up        */  ULOG_GLOBUS_RESOURCE_UP 		= 19,
	/** Globus Resource Down      */  ULOG_GLOBUS_RESOURCE_DOWN 	= 20,
};

/// For printing the enum value.  cout << ULogEventNumberNames[eventNumber];
extern const char * ULogEventNumberNames[];

//----------------------------------------------------------------------------
/** Enumeration of possible outcomes after attempting to read an event.
    If you modify this enum, you must also modify ULogEventOutcomeNames array
*/
enum ULogEventOutcome
{
    /** Event is valid               */ ULOG_OK,
    /** No event occured (like EOF)  */ ULOG_NO_EVENT,
    /** Error reading log file       */ ULOG_RD_ERROR,
    /** Unknown Error                */ ULOG_UNK_ERROR
};

/// For printing the enum value.  cout << ULogEventOutcomeNames[outcome];
extern const char * ULogEventOutcomeNames[];

//----------------------------------------------------------------------------
/** Framework for a single User Log Event object.  This class is an abstract
    base class for more specific types of event objects.  The general
    procedure for using a ULogEvent object is first instantiate one of the
    _derived_ classes of ULogEvent, either with with its default constructor,
    or with the instantiateEvent() function.<p>

    If the event is being created for the purpose of writing to a log file,
    then fill it with data by setting each of its public members, and call
    putEvent(). <p>

    If the event is being read from a log, then call getEvent and then read
    the appropriate data members from the object.  <p>

    Below is an example log entry from Condor v6.  The first line not
    including "Job terminated" represents the log entry header, the last line
    "..."  terminates the log entry, and the lines between are the log body.
    <p>

    <pre>
    005 (5173.000.000) 10/20 16:59:24 Job terminated.
            (1) Normal termination (return value 0)
                    Usr 0 00:01:21, Sys 0 00:00:00  -  Run Remote Usage
                    Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
                    Usr 0 00:01:21, Sys 0 00:00:00  -  Total Remote Usage
                    Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
    ...
    </pre>

    <DL>
    <DT>Log Header
    <DD>"005" is the (enum ULogEventNumber) ULOG_JOB_TERMINATED.
        (5173.000.000) is the condorID: cluster, proc, subproc
        The next 2 fields are the date and time of day.
    <DT>Log Body
    <DD>The last field on the first line is a human readable version of the
    UlogEventNumber
    </DL>

    The remaining lines are human readable text.
*/
class ULogEvent {
  public:
    /** Default Constructor.  Initializes this event object with
        invalid condor ID, invalid eventNumber, 
        and sets eventTime to the current time.
     */
    ULogEvent();
    
    /// Destructor
    virtual ~ULogEvent();
    
    /** Get the next event from the log.  Gets the log header and body.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    int getEvent (FILE *file);
    
    /** Write the currently stored event values to the log file.
        Writes the log header and body.
        @param file the non-NULL writable log file.
        @return 0 for failure, 1 for success
    */
    int putEvent (FILE *file);
    
    /// The event last read, or to be written.
    ULogEventNumber    eventNumber;

    /// The time this event occurred
    struct tm          eventTime;

    /// The cluster field of the Condor ID for this event
    int                cluster;
    /// The proc    field of the Condor ID for this event
    int                proc;            
    /// The subproc field of the Condor ID for this event
    int                subproc;
    
  protected:

    /** Read the resource usage from the log file.
        @param file the non-NULL readable log file
        @param usage the rusage buffer to modify
        @return 0 for failure, 1 for success
    */
    int readRusage (FILE * file, rusage & usage);

    /** Write the resource usage to the log file.
        @param file the non-NULL writable log file.
        @param usage the rusage buffer to write with (not modified)
        @return 0 for failure, 1 for success
    */
    int writeRusage (FILE *, rusage &);

    /** Read the body of the next event.  This virtual function will
        be implemented differently for each specific type of event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *file) = 0;

    /** Write the body of the next event.  This virtual function will
        be implemented differently for each specific type of event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *file) = 0;
    
    /** Read the header from the log file
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    int readHeader (FILE *file);

    /** Write the header to the log file
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    int writeHeader (FILE *file);
};

//----------------------------------------------------------------------------
/** Instantiates an event object based on the given event number.
    @param event The event number
    @return a new object, subclass of ULogEvent
*/
ULogEvent *instantiateEvent (ULogEventNumber event);

//----------------------------------------------------------------------------
/** Framework for a single Submit Log Event object.  Below is an example
    Submit Log entry from Condor v6. <p>

<PRE>
000 (172.000.000) 10/20 16:56:54 Job submitted from host: <128.105.165.12:32779>
...
</PRE>
*/
class SubmitEvent : public ULogEvent
{
  public:
    ///
    SubmitEvent();
    ///
    ~SubmitEvent();

    /** Read the body of the next Submit event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Submit event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// For Condor v6, a host string in the form: "<128.105.165.12:32779>".
    char submitHost[128];

	// user-supplied text to include in the log event
    char* submitEventLogNotes;
};


//----------------------------------------------------------------------------
/** Framework for a Generic User Log Event object.
    This subclass of ULogEvent provides a application programmer with
    the mechanism to add his/her own custom defined log entry. <p>

    All generic events are seen the same by Condor, but a specific application
    can differentiate different subtypes of generic event by putting a special
    string into GenericEvent::info.  GenericEvent::readEvent and
    GenericEvent::writeEvent will simply parse the entire event body without
    interpretation.  The application can then do something special with
    that string if it wishes.
*/
class GenericEvent : public ULogEvent
{
  public:
    ///
    GenericEvent();
    ///
    ~GenericEvent();

    /** Read the body of the next Generic event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Generic event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// A string with unspecified format.
    char info[128];
};

//----------------------------------------------------------------------------
/** This event occurs when a job begins running on a machine.
    Below is an example Execute Log Event from Condor v6. <p>

<PRE>
001 (5176.000.000) 10/20 16:57:47 Job executing on host: <128.105.65.28:35247>
...
</PRE>
 */
class ExecuteEvent : public ULogEvent
{
  public:
    ///
    ExecuteEvent();
    ///
    ~ExecuteEvent();

    /** Read the body of the next Execute event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Execute event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// For Condor v6, a host string in the form: "<128.105.165.12:32779>".
    char executeHost[128];
};

//----------------------------------------------------------------------------
/// Enumeration of possible Errors when executing a program
enum ExecErrorType {
    /** Program is not an executable       */ CONDOR_EVENT_NOT_EXECUTABLE,
    /** Executable was not properly linked */ CONDOR_EVENT_BAD_LINK
};

//----------------------------------------------------------------------------
/// Framework for a ExecutableError Event object.
class ExecutableErrorEvent : public ULogEvent
{
  public:
    ///
    ExecutableErrorEvent();
    ///
    ~ExecutableErrorEvent();

    /** Read the body of the next ExecutableError event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next ExecutableError event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// The type of error
    ExecErrorType   errType;
};
    
//----------------------------------------------------------------------------
/** Framework for a Checkpointed Event object. Occurs when a job
    is checkpointed.
*/
class CheckpointedEvent : public ULogEvent
{
  public:
    ///
    CheckpointedEvent();
    ///
    ~CheckpointedEvent();

    /** Read the body of the next Checkpointed event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Checkpointed event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /** Local  Usage for the run */  rusage  run_local_rusage;
    /** Remote Usage for the run */  rusage  run_remote_rusage;
};


//----------------------------------------------------------------------------
/** Framework for a JobAborted Event object.  Occurs when a job
    is aborted from the machine on which it is running.
*/
class JobAbortedEvent : public ULogEvent
{
  public:
    ///
    JobAbortedEvent();
    ///
    ~JobAbortedEvent();

    /** Read the body of the next JobAborted event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobAborted event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	const char* getReason();
	void setReason( const char* );

 private:
	char* reason;
};


//----------------------------------------------------------------------------
/** Framework for an Evicted Event object.
    Below is an example Evicted Log entry for Condor v6<p>

<PRE>
004 (5164.000.000) 10/20 17:08:13 Job was evicted.
        (0) Job was not checkpointed.
                Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
                Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
...
</PRE>

<p> However, the evict event might also look like this:

<PRE>
004 (3236.000.000) 03/28 19:28:07 Job was evicted.
    (0) Job terminated and was requeued
        Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
        Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
    29  -  Run Bytes Sent By Job
    144  -  Run Bytes Received By Job
    (0) Abnormal termination (signal 9)
    (1) Corefile in: /some/interesting/path/core.3236.0
    The OnExitRemove expression 'ExitBySignal == False' evaluated to False
...
</PRE>

<p>or like this:

<PRE>
004 (3236.000.000) 03/28 19:28:07 Job was evicted.
    (0) Job terminated and was requeued
        Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
        Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
    29  -  Run Bytes Sent By Job
    144  -  Run Bytes Received By Job
    (1) Normal termination (return value 3)
    The OnExitRemove expression 'ExitCode == 0' evaluated to False
...
</PRE>



<p> In this case, the event shows that the job terminated but was
requeued due to the user-specified policy expressions.  The old
parsing code will just see this as an eviction w/o checkpoint.
However, the new code will correctly parse the other useful info out
of it.  
*/

class JobEvictedEvent : public ULogEvent
{
  public:
    ///
    JobEvictedEvent();
    ///
    ~JobEvictedEvent();

    /** Read the body of the next Evicted event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Evicted event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// Was the job checkpointed on eviction?
    bool    checkpointed;

    /** Local  Usage for the run */ rusage  run_local_rusage;
    /** Remote Usage for the run */ rusage  run_remote_rusage;

	/// bytes sent by the job over network for the run
	float sent_bytes;

	/// bytes received by the job over the network for the run
	float recvd_bytes;

    /// Did it terminate and get requeued?
    bool    terminate_and_requeued;

	/// Did it terminate normally? (only valid for requeue eviction) 
    bool    normal;

    /// return value (valid only on normal exit)
    int     return_value;

    /// The signal that terminated it (valid only on abnormal exit)
    int     signal_number;

	const char* getReason();
	void setReason( const char* );

	const char* getCoreFile();
	void setCoreFile( const char* );

 private:
	char* reason;
	char* core_file;

};


//----------------------------------------------------------------------------

/** This is an abstract base class for TerminatedEvents.  Both
	JobTerminatedEvent and NodeTerminatedEvent are derived from this.  
*/
class TerminatedEvent : public ULogEvent
{
  public:
    ///
    TerminatedEvent();
    ///
    ~TerminatedEvent();

    /** Read the body of the next Terminated event.
		This is pure virtual to make sure this is an abstract base class
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent( FILE * file ) = 0;

    /** Read the body of the next Terminated event.
        @param file the non-NULL readable log file
		@param header the header to use for this event (either "Job"
		or "Node")
        @return 0 for failure, 1 for success
    */
    int readEvent( FILE *, const char* header );


    /** Write the body of the next Terminated event.
		This is pure virtual to make sure this is an abstract base class
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE * file) = 0;

    /** Write the body of the next Terminated event.
        @param file the non-NULL writable log file
		@param header the header to use for this event (either "Job"
		or "Node")
        @return 0 for failure, 1 for success
    */
    int writeEvent( FILE * file, const char* header );

    /// Did it terminate normally?
    bool    normal;

    /// return value (valid only on normal exit)
    int     returnValue;

    /// The signal that terminated it (valid only on abnormal exit)
    int     signalNumber;

	const char* getCoreFile();
	void setCoreFile( const char* );

    /** Local  usage for the run */    rusage  run_local_rusage;
    /** Remote usage for the run */    rusage  run_remote_rusage;
    /** Total Local  rusage      */    rusage  total_local_rusage;
    /** Total Remote rusage      */    rusage  total_remote_rusage;

	/// bytes sent by the job over network for the run
	float sent_bytes;
	/// bytes received by the job over the network for the run
	float recvd_bytes;
	/// total bytes sent by the job over network for the lifetime of the job
	float total_sent_bytes;
	/// total bytes received by the job over the network for the lifetime
	/// of the job
	float total_recvd_bytes;

 private:

	char* core_file;

};


/** Framework for a single Job Terminated Event Log object.
    Below is an example Job Termination Event Log entry for Condor v6.<p>

<PRE>
005 (5170.000.000) 10/20 17:04:41 Job terminated.
        (1) Normal termination (return value 0)
                Usr 0 00:01:16, Sys 0 00:00:00  -  Run Remote Usage
                Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
                Usr 0 00:01:16, Sys 0 00:00:00  -  Total Remote Usage
                Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
...
</PRE>
*/
class JobTerminatedEvent : public TerminatedEvent
{
  public:
    ///
    JobTerminatedEvent();
    ///
    ~JobTerminatedEvent();

    /** Read the body of the next Terminated event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Terminated event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);
};


class NodeTerminatedEvent : public TerminatedEvent
{
  public:
    ///
    NodeTerminatedEvent();
    ///
    ~NodeTerminatedEvent();

    /** Read the body of the next Terminated event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Terminated event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

		/// node identifier for this event
	int node;
};


class PostScriptTerminatedEvent : public ULogEvent
{
 public:
    ///
    PostScriptTerminatedEvent();
    ///
    ~PostScriptTerminatedEvent();

    /** Read the body of the next Terminated event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    int readEvent( FILE* file );

    /** Write the body of the next Terminated event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    int writeEvent( FILE* file );

    /// did it exit normally
    bool normal;

    /// return value (valid only on normal exit)
    int returnValue;

    /// terminating signal (valid only on abnormal exit)
    int signalNumber;
};



//----------------------------------------------------------------------------
/** Framework for a GlobusSubmitEvent object.  Occurs when a Globus Universe
    job is actually submitted to a Globus Gatekeeper (and the submit is 
	committed if using a recent version of Globus which understands the
	two-phase commit protocol).
*/
class GlobusSubmitEvent : public ULogEvent
{
  public:
    ///
    GlobusSubmitEvent();
    ///
    ~GlobusSubmitEvent();

    /** Read the body of the next Submit event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Submit event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// Globus Resource Manager (Gatekeeper) Conctact String
    char* rmContact;

	/// Globus Job Manager Contact String
    char* jmContact;

	/// If true, then the JobManager supports restart recovery
	bool restartableJM;
};

//----------------------------------------------------------------------------
/** Framework for a GlobusSubmitiAbortedEvent object.  Occurs when a Globus 
	Universe job is is removed from the queue because it was unable to be
	sucessfully submitted to a Globus Gatekeeper after a certain number of 
	attempts.
*/
class GlobusSubmitFailedEvent : public ULogEvent
{
  public:
    ///
    GlobusSubmitFailedEvent();
    ///
    ~GlobusSubmitFailedEvent();

    /** Read the body of the next SubmitAborted event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next SubmitAborted event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// Globus Resource Manager (Gatekeeper) Conctact String
    char* reason;

};

class GlobusResourceUpEvent : public ULogEvent
{
  public:
    ///
    GlobusResourceUpEvent();
    ///
    ~GlobusResourceUpEvent();

    /** Read the body of the next ResoruceUp event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next SubmitAborted event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// Globus Resource Manager (Gatekeeper) Conctact String
    char* rmContact;

};

class GlobusResourceDownEvent : public ULogEvent
{
  public:
    ///
    GlobusResourceDownEvent();
    ///
    ~GlobusResourceDownEvent();

    /** Read the body of the next SubmitAborted event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next SubmitAborted event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// Globus Resource Manager (Gatekeeper) Conctact String
    char* rmContact;

};

//----------------------------------------------------------------------------
/** Framework for a JobImageSizeEvent object.  Occurs when the image size of a
    job is updated.
*/
class JobImageSizeEvent : public ULogEvent
{
  public:
    ///
    JobImageSizeEvent();
    ///
    ~JobImageSizeEvent();

    /** Read the body of the next ImageSize event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next ImageSize event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// The new size of the image
    int size;
};

//----------------------------------------------------------------------------
/** Framework for a ShadowException event object.  Occurs if the shadow
    throws an exception and quits, this event is logged
*/
class ShadowExceptionEvent : public ULogEvent
{
  public:
    ///
    ShadowExceptionEvent ();
    ///
    ~ShadowExceptionEvent ();

    /** Read the body of the next ShadowException event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next ShadowException event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/// exception message
	char	message[BUFSIZ];
	/// bytes sent by the job over network for the run
	float sent_bytes;
	/// bytes received by the job over the network for the run
	float recvd_bytes;
};

//----------------------------------------------------------------------------
/** Framework for a JobSuspended event object.  Occurs if the starter
	suspends a job.
*/
class JobSuspendedEvent : public ULogEvent
{
  public:
    ///
    JobSuspendedEvent ();
    ///
    ~JobSuspendedEvent ();

    /** Read the body of the next JobSuspendedEvent event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobSuspendedEvent event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/// How many pids the starter suspended
	int num_pids;
};

//----------------------------------------------------------------------------
/** Framework for a JobUnsuspended event object.  Occurs if the starter
	unsuspends a job.
*/
class JobUnsuspendedEvent : public ULogEvent
{
  public:
    ///
    JobUnsuspendedEvent ();
    ///
    ~JobUnsuspendedEvent ();

    /** Read the body of the next JobUnsuspendedEvent event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobUnsuspendedEvent event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/** We don't have a number of jobs unsuspended in here because since this
		is such a hack(the starter isn't supposed to write to the client log
		socket), we can't allow the starter to write to the socket AFTER it
		unsuspends the jobs. Otherwise it would have been nice to know if the
		starter unsuspended the same number of jobs it suspended. Alas.
	*/
};

//------------------------------------------------------------------------
/** Framework for a JobHeld event object.  Occurs if the job goes on hold.
*/
class JobHeldEvent : public ULogEvent
{
  public:
    ///
    JobHeldEvent ();
    ///
    ~JobHeldEvent ();

    /** Read the body of the next JobHeldEvent event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobHeldEvent event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getReason();

		/// makes a copy of the string in our "reason" member
	void setReason( const char* );

 private:

		/// why the job was held 
	char* reason;
};

//------------------------------------------------------------------------
/** Framework for a JobReleased event object.  Occurs if the job becomes
	released from the held state.
*/
class JobReleasedEvent : public ULogEvent
{
  public:
    ///
    JobReleasedEvent ();
    ///
    ~JobReleasedEvent ();

    /** Read the body of the next JobReleasedEvent event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobReleasedEvent event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getReason();

		/// makes a copy of the string in our "reason" member
	void setReason( const char* );

 private:

		/// why the job was released
	char* reason;
};

/* MPI events */
class NodeExecuteEvent : public ULogEvent
{
  public:
    ///
    NodeExecuteEvent();
    ///
    ~NodeExecuteEvent();

    /** Read the body of the next Execute event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next Execute event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

    /// For Condor v6, a host string in the form: "<128.105.165.12:32779>".
    char executeHost[128];

		/// Node identifier
	int node;
};

#endif // __CONDOR_EVENT_H__

