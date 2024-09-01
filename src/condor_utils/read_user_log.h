/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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


#ifndef _CONDOR_READ_USER_LOG_CPP_H
#define _CONDOR_READ_USER_LOG_CPP_H

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */
#include "condor_event.h"

/* Predeclare some classes */
class FileLockBase;
class FileLock;


/** API for reading a log file.

    This class was written at the same time the ULogEvent class was written,
    so it does not contain the old deprecated functions and parameters that
    the WriteUserLog class is plagued with.
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

	/** Returned values from the file status check operation
	 */
	enum FileStatus {
		LOG_STATUS_ERROR = -1,
		LOG_STATUS_NOCHANGE,
		LOG_STATUS_GROWN,
		LOG_STATUS_SHRUNK
	};

	/** Simple error information
	 */
	enum ErrorType {
		LOG_ERROR_NONE,					/* No error */
		LOG_ERROR_NOT_INITIALIZED,		/* Reader not initialized */
		LOG_ERROR_RE_INITIALIZE,		/* Attempt to re-initialize */
		LOG_ERROR_FILE_NOT_FOUND,		/* Log file not found */
		LOG_ERROR_FILE_OTHER,			/* Other file error */
		LOG_ERROR_STATE_ERROR			/* Invalid state */
	};

    /** Default constructor.
        @param isEventLog setup reader to read the global event log?
	*/
    ReadUserLog( bool isEventLog = false );

    /** Constructor for a limited functionality reader
		- No rotation handling
		- No locking
        @param fp the file to read from
		@param XML mode?
		@param close the FP when done?
	*/
    ReadUserLog( FILE *fp, UserLogType log_type, bool enable_close = false );

    /** Constructor.
        @param filename the file to read from
	*/
    ReadUserLog(const char * filename, bool read_only = false );

    /** Constructor.
        @param State to restore from
		@param Read only access to the file (locking disabled)
	*/
    ReadUserLog( const FileState &state, bool read_only = false );

    /** Destructor.
	*/
    ~ReadUserLog() { releaseResources(); }

    /** Detect whether the object has been initialized
	*/
    bool isInitialized( void ) const { return m_initialized; };


    /** Initialize to read the EventLog file.  This function will return
		false if it can't open the event log (among other problems).
        @return true for success
    */
    bool initialize ( void );

    /** Initialize the log file.  This function will return false
        if it can't open the log file (among other problems).
        @param file the file to read from
		@param handle_rotation enables the reader to handle rotating
		 log files (only useful for global user logs)
		@param check_for_rotated try to open the rotated
		 (".old" or ".1", ".2", ..) files first?
        @return true for success
    */
    bool initialize (const char *filename,
					 bool handle_rotation = false,
					 bool check_for_rotated = false,
					 bool read_only = false );

    /** Initialize the log file.  Similar to above, but takes an integer
		as its second parameter. This function will return false
        if it can't open the log file (among other problems).
        @param file the file to read from
		@param max_rotation sets max rotation file # the reader will
		 try to read (0 disables) (only useful for global user logs)
		@param check_for_rotated try to open the rotated
		 (".old" or ".1", ".2", ..) files first?
        @return true for success
    */
    bool initialize (const char *filename,
					 int max_rotation,
					 bool check_for_rotated = true,
					 bool read_only = false );

    /** Initialize the log file from a saved state.
		This function will return false if it can't open the log file
		 (among other problems).
        @param FileState saved file state
		@param Read only access to the file (locking disabled)
        @return true for success
    */
    bool initialize (const ReadUserLog::FileState &state,
					 bool read_only = false );

    /** Initialize the log file from a saved state and set the
		  rotation parameters
		This function will return false if it can't open the log file
		 (among other problems).
        @param FileState saved file state
        @param Maximum rotation files
		@param Read only access to the file (locking disabled)
        @return true for success
    */
    bool initialize (const ReadUserLog::FileState &state,
					 int max_rotations,
					 bool read_only = false );

    /** Read the next event from the log file.  The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
        @return the outcome of attempting to read the log
    */
	ULogEventOutcome readEvent (ULogEvent * & event) { return internalReadEvent( event, true ); }

    /** Synchronize the log file if the last event read was an error.  This
        safe guard function should be called if there is some error reading an
        event, but there are events after it in the file.  Will skip over the
        "bad" event (i.e., read up to and including the event separator (...))
        so that the rest of the events can be read.
        @return true: success, false: failure
    */
    bool synchronize ();

	/** Output the current file position
	 */
	void outputFilePos(const char *pszWhereAmI);

	/** Lock / unlock the file
	 */
	//void Lock(void)   { Lock(true);   };
	//void Unlock(void) { Unlock(true); };

	/** Enable / disable locking
	 */
	void setLock( bool enable ) { m_lock_enable = enable; };

	/** Set the # of rotations used
	 */
	void setRotations( int rotations ) { m_max_rotations = rotations; }

	/** Set whether the log file should be treated as XML. The constructor will
		attempt to figure this out on its own.
		@param is_xml should be true if we have an XML log file,
		 false otherwise
	*/
	void setLogType( UserLogType  log_type );

    /** Check the status of the file - has it grown, shrunk, etc.
		@return LOG_STATUS_{ERROR,NOCHANGE,GROWN,SHRUNK}
	*/
	FileStatus CheckFileStatus( void );	

    /** Check the status of the file - has it grown, shrunk, etc.
		@param reference: returned as true of the file is empty
		@return LOG_STATUS_{ERROR,NOCHANGE,GROWN,SHRUNK}
	*/
	FileStatus CheckFileStatus( bool &is_emtpy );	

	/** Set whether the log file should be treated as "old-style" (non-XML).
	    The constructor will attempt to figure this out on its own.
		@param is_old should be true if we have a "old-style" log file,
		false otherwise
	*/
	//void setIsOldLog( bool is_old );

	/** Determine whether this ReadUserLog thinks its log file is "old-style"
	    (non-XML).
		Note that false may mean that the type is not yet known (e.g.,
		the log file is empty).
		@return true if "old-style", false otherwise
	*/
	//bool getIsOldLog( void ) const;

	/** Get error data
		@param error type
		@param source line # where error was detected
		@return void
	 */
	void getErrorInfo( ErrorType &error,
					   const char *& error_str,
					   unsigned &line_num ) const;

	/** These methods only do anything useful for anything other
	    than job event logs (which do not rotate). */
	size_t getOffset() const { return ftell(m_fp); }
	void setOffset(size_t offset) { fseek(m_fp, offset, SEEK_SET); }

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
		  status = reader.initialize( statebuf );

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
	static bool UninitFileState( ReadUserLog::FileState &state );

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

	/** Release all resources this object has claimed/allocated.
	*/
	void releaseResources( void );

  private:

    /** Internal initializer from saved state.
		This function will return falseif it can't open the log file
		 (among other problems).
        @param FileState saved file state
        @param Change the max rotation value?
		@param max_rotations max rotations value of the logs
        @return true for success
    */
    bool InternalInitialize ( const ReadUserLog::FileState &state,
							  bool set_rotations,
							  int max_rotations,
							  bool read_only );

    /** Internal initializer.  This function will return false
        if it can't open the log file (among other things).
		@param max_rotation sets max rotation file # the reader will
		 try to read (0 disables) (only useful for global user logs)
		@param Max number of rotations to handle (0 = none)
		@param Restore previous file position
		@param Enable reading of the file header?
        @return true for success
    */
    bool InternalInitialize ( int max_rotation,
							  bool check_for_rotated,
							  bool restore_position,
							  bool enable_header_read,
							  bool read_only );

	/** Initialize rotation parameters
		@param Max number of rotations to handle (0 = none)
	 */
	void initRotParms( int max_rotation );

	/** Get max rotation parameter based
		@param Handle rotations?
	 */
	int getMaxRot( bool handle_rotation );

	/** Internal lock/unlock methods
	 */
	bool Lock();
	bool Unlock();

	/** Set all members to their cleared values.
	*/
	void clear( void );

    /** Read the next event from the log file.  The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
		@param store the state after the read?
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome internalReadEvent (ULogEvent * & event, bool store_state );

    /** Raw read of the next event from the log file.
        @param event pointer to be set to new object
		@param should the caller try the read again (after reopen) (returned)?
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome rawReadEvent (ULogEvent * & event, bool *try_again );

	/** Determine the type of log this is; note that if called on an
	    empty log, this will return true and the log type will stay
		LOG_TYPE_UNKNOWN.
        @return true for success, false otherwise
	*/
	bool determineLogType( void );

	/** Skip the XML header of this log, if there is one.
	    @param the first character after the opening '<'
		@param the initial file position
        @return true for success, false otherwise
	*/
	bool skipXMLHeader(int afterangle, long filepos);

    /** Read the next event from the XML log file. The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome readEventClassad (ULogEvent * & event, int log_type);

    /** Read the next event from the old style log file. The event pointer to
        set to point to a newly instatiated ULogEvent object.
        @param event pointer to be set to new object
        @return the outcome of attempting to read the log
    */
    ULogEventOutcome readEventNormal (ULogEvent * & event);

	/** Reopen the log file
		@param Restore from state?
		@return the outcome of the re-open attempt
	 */
	ULogEventOutcome ReopenLogFile( bool restore = false );

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
		@param force close even if "don't close" mode is set?
		@return true:success, false:failure
	 */
	bool CloseLogFile( bool force );

	/** Report error
		@param error type
		@param line number where error was detected
	*/
	void Error( ErrorType error, unsigned line_num ) const
		{ m_error = error; m_line_num = line_num; };


	/** Class private data
	 */
	bool				 m_initialized;	  /** Are we initialized? */
	bool				 m_missed_event;  /** Need to report event lost? */

	ReadUserLogState	*m_state;		  /** The state of the file */
	ReadUserLogMatch	*m_match;		  /** Detects file matches */

    int    				 m_fd;			  /** The log's file descriptor */
    FILE				*m_fp;			  /** The log's file pointer */

	bool				 m_close_file;	  /** Close file between operations? */
	bool				 m_enable_close;  /** enable close operations? */
	bool				 m_handle_rot;	  /** Do we handle file rotation? */
	int					 m_max_rotations; /** Max rotation number */
	bool				 m_read_header;	  /** Read the file's header? */
	bool				 m_read_only;	  /** Open file read only? */

	bool				 m_lock_enable;	  /** Should we lock the file? */
    FileLockBase		*m_lock;		  /** The log file lock */
	int					 m_lock_rot;	  /** Lock managing what rotation #? */

	/* Error history data */
	mutable ErrorType	 m_error;		/** Type of latest error (think errno) */
	mutable unsigned	 m_line_num;	/** Line number of latest error */
};


