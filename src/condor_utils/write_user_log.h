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
#include <map>
#include <set>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifndef WIN32
#  include <unistd.h>
#endif

/* Predeclare some classes */
class UserLogHeader;
class FileLockBase;
class FileLock;
class StatWrapper;
class ReadUserLogHeader;
class WriteUserLogState;
class CondorError;

/*
	This function tells the caller if a UserLog object should be
	constructed or not, and if so, says where the user wants the user
	log file to go. The difference between this function and simply
	doing a LookupString() on ATTR_ULOG_FILE is that A) the result is
	combined with IWD if necessary to form an absolute path, and B) if
	EVENT_LOG is defined in the condor_config file, then the result
	will be /dev/null even if ATTR_ULOG_FILE is not defined (since we
	still want a UserLog object in this case so the global event log
	is updated). Return function is true if ATTR_ULOG_FILE is found or
	if EVENT_LOG is defined, else false.
*/
bool getPathToUserLog(const classad::ClassAd *job_ad, std::string &result,
                      const char* ulog_path_attr = NULL);


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
    typedef std::set<std::pair<int, int> > log_file_cache_refset_t;

    struct log_file {
    /** Copy of path to the log file */  std::string path;
    /** The log file lock            */  FileLockBase *lock;
    /** The log file                 */  int fd;
    /** Implementation detail        */  mutable bool copied;
    /** Whether to use user priv     */  bool user_priv_flag;

      // set of jobs that are using this log file
      log_file_cache_refset_t refset;

      log_file(const char* p) : path(p), lock(NULL), fd(-1),
        copied(false), user_priv_flag(false) {}
      log_file() : lock(NULL), fd(-1), copied(false), user_priv_flag(false) {}
      log_file(const log_file& orig);
      ~log_file(); 
      log_file& operator=(const log_file& rhs);
      void set_user_priv_flag(bool v) { user_priv_flag = v; }
      bool get_user_priv_flag() const { return user_priv_flag; }
    };

    typedef std::map<std::string, log_file*> log_file_cache_map_t;

    WriteUserLog();
    
    virtual ~WriteUserLog();
    
    /** Initialize the log file.
        @param file the path name of the log file to be written (copied)
        @param c the condor ID cluster to put into each ULogEvent
        @param p the condor ID proc    to put into each ULogEvent
        @param s the condor ID subproc to put into each ULogEvent
		@return true on success
    */
    bool initialize(const char *file, int c, int p, int s, int format_opts = USERLOG_FORMAT_DEFAULT);
private:
    bool initialize(const std::vector<const char *>& file, int c, int p, int s);
public:

	/* Initialize the log writer based on the given job ad.
	 * Check for the user log, dagman log, and global event log.
	 * Switch to user/condor priv for file I/O.
	 * If init_user=true, call init_user_ids() (uninit_user_ids() will
	 * then be called in the destructor).
	 * Return true on success (even if no files will be written to).
	 */
	bool initialize(const ClassAd &job_ad, bool init_user = false);

    /* Set the job id, which will used for each ULogEvent passed to
     * writeEvent(). Does not do any other initialization.
     */
    void setJobId(int c, int p, int s);

	/** Read in the configuration parameters
		@param force Force a reconfigure; otherwise Configure() will
		  do nothing if the object is already configured
		@return true on success
	*/
	bool Configure( bool force = true );

	void setUseCLASSAD(int fmt_type); // 0 = not key value pair, 1 = XML, 2 = JSON

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

    void setLogFileCache(log_file_cache_map_t* cache) { log_file_cache = cache; }
    void freeLogs();

	// Returns whether any files are configured to be written to.
	// I.e. will a call to writeEvent() try to write anything.
	bool willWrite() const {
		return ( !logs.empty() || (m_global_fd >= 0) );
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
	bool getEnableFsync() const;

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
										 const std::string & /*id*/ )
		{ return; };


	// Methods for internal use *ONLY*:

    /** Write an event to the global log file.  Caution: if the log
        file is not initialized, then no event will be written, and
        this function will return a successful value.  This is a
        specialized method designed for internal use in the writing of
        the event log header.

        @param event the event to be written
        @param file pointer to write event to (or NULL for default global FD)
        @param Seek to the start of the file before writing event?
        @return 0 for failure, 1 for success
    */
    int writeGlobalEvent ( ULogEvent &event,
						   int fd,
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
	bool getGlobalLogSize( unsigned long &, bool use_fd );

	bool isGlobalEnabled( void ) const {
		return ( ( m_global_disable == false ) && ( NULL != m_global_path ) );
	};
	
	void AddToMask(ULogEventNumber e) {
		mask.push_back(e);
	}

	FileLockBase *getLock(CondorError &err);

  private:

	///
    void Reset( void );
    bool internalInitialize(int c, int p, int s);
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
				   int & fd );


	// options are flags from the ULogEvent::formatOpt enum
	bool doWriteEvent( int fd, ULogEvent *event, int format_options );
	void GenerateGlobalId( std::string &id );

	bool checkGlobalLogRotation(void);
	bool globalLogRotated( ReadUserLogHeader &reader );
	bool openGlobalLog( bool reopen );
	bool openGlobalLog( bool reopen, const UserLogHeader &header );
	bool closeGlobalLog( void);
	int doRotation( const char *path, int &fd,
					std::string &rotated, int max_rotations );


	bool updateGlobalStat( void );

    
    /// Deprecated.  condorID cluster, proc & subproc of the next event.
    int 		m_cluster;
    int         m_proc;
    int         m_subproc;

	/** Write to the user log? */		 bool		m_userlog_enable;
	bool doWriteEvent( ULogEvent *event,
		WriteUserLog::log_file& log,
		bool is_global_event,
		bool is_header_event,
		int format_opts,
		ClassAd *ad);
	void writeJobAdInfoEvent(char const *attrsToWrite,
		WriteUserLog::log_file& log, ULogEvent *event, ClassAd *param_jobad,
		bool is_global_event, int format_opts );

	std::vector<log_file*> logs;
    log_file_cache_map_t* log_file_cache;

	bool doWriteGlobalEvent( ULogEvent *event, ClassAd *ad);
    /** Enable locking?              */  bool		m_enable_locking;
	/** Enable fsync() after writes? */  bool       m_enable_fsync;

	/** Enable close after writes    */  bool       m_global_close;
	/** Write to the global log? */		 bool		m_global_disable;
    /** Copy of path to global log   */  char     * m_global_path;
    /** The global log file          */  int        m_global_fd;
    /** The global log file lock     */  FileLockBase *m_global_lock;
	/** ULogEvent::formatOpt flags   */  int        m_global_format_opts; // formerly m_global_use_xml
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

	/** ULogEvent::formatOpt flags   */  int        m_format_opts; // formerly m_use_xml

	/** Previously configured?       */  bool       m_configured;
	/** Initialized?                 */  bool       m_initialized;
	/** called init_user_ids()?      */  bool       m_init_user_ids;
	/** switch to user priv?         */  bool       m_set_user_priv;
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
