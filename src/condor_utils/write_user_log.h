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
#ifndef _CONDOR_WRITE_USER_LOG_CPP_H
#define _CONDOR_WRITE_USER_LOG_CPP_H

#if defined(__cplusplus)

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */

#include "condor_event.h"
#include <string>
#include <vector>
#define XML_USERLOG_DEFAULT 0

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifndef WIN32
#  include <unistd.h>
#endif

/* Predeclare some classes */
class MyString;
class UserLogHeader;
class FileLockBase;
class FileLock;
class StatWrapper;
class ReadUserLogHeader;
class WriteUserLogState;


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
class WriteUserLog
{
  public:
    ///
    WriteUserLog( bool disable_event_log = false );
    
    /** Constructor
        @param owner Username of the person whose job is being logged
        @param file the path name of the log file to be written (copied)
        @param clu  condorID cluster to put into each ULogEvent
        @param proc condorID proc    to put into each ULogEvent
        @param subp condorID subproc to put into each ULogEvent
		@param xml  make this true to write XML logs, false to use the old form
		@param gjid global job ID
    */
    WriteUserLog(const char *owner, const char *domain,
				 const std::vector<const char*>& file,
				 int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT,
				 const char *gjid = NULL);
    WriteUserLog(const char *owner, const char *domain,
				 const char* file,
				 int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT,
				 const char *gjid = NULL);
    
    WriteUserLog(const char *owner, const char *file,
				 int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT);
    WriteUserLog(const char *owner, const std::vector<const char *>& file,
				 int clu, int proc, int subp, bool xml = XML_USERLOG_DEFAULT);
    ///
    virtual ~WriteUserLog();
    
    /** Initialize the log file, if not done by constructor.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
        @param gjid the condor global job id to put into each ULogEvent
		@return true on success
    */
    bool initialize(const char *owner, const char *domain,
			   const std::vector<const char *>& file,
			   int c, int p, int s, const char *gjid);
    bool initialize(const char *owner, const char *domain,
			   const char *file, int c, int p, int s, const char *gjid);
    
    /** Initialize the log file.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return true on success
    */
    bool initialize(const char *file, int c, int p, int s,
			const char *gjid);
    bool initialize(const std::vector<const char *>& file, int c, int p, int s,
			const char *gjid);
   
#if !defined(WIN32)
    /** Initialize the log file (PrivSep mode only)
        @param uid the user's UID
        @param gid the user's GID
        @param vector of filenames of log files to be written
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return true on success
    */
    bool initialize(uid_t, gid_t, const std::vector<const char *>&, int, int, int,
			const char *gjid);
    bool initialize(uid_t u, gid_t g, const char *f, int c, int p, int s,
				const char *gjid) {
			return initialize(u,g,std::vector<const char*>(1,f),c,p,s,gjid);
		}
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
		@param force Force a reconfigure; otherwise Configure() will
		  do nothing if the object is already configured
		@return true on success
	*/
	bool Configure( bool force = true );

	void setUseXML(bool new_use_xml){ m_use_xml = new_use_xml; }

	/** Enable / disable writing of user or global logs
	 */
	bool setEnableUserLog( bool enable ) {
		bool tmp = m_userlog_enable;
		m_userlog_enable = enable;
		return tmp;
	};
	bool setEnableGlobalLog( bool enable ) {
		bool tmp = !m_global_disable;
		m_global_disable = !enable;
		return tmp;
	};

	void setCreatorName(const char *);

	/** Verify that the event log is initialized
		@return true on success
	 */
	bool isInitialized( void ) const {
		return ( !logs.empty() || (m_global_fp != NULL) );
	};

    /** Write an event to the log file.  Caution: if the log file is
        not initialized, then no event will be written, and this function
        will return a successful value.  Also, if the global event log
		is enabled, but writing to that log fails, this function will
		still return success if it is able to write to the "normal" log.

        @param event the event to be written
		@param related job ad
		@param was the event actually written (see above caution).
        @return false for failure, true for success
    */
    bool writeEvent (ULogEvent *event, ClassAd *jobad = NULL,
					 bool *written = NULL );

	/** This function is just like writeEvent(), but it ensures
		that no call to fsync is made on the user log.
	*/
	bool writeEventNoFsync (ULogEvent *event, ClassAd *jobad = NULL,
							bool *written = NULL );

	/**Control whether writeEvent() calls fsync.  This overrides the
	   configured default ENABLE_USERLOG_FSYNC.  This only applies to
	   the user log, not the global event log.
	   @param enabled If true, enable fsync in writeEvent()
	 */
	void setEnableFsync(bool enabled);

	/**@return false if disabled, true if enabled*/
	bool getEnableFsync();

	/** APIs for testing */
	int getGlobalSequence( void ) const { return m_global_sequence; };


	// Rotation callbacks -- for testing purposes *ONLY*

	/** Notification that the global log is about to be rotated
		@param current size of the file
		@return false to prevent rotation
	*/
	virtual bool globalRotationStarting( unsigned long /*filesize*/ )
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


	// Methods for internal use *ONLY*:

