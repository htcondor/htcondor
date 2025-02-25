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
#include <time.h>
#include <sys/resource.h>       /* for struct rusage */
#elif (_MSC_VER >= 1600 )
// fix vs2010 compiler issue
#include <stdint.h> 
#endif

#include <string>
#include <chrono>

namespace ToE {
    class Tag;
}

/* 
	Since the ULogEvent class definition only deals with the ClassAd via a
	black box pointer and never needs to know the size of the actual
	object, I can just declare a forward reference to it here to avoid
	bringing in the full classad.h header file structure causing complications
	to third parties using this API. -psilord 02/21/03
*/
namespace classad {
	class ClassAd;
}
using classad::ClassAd;

// Log file type - what format the events are written as
// AUTO is a special case used by read_user_log to indicate it should figure out the format type
enum UserLogType {
	LOG_TYPE_UNKNOWN = -1, LOG_TYPE_NORMAL=0, LOG_TYPE_AUTO, LOG_TYPE_XML, LOG_TYPE_JSON
};

class ULogFile {
public:
	ULogFile() = default;
	ULogFile(const ULogFile& that) = delete;
	~ULogFile() { if (fp) { fclose(fp); fp = nullptr; } }

	int atEnd() { return feof(fp); }
	void clearEof() { clearerr(fp); }

	const char * stash(const char * line) {
		const char * prev = stashed_line;
		stashed_line = line;
		return prev;
	}

	char* readLine(char * buf, size_t bufsize);
	bool readLine(std::string& str, bool append);
	//int read_formatted(const char * fmt, va_list args);

	// hack to work with read_user_log.cpp for now
	FILE* attach(FILE* _fp) {
		stashed_line = nullptr;
		FILE * ret = fp; fp = _fp; return ret;
	}

private:
	FILE* fp = nullptr;
	const char * stashed_line = nullptr;
};

//----------------------------------------------------------------------------
/** Enumeration of all possible events.
    If you modify this enum, you must also modify ULogEventNumberNames array
	(in condor_event.cpp) and the python enums (in
	src/python-bindings/JobEventLog.cpp).
	WARNING: DO NOT CHANGE THE NUMBERS OF EXISTING EVENTS !!!
	         ^^^^^^           
*/
enum ULogEventNumber {
	/** not a valid event         */  ULOG_NO = -1,
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
	/** Job performing stage-out  */  ULOG_JOB_STAGE_OUT			= 32,
	/** Attribute updated         */  ULOG_ATTRIBUTE_UPDATE			= 33,
	/** PRE_SKIP event for DAGMan */  ULOG_PRESKIP					= 34,
	/** Cluster submitted         */  ULOG_CLUSTER_SUBMIT			= 35,
	/** Cluster removed           */  ULOG_CLUSTER_REMOVE			= 36,
	/** Factory paused            */  ULOG_FACTORY_PAUSED			= 37,
	/** Factory resumed           */  ULOG_FACTORY_RESUMED			= 38,
	/** For the Python bindings   */  ULOG_NONE						= 39,
	/** File transfer             */  ULOG_FILE_TRANSFER			= 40,
	/** Reserve space for xfer    */ ULOG_RESERVE_SPACE				= 41,
	/** Release reserved space    */ ULOG_RELEASE_SPACE				= 42,
	/** File xfer completed       */ ULOG_FILE_COMPLETE				= 43,
	/** Data reused               */ ULOG_FILE_USED					= 44,
	/** File removed from reuse   */ ULOG_FILE_REMOVED				= 45,
	/** Dataflow job skipped      */ ULOG_DATAFLOW_JOB_SKIPPED		= 46,

	// Debugging events in the Startd/Starter
	ULOG_EP_FIRST = 100,
	ULOG_EP_LAST =  199,
};

enum ULogEPEventNumber {
	ULOG_EP_STARTUP          = ULOG_EP_FIRST + 0,
	ULOG_EP_READY            = ULOG_EP_FIRST + 1,
	ULOG_EP_RECONFIG         = ULOG_EP_FIRST + 2,
	ULOG_EP_SHUTDOWN         = ULOG_EP_FIRST + 3,
	ULOG_EP_REQUEST_CLAIM    = ULOG_EP_FIRST + 4,
	ULOG_EP_RELEASE_CLAIM    = ULOG_EP_FIRST + 5,
	ULOG_EP_ACTIVATE_CLAIM   = ULOG_EP_FIRST + 6,
	ULOG_EP_DEACTIVATE_CLAIM = ULOG_EP_FIRST + 7,
	ULOG_EP_VACATE_CLAIM     = ULOG_EP_FIRST + 8,
	ULOG_EP_DRAIN            = ULOG_EP_FIRST + 9,
	ULOG_EP_RESOURCE_BREAK   = ULOG_EP_FIRST + 10,
	// when you add an event, also update ULogEPEventNumberNames, EPEventTypeNames,
	ULOG_EP_FUTURE_EVENT
};

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
    /** Unknown Error                */ ULOG_UNK_ERROR,
    /**                              */ ULOG_INVALID
};

