/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef __CONDOR_EVENT_H__
#define __CONDOR_EVENT_H__

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */

#include <stdio.h>              /* for FILE type */
#if !defined(WIN32) 
#include <sys/time.h>
#include <sys/resource.h>       /* for struct rusage */
#endif

/* 
	Since the ULogEvent class definition only deals with the ClassAd via a
	black box pointer and never needs to know the size of the actual
	object, I can just declare a forward reference to it here to avoid
	bringing in the full classad.h header file structure causing complications
	to third parties using this API. -psilord 02/21/03
*/
#ifdef WANT_OLD_CLASSADS
class ClassAd;
#else
namespace compat_classad {
  class ClassAd;
}
using namespace compat_classad;
#endif




//----------------------------------------------------------------------------
/** Enumeration of all possible events.
    If you modify this enum, you must also modify ULogEventNumberNames array
	(in condor_event.C)
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
	/** Remote Error              */  ULOG_REMOTE_ERROR             = 21,
	/** RSC socket lost           */  ULOG_JOB_DISCONNECTED         = 22,
	/** RSC socket re-established */  ULOG_JOB_RECONNECTED          = 23,
	/** RSC reconnect failure     */  ULOG_JOB_RECONNECT_FAILED     = 24,
	/** Grid Resource Up          */  ULOG_GRID_RESOURCE_UP         = 25,
	/** Grid Resource Down        */  ULOG_GRID_RESOURCE_DOWN       = 26,
	/** Job Submitted remotely    */  ULOG_GRID_SUBMIT 	    	    = 27,
	/** Report job ad information */  ULOG_JOB_AD_INFORMATION		= 28,
	/** Job Status Unknown        */  ULOG_JOB_STATUS_UNKNOWN       = 29,
	/** Job Status Known          */  ULOG_JOB_STATUS_KNOWN         = 30,
	/** Job performing stage-in   */  ULOG_JOB_STAGE_IN				= 31,
	/** Job performing stage-out  */  ULOG_JOB_STAGE_OUT			= 32
};

/// For printing the enum value.  cout << ULogEventNumberNames[eventNumber];
extern const char * ULogEventNumberNames[];

//----------------------------------------------------------------------------
/** Enumeration of possible outcomes after attempting to read an event.
    If you modify this enum, you must also modify ULogEventOutcomeNames array
	(in condor_event.C)
*/
enum ULogEventOutcome
{
    /** Event is valid               */ ULOG_OK,
    /** No event occured (like EOF)  */ ULOG_NO_EVENT,
    /** Error reading log file       */ ULOG_RD_ERROR,
    /** Missed event                 */ ULOG_MISSED_EVENT,
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
    ULogEvent(void);
    
    /// Destructor
    virtual ~ULogEvent(void);
    
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

		// returns a pointer to the current event name char[], or NULL
	const char* eventName(void) const;

	/** Return a ClassAd representation of this ULogEvent. This is implemented
		differently in each of the known (by John Bethencourt as of 6/5/02)
		subclasses of ULogEvent. Each implementation also calls
		ULogEvent::toClassAd to do the things common to all implementations.
		@return NULL for failure, or the ClassAd pointer for success
	*/
	virtual ClassAd* toClassAd(void);
	    
	/** Initialize from this ClassAd. This is implemented differently in each
		of the known (by John Bethencourt as of 6/5/02) subclasses of
		ULogEvent. Each implementation also calls ULogEvent::initFromClassAd
		to do the things common to all implementations.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
	   
	void setGlobalJobId(const char *gjid) { m_gjid = gjid; } 
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
    
    /// Added by Ameet
    char *scheddname;
    //char globaljobid[100];

	time_t eventclock;

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

    /** Make a formatted string with the resource usage information.
        @param usage the usage to consider
        @return NULL for failure, the string for success
    */
    char* rusageToStr (rusage usage);

    /** Parse a formatted string with the resource usage information.
        @param rusageStr a string like the ones made by rusageToStr.
        @param usage the rusage buffer to modify
        @return 0 for failure, 1 for success
    */
    int strToRusage (char* rusageStr, rusage & usage);

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

