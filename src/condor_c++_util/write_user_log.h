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


#ifndef _CONDOR_WRITE_USER_LOG_CPP_H
#define _CONDOR_WRITE_USER_LOG_CPP_H

#if defined(__cplusplus)

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */

#if defined(NEW_PROC)
#  include "proc.h"
#endif
#include "condor_event.h"

#define XML_USERLOG_DEFAULT 0

#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

/* Predeclare some classes */
class MyString;
class UserLogHeader;
class FileLockBase;
class FileLock;
class ReadUserLogHeader;


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
class UserLog
{
  public:
    ///
    UserLog( void );
    
    /** Constructor
        @param owner Username of the person whose job is being logged
        @param file the path name of the log file to be written (copied)
        @param clu  condorID cluster to put into each ULogEvent
        @param proc condorID proc    to put into each ULogEvent
        @param subp condorID subproc to put into each ULogEvent
		@param xml  make this true to write XML logs, false to use the old form
		@param gjid global job ID
    */
    UserLog(const char *owner, const char *domain, const char *file,
			int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT,
			const char *gjid = NULL);
    
    UserLog(const char *owner, const char *file,
			int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT);
    ///
    virtual ~UserLog();
    
    /** Initialize the log file, if not done by constructor.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
        @param gjid the condor global job id to put into each ULogEvent
		@return true on success
    */
    bool initialize(const char *owner, const char *domain, const char *file,
		   	int c, int p, int s, const char *gjid);
    
    /** Initialize the log file.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return true on success
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
		@return true on success
    */
    bool initialize(uid_t, gid_t, const char *, int, int, int, const char *gjid);
#endif

    /** Initialize the condorID, which will fill in the condorID
        for each ULogEvent passed to writeEvent().

        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return true on success
    */
    bool initialize(int c, int p, int s, const char *gjid);

	/** Read in the configuration parameters
		@return true on success
	*/
	bool Configure( void );

	void setUseXML(bool new_use_xml){ m_use_xml = new_use_xml; }

	void setWriteUserLog(bool b){ m_write_user_log = b; }
	void setWriteGlobalLog(bool b){ m_write_global_log = b; }

    /** Write an event to the log file.  Caution: if the log file is
        not initialized, then no event will be written, and this function
        will return a successful value.

        @param event the event to be written
        @return false for failure, true for success
    */
    bool writeEvent (ULogEvent *event, ClassAd *jobad = NULL);

    /** Write an event to the global log file.  Caution: if the log file is
        not initialized, then no event will be written, and this function
        will return a successful value.  This is a specialized method
		for designed internal use in the writing of the event log header.

        @param event the event to be written
        @param file pointer to write event to (or NULL for default global FP)
        @param Seek to the start of the file before writing event?
        @return 0 for failure, 1 for success
    */
    int writeGlobalEvent ( ULogEvent &event,
						   FILE *fp,
						   bool is_header_event = true );

	/** APIs for testing */
	int getGlobalSequence( void ) const { return m_global_sequence; };


	// Rotation callbacks -- for testing purposes only

	/** Notification that the global log is about to be rotated
		@param current size of the file
		@return false to prevent rotation
	*/
	virtual bool globalRotationStarting( long /*filesize*/ )
		{ return true; };

	/** Notification that number of events in the global log
		have been counted
		Note: this will only be called if EVENT_LOG_COUNT_EVENTS is true
		@param number of events counted
	*/
	virtual void globalRotationEvents( int /*events*/ )
		{ return; };

	/** Notification that the global log has finished rotation
		@param number of files rotated
	*/
	virtual void globalRotationComplete( int /*num_rotations*/,
										 int /*sequence*/,
										 const MyString & /*id*/ )
		{ return; };


	// Accessor methods for testing *ONLY*:

	//  get/set cluster/proc/subproc values
	int getGlobalCluster( void ) const { return m_cluster; };
	void setGlobalCluster( int cluster ) { m_cluster = cluster; };
	int getGlobalProc( void ) const { return m_proc; };
	void setGlobalProc( int proc ) { m_proc = proc; };
	int getGlobalSubProc( void ) const { return m_subproc; };
	void setGlobalSubProc( int subproc ) { m_subproc = subproc; };

	// Get the global log file and it's size
	const char *getGlobalPath( void ) const { return m_global_path; };
	long getGlobalLogSize( void ) const;

  private:

	///
    void Reset( void );

	// Write header event to global file
	bool writeHeaderEvent ( const UserLogHeader &header );

	bool openFile( const char *file,
				   bool log_as_user,
				   bool use_lock,
				   bool append,
				   FileLockBase *& lock, 
				   FILE *& fp );

	bool doWriteEvent( ULogEvent *event,
					   bool is_global_event,
					   bool is_header_event,
					   ClassAd *ad);

	bool doWriteEvent( FILE *fp, ULogEvent *event, bool do_use_xml );
	void GenerateGlobalId( MyString &id );

	bool checkGlobalLogRotation(void);
	bool globalLogRotated( ReadUserLogHeader &reader );
	bool initializeGlobalLog(const UserLogHeader &header );
	int doRotation( const char *path, FILE *&fp,
					MyString &rotated, int max_rotations );

    
    /// Deprecated.  condorID cluster, proc & subproc of the next event.
    int 		m_cluster;
    int         m_proc;
    int         m_subproc;

	/** Write to the user log? */		 bool		m_write_user_log;
    /** Copy of path to the log file */  char     * m_path;
    /** The log file                 */  FILE     * m_fp;
    /** The log file lock            */  FileLockBase *m_lock;

    /** Enable locking?              */  bool		m_enable_locking;

	/** Write to the global log? */		 bool		m_write_global_log;
    /** Copy of path to global log   */  char     * m_global_path;
    /** The global log file          */  FILE     * m_global_fp;
    /** The global log file lock     */  FileLockBase *m_global_lock;
	/** Whether we use XML or not    */  bool       m_global_use_xml;
	/** The log file uniq ID base    */  char     * m_global_uniq_base;
	/** The current sequence number  */  int        m_global_sequence;
	/** Count event log events?      */  bool       m_global_count_events;
	/** Max size of event log        */  long		m_global_max_filesize;
	/** Max event log rotations      */  int		m_global_max_rotations;
	/** Current global log file size */  long		m_global_filesize;

    /** Copy of path to rotation lock*/  char     * m_rotation_lock_path;
    /** FD of the rotation lock      */  int        m_rotation_lock_fd;
    /** The global log file lock     */  FileLock * m_rotation_lock;

	/** Whether we use XML or not    */  bool       m_use_xml;

#if !defined(WIN32)
	/** PrivSep: the user's UID      */  uid_t      m_privsep_uid;
	/** PrivSep: the user's GID      */  gid_t      m_privsep_gid;
#endif
	/** The GlobalJobID for this job */  char     * m_gjid;
};


#endif /* __cplusplus */

#endif /* _CONDOR_USER_LOG_CPP_H */

/*
### Local Variables: ***
### mode:C++ ***
### End: **
*/