/// For printing the enum value.  cout << ULogEventOutcomeNames[outcome];
extern const char * const ULogEventOutcomeNames[];
extern const  char SynchDelimiter[];

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

	// temporary parsing hack as we fixup read_user_log
	static ULogEventNumber readEventNumber(ULogFile& file, char* headbuf, size_t bufsize);

	// populate the event from a file where the first line has already been read
	int getEvent (ULogFile &file, const char * header_line, bool & got_sync_line);

	/** Format the event into a string suitable for writing to the log file.
	 *  Includes the event header and body, but not the synch delimiter.
	 *  @param out string to which the formatted text should be appended
	 *  @return false for failure, true for success
	 */
	bool formatEvent( std::string &out, int options );
	// Flags for the formatting options for formatEvent and formatHeader
	enum formatOpt { XML = 0x0001, JSON = 0x0002, CLASSAD = 0x0003, ISO_DATE=0x0010, UTC=0x0020, SUB_SECOND=0x0040, };
	static int parse_opts(const char * fmt, int default_opts);

		// returns a pointer to the current event name char[], or NULL
	const char* eventName(void) const;

		// helper function that sets ULogEvent fields into the given classad
	ClassAd* toClassAd(ClassAd& ad, bool event_time_utc) const;

	/** Return a ClassAd representation of this ULogEvent. This is implemented
		differently in each of the known (by John Bethencourt as of 6/5/02)
		subclasses of ULogEvent. Each implementation also calls
		ULogEvent::toClassAd to do the things common to all implementations.
		@return NULL for failure, or the ClassAd pointer for success
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd. This is implemented differently in each
		of the known (by John Bethencourt as of 6/5/02) subclasses of
		ULogEvent. Each implementation also calls ULogEvent::initFromClassAd
		to do the things common to all implementations.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
	   
    /// The event last read, or to be written.
    ULogEventNumber    eventNumber;

	/** Get the time at which this event occurred, prior to 8.8.2 this was a struct tm, but
		but keeping that in sync with the eventclock was a problem, so we got rid of it
		@return The time at which this event occurred.
	*/
	time_t GetEventTime(struct timeval *tv=NULL) const {
		if (tv) {
			tv->tv_sec = eventclock;
			tv->tv_usec = event_usec;
		}
		return eventclock;
	}

	/** Get the time at which this event occurred, in the form
	    of a time_t.
		@return The time at which this event occurred.
	*/
	time_t GetEventclock() const { return eventclock; }

    /// The cluster field of the Condor ID for this event
    int                cluster;
    /// The proc    field of the Condor ID for this event
    int                proc;            
    /// The subproc field of the Condor ID for this event
    int                subproc;
    
  protected:

    /** Read the resource usage line from the log file.
        @param file   the non-NULL readable log file
        @param usage  the rusage buffer to modify
        @param line   rusage line is read into this buffer
        @param remain set to offset in the buffer where rusage parser ended
        @return 0 for failure, 1 for success
    */
    bool readRusageLine (std::string &line, ULogFile& file, bool & got_sync_line, rusage & usage, int & remain);

    /** Format the resource usage for writing to the log file.
        @param out string to which the usage should be appended
        @param usage the rusage buffer to read from (not modified)
        @return 0 for failure, 1 for success
    */
    bool formatRusage (std::string &out, const rusage &usage);

    /** Make a formatted string with the resource usage information.
        @param usage the usage to consider
        @return NULL for failure, the string for success
    */
    char* rusageToStr (const rusage &usage);

    /** Parse a formatted string with the resource usage information.
        @param rusageStr a string like the ones made by rusageToStr.
        @param usage the rusage buffer to modify
        @return 0 for failure, 1 for success
    */
    int strToRusage (const char* rusageStr, rusage & usage);

    /** Read the body of the next event.  This virtual function will
        be implemented differently for each specific type of event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line) = 0;

    /** Format the body of this event for writing to the log file.
     *  This virtual function will be implemented differently for each
     *  specific type of event.
     *  @param out string to which the body should be appended
     *  @return false for failure, true for success
     */
    virtual bool formatBody( std::string &out ) = 0;

    /** populate the ULogEvent fields from the header line of the event, returning a
          pointer to the first character after the space at the end of the header fields
        @param ptr - the first line of the event
        @return null for parse failure, a pointer after the header fields on success
    */
	const char * readHeader(const char * ptr);

    /** Format the header for the log file
     *  @param out string to which the header should be appended
     *  @return false for failure, true for success
     */
    bool formatHeader( std::string &out, int options );

	// helper functions

	// 
	bool readLine(std::string& str, ULogFile& file, bool append=false) { return file.readLine(str, append); }

	// returns true if the input is ... or ...\n or ...\r\n
	bool is_sync_line(const char * line);

	// read a line into the supplied buffer and return true if there was any data read
	// and the data is not a sync line, if return values is false got_sync_line will be set accordingly
	// if chomp is true, trailing \r and \n will be changed to \0
	bool read_optional_line(ULogFile& file, bool & got_sync_line, char * buf, size_t bufsize, bool chomp=true, bool trim=false);

	// read a line into a string 
	bool read_optional_line(std::string & str, ULogFile& file, bool & got_sync_line, bool want_chomp=true, bool want_trim=false);

	// read a value after a prefix into a string
	bool read_line_value(const char * prefix, std::string & val, ULogFile& file, bool & got_sync_line, bool want_chomp=true);

	// set a string member that converting \n to | and \r to space
	void set_reason_member(std::string & reason_out, const std::string & reason_in);

	void reset_event_time();

  private:
    /// The time this event occurred as a UNIX timestamp
	time_t				eventclock;
	long				event_usec;
};