    /** Write an event to the global log file.  Caution: if the log
        file is not initialized, then no event will be written, and
        this function will return a successful value.  This is a
        specialized method designed for internal use in the writing of
        the event log header.

        @param event the event to be written
        @param file pointer to write event to (or NULL for default global FP)
        @param Seek to the start of the file before writing event?
        @return 0 for failure, 1 for success
    */
    int writeGlobalEvent ( ULogEvent &event,
						   FILE *fp,
						   bool is_header_event = true );


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
	bool getGlobalLogSize( unsigned long &, bool use_fp );

	bool isGlobalEnabled( void ) const {
		return ( ( m_global_disable == false ) && ( NULL != m_global_path ) );
	};
	
	void AddToMask(ULogEventNumber e) {
		mask.push_back(e);
	}

  private:

	///
    void Reset( void );
    bool internalInitialize(int c, int p, int s, const char *gjid);
	void FreeAllResources( void );
	void FreeGlobalResources( bool final );
	void FreeLocalResources( void );
	const char *GetGlobalIdBase( void );

	// Write header event to global file
	bool writeHeaderEvent ( const UserLogHeader &header );

	bool openFile( const char *file,
				   bool log_as_user,
				   bool use_lock,
				   bool append,
				   FileLockBase *& lock, 
				   FILE *& fp );


	bool doWriteEvent( FILE *fp, ULogEvent *event, bool do_use_xml );
	void GenerateGlobalId( MyString &id );

	bool checkGlobalLogRotation(void);
	bool globalLogRotated( ReadUserLogHeader &reader );
	bool openGlobalLog( bool reopen );
	bool openGlobalLog( bool reopen, const UserLogHeader &header );
	bool closeGlobalLog( void);
	int doRotation( const char *path, FILE *&fp,
					MyString &rotated, int max_rotations );


	bool updateGlobalStat( void );

    
    /// Deprecated.  condorID cluster, proc & subproc of the next event.
    int 		m_cluster;
    int         m_proc;
    int         m_subproc;

	/** Write to the user log? */		 bool		m_userlog_enable;
    struct log_file {
    /** Copy of path to the log file */  std::string path;
    /** The log file                 */  FILE     * fp;
    /** The log file lock            */  FileLockBase *lock;
    /** Implementation detail        */  mutable bool copied;

      log_file(const char* p) : path(p), fp(NULL), lock(NULL),
        copied(false) {}
      log_file() : fp(NULL), lock(NULL), copied(false) {}
      log_file(const log_file& orig);
      ~log_file(); 
      log_file& operator=(const log_file& rhs);
    };
	bool doWriteEvent( ULogEvent *event,
		WriteUserLog::log_file& log,
		bool is_global_event,
		bool is_header_event,
		bool use_xml,
		ClassAd *ad);
	void writeJobAdInfoEvent(char const *attrsToWrite,
		WriteUserLog::log_file& log, ULogEvent *event, ClassAd *param_jobad,
		bool is_global_event, bool use_xml );

		std::vector<log_file> logs;
	bool doWriteGlobalEvent( ULogEvent *event, ClassAd *ad);
    /** Enable locking?              */  bool		m_enable_locking;
	/** Enable fsync() after writes? */  bool       m_enable_fsync;

	/** Enable close after writes    */  bool       m_global_close;
	/** Write to the global log? */		 bool		m_global_disable;
    /** Copy of path to global log   */  char     * m_global_path;
    /** The global log file          */  FILE     * m_global_fp;
    /** The global log file lock     */  FileLockBase *m_global_lock;
	/** Whether we use XML or not    */  bool       m_global_use_xml;
	/** The log file uniq ID base    */  char     * m_global_id_base;
	/** The current sequence number  */  int        m_global_sequence;
	/** Count event log events?      */  bool       m_global_count_events;
	/** Max size of event log        */  long		m_global_max_filesize;
	/** Max event log rotations      */  int		m_global_max_rotations;
	/** StatWrapper of global file   */  StatWrapper *m_global_stat;
    /** Enable global locking?       */  bool		m_global_lock_enable;
	/** Enable fsync() after writes? */  bool       m_global_fsync_enable;
	/** State of the log file        */  WriteUserLogState *m_global_state;

    /** Copy of path to rotation lock*/  char     * m_rotation_lock_path;
    /** FD of the rotation lock      */  int        m_rotation_lock_fd;
    /** The global log file lock     */  FileLockBase *m_rotation_lock;

	/** Whether we use XML or not    */  bool       m_use_xml;

#if !defined(WIN32)
	/** PrivSep: the user's UID      */  uid_t      m_privsep_uid;
	/** PrivSep: the user's GID      */  gid_t      m_privsep_gid;
#endif
	/** The GlobalJobID for this job */  char     * m_gjid;

	/** Previously configured?       */  bool       m_configured;
	/** Initialized?                 */  bool       m_initialized;
	/** Creator Name (schedd name)   */  char     * m_creator_name;
	/** Mask for events              */  std::vector<ULogEventNumber> mask;
};

#endif /* __cplusplus */

#endif /* _CONDOR_USER_LOG_CPP_H */

/*
### Local Variables: ***
### mode:C++ ***
### End: **
*/
