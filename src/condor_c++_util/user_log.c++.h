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

#if !defined( _CONDOR_USER_LOG_CPP_H )
#define _CONDOR_USER_LOG_CPP_H

#if defined(__cplusplus)

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */

#if !defined(WIN32)
#ifndef BOOLEAN_TYPE_DEFINED
typedef int BOOLEAN;
typedef int BOOL_T;
#define BOOLAN_TYPE_DEFINED
#endif

#if !defined(TRUE)
#   define TRUE 1
#   define FALSE 0
#endif
#endif

#if defined(NEW_PROC)
#include "proc.h"
#endif
#include "file_lock.h"
#include "condor_event.h"

#define XML_USERLOG_DEFAULT 0

/** API for writing a log file.  Since an API for reading a log file
    was not originally needed, a ReadUserLog class did not exist,
    so it was not forseen to call this class WriteUserLog. <p>

    This class was written before the ULogEvent class existed.  Therefore,
    there are several functions and parameters, marked deprecated,
    whose necessity is rid of with the use of ULogEvent.  Those functions
    and parameters still exist for backward compatibility with Condor
    code that hasn't yet been update.  New Condor code ABSOLUTELY SHOULD NOT
    use these deprecated functions. <p>

    The typical use of this class (for example, condor_shadow) producing
    log events for a job, will be the following:<p>

    <UL>
      Initialize this object with the job owner's username, the log file path,
      and the job's condorID, which the UserLog object will fill in the
      condorID for each event given to it in writeEvent(). <p>

      Call writeEvent() for each event to be written to the log.  The condorID
      of the event will automatically be assigned the condorID of this
      ULogEvent object.
    </UL>
*/
class UserLog {
  public:
    ///
    UserLog() : cluster(-1), proc(-1), subproc(-1), in_block(FALSE), path(0),
				fp(0), lock(NULL), use_xml(XML_USERLOG_DEFAULT) {}
    
    /** Constructor
        @param owner Username of the person whose job is being logged
        @param file the path name of the log file to be written (copied)
        @param clu  condorID cluster to put into each ULogEvent
        @param proc condorID proc    to put into each ULogEvent
        @param subp condorID subproc to put into each ULogEvent
		@param xml  make this TRUE to write XML logs, FALSE to use the old form
    */
    UserLog(const char *owner, const char *domain, const char *file,
			int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT);
    
    UserLog(const char *owner, const char *file,
			int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT);
    ///
    ~UserLog();
    
    /** Initialize the log file, if not done by constructor.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return TRUE on success
    */
    bool initialize(const char *owner, const char *domain, const char *file,
		   	int c, int p, int s);
    
    /** Initialize the log file.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return TRUE on success
    */
    bool initialize(const char *file, int c, int p, int s);
    
    /** Initialize the condorID, which will fill in the condorID
        for each ULogEvent passed to writeEvent().

        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return TRUE on success
    */
    bool initialize(int c, int p, int s);

	void setUseXML(bool new_use_xml){ use_xml = new_use_xml; }

    /** Write an event to the log file.  Caution: if the log file is
        not initialized, then no event will be written, and this function
        will return a successful value.

        @param event the event to be written
        @return 0 for failure, 1 for success
    */
    int writeEvent (ULogEvent *event);
    
    /** Deprecated Function. */  void put( const char *fmt, ... );
    /** Deprecated Function. */  void display();
    /** Deprecated Function. */  void begin_block();
    /** Deprecated Function. */  void end_block();
  private:
    /// Deprecated Function.  Print the header to the log.
    void        output_header();
    
    /// Deprecated.  condorID cluster of the next event.
    int         cluster;

    /// Deprecated.  condorID proc of the next event.
    int         proc;

    /// Deprecated.  condorID subproc of the next event.
    int         subproc;

    /// Deprecated.  Are we currently in a block?
    int         in_block;

    /** Copy of path to the log file */  char     * path;
    /** The log file                 */  FILE     * fp;
    /** The log file lock            */  FileLock * lock;
	/** Whether we use XML or not    */  bool       use_xml;
};