#define USERLOG_FORMAT_DEFAULT ULogEvent::formatOpt::ISO_DATE

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


// For events that include a ClassAd of resource usage information,
// populate the usage ad based on the given job ad.
void setEventUsageAd(const ClassAd& jobAd, ClassAd ** ppusageAd);


// This event type is used for all events where the event ID is not in the ULogEventNumber enum
// for this build.
class FutureEvent : public ULogEvent
{
  public:
    FutureEvent(ULogEventNumber en) { eventNumber = en; };
    ~FutureEvent(void) {};

    /** Read the body of the next event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this SubmitEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	const std::string & Head() { return head; } // remainder of event line (without trailing \n)
	const std::string & Payload() { return payload; } // other lines of the event (\n separated)
	void setHead(const char * head_text);
	void setPayload(const char * payload_text);

private:
	std::string head; // the remainder of the first line of the event, after the  timestamp (may be empty)
	std::string payload; // the body text of the event (may be empty)
};

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

    /** Read the body of the next Submit event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this SubmitEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	void setSubmitHost(char const *addr);

	char const *getSubmitHost() { return submitHost.c_str(); }

	/// For Condor v6, a host string in the form: "<128.105.165.12:32779>".
	std::string submitHost;
	// dagman-supplied text to include in the log event
	std::string submitEventLogNotes;
	// user-supplied text to include in the log event
	std::string submitEventUserNotes;
	// schedd-supplied warning about unmet future requirements
	std::string submitEventWarnings;
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

    /** Read the body of the next RemoteError event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this RemoteErrorEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	//Methods for accessing the error description.
	char const *getErrorText(void) {return error_str.c_str();}

	char const *getDaemonName(void) {return daemon_name.c_str();}

	char const *getExecuteHost(void) {return execute_host.c_str();}

	bool isCriticalError(void) const {return critical_error;}
	void setCriticalError(bool f);

	void setHoldReasonCode(int hold_reason_code);
	void setHoldReasonSubCode(int hold_reason_subcode);

    /// A host string in the form: "<128.105.165.12:32779>".
	std::string execute_host;
	/// Normally "condor_starter":
	std::string daemon_name;
	std::string error_str; // ok for this to be multiple lines
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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this GenericEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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
    virtual ~ExecuteEvent(void);

    /** Read the body of the next Execute event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this ExecuteEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

		/** @return execute host or empty string (never NULL) */
	const char *getExecuteHost();

	void setExecuteHost(char const *addr);
	void setSlotName(const char *name);
	bool hasProps();     // return true if non-zero number of execute properties
	ClassAd & setProp(); // get a ref to the execute properties ad, creating if does not exist.