	/// the global job id for the job associated with this event
	void insertCommonIdentifiers(ClassAd &adToFill);
	const char *m_gjid;
};

//----------------------------------------------------------------------------
/** Instantiates an event object based on the given event number.
    @param event The event number
    @return a new object, subclass of ULogEvent
*/
ULogEvent *instantiateEvent (ULogEventNumber event);


/** Create an event object from a ClassAd.
	@param ad The ClassAd
	@return a new object, subclass of ULogEvent
*/
ULogEvent *instantiateEvent (ClassAd *ad);

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
    SubmitEvent(void);
    ///
    ~SubmitEvent(void);

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

	/** Return a ClassAd representation of this SubmitEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// For Condor v6, a host string in the form: "<128.105.165.12:32779>".
    char submitHost[128];

    // dagman-supplied text to include in the log event
    char* submitEventLogNotes;
    // user-supplied text to include in the log event
    char* submitEventUserNotes;
};

//----------------------------------------------------------------------------
/** RemoteErrorEvent object.
	This subclass of ULogEvent is used by condor_starter to report
	errors that the user needs to know about, such as failure to
	access input files.  At this time, such events are not written
    directly into the user log.  Instead, they are translated into
    ShadowExceptionEvents in the shadow.
*/
class RemoteErrorEvent : public ULogEvent
{
 public:
	RemoteErrorEvent(void);
	~RemoteErrorEvent(void);

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

	/** Return a ClassAd representation of this RemoteErrorEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	//Methods for accessing the error description.
	void setErrorText(char const *str);
	char const *getErrorText(void) {return error_str;}

	void setDaemonName(char const *name);
	char const *getDaemonName(void) {return daemon_name;}

	void setExecuteHost(char const *host);
	char const *getExecuteHost(void) {return execute_host;}

	bool isCriticalError(void) {return critical_error;}
	void setCriticalError(bool f);

	void setHoldReasonCode(int hold_reason_code);
	void setHoldReasonSubCode(int hold_reason_subcode);

 private:
    /// A host string in the form: "<128.105.165.12:32779>".
    char execute_host[128];
	/// Normally "condor_starter":
	char daemon_name[128];
	char *error_str;
	bool critical_error; //tells shadow to give up
	int hold_reason_code;
	int hold_reason_subcode;
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
    GenericEvent(void);
    ///
    ~GenericEvent(void);

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

	/** Return a ClassAd representation of this GenericEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	//Preferred methods for accessing the info text.
	void setInfoText(char const *str);
	char const *getInfoText(void) {return info;}

    /// A string with unspecified format.
    char info[1024];
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
    ExecuteEvent(void);
    ///
    ~ExecuteEvent(void);

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

	/** Return a ClassAd representation of this ExecuteEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	/** Identifier for the machine the job executed on.
		For Vanilla, Standard, and other non-Grid Universes, a
		host string in the form: "<128.105.165.12:32779>".
		For the Globus GridType a hostname.
		This may be an empty string for some JobUniverses
		or GridTyps.
	*/
    char executeHost[128];
	char remoteName[256];
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
    ExecutableErrorEvent(void);
    ///
    ~ExecutableErrorEvent(void);

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

	/** Return a ClassAd representation of this ExecutableErroEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

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
    CheckpointedEvent(void);
    ///
    ~CheckpointedEvent(void);

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

	/** Return a ClassAd representation of this CheckpointedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /** Local  Usage for the run */  rusage  run_local_rusage;
    /** Remote Usage for the run */  rusage  run_remote_rusage;

	/// bytes sent by the job over network for checkpoint
	float sent_bytes;
};


//----------------------------------------------------------------------------
/** Framework for a JobAborted Event object.  Occurs when a job
    is aborted from the machine on which it is running.
*/
class JobAbortedEvent : public ULogEvent
{
  public:
    ///
    JobAbortedEvent(void);
    ///
    ~JobAbortedEvent(void);

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

