/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#define BOOLEAN_TYPE_DEFINED
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

#if !defined(WIN32)
#include <sys/types.h>
#endif
#include <limits.h>             /* for _POSIX_PATH_MAX */

/* Predeclare some classes */
class MyString;


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
    UserLog( void );
    
    /** Constructor
        @param owner Username of the person whose job is being logged
        @param file the path name of the log file to be written (copied)
        @param clu  condorID cluster to put into each ULogEvent
        @param proc condorID proc    to put into each ULogEvent
        @param subp condorID subproc to put into each ULogEvent
		@param xml  make this TRUE to write XML logs, FALSE to use the old form
		@param gjid global job ID
    */
    UserLog(const char *owner, const char *domain, const char *file,
			int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT,
			const char *gjid = NULL);
    
    UserLog(const char *owner, const char *file,
			int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT);
    ///
    ~UserLog();

	///
    void Reset( void );
    
    /** Initialize the log file, if not done by constructor.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
        @param gjid the condor global job id to put into each ULogEvent
		@return TRUE on success
    */
    bool initialize(const char *owner, const char *domain, const char *file,
		   	int c, int p, int s, const char *gjid);
    
    /** Initialize the log file.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return TRUE on success
    */
    bool initialize(const char *file, int c, int p, int s, const char *gjid);
   
#if !defined(WIN32)
    /** Initialize the log file (PrivSep mode only)
        @param uid the user's UID
        @param gid the user's GID
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return TRUE on success
    */
    bool initialize(uid_t, gid_t, const char *, int, int, int, const char *gjid);
#endif

    /** Initialize the condorID, which will fill in the condorID
        for each ULogEvent passed to writeEvent().

        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return TRUE on success
    */
    bool initialize(int c, int p, int s, const char *gjid);

	void setUseXML(bool new_use_xml){ m_use_xml = new_use_xml; }

	void setWriteUserLog(bool b){ m_write_user_log = b; }
	void setWriteGlobalLog(bool b){ m_write_global_log = b; }

    /** Write an event to the log file.  Caution: if the log file is
        not initialized, then no event will be written, and this function
        will return a successful value.

        @param event the event to be written
        @return 0 for failure, 1 for success
    */
    int writeEvent (ULogEvent *event, ClassAd *jobad = NULL);

#if 0 /* deprecated cruft */    
    /** Deprecated Function. */  void put( const char *fmt, ... );
    /** Deprecated Function. */  void display();
    /** Deprecated Function. */  void begin_block();
    /** Deprecated Function. */  void end_block();
#endif

  private:

	bool initialize_global_log();

	bool open_file( const char *file, bool log_as_user, FileLock* & lock, 
					FILE* & fp );

	int doWriteEvent( ULogEvent *event, bool is_global_event, ClassAd *ad);
	int doWriteEvent( FILE *fp, ULogEvent *event, bool do_use_xml );
	void GenerateGlobalId( MyString &id );

	bool handleGlobalLogRotation();

	
#if 0 /* deprecated cruft */
    /// Deprecated Function.  Print the header to the log.
    void        output_header();
#endif
    
    /// Deprecated.  condorID cluster of the next event.
    int         cluster;

    /// Deprecated.  condorID proc of the next event.
    int         proc;

    /// Deprecated.  condorID subproc of the next event.
    int         subproc;

    /// Deprecated.  Are we currently in a block?
    int         in_block;

	/** Write to the user log? */		 bool		m_write_user_log;
    /** Copy of path to the log file */  char     * m_path;
    /** The log file                 */  FILE     * m_fp;
    /** The log file lock            */  FileLock * m_lock;

	/** Write to the global log? */		 bool		m_write_global_log;
    /** Copy of path to global log   */  char     * m_global_path;
    /** The global log file          */  FILE     * m_global_fp;
    /** The global log file lock     */  FileLock * m_global_lock;
	/** Whether we use XML or not    */  bool       m_global_use_xml;
	/** The log file uniq ID base    */  char     * m_global_uniq_base;
	/** The current sequence number  */  int        m_global_sequence;

	/** Whether we use XML or not    */  bool       m_use_xml;

#if !defined(WIN32)
	/** PrivSep: the user's UID      */  uid_t      m_privsep_uid;
	/** PrivSep: the user's GID      */  gid_t      m_privsep_gid;
#endif
	/** The GlobalJobID for this job */  char     * m_gjid;
};