	/** Identifier for the machine the job executed on.
		For Vanilla, Standard, and other non-Grid Universes, a
		host string in the form: "<128.105.165.12:32779>".
		For the Globus GridType a hostname.
		This may be an empty string for some JobUniverses
		or GridTyps.
	*/
	std::string executeHost;
	std::string slotName;
	ClassAd * executeProps = nullptr;
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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this ExecutableErrorEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this CheckpointedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /** Local  Usage for the run */  rusage  run_local_rusage;
    /** Remote Usage for the run */  rusage  run_remote_rusage;

	/// bytes sent by the job over network for checkpoint
	double sent_bytes;
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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobAbortedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	const char* getReason(void) const { return reason.c_str(); }

	void setToeTag( classad::ClassAd * toeTag );

	void setReason(const std::string & reason_in) { set_reason_member(reason, reason_in); }
private:
	std::string reason;
public:
	ToE::Tag * toeTag;
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

    /** Read the body of the next JobEvicted event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobEvictedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

    /// Was the job checkpointed on eviction?
    bool    checkpointed;

    /** Local  Usage for the run */ rusage  run_local_rusage;
    /** Remote Usage for the run */ rusage  run_remote_rusage;

	/// bytes sent by the job over network for the run
	double sent_bytes;

	/// bytes received by the job over the network for the run
	double recvd_bytes;

    /// Did it terminate and get requeued?
    bool    terminate_and_requeued;

	/// Did it terminate normally? (only valid for requeue eviction) 
    bool    normal;

    /// return value (valid only on normal exit)
    int     return_value;

    /// The signal that terminated it (valid only on abnormal exit)
    int     signal_number;

	ClassAd * pusageAd; // attributes represening resource used/provisioned etc

	const char* getReason(void) const { return reason.c_str(); }

	const char* getCoreFile(void) { return core_file.c_str(); }

	void setReason(const std::string & reason_in) { set_reason_member(reason, reason_in); }
private:
	std::string reason;
public:
	std::string core_file;
	int reason_code{0};
	int reason_subcode{0};

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
    virtual int readEvent(ULogFile& file, bool & got_sync_line ) = 0;

    /** Read the body of the next Terminated event.
        @param file the non-NULL readable log file
		@param header the header to use for this event (either "Job"
		or "Node")
        @return 0 for failure, 1 for success
    */
    int readEventBody(ULogFile& file, bool & got_sync_line, const char* header );

	/** Populate the pusageAd from the given classad
		@return 0 for failure, 1 for success
	*/
	int initUsageFromAd(const classad::ClassAd& ad);


    /** Format the body of the next Terminated event.
		This is pure virtual to make sure this is an abstract base class
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out ) = 0;

    /** Format the body of the next Terminated event.
        @param out string to which the formatted text should be appended
		@param header the header to use for this event (either "Job"
		or "Node")
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out, const char *head );

    /// Did it terminate normally?
    bool    normal;

    /// return value (valid only on normal exit)
    int     returnValue;

    /// The signal that terminated it (valid only on abnormal exit)
    int     signalNumber;

	const char* getCoreFile(void) { return core_file.c_str(); }

    /** Local  usage for the run */    rusage  run_local_rusage;
    /** Remote usage for the run */    rusage  run_remote_rusage;
    /** Total Local  rusage      */    rusage  total_local_rusage;
    /** Total Remote rusage      */    rusage  total_remote_rusage;

	/// bytes sent by the job over network for the run
	double sent_bytes;
	/// bytes received by the job over the network for the run
	double recvd_bytes;
	/// total bytes sent by the job over network for the lifetime of the job
	double total_sent_bytes;
	/// total bytes received by the job over the network for the lifetime
	/// of the job
	double total_recvd_bytes;

	ClassAd * pusageAd; // attributes represening resource used/provisioned etc

	// Subclasses wishing to be more efficient can override this to store
	// the values in the toeTag that they care about in member variables.
	// This method just makes a copy of toeTag (if it's not NULL).
	virtual void setToeTag( classad::ClassAd * toeTag );

	classad::ClassAd * toeTag;

	std::string core_file;

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