	/** Return a ClassAd representation of this JobAbortedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	const char* getReason(void) const;
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
    JobEvictedEvent(void);
    ///
    ~JobEvictedEvent(void);

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

	/** Return a ClassAd representation of this JobEvictedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

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

	const char* getReason(void) const;
	void setReason( const char* );

	const char* getCoreFile(void);
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
    TerminatedEvent(void);
    ///
    ~TerminatedEvent(void);

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

	const char* getCoreFile(void);
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
    JobTerminatedEvent(void);
    ///
    ~JobTerminatedEvent(void);

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

	/** Return a ClassAd representation of this JobTerminatedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
};


class NodeTerminatedEvent : public TerminatedEvent
{
  public:
    ///
    NodeTerminatedEvent(void);
    ///
    ~NodeTerminatedEvent(void);

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

	/** Return a ClassAd representation of this NodeTerminatedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

		/// node identifier for this event
	int node;
};


class PostScriptTerminatedEvent : public ULogEvent
{
 public:
    ///
    PostScriptTerminatedEvent(void);
    ///
    ~PostScriptTerminatedEvent(void);

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

	/** Return a ClassAd representation of this PostScriptTerminatedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// did it exit normally
    bool normal;

    /// return value (valid only on normal exit)
    int returnValue;

    /// terminating signal (valid only on abnormal exit)
    int signalNumber;

		// DAG node name
	char* dagNodeName;

		// text label printed before DAG node name
	const char* const dagNodeNameLabel;

		// classad attribute name for DAG node name string
	const char* const dagNodeNameAttr;
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
    GlobusSubmitEvent(void);
    ///
    ~GlobusSubmitEvent(void);

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

	/** Return a ClassAd representation of this GlobusSubmitEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

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
    GlobusSubmitFailedEvent(void);
    ///
    ~GlobusSubmitFailedEvent(void);

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

	/** Return a ClassAd representation of this GlobusSubmitFailedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// Globus Resource Manager (Gatekeeper) Conctact String
    char* reason;

};

class GlobusResourceUpEvent : public ULogEvent
{
  public:
    ///
    GlobusResourceUpEvent(void);
    ///
    ~GlobusResourceUpEvent(void);

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

	/** Return a ClassAd representation of this GlobusResourceUpEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// Globus Resource Manager (Gatekeeper) Conctact String
    char* rmContact;

};

class GlobusResourceDownEvent : public ULogEvent
{
  public:
    ///
    GlobusResourceDownEvent(void);
    ///
    ~GlobusResourceDownEvent(void);

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

	/** Return a ClassAd representation of this GlobusResourceDownEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

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
    JobImageSizeEvent(void);
    ///
    ~JobImageSizeEvent(void);

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

	/** Return a ClassAd representation of this JobImageSizeEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

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
    ShadowExceptionEvent (void);
    ///
    ~ShadowExceptionEvent (void);

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

	/** Return a ClassAd representation of this ShadowExceptionEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	/// exception message
	char	message[BUFSIZ];
	/// bytes sent by the job over network for the run
	float sent_bytes;
	/// bytes received by the job over the network for the run
	float recvd_bytes;
	bool began_execution;
};

//----------------------------------------------------------------------------
/** Framework for a JobSuspended event object.  Occurs if the starter
	suspends a job.
*/
class JobSuspendedEvent : public ULogEvent
{
  public:
    ///
    JobSuspendedEvent (void);
    ///
    ~JobSuspendedEvent (void);

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

	/** Return a ClassAd representation of this JobSuspendedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

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
    JobUnsuspendedEvent (void);
    ///
    ~JobUnsuspendedEvent (void);

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

	/** Return a ClassAd representation of this JobUnsuspendedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

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
    JobHeldEvent (void);
    ///
    ~JobHeldEvent (void);

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

	/** Return a ClassAd representation of this JobHeldEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getReason(void) const;

	int getReasonCode(void) const;
	int getReasonSubCode(void) const;

		/// makes a copy of the string in our "reason" member
	void setReason( const char* );

	void setReasonCode( const int );
	void setReasonSubCode( const int );

 private:

		/// why the job was held 
	char* reason;
	int code;
	int subcode;
};

//------------------------------------------------------------------------
/** Framework for a JobReleased event object.  Occurs if the job becomes
	released from the held state.
*/
class JobReleasedEvent : public ULogEvent
{
  public:
    ///
    JobReleasedEvent (void);
    ///
    ~JobReleasedEvent (void);

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