/** API for reading a log file.

    This class was written at the same time the ULogEvent class was written,
    so it does not contain the old deprecated functions and parameters that
    the UserLog class is plagued with.

	There is one know problem with the reader:
	First, let's define 3 files:
	  file1 = the "current" file - let's say it's inode is 1001
	  file2 = the next file - inode 1002
	  file3 = the next file - inode 1003

	Next, assume that the reader is reading file1, hits the end of the
	file, generates a NO_EVENT to the application, but keeps the file
	open.

	Next, the reader sleeps for a period of time after this

	While the reader is sleeping, the writer rotate the log file
	*twice*, such that file3 becomes the current file being written
	to, and file2 becomes the ".old" (note that the reader still has
	file1 open, although all directory entries to it have been
	removed)

	After this, but before the writer rotates a 3rd time, the reader
	wakes up.  It reads to the end of file1 (which it still has open),
	reads the remaining events, and eventutally hits end of file, and
	then looks for the next file.  It cannot find a file with the
	signature of file1, assumes the it must have missed an event.
	When it continues after the MISSED_EVENT, however, it starts
	reading file2, which is, in fact, the next file, and no events
	have actually been missed.

	The writer object currently has a sequence number that it writes
	to the file header, *but* that sequence number and the uniq ID are
	unique to the writer object that's currently writing it.  In
	theory, the reader could use these squence numbers to recognize
	that the above file2 is, in fact, the next in the sequence and
	that no events have acutally been lost.  A complicating factor for
	this, however, is that, if there are multipe writers, there the
	uniq ID and sequence nubmers are specific to each writer, and,
	thus, relying on the sequence number in the reader is meaningless.

	We have plans to have the writer re-write the header after
	rotation, and, at that time, it could update the sequence info,
	but that's currently not implemented.

*/
class ReadUserLogState;
class ReadUserLogMatch;
class ReadUserLog
{
  public:

	/** Use this to structure to serialize the current state.
		Always use InitFileState() to initialize this structure.
		More documentation below.
	*/
	struct FileState {
		void	*buf;
		int		size;
	};

    /** Default constructor.
	*/
    ReadUserLog() { clear(); }

    /** Constructor.
        @param file the file to read from
	*/
    ReadUserLog (const char * filename);

    /** Constructor.
        @param State to restore from
	*/
    ReadUserLog ( const FileState &state );
                                      
    /** Destructor.
	*/
    ~ReadUserLog() { releaseResources(); }
                                      
    /** Detect whether the object has been initialized
	*/
    bool isInitialized( void ) const { return m_initialized; };


    /** Initialize the log file.  This function will return FALSE
        if it can't open the log file (among other things).
        @param file the file to read from
		@param handle_rotation enables the reader to handle rotating
		 log files (only useful for global user logs)
		@param check_for_rotated try to open the rotated (".old") file first?
        @return TRUE for success
    */
    bool initialize (const char *filename,
					 bool handle_rotation = false,
					 bool check_for_rotated = false );

    /** Initialize the log file from a saved state.
		This function will return FALSE
        if it can't open the log file (among other things).
        @param FileState saved file state
		@param handle_rotation enables the reader to handle rotating
		 log files (only useful for global user logs)
        @return TRUE for success
    */
    bool initialize (const ReadUserLog::FileState &state,
					 bool handle_rotation = true );

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
    int getfd() const { return m_fd; }
	FILE *getfp() const { return m_fp; }

	void outputFilePos(const char *pszWhereAmI);

	/** Lock / unlock the file
	 */
	void Lock(void)   { Lock(true);   };
	void Unlock(void) { Unlock(true); };

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
	bool getIsXMLLog( void ) const;

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
	bool getIsOldLog( void ) const;

	/** Methods to serialize the state.
		Always use InitFileState() to initialize this structure.
		All of these methods take a reference to a state buffer
		  as their only parameter.
		All of these methods return true upon success.

		To save the state, do something like this:
		  ReadUserLog				reader;
		  ReadUserLog::FileState	statebuf;

		  status = ReadUserLog::InitFileState( statebuf );

		  status = reader.GetFileState( statebuf );
		  write( fd, statebuf.buf, statebuf.size );
		  ...
		  status = reader.GetFileState( statebuf );
		  write( fd, statebuf.buf, statebuf.size );
		  ...

		  status = UninitFileState( statebuf );

		To restore the state, do something like this:
		  ReadUserLog::FileState	statebuf;
		  status = ReadUserLog::InitFileState( statebuf );

		  read( fd, statebuf.buf, statebuf.size );

		  ReadUserLog  				reader;
		  status = userlog.initialize( statebuf );

		  status = UninitFileState( statebuf );
		  ....
	 */