    /** Read the body of the next JobTerminated event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobTerminatedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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

    /** Read the body of the next NodeTerminated event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this NodeTerminatedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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

    /** Read the body of the next PostScriptTerminated event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    int readEvent(ULogFile& file, bool & got_sync_line );

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this PostScriptTerminatedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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
	std::string dagNodeName;

		// text label printed before DAG node name
	const char* const dagNodeNameLabel;

		// classad attribute name for DAG node name string
	const char* const dagNodeNameAttr;
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

    /** Read the body of the next JobImageSize event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobImageSize.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	/// The new size of the image
	long long image_size_kb;
	long long resident_set_size_kb;
	long long proportional_set_size_kb;
	long long memory_usage_mb;
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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this ShadowExceptionEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	const char * getMessage() { return message.c_str(); }
	void setMessage(const std::string & msg) { set_reason_member(message, msg); }

	/// exception message
private:
	std::string message;
public:
	/// bytes sent by the job over network for the run
	double sent_bytes;
	/// bytes received by the job over the network for the run
	double recvd_bytes;
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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobSuspendedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobUnsuspendedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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

    /** Read the body of the next JobHeld event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobHeldEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getReason(void) const;

	int getReasonCode(void) const;
	int getReasonSubCode(void) const;

		/// why the job was held 
	void setReason(const std::string & reason_in) { set_reason_member(reason, reason_in); }
private:
	std::string reason;
public:
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

    /** Read the body of the next JobReleased event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobReleasedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

		/// @return pointer to our copy of the reason, or NULL if not set
	const char* getReason(void) const;

		/// why the job was released
	void setReason(const std::string & reason_in) { set_reason_member(reason, reason_in); }
private:
	std::string reason;
public:
};

/* MPI (or parallel) events */
class NodeExecuteEvent : public ULogEvent
{
  public:
    ///
    NodeExecuteEvent(void);
	virtual ~NodeExecuteEvent(void);

    /** Read the body of the next NodeExecute event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this NodeExecuteEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	const char *getExecuteHost() { return executeHost.c_str(); }
	void setSlotName(const char *name);
	bool hasProps();     // return true if non-zero number of execute properties
	ClassAd & setProp(); // get a ref to the execute properties ad, creating if does not exist.

		/// Node identifier
	int node;

    /// For Condor v6, a host string in the form: "<128.105.165.12:32779>".
	std::string executeHost;
	std::string slotName;
	ClassAd * executeProps = nullptr;
};


//----------------------------------------------------------------------------
/** JobDisconnectedEvent object.
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

	virtual int readEvent(ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	virtual ClassAd* toClassAd(bool event_time_utc);

	virtual void initFromClassAd( ClassAd* ad );

	const char* getStartdAddr(void) const {return startd_addr.c_str();}
	const char* getStartdName(void) const {return startd_name.c_str();}
	const char* getDisconnectReason(void) const {return disconnect_reason.c_str();};
	void setDisconnectReason(const std::string & reason_in) { set_reason_member(disconnect_reason, reason_in); }

	std::string startd_addr;
	std::string startd_name;
private:
	std::string disconnect_reason;
};


class JobReconnectedEvent : public ULogEvent
{
public:
	JobReconnectedEvent(void);

	virtual int readEvent(ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	virtual ClassAd* toClassAd(bool event_time_utc);

	virtual void initFromClassAd( ClassAd* ad );

	const char* getStartdAddr(void) const {return startd_addr.c_str();}

	const char* getStartdName(void) const {return startd_name.c_str();}

	const char* getStarterAddr(void) const {return startd_addr.c_str();}

	std::string startd_addr;
	std::string startd_name;
	std::string starter_addr;
};


class JobReconnectFailedEvent : public ULogEvent
{
public:
	JobReconnectFailedEvent(void);

	virtual int readEvent(ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	virtual ClassAd* toClassAd(bool event_time_utc);

	virtual void initFromClassAd( ClassAd* ad );

	const char* getReason(void) const {return reason.c_str();};
	void setReason(const std::string & reason_in) { set_reason_member(reason, reason_in); }

	const char* getStartdName(void) const {return startd_name.c_str();}

	std::string startd_name;
private:
	std::string reason;
};


class GridResourceUpEvent : public ULogEvent
{
  public:
    ///
    GridResourceUpEvent(void);

    /** Read the body of the next GridResoruceUp event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this GridResourceUpEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	/// Name of the remote resource (GridResource attribute)
	std::string resourceName;
};

class GridResourceDownEvent : public ULogEvent
{
  public:
    ///
    GridResourceDownEvent(void);

    /** Read the body of the next GridResourceDown event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this GridResourceDownEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	/// Name of the remote resource (GridResource attribute)
	std::string resourceName;
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

    /** Read the body of the next GridSubmit event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this GridSubmitEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	/// Name of the remote resource (GridResource attribute)
	std::string resourceName;

	/// Job ID on the remote resource (GridJobId attribute)
	std::string jobId;
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

    /** Read the body of the next JobAdInformation event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	bool formatBody( std::string &out, ClassAd *jobad_arg);

	/** Return a ClassAd representation of this JobAdInformationEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	// Methods for setting the info
	void Assign(const char *attr, const char * value);
	void Assign(const char * attr, long long value);
	void Assign(const char * attr, int value);
	void Assign(const char * attr, double value);
	void Assign(const char * attr, bool value);

	// Methods for accessing the info.
	int LookupString (const char *attributeName, std::string &value) const;
	int LookupInteger (const char *attributeName, int &value) const;
	int LookupInteger (const char *attributeName, long long &value) const;
	int LookupFloat (const char *attributeName, double &value) const;
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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobStatusUnknownEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobStatusKnownEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobStageInEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

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
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobStageOutEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
};


//----------------------------------------------------------------------------
/** Framework for a AttributeUpdate object.
*/
class AttributeUpdate : public ULogEvent
{
	public:
	///
	AttributeUpdate(void);
	///
	~AttributeUpdate(void);