	/** Return a ClassAd representation of this JobReleasedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getReason(void) const;

		/// makes a copy of the string in our "reason" member
	void setReason( const char* );

 private:

		/// why the job was released
	char* reason;
};

/* MPI (or parallel) events */
class NodeExecuteEvent : public ULogEvent
{
  public:
    ///
    NodeExecuteEvent(void);
    ///
    ~NodeExecuteEvent(void);

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

	/** Return a ClassAd representation of this NodeExecuteEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// For Condor v6, a host string in the form: "<128.105.165.12:32779>".
    char executeHost[128];

		/// Node identifier
	int node;
};


//----------------------------------------------------------------------------
/** JobConnectionEvent object.
	This subclass of ULogEvent is a base class used for the various
	userlog events related to the connection between the submit and
	execute hosts being lost (and potentially re-established).
	This is an abstract base class, you can't instantiate one of
	these.  Instead, you must instantiate one of its children:
	JobDisconnectedEvent, JobReconnectedEvent, or
	JobReconnectFailedEvent. 
*/

class JobDisconnectedEvent : public ULogEvent
{
public:
	JobDisconnectedEvent(void);
	~JobDisconnectedEvent(void);

	virtual int readEvent( FILE * );

	virtual int writeEvent( FILE * );

	virtual ClassAd* toClassAd( void );

	virtual void initFromClassAd( ClassAd* ad );

		/// stores a copy of the string in our "startd_addr" member
	void setStartdAddr( char const *startd );
	const char* getStartdAddr(void) const {return startd_addr;}

		/// stores a copy of the string in our "startd_name" member
	void setStartdName( char const *name );
	const char* getStartdName(void) const {return startd_name;}

		/// stores a copy of the string in our "reason" member
	void setDisconnectReason( const char* );
		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getDisconnectReason(void) const {return disconnect_reason;};

		/// stores a copy of the string in our "reason" member
	void setNoReconnectReason( const char* );
		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getNoReconnectReason(void) const {return no_reconnect_reason;};

		/** This flag defaults to true, and is set to false if a
			NoReconnectReason is specified for the event */
	bool canReconnect( void ) {return can_reconnect; };

private:
	char *startd_addr;
	char *startd_name;
	char *disconnect_reason;
	char *no_reconnect_reason;
	bool can_reconnect;
};


class JobReconnectedEvent : public ULogEvent
{
public:
	JobReconnectedEvent(void);
	~JobReconnectedEvent(void);

	virtual int readEvent( FILE * );

	virtual int writeEvent( FILE * );

	virtual ClassAd* toClassAd( void );

	virtual void initFromClassAd( ClassAd* ad );

		/// stores a copy of the string in our "startd_addr" member
	void setStartdAddr( char const *startd );
	const char* getStartdAddr(void) const {return startd_addr;}

		/// stores a copy of the string in our "startd_name" member
	void setStartdName( char const *name );
	const char* getStartdName(void) const {return startd_name;}

		/// stores a copy of the string in our "starter_addr" member
	void setStarterAddr( char const *starter );
	const char* getStarterAddr(void) const {return startd_addr;}

private:
	char *startd_addr;
	char *startd_name;
	char *starter_addr;
};


class JobReconnectFailedEvent : public ULogEvent
{
public:
	JobReconnectFailedEvent(void);
	~JobReconnectFailedEvent(void);

	virtual int readEvent( FILE * );

	virtual int writeEvent( FILE * );

	virtual ClassAd* toClassAd( void );

	virtual void initFromClassAd( ClassAd* ad );

		/// stores a copy of the string in our "reason" member
	void setReason( const char* );
		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getReason(void) const {return reason;};

		/// stores a copy of the string in our "startd_name" member
	void setStartdName( char const *name );
	const char* getStartdName(void) const {return startd_name;}

private:
	char *startd_name;
	char *reason;
};


class GridResourceUpEvent : public ULogEvent
{
  public:
    ///
    GridResourceUpEvent(void);
    ///
    ~GridResourceUpEvent(void);

