/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#if defined(__cplusplus)

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */
#include "condor_event.h"
#include "MyString.h"
#include "user_log_config.h"
#include "condor_sys_formats.h"

#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

/* Predeclare some classes */
class MyString;
class FileLockBase;
class FileLock;
class UserLog;


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

	/** Returned values from the file status check operation
	 */
	enum FileStatus {
		LOG_STATUS_ERROR = -1,
		LOG_STATUS_NOCHANGE,
		LOG_STATUS_GROWN,
		LOG_STATUS_SHRUNK,
	};

    /** Default constructor.
	*/
    ReadUserLog(void) { clear(); }

    /** Constructor for a limited functionality reader
		- No rotation handling
		- No locking
        @param fp the file to read from
	*/
    ReadUserLog( FILE *fp, bool is_xml );

    /** Constructor.
        @param filename the file to read from
	*/
    ReadUserLog(const char * filename);

    /** Constructor.
        @param State to restore from
	*/
    ReadUserLog( const FileState &state );
                                      
    /** Destructor.
	*/
    ~ReadUserLog() { releaseResources(); }
                                      
    /** Detect whether the object has been initialized
	*/
    bool isInitialized( void ) const { return m_initialized; };


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
					 bool check_for_rotated = false );

    /** Initialize the log file.  Similar to above, but takes an integer
		as it's second parameter. This function will return false
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
					 bool check_for_rotated = true );

    /** Initialize the log file from a saved state.
		This function will return false if it can't open the log file
		 (among other problems).
        @param FileState saved file state
        @return true for success
    */
    bool initialize (const ReadUserLog::FileState &state );

    /** Initialize the log file from a saved state and set the
		  rotation parameters
		This function will return false if it can't open the log file
		 (among other problems).
        @param FileState saved file state
        @param  Maximum rotation files
        @return true for success
    */
    bool initialize (const ReadUserLog::FileState &state, int max_rotations );

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
        @return true: success, false: failure
    */
    bool synchronize ();

	/** Output the current file position
	 */
	void outputFilePos(const char *pszWhereAmI);

	/** Lock / unlock the file
	 */
	void Lock(void)   { Lock(true);   };
	void Unlock(void) { Unlock(true); };

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
	void setIsXMLLog( bool is_xml );

	/** Determine whether this ReadUserLog thinks its log file is XML.
		Note that false may mean that the type is not yet known (e.g.,
		the log file is empty).
		@return true if XML, false otherwise
	*/
	bool getIsXMLLog( void ) const;

    /** Check the status of the file - has it grown, shrunk, etc.
		@return LOG_STATUS_{ERROR,NOCHANGE,GROWN,SHRUNK}
	 */
    FileStatus CheckFileStatus( void );

	/** Set whether the log file should be treated as "old-style" (non-XML).
	    The constructor will attempt to figure this out on its own.
		@param is_old should be true if we have a "old-style" log file,
		false otherwise
	*/
	void setIsOldLog( bool is_old );

	/** Determine whether this ReadUserLog thinks its log file is "old-style"
	    (non-XML).
		Note that false may mean that the type is not yet known (e.g.,
		the log file is empty).
		@return true if "old-style", false otherwise
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

	/** For debugging; this is used to dump the current file state
		@param string to write the state info into
		@param label to put at the header of the string
	 */
	void FormatFileState( MyString &str, const char *label = NULL ) const;
	void FormatFileState( const ReadUserLog::FileState &state,
						  MyString &str, const char *label = NULL ) const;

	/** Get specific pieces of data about the current file state:
		Get base file path from current state.
		@return string with the base path
	 */
	const char *FileStateGetBasePath( void ) const;

	/** Get specific pieces of data about the current file state:
		Get base file path from passed in state.
		@return string with the base path
	 */
	const char *FileStateGetBasePath( const ReadUserLog::FileState & ) const;
	
	/** Get specific pieces of data about the current file state:
		Get current file path from current state.
		@return string with the current path
	 */
	const char *FileStateGetCurrentPath( void ) const;

	/** Get specific pieces of data about the current file state:
		Get current file path from passed in state.
		@return string with the current path
	 */
	const char *FileStateGetCurrentPath( const ReadUserLog::FileState & )const;
	
	/** Get specific pieces of data about the current file state:
		Get current file rotation # from current state.
		@return the current file rotation number
	 */
	int FileStateGetRotation( void ) const;

	/** Get specific pieces of data about the current file state:
		Get current file rotation number from passed in state.
		@return the current file rotation number
	 */
	int FileStateGetRotation( const ReadUserLog::FileState & ) const;

	/** Get specific pieces of data about the current file state:
		Get current file offset current state.
		@return the current file offset
	 */
	filesize_t FileStateGetOffset( void ) const;

	/** Get specific pieces of data about the current file state:
		Get current file offset from passed in state.
		@return the current file offset
	 */
	filesize_t FileStateGetOffset( const ReadUserLog::FileState & ) const;

	/** Get specific pieces of data about the current file state:
		Get current global log position
		@return the global log position
	 */
	filesize_t FileStateGetGlobalPosition( void ) const;

	/** Get specific pieces of data about the current file state:
		Get current file offset from passed in state.
		@return the current file offset TODO
	 */
	filesize_t FileStateGetGlobalPosition( const ReadUserLog::FileState & )
		const;

	/** Get specific pieces of data about the current file state:
		Get current file file offset current state.
		@return the current file offset TODO
	 */
	filesize_t FileStateGetGlobalRecordNo( void ) const;

	/** Get specific pieces of data about the current file state:
		Get current file offset from passed in state.
		@return the current file offset TODO
	 */
	filesize_t FileStateGetGlobalRecordNo( const ReadUserLog::FileState & )
		const;
	
  private:

    /** Internal initializer from saved state.
		This function will return falseif it can't open the log file
		 (among other problems).
        @param FileState saved file state
        @param Change the max rotation value?
		@param max_rotations max rotations value of the logs
        @return true for success
    */
    bool InternalInitialize (const ReadUserLog::FileState &state,
							 bool set_rotations,
							 int max_rotations = 0 );

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
							  bool enable_header_read ,
							  bool force_disable_locking = false );

	/** Initialize rotation parameters
		@param Max number of rotations to handle (0 = none)
	 */
	void initRotParms( int max_rotation );

	/** Get max rotation parameter based
		@param Handle rotations?
	 */
	int getMaxRot( bool handle_rotation );

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

	bool				 m_never_close_fp; /** Never close the FP? */
	bool				 m_close_file;	/** Close file between operations? */
	bool				 m_handle_rot;	/** Do we handle file rotation? */
	int					 m_max_rotations; /** Max rotation number */
	bool				 m_read_header;	/** Read the file's header? */

	bool				 m_lock_enable;	/** Should we lock the file?  */
    FileLockBase		*m_lock;		/** The log file lock         */
	int					 m_lock_rot;	/** Lock managing what rotation #? */
};

#endif /* __cplusplus */

#endif /* _CONDOR_USER_LOG_CPP_H */

/*
### Local Variables: ***
### mode:C++ ***
### End: **
*/