	/** Read the body of the next AttributeUpdate event.
		@param file the non-NULL readable log file
		@return 0 for failure, 1 for success
	*/
	virtual int readEvent (ULogFile& file, bool & got_sync_line);

	/** Format the body of this event.
		@param out string to which the formatted text should be appended
		@return false for failure, true for success
	*/
	virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this AttributeUpdate event.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	/** Set the attribute name.
		@param the name of the attribure
	*/
	virtual void setName(const char* attr_name);

	/** Set the attribute value.
		@param the value of the attribure
	*/

	virtual void setValue(const char* attr_value);

	/** Set the attribute value before the change.
		@param the value of the attribure before the change
	*/
	virtual void setOldValue(const char* attr_value);

	char *name;
	char *value;
	char *old_value;
};

class PreSkipEvent : public ULogEvent
{
  public:
    ///
    PreSkipEvent(void);

    /** Read the body of the next PreSkip event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this PreSkipEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);
	
	// dagman-supplied text to include in the log event
	std::string skipEventLogNotes;

};

//----------------------------------------------------------------------------
/** Framework for a Cluster Submit Log Event object.  Below is an example
    Cluster Submit Log entry from Condor v8. <p>

<PRE>
000 (172.000.000) 10/20 16:56:54 Cluster submitted from host: <128.105.165.12:32779>
...
</PRE>
*/
class ClusterSubmitEvent : public ULogEvent
{
  public:
    ///
    ClusterSubmitEvent(void);

    /** Read the body of the next Submit event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

    /** Return a ClassAd representation of this SubmitEvent.
        @return NULL for failure, the ClassAd pointer otherwise
    */
    virtual ClassAd* toClassAd(bool event_time_utc);

    /** Initialize from this ClassAd.
        @param a pointer to the ClassAd to initialize from
    */
    virtual void initFromClassAd(ClassAd* ad);

    void setSubmitHost(char const *addr);

	/// For Condor v8, a host string in the form: "<128.105.165.12:32779>".
	std::string submitHost;
	// dagman-supplied text to include in the log event
	std::string submitEventLogNotes;
	// user-supplied text to include in the log event
	std::string submitEventUserNotes;
};

//----------------------------------------------------------------------------
/** Framework for a Cluster Remove Log Event object.  Below is an example
    Cluster Remove Log entry from Condor v8. <p>

<PRE>
000 (172.000.000) 10/20 16:56:54 Cluster removed
...
</PRE>
*/
class ClusterRemoveEvent : public ULogEvent
{
  public:
    ///
    ClusterRemoveEvent(void);
    ///
    ~ClusterRemoveEvent(void);

/** Read the body of the next Submit event.
    @param file the non-NULL readable log file
    @return 0 for failure, 1 for success
*/
virtual int readEvent (ULogFile& file, bool & got_sync_line);

/** Format the body of this event.
    @param out string to which the formatted text should be appended
    @return false for failure, true for success
*/
virtual bool formatBody( std::string &out );

/** Return a ClassAd representation of this SubmitEvent.
    @return NULL for failure, the ClassAd pointer otherwise
*/
virtual ClassAd* toClassAd(bool event_time_utc);

/** Initialize from this ClassAd.
    @param a pointer to the ClassAd to initialize from
*/
virtual void initFromClassAd(ClassAd* ad);

	enum CompletionCode {
		Error=-1, Incomplete=0, Complete=1, Paused=2
	};

	int next_proc_id;
	int next_row;
	CompletionCode completion; // -1 == error, 0 = incomplete, 1 = normal-completion, 2 = paused
	std::string notes;
};

//------------------------------------------------------------------------
/** Framework for a job factory paused event object.
 *  Occurs if job factory pauses for a reason other than completion
*/
class FactoryPausedEvent : public ULogEvent
{
private:
	std::string reason; // why the factory was paused
	int pause_code;  // hold code if the factory is paused because the cluster is held
	int hold_code;

public:
	FactoryPausedEvent () : pause_code(0), hold_code(0) { eventNumber = ULOG_FACTORY_PAUSED; };
	~FactoryPausedEvent () {}