// Provide access to parts of the user log state data
class ReadUserLogFileState;
class ReadUserLogStateAccess
{
public:
	
	ReadUserLogStateAccess(const ReadUserLog::FileState &state);
	~ReadUserLogStateAccess(void);

	// Is the buffer initialized?
	bool isInitialized( void ) const;

	// Is the buffer valid for use by the ReadUserLog::initialize()?
	bool isValid( void ) const;

	// Position in individual file
	// Note: Can return an error if the result is too large
	//  to be stored in a long
	bool getFileOffset( unsigned long &pos ) const;

	// Positional difference between to states (this - other)
	bool getFileOffsetDiff( const ReadUserLogStateAccess &other,
							long &diff ) const;

	// Event number in individual file
	// Note: Can return an error if the result is too large
	//  to be stored in a long
	bool getFileEventNum( unsigned long &num ) const;

	// # of events between to states (this - other)
	bool getFileEventNumDiff( const ReadUserLogStateAccess &other,
							  long &diff ) const;

	// Position OF THE CURRENT FILE in overall log
	// Note: Can return an error if the result is too large
	//  to be stored in a long
	bool getLogPosition( unsigned long &pos ) const;

	// Positional difference between to states (this - other)
	bool getLogPositionDiff( const ReadUserLogStateAccess &other,
							 long &diff ) const;

	// Absolute event number OF THE FIRST EVENT IN THE CURRENT FILE
	// Note: Can return an error if the result is too large
	//  to be stored in a long
	bool getEventNumber( unsigned long &num ) const;

	// # of events between to states (this - other)
	bool getEventNumberDiff( const ReadUserLogStateAccess &other,
						  long &diff ) const;

	// Get the unique ID and sequence # of the associated state file
	bool getUniqId( char *buf, int len ) const;
	bool getSequenceNumber( int &seqno ) const;

	// Access to the state
protected:
	bool getState( const ReadUserLogFileState *&state ) const;

private:
	const ReadUserLogFileState	*m_state;
};

#endif /* _CONDOR_USER_LOG_CPP_H */