    /** Read the body of the next GridResoruceUp event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next GridResourceUp event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/** Return a ClassAd representation of this GridResourceUpEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// Name of the remote resource (GridResource attribute)
    char* resourceName;

};

class GridResourceDownEvent : public ULogEvent
{
  public:
    ///
    GridResourceDownEvent(void);
    ///
    ~GridResourceDownEvent(void);

    /** Read the body of the next GridResourceDown event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next GridResourceDown event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/** Return a ClassAd representation of this GridResourceDownEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// Name of the remote resource (GridResource attribute)
    char* resourceName;

};

//----------------------------------------------------------------------------
/** Framework for a GridSubmitEvent object.  Occurs when a Grid Universe
    job is actually submitted to a grid resource and we have a remote
    job id for it
*/
class GridSubmitEvent : public ULogEvent
{
  public:
    ///
    GridSubmitEvent(void);
    ///
    ~GridSubmitEvent(void);

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

	/** Return a ClassAd representation of this GridSubmitEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// Name of the remote resource (GridResource attribute)
    char* resourceName;

	/// Job ID on the remote resource (GridJobId attribute)
    char* jobId;
};

//----------------------------------------------------------------------------
/** Framework for a Job Ad Information object.
*/
class JobAdInformationEvent : public ULogEvent
{
  public:
    ///
    JobAdInformationEvent(void);
    ///
    ~JobAdInformationEvent(void);

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

	int writeEvent (FILE *, ClassAd *jobad_arg);

	/** Return a ClassAd representation of this GenericEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	// Methods for accessing the info.
	int LookupString (const char *attributeName, char **value) const;
	int LookupInteger (const char *attributeName, int &value) const;
	int LookupFloat (const char *attributeName, float &value) const;
	int LookupBool  (const char *attributeName, bool &value) const;

  protected:
	  ClassAd *jobad;
};

//----------------------------------------------------------------------------
/** Framework for a JobStatusUnknown object.  Occurs when the remote
    status of a grid job hasn't been updated for a long period of time.
*/
class JobStatusUnknownEvent : public ULogEvent
{
  public:
    ///
    JobStatusUnknownEvent(void);
    ///
    ~JobStatusUnknownEvent(void);

    /** Read the body of the next JobStatusUnknown event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobStatusUnknown event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/** Return a ClassAd representation of this JobStatusUnknownEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
};

//----------------------------------------------------------------------------
/** Framework for a JobStatusKnown object.  Occurs when the remote
    status of a grid job is updated after a long period of time (and a
	JobStatusUnknownEvent was previously triggered).
*/
class JobStatusKnownEvent : public ULogEvent
{
  public:
    ///
    JobStatusKnownEvent(void);
    ///
    ~JobStatusKnownEvent(void);

    /** Read the body of the next JobStatusKnown event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobStatusKnown event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/** Return a ClassAd representation of this JobStatusKnownEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
};

//----------------------------------------------------------------------------
/** Framework for a JobStageInEvent object.  
 	Not yet used.
*/
class JobStageInEvent : public ULogEvent
{
  public:
    ///
    JobStageInEvent(void);
    ///
    ~JobStageInEvent(void);

    /** Read the body of the next JobStageInEvent event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobStageInEvent event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/** Return a ClassAd representation of this JobStageInEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
};

//----------------------------------------------------------------------------
/** Framework for a JobStageOutEvent object.  
	Not yet used.
*/
class JobStageOutEvent : public ULogEvent
{
  public:
    ///
    JobStageOutEvent(void);
    ///
    ~JobStageOutEvent(void);

    /** Read the body of the next JobStageOutEvent event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (FILE *);

    /** Write the body of the next JobStageOutEvent event.
        @param file the non-NULL writable log file
        @return 0 for failure, 1 for success
    */
    virtual int writeEvent (FILE *);

	/** Return a ClassAd representation of this JobStageOutEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(void);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
};


#endif // __CONDOR_EVENT_H__