	// initialize this class by reading the next event from the given log file
	virtual int readEvent (ULogFile& file, bool & got_sync_line);

	// Format the body of this event, and append to the given std::string
	virtual bool formatBody( std::string &out );

	// Return a ClassAd representation of this event
	virtual ClassAd* toClassAd(bool event_time_utc);

	// initialize this class from the given ClassAd
	virtual void initFromClassAd(ClassAd* ad);

	// @return pointer to reason, will be NULL if not set
	const std::string getReason() const { return reason; }
	int getPauseCode() const { return pause_code; }
	int getHoldCode() const { return hold_code; }

	// set the reason member to the given string
	void setReason(const char* str);
	void setPauseCode(const int code) { pause_code = code; }
	void setHoldCode(const int code) { hold_code = code; }
};

class FactoryResumedEvent : public ULogEvent
{
private:
	std::string reason;

public:
	FactoryResumedEvent () { eventNumber = ULOG_FACTORY_RESUMED; };
	~FactoryResumedEvent () { };

	// initialize this class by reading the next event from the given log file
	virtual int readEvent (ULogFile& file, bool & got_sync_line);
	// Format the body of this event, and append to the given std::string
	virtual bool formatBody( std::string &out );
	// Return a ClassAd representation of this event
	virtual ClassAd* toClassAd(bool event_time_utc);
	// initialize this class from the given ClassAd
	virtual void initFromClassAd(ClassAd* ad);

	// @return pointer to reason, will be "" if not set
	std::string getReason() const { return reason; }

	// set the reason member to the given string
	void setReason(const char* str);
};

// ----------------------------------------------------------------------------

class FileTransferEvent : public ULogEvent {
	public:
		FileTransferEvent();
		~FileTransferEvent();

		virtual int readEvent( ULogFile& file, bool & got_sync_line );
		virtual bool formatBody( std::string & out );

		virtual ClassAd * toClassAd(bool event_time_utc);
		virtual void initFromClassAd( ClassAd * ad );

		enum FileTransferEventType {
			NONE = 0,
			IN_QUEUED = 1,
			IN_STARTED = 2,
			IN_FINISHED = 3,
			OUT_QUEUED = 4,
			OUT_STARTED = 5,
			OUT_FINISHED = 6,
			MAX = 7
		};

		static const char * FileTransferEventStrings[];

		void setType( FileTransferEventType ftet ) { type = ftet; }
		FileTransferEventType getType() const { return type; }

		void setQueueingDelay( time_t duration ) { queueingDelay = duration; }
		time_t getQueueingDelay() const { return queueingDelay; }

		void setHost( const std::string & h) { host = h; }
		const std::string & getHost() { return host; }

	protected:
		std::string host;
		time_t queueingDelay;
		FileTransferEventType type;
};


class ReserveSpaceEvent final : public ULogEvent {
public:
	ReserveSpaceEvent() {eventNumber = ULOG_RESERVE_SPACE;}
	~ReserveSpaceEvent() {};

	virtual int readEvent( ULogFile& file, bool & got_sync_line ) override;
	virtual bool formatBody( std::string & out ) override;

	virtual ClassAd * toClassAd(bool event_time_utc) override;
	virtual void initFromClassAd( ClassAd * ad ) override;

	void setExpirationTime(const std::chrono::system_clock::time_point &expiry) {m_expiry = expiry;}
	std::chrono::system_clock::time_point getExpirationTime() const {return m_expiry;}

	void setReservedSpace(size_t space) {m_reserved_space = space;}
	size_t getReservedSpace() const {return m_reserved_space;}

	void setUUID(const std::string &uuid) {m_uuid = uuid;}
	std::string generateUUID();
	const std::string &getUUID() const {return m_uuid;}

	void setTag(const std::string &tag) {m_tag = tag;}
	const std::string &getTag() const {return m_tag;}

private:
	std::chrono::system_clock::time_point m_expiry;
	size_t m_reserved_space{0};
	std::string m_uuid;
	std::string m_tag;
};


class ReleaseSpaceEvent final : public ULogEvent {
public:
	ReleaseSpaceEvent() {eventNumber = ULOG_RELEASE_SPACE;}
	~ReleaseSpaceEvent() {};