	/** Use this method to initialize a file state buffer
		@param file state buffer
		@return true on success
	 */
	static bool InitFileState( ReadUserLog::FileState &state );

	/** Use this method to clean up a file state buffer (when done with it)
		@param file state buffer
		@return true on success
	 */
	bool UninitFileState( ReadUserLog::FileState &state ) const;

	/** Use this method to get the current state (before saving it off)
		@param file state buffer
		@return true on success
	 */
	bool GetFileState( ReadUserLog::FileState &state ) const;

	/** Use this method to set the current state (after restoring it).
		NOTE: The state buffer is *not* automatically updated; you *must*
		call the GetFileState() method each time before persisting the
		buffer to disk (or however else).
		@param file state buffer
		@return true on success
	 */
	bool SetFileState( const ReadUserLog::FileState &state );

	/** For debugging; this is used to dump the current file state
		@param string to write the state info into
		@param label to put at the header of the string
	 */
	void FormatFileState( MyString &str, const char *label = NULL ) const;
	
  private:

    /** Internal initializer.  This function will return FALSE
        if it can't open the log file (among other things).
		@param Handle file rotation?
		@param check_for_old try to open rotated (".old") file first?
		@param Restore previous file position
		@param Enable reading of the file header?
        @return TRUE for success
    */
    bool initialize ( bool handle_rotation,
					  bool check_for_rotated,
					  bool restore_position,
					  bool enable_header_read );

	/** Internal lock/unlock methods
		@param Verify that initialization has been done
	 */
	void Lock( bool verify_init );
	void Unlock( bool verify_init );
	
	/** Set all members to their cleared values.
	*/
	void clear( void );

	/** Release all resources this object has claimed/allocated.
	*/
	void releaseResources( void );

    /** Read the next event from the log file.  The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
		@param store the state after the read?
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome readEvent (ULogEvent * & event, bool store_state );

    /** Raw read of the next event from the log file.
        @param event pointer to be set to new object
		@param should the caller try the read again (after reopen) (returned)?
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome readEvent (ULogEvent * & event, bool *try_again );

	/** Determine the type of log this is; note that if called on an
	    empty log, this will return TRUE and the log type will stay
		LOG_TYPE_UNKNOWN.
        @return TRUE for success, FALSE otherwise
	*/
	bool determineLogType( void );

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

	/** Reopen the log file
		@return the outcome of the re-open attempt
	 */
	ULogEventOutcome ReopenLogFile( void );

	/** Find the previous log file starting with start
		@param log file rotation # to start the search with
		@param max number of files to look for before giving up (0==all)
		@param Store stat in state?
		@return true:success, false:none found
	 */
	bool FindPrevFile( int start, int num, bool store_stat );

	/** Open the log file
		@param Seek to previous location?
		@param Read file header?
		@return outcome of the open attempt
	 */
	ULogEventOutcome OpenLogFile( bool do_seek, bool read_header = true );

	/** Close the log file between operations
		@return true:success, false:failure
	 */
	bool CloseLogFile( );


	/** Class private data
	 */
	bool				 m_initialized;	/** Are we initialized? */
	bool				 m_missed_event; /** Need to report event lost? */

	ReadUserLogState	*m_state;		/** The state of the file     */
	ReadUserLogMatch	*m_match;		/** Detects file matches */

    int    				 m_fd;			/** The log's file descriptor */
    FILE				*m_fp;			/** The log's file pointer    */

	bool				 m_close_file;	/** Close file between operations? */
	bool				 m_handle_rot;	/** Do we handle file rotation? */
	int					 m_max_rot;		/** Max rotation number */
	bool				 m_read_header;	/** Read the file's header? */

	bool				 m_lock_file;	/** Should we lock the file?  */
    FileLock			*m_lock;		/** The log file lock         */
	bool				 m_is_locked;	/** Is the file locked?       */
	int					 m_lock_rot;	/** Lock managing what rotation #? */
};

#endif /* __cplusplus */

#if 0 /* deprecated cruft */
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
#endif

#endif /* _CONDOR_USER_LOG_CPP_H */