/** API for reading a log file.

    This class was written at the same time the ULogEvent class was written,
    so it does not contain the old deprecated functions and parameters that
    the UserLog class is plagued with.
*/
class ReadUserLog
{
  public:

    /** Default constructor.
	*/
    ReadUserLog() { clear(); }

    /** Constructor.
        @param file the file to read from
	*/
    ReadUserLog (const char * filename);
                                      
    /** Destructor.
	*/
    inline ~ReadUserLog() { releaseResources(); }

    /** Initialize the log file.  This function will return FALSE
        if it can't open the log file (among other things).
        @param file the file to read from
        @return TRUE for success
    */
    bool initialize (const char *filename);

    /** Read the next event from the log file.  The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome readEvent (ULogEvent * & event);

    /** Synchronize the log file if the last event read was an error.  This
        safe guard function should be called if there is some error reading an
        event, but there are events after it in the file.  Will skip over the
        "bad" event (i.e., read up to and including the event separator (...))
        so that the rest of the events can be read.

        @return TRUE: success, FALSE: failure
    */
    bool synchronize ();

    /// Get the log's file descriptor
    inline int getfd() const { return _fd; }
	inline FILE *getfp() const { return _fp; }

	void outputFilePos(const char *pszWhereAmI);

	void Lock();
	void Unlock();

	/** Set whether the log file should be treated as XML. The constructor will
		attempt to figure this out on its own.
		@param is_xml should be TRUE if we have an XML log file, FALSE otherwise
	*/
	void setIsXMLLog( bool is_xml );

	/** Determine whether this ReadUserLog thinks its log file is XML.
		Note that false may mean that the type is not yet known (e.g.,
		the log file is empty).
		@return TRUE if XML, FALSE otherwise
	*/
	bool getIsXMLLog();

	/** Set whether the log file should be treated as "old-style" (non-XML).
	    The constructor will attempt to figure this out on its own.
		@param is_old should be TRUE if we have a "old-style" log file,
		FALSE otherwise
	*/
	void setIsOldLog( bool is_old );

	/** Determine whether this ReadUserLog thinks its log file is "old-style"
	    (non-XML).
		Note that false may mean that the type is not yet known (e.g.,
		the log file is empty).
		@return TRUE if "old-style", FALSE otherwise
	*/
	bool getIsOldLog();


    private:

	/** Set all members to their cleared values.
	*/
	void clear();

	/** Release all resources this object has claimed/allocated.
	*/
	void releaseResources();

	/** Determine the type of log this is; note that if called on an
	    empty log, this will return TRUE and the log type will stay
		LOG_TYPE_UNKNOWN.
        @return TRUE for success, FALSE otherwise
	*/
	bool determineLogType();

	/** Skip the XML header of this log, if there is one.
	    @param the first character after the opening '<'
		@param the initial file position
        @return TRUE for success, FALSE otherwise
	*/
	bool skipXMLHeader(char afterangle, long filepos);

    /** Read the next event from the XML log file. The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome readEventXML (ULogEvent * & event);

    /** Read the next event from the old style log file. The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome readEventOld (ULogEvent * & event);

	enum LogType {
		LOG_TYPE_UNKNOWN = 0,
		LOG_TYPE_OLD,
		LOG_TYPE_XML
	};

    /** The log's file descriptor */  int    _fd;
    /** The log's file pointer    */  FILE * _fp;
    /** The log file lock         */  FileLock* lock;
	/** Is the file locked?       */  bool is_locked;

	/** Type of this log		  */  LogType log_type;
};

#endif /* __cplusplus */

typedef void LP;

#if defined(__cplusplus)
extern "C" {
#endif
    /// Deprecated.
    LP *InitUserLog( const char *own, const char *domain, const char *file,
		   	int c, int p, int s );

    /// Deprecated.
    void CloseUserLog( LP * lp );

    /// Deprecated.
    void PutUserLog( LP *lp,  const char *fmt, ... );

    /// Deprecated.
    void BeginUserLogBlock( LP *lp );

    /// Deprecated.
    void EndUserLogBlock( LP *lp );
#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_USER_LOG_CPP_H */