	virtual int readEvent( ULogFile& file, bool & got_sync_line ) override;
	virtual bool formatBody( std::string & out ) override;

	virtual ClassAd * toClassAd(bool event_time_utc) override;
	virtual void initFromClassAd( ClassAd * ad ) override;

	void setUUID(const std::string &uuid) {m_uuid = uuid;}
	const std::string &getUUID() const {return m_uuid;}

private:
	std::string m_uuid;
};


class FileCompleteEvent final : public ULogEvent {
public:
	FileCompleteEvent() : m_size(0) {eventNumber = ULOG_FILE_COMPLETE;}
	~FileCompleteEvent() {};

	virtual int readEvent( ULogFile& file, bool & got_sync_line ) override;
	virtual bool formatBody( std::string & out ) override;

	virtual ClassAd * toClassAd(bool event_time_utc) override;
	virtual void initFromClassAd( ClassAd * ad ) override;

	void setUUID(const std::string &uuid) {m_uuid = uuid;}
	const std::string &getUUID() const {return m_uuid;}

	void setSize(size_t size) {m_size = size;}
	size_t getSize() const {return m_size;}

	void setChecksumType(const std::string &type) {m_checksum_type = type;}
	const std::string &getChecksumType() const {return m_checksum_type;}

	void setChecksum(const std::string &value) {m_checksum = value;}
	const std::string &getChecksum() const {return m_checksum;}

private:
	size_t m_size;
	std::string m_checksum;
	std::string m_checksum_type;
	std::string m_uuid;
};


class FileUsedEvent final : public ULogEvent {
public:
	FileUsedEvent() {eventNumber = ULOG_FILE_USED;}
	~FileUsedEvent() {};

	virtual int readEvent( ULogFile& file, bool & got_sync_line ) override;
	virtual bool formatBody( std::string & out ) override;

	virtual ClassAd * toClassAd(bool event_time_utc) override;
	virtual void initFromClassAd( ClassAd * ad ) override;

	void setChecksumType(const std::string &type) {m_checksum_type = type;}
	const std::string &getChecksumType() const {return m_checksum_type;}

	void setChecksum(const std::string &value) {m_checksum = value;}
	const std::string &getChecksum() const {return m_checksum;}

	void setTag(const std::string &tag) {m_tag = tag;}
	const std::string &getTag() const {return m_tag;}
private:
	std::string m_checksum;
	std::string m_checksum_type;
	std::string m_tag;
};


class FileRemovedEvent final : public ULogEvent {
public:
	FileRemovedEvent() : m_size(0) {eventNumber = ULOG_FILE_REMOVED;}
	~FileRemovedEvent() {};

	virtual int readEvent( ULogFile& file, bool & got_sync_line ) override;
	virtual bool formatBody( std::string & out ) override;

	virtual ClassAd * toClassAd(bool event_time_utc) override;
	virtual void initFromClassAd( ClassAd * ad ) override;

	void setSize(size_t size) {m_size = size;}
	size_t getSize() const {return m_size;}

	void setChecksumType(const std::string &type) {m_checksum_type = type;}
	const std::string &getChecksumType() const {return m_checksum_type;}

	void setChecksum(const std::string &value) {m_checksum = value;}
	const std::string &getChecksum() const {return m_checksum;}

	void setTag(const std::string &tag) {m_tag = tag;}
	const std::string &getTag() const {return m_tag;}
private:
	size_t m_size;
	std::string m_checksum;
	std::string m_checksum_type;
	std::string m_tag;
};

//----------------------------------------------------------------------------
/** Framework for a DataflowJobSkipped Event object.
 *  Occurs when a dataflow job is detected and then s kipped.
*/
class DataflowJobSkippedEvent : public ULogEvent
{
  public:
    ///
    DataflowJobSkippedEvent(void);
    ///
    ~DataflowJobSkippedEvent(void);

    /** Read the body of the next JobAborted event.
        @param file the non-NULL readable log file
        @return 0 for failure, 1 for success
    */
    virtual int readEvent (ULogFile& file, bool & got_sync_line);

    /** Format the body of this event.
        @param out string to which the formatted text should be appended
        @return false for failure, true for success
    */
    virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this JobAbortedEvent.
		@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
		@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	const char* getReason(void) const { return reason.c_str(); }
	void setReason(const std::string & reason_in) { set_reason_member(reason, reason_in); }

	void setToeTag( classad::ClassAd * toeTag );

private:
	std::string reason;
public:
	ToE::Tag * toeTag;
};


#endif // __CONDOR_EVENT_H__
