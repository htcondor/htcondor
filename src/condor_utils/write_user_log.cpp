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

#include "condor_common.h"
#include "condor_open.h"
#include "condor_debug.h"
#include "util_lib_proto.h"
#include <stdarg.h>
#include "write_user_log.h"
#include "write_user_log_state.h"
#include "read_user_log.h"
#include <time.h>
#include "condor_uid.h"
#include "condor_config.h"
#include "utc_time.h"
#include "stat_wrapper.h"
#include "file_lock.h"
#include "user_log_header.h"
#include "condor_fsync.h"
#include "condor_attributes.h"
#include "CondorError.h"

#include <string>
#include <algorithm>
#include "basename.h"

// Set to non-zero to enable fine-grained rotation debugging / timing
#define ROTATION_TRACE	0

static const char SynchDelimiter[] = "...\n";

// Simple class to normalize use of 64 bit ints
class UserLogInt64_t
{
public:
	UserLogInt64_t( void )
		{ m_value = 0; };
	UserLogInt64_t( int64_t value )
		{ m_value = value; };
	~UserLogInt64_t( void );

	int64_t Set( int64_t value )
		{ return m_value = value; };
	int64_t Get( void ) const
		{ return m_value; };
	UserLogInt64_t& operator =( int64_t value )
		{ m_value = value; return *this; };
	UserLogInt64_t& operator +=( int64_t value )
		{ m_value += value; return *this; };

private:
	int64_t		m_value;

};
class UserLogFilesize_t : public UserLogInt64_t
{
};


// Note this implementation is a bit different
// than the one in uids.cpp
static int should_use_keyring_sessions_for_log() {
#ifdef LINUX
	static int UseKeyringSessions = FALSE;
	static int DidParamForKeyringSessions = FALSE;

	if(!DidParamForKeyringSessions) {
		UseKeyringSessions = param_boolean("USE_KEYRING_SESSIONS", false);
		DidParamForKeyringSessions = true;
	}
	return UseKeyringSessions;
#else
	return false;
#endif
}

bool getPathToUserLog(const classad::ClassAd *job_ad, std::string &result,
                      const char* ulog_path_attr)
{
	bool ret_val = true;
	char *global_log = NULL;

	if ( ulog_path_attr == NULL ) {
		ulog_path_attr = ATTR_ULOG_FILE;
	}
	if ( job_ad == NULL ||
	     job_ad->EvaluateAttrString(ulog_path_attr,result) == false )
	{
		// failed to find attribute, check config file
		global_log = param("EVENT_LOG");
		if ( global_log ) {
			// canonicalize to UNIX_NULL_FILE even on Win32
			result = UNIX_NULL_FILE;
		} else {
			ret_val = false;
		}
	}

	if ( global_log ) free(global_log);

	if( ret_val && !fullpath(result.c_str()) ) {
		std::string iwd;
		if( job_ad && job_ad->EvaluateAttrString(ATTR_JOB_IWD,iwd) ) {
			iwd += "/";
			iwd += result;
			result = iwd;
		}
	}

	return ret_val;
}


// ***************************
//  WriteUserLog constructors
// ***************************
WriteUserLog::WriteUserLog()
{
	log_file_cache = NULL;
	Reset( );
}

// Destructor
WriteUserLog::~WriteUserLog()
{
	FreeGlobalResources( true );
	FreeLocalResources( );
	if ( m_init_user_ids ) {
		uninit_user_ids();
	}
}


// ***********************************
//  WriteUserLog initialize() methods
// ***********************************

bool
WriteUserLog::initialize(const ClassAd &job_ad, bool init_user)
{
	int cluster = -1;
	int proc = -1;
	std::string user_log_file;
	std::string dagman_log_file;

	m_global_disable = false;

	if ( init_user ) {
		std::string owner;
		std::string domain;

		job_ad.LookupString(ATTR_OWNER, owner);
		job_ad.LookupString(ATTR_NT_DOMAIN, domain);

		uninit_user_ids();
		if ( ! init_user_ids(owner.c_str(), domain.c_str()) ) {
			if ( ! domain.empty()) { owner += "@"; owner += domain; }
			dprintf(D_ALWAYS,
				"WriteUserLog::initialize: init_user_ids(%s) failed!\n", owner.c_str());
			return false;
		}
		m_init_user_ids = true;
	}
	m_set_user_priv = true;

	TemporaryPrivSentry temp_priv;

	// switch to user priv
	set_user_priv();

	job_ad.LookupInteger(ATTR_CLUSTER_ID, cluster);
	job_ad.LookupInteger(ATTR_PROC_ID, proc);

	std::vector<const char*> logfiles;
	if ( getPathToUserLog(&job_ad, user_log_file) ) {
		logfiles.push_back(user_log_file.c_str());
	}
	if ( getPathToUserLog(&job_ad, dagman_log_file, ATTR_DAGMAN_WORKFLOW_LOG) ) {
		logfiles.push_back(dagman_log_file.c_str());
		std::string msk;
		job_ad.LookupString(ATTR_DAGMAN_WORKFLOW_MASK, msk);
		Tokenize(msk);
		while(const char* mask = GetNextToken(",",true)) {
			AddToMask(ULogEventNumber(atoi(mask)));
		}
	}
	if( !initialize (logfiles, cluster, proc, 0)) {
		return false;
	}
	if( !logfiles.empty()) {
		int use_classad = 0;
		job_ad.LookupInteger(ATTR_ULOG_USE_XML, use_classad);
		setUseCLASSAD(use_classad & ULogEvent::formatOpt::CLASSAD);
	}
	return true;
}

bool
WriteUserLog::initialize( const char *file, int c, int p, int s, int format_opts )
{
	m_format_opts = format_opts;
	return initialize(std::vector<const char*>(1,file),c,p,s);
}

bool
WriteUserLog::initialize( const std::vector<const char *>& file, int c, int p, int s)
{
		// Save parameter info
	FreeLocalResources( );
	Configure(false);
	size_t failed_init = 0; //Count of logs that failed to initialize
	if ( m_userlog_enable ) {
		bool first = true;
		for(std::vector<const char*>::const_iterator it = file.begin();
				it != file.end(); ++it) {

			if (log_file_cache != NULL) {
				dprintf(D_FULLDEBUG, "WriteUserLog::initialize: looking up log file %s in cache\n", *it);
				log_file_cache_map_t::iterator f(log_file_cache->find(*it));
				if (f != log_file_cache->end()) {
					dprintf(D_FULLDEBUG, "WriteUserLog::initialize: found log file %s in cache, re-using\n", *it);
					logs.push_back(f->second);
					logs.back()->refset.insert(std::make_pair(c,p));
					first = false;
					continue;
				}
			}

			log_file* log = new log_file(*it);
			if (first) {
				// The first entry in the vector is the user's userlog, which we rarely want to fsync
				// subsequent ones are the dagman nodes.log, which we (for now) we usually want to fsync
				log->set_should_fsync(param_boolean("ENABLE_USERLOG_FSYNC", false));
				first = false;
			}
			//If last log and has dag log then set apply_mask to true
			if (std::distance(it,file.end()) == 1 && !mask.empty()) { log->is_dag_log = true; }

			if(!openFile(log->path.c_str(), true, m_enable_locking, true, log->lock, log->fd) ) {
				dprintf(D_ALWAYS, "WriteUserLog::initialize: failed to open file %s\n", log->path.c_str() );
				failed_init++;
				delete log;
				continue;
			} else {
				dprintf(D_FULLDEBUG, "WriteUserLog::initialize: opened %s successfully\n",
					log->path.c_str());
				logs.push_back(log);

				// setting the flag m_init_user_ids will cause the logging code in doWriteEvent()
				// to switch to PRIV_USER every time it does a write (as opposed to PRIV_CONDOR).
				// even though the file is already open, this is necessary because AFS needs access
				// to the user token on every write(), whereas other filesystems typically only need
				// permission on open().
				//
				// perhaps we should *always* do this, but because this went in the stable series I
				// wanted to change as little behavior as possible for all of the places where this
				// code is used.  -zmiller
				//
				// furthermore, we need to have tokens when calling close() as well.  because close()
				// is called in the destructor of the log_file object, we need to set a flag inside
				// that object as well. -zmiller
				//
				if(should_use_keyring_sessions_for_log()) {
					dprintf(D_FULLDEBUG, "WriteUserLog::initialize: current priv is %i\n", get_priv_state());
					if(get_priv_state() == PRIV_USER || get_priv_state() == PRIV_USER_FINAL) {
						dprintf(D_FULLDEBUG, "WriteUserLog::initialize: opened %s in priv state %i\n", log->path.c_str(), get_priv_state());
						// TODO Shouldn't set m_init_user_ids here
						m_init_user_ids = true;
						m_set_user_priv = true;
						log->set_user_priv_flag(true);
					}
				}

				if (log_file_cache != NULL) {
					dprintf(D_FULLDEBUG, "WriteUserLog::initialize: caching log file %s\n", *it);
					(*log_file_cache)[*it] = log;
					log->refset.insert(std::make_pair(c,p));
				}
			}
		}
	}
	if (!file.empty() && failed_init == file.size()) {
		dprintf(D_FULLDEBUG,"WriteUserLog::initialize: failed to initialize all %zu log file(s).\n",failed_init);
		freeLogs();
		logs.clear();
	}
	return internalInitialize( c, p, s );
}

void
WriteUserLog::setJobId( int c, int p, int s )
{
	m_cluster = c;
	m_proc = p;
	m_subproc = s;
}

// Internal-only initializer, invoked by all of the others
bool
WriteUserLog::internalInitialize( int c, int p, int s )
{

	m_cluster = c;
	m_proc = p;
	m_subproc = s;

		// Important for performance: We do not re-open the global log
		// if we already have done so (i.e. if m_global_fd >= 0).
	if ( !m_global_disable && m_global_path && m_global_fd < 0 ) {
		priv_state priv = set_condor_priv();
		openGlobalLog( true );
		set_priv( priv );
	}

	m_initialized = true;
	return true;
}

// Read in just the m_format_opts configuration
void WriteUserLog::setUseCLASSAD(int fmt_type)
{
	if ( ! m_configured) {
		m_format_opts = USERLOG_FORMAT_DEFAULT;
		auto_free_ptr fmt(param("DEFAULT_USERLOG_FORMAT_OPTIONS"));
		if (fmt) {
			m_format_opts = ULogEvent::parse_opts(fmt, m_format_opts);
		}
	}
	m_format_opts &= ~(ULogEvent::formatOpt::CLASSAD);
	m_format_opts |= (ULogEvent::formatOpt::CLASSAD & fmt_type);
}

// Read in our configuration information
bool
WriteUserLog::Configure( bool force )
{
	priv_state previous;
	// If we're already configured and not in "force" mode, do nothing
	if (  m_configured && ( !force )  ) {
		return true;
	}
	FreeGlobalResources( false );
	m_configured = true;

	m_skip_fsync_this_event = false;
	m_enable_locking = param_boolean( "ENABLE_USERLOG_LOCKING", false );

	// TODO: revisit this if we let the job choose to enable or disable UTC, SUB_SECOND or ISO_DATE
	// if we are merging job and defult flags, we need to do a better job than this.
	auto_free_ptr fmt(param("DEFAULT_USERLOG_FORMAT_OPTIONS"));
	if (fmt) {
		m_format_opts = ULogEvent::parse_opts(fmt, USERLOG_FORMAT_DEFAULT);
	}

	if ( m_global_disable ) {
		return true;
	}
	m_global_path = param( "EVENT_LOG" );
	if ( NULL == m_global_path ) {
		return true;
	}
	m_global_stat = new StatWrapper( m_global_path );
	m_global_state = new WriteUserLogState( );


	m_rotation_lock_path = param( "EVENT_LOG_ROTATION_LOCK" );
	if ( NULL == m_rotation_lock_path ) {
		
		int len = strlen(m_global_path) + 6;
		char *tmp = (char*) malloc(len);
		ASSERT(tmp);
		snprintf( tmp, len, "%s.lock", m_global_path );
		m_rotation_lock_path = tmp;
	}

	// Make sure the global lock exists
	previous = set_priv(PRIV_CONDOR);
	m_rotation_lock_fd = safe_open_wrapper_follow( m_rotation_lock_path, O_WRONLY|O_CREAT, 0666 );
	if ( m_rotation_lock_fd < 0 ) {
		dprintf( D_ALWAYS,
				 "Warning: WriteUserLog Failed to open event rotation lock file %s:"
				 " %d (%s)\n",
				 m_rotation_lock_path, errno, strerror(errno) );
		m_rotation_lock = new FakeFileLock( );
	} else {
		m_rotation_lock = new FileLock( m_rotation_lock_fd,
										NULL,
										m_rotation_lock_path );
		dprintf( D_FULLDEBUG, "WriteUserLog Created rotation lock %s @ %p\n",
				 m_rotation_lock_path, m_rotation_lock );
	}
	set_priv(previous);

	m_global_format_opts = 0;
	fmt.set(param("EVENT_LOG_FORMAT_OPTIONS"));
	if (fmt) { m_global_format_opts |= ULogEvent::parse_opts(fmt, 0); }
	if (param_boolean("EVENT_LOG_USE_XML", false)) {
		m_global_format_opts &= ~(ULogEvent::formatOpt::CLASSAD);
		m_global_format_opts |= ULogEvent::formatOpt::XML;
	}
	m_global_count_events = param_boolean( "EVENT_LOG_COUNT_EVENTS", false );
	m_global_max_rotations = param_integer( "EVENT_LOG_MAX_ROTATIONS", 1, 0 );
	m_global_fsync_enable = param_boolean( "EVENT_LOG_FSYNC", false );
	m_global_lock_enable = param_boolean( "EVENT_LOG_LOCKING", false );
	m_global_max_filesize = param_integer( "EVENT_LOG_MAX_SIZE", -1 );
	if ( m_global_max_filesize < 0 ) {
		m_global_max_filesize = param_integer( "MAX_EVENT_LOG", 1'000'000, 0 );
	}
	if ( m_global_max_filesize == 0 ) {
		m_global_max_rotations = 0;
	}

	// Allow closing of the event log after each write...  This is to
	// emulate the Windows behavior on UNIX for testing purposes.
	// This knob should never be documented or set in production use
# if defined(WIN32)
	bool default_close = true;
# else
	bool default_close = false;
# endif
	m_global_close = param_boolean( "EVENT_LOG_FORCE_CLOSE", default_close );

	return true;
}

void
WriteUserLog::Reset( void )
{
	m_initialized = false;
	m_configured = false;
	m_init_user_ids = false;
	m_set_user_priv = false;

	m_cluster = -1;
	m_proc = -1;
	m_subproc = -1;

	m_userlog_enable = true;

    freeLogs();
   	logs.clear();
    log_file_cache = NULL;

	m_enable_locking = true;
	m_skip_fsync_this_event = false;

	m_global_path = NULL;
	m_global_fd = -1;
	m_global_lock = NULL;
	m_global_stat = NULL;
	m_global_state = NULL;

	m_rotation_lock = NULL;
	m_rotation_lock_fd = -1;
	m_rotation_lock_path = NULL;

	m_format_opts = USERLOG_FORMAT_DEFAULT;

	m_creator_name = NULL;

	m_global_disable = true;
	m_global_format_opts = 0;
	m_global_count_events = false;
	m_global_max_filesize = 1'000'000;
	m_global_max_rotations = 1;
	m_global_lock_enable = true;
	m_global_fsync_enable = false;

	// For Win32, always close the global after each write to allow
	// other writers to rotate
# if defined(WIN32)
	m_global_close = true;
# else
	m_global_close = false;
# endif

	m_global_id_base = NULL;
	(void) GetGlobalIdBase( );
	m_global_sequence = 0;
}

void
WriteUserLog::FreeGlobalResources( bool final )
{

	if (m_global_path) {
		free(m_global_path);
		m_global_path = NULL;
	}

	closeGlobalLog();	// Close & release global file handle & lock

	if ( final && (m_global_id_base != NULL) ) {
		free( m_global_id_base );
		m_global_id_base = NULL;
	}
	if (m_global_stat != NULL) {
		delete m_global_stat;
		m_global_stat = NULL;
	}
	if (m_global_state != NULL) {
		delete m_global_state;
		m_global_state = NULL;
	}

	if (m_rotation_lock_path) {
		free(m_rotation_lock_path);
		m_rotation_lock_path = NULL;
	}
	if (m_rotation_lock_fd >= 0) {
		close(m_rotation_lock_fd);
		m_rotation_lock_fd = -1;
	}
	if (m_rotation_lock) {
		delete m_rotation_lock;
		m_rotation_lock = NULL;
	}

}

// This should be correct for our use.
// We create one of these things and shove it into a vector.
// After it enters the vector, it never leaves; it gets destroyed.
// Probably ought to use shared_ptr or something to be truly safe.
// The (!copied) case is probably not necessary, but I am trying
// to be as safe as possible.

WriteUserLog::log_file& WriteUserLog::log_file::operator=(const WriteUserLog::log_file& rhs)
{
	if(this != &rhs) {
		if(!copied) {
			if(fd >= 0) {
				priv_state priv = PRIV_UNKNOWN;
				dprintf( D_FULLDEBUG, "WriteUserLog::user_priv_flag (=) is %i\n", user_priv_flag);
				if ( user_priv_flag ) {
					priv = set_user_priv();
				}
				if(close(fd) != 0) {
					dprintf( D_ALWAYS,
							 "WriteUserLog::FreeLocalResources(): "
							 "close() failed - errno %d (%s)\n",
							 errno, strerror(errno) );
				}
				if ( user_priv_flag ) {
					set_priv( priv );
				}
			}
			delete lock;
		}
		path = rhs.path;
		fd = rhs.fd;
		lock = rhs.lock;
		should_fsync = rhs.should_fsync;
		rhs.copied = true;
		user_priv_flag = rhs.user_priv_flag;
	}
	return *this;
}
WriteUserLog::log_file::log_file(const log_file& orig) : path(orig.path),
	lock(orig.lock), fd(orig.fd), copied(false), user_priv_flag(orig.user_priv_flag), 
	is_dag_log(orig.is_dag_log), should_fsync(orig.should_fsync)
{
	orig.copied = true;
}

WriteUserLog::log_file::~log_file()
{
	if(!copied) {
		if(fd >= 0) {
			priv_state priv = PRIV_UNKNOWN;
			dprintf( D_FULLDEBUG, "WriteUserLog::user_priv_flag (~) is %i\n", user_priv_flag);
			if ( user_priv_flag ) {
				priv = set_user_priv();
			}
			if(close(fd) != 0) {
				dprintf( D_ALWAYS,
						 "WriteUserLog::FreeLocalResources(): "
						 "close() failed - errno %d (%s)\n",
						 errno, strerror(errno) );
			}
			if ( user_priv_flag ) {
				set_priv( priv );
			}
			fd = -1;
		}
		delete lock;
		lock = NULL;
	}
}

void WriteUserLog::freeLogs() {
    // we do this only if local log files aren't being cached
    if (log_file_cache != NULL) return;
    for (std::vector<log_file*>::iterator j(logs.begin());  j != logs.end();  ++j) {
        delete *j;
    }
}

void
WriteUserLog::FreeLocalResources( void )
{
    freeLogs();
	logs.clear();
	if (m_creator_name) {
		free( m_creator_name );
		m_creator_name = NULL;
	}
}

void
WriteUserLog::setCreatorName( const char *name )
{
	if ( name ) {
		if ( m_creator_name ) {
			free( const_cast<char*>(m_creator_name) );
			m_creator_name = NULL;
		}
		m_creator_name = strdup( name );
	}
}

bool
WriteUserLog::openFile(
	const char	 *file,
	bool		  log_as_user,	// if false, we are logging to the global file
	bool		  use_lock,		// use the lock
	bool		  append,		// append mode?
	FileLockBase *&lock,
	int			  &fd )
{
	(void)  log_as_user;	// Quiet warning

	if ( file == NULL ) {
		dprintf( D_ALWAYS, "WriteUserLog::openFile: NULL filename!\n" );
		return false;
	}

	if ( strcmp(file,UNIX_NULL_FILE)==0 ) {
		// special case - deal with /dev/null.  we don't really want
		// to open /dev/null, but we don't want to fail in this case either
		// because this is common when the user does not want a log, but
		// the condor admin desires a global event log.
		// Note: we always check UNIX_NULL_FILE, since we canonicalize
		// to this even on Win32.
		fd = -1;
		lock = NULL;
		return true;
	}

	// Unix
	int	flags = O_WRONLY | O_CREAT;
	if ( append ) {
		flags |= O_APPEND;
	}
#if defined(WIN32)

	// if we want lock-free append, we have to open the handle in a diffent file mode than what the
	// c-runtime uses.  FILE_APPEND_DATA but NOT FILE_WRITE_DATA or GENERIC_WRITE.
	// note that we do NOT pass _O_APPEND to _open_osfhandle() since what that does in the current (broken)
	// c-runtime is tell it to call seek before every write, but you *can't* seek an append-only file...
	// PRAGMA_REMIND("TJ: remove use_lock test here for 8.5.x")
	if (append && ! use_lock) {
		DWORD err = 0;
		DWORD attrib =  FILE_ATTRIBUTE_NORMAL; // set to FILE_ATTRIBUTE_READONLY based on mode???
		DWORD create_mode = (flags & O_CREAT) ? OPEN_ALWAYS : OPEN_EXISTING;
		DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
		HANDLE hf = CreateFile(file, FILE_APPEND_DATA, share_mode, NULL, create_mode, attrib, NULL);
		if (hf == INVALID_HANDLE_VALUE) {
			fd = -1;
			err = GetLastError();
		} else {
			fd = _open_osfhandle((intptr_t)hf, flags & (/*_O_APPEND | */_O_RDONLY | _O_TEXT | _O_WTEXT));
			if (fd < 0) {
				// open_osfhandle can sometimes set errno and sometimes _doserrno (i.e. GetLastError()),
				// the only non-windows error code it sets is EMFILE when the c-runtime fd table is full.
				if (errno == EMFILE) {
					err = ERROR_TOO_MANY_OPEN_FILES;
				} else {
					err = _doserrno;
					if (err == NO_ERROR) err = ERROR_INVALID_FUNCTION; // make sure we get an error code
				}
			}
		}

		if (fd < 0) {
			dprintf( D_ALWAYS,
					 "WriteUserLog::initialize: "
						 "CreateFile/_open_osfhandle(\"%s\") failed - err %d (%s)\n",
					 file,
					 err,
					 GetLastErrorString(err) );
			return false;
		}

		// prepare to lock the file.
		if ( use_lock ) {
			lock = new FileLock( fd, NULL, file );
		} else {
			lock = new FakeFileLock( );
		}
		return true;
	}
#endif
	mode_t mode = 0664;
	fd = safe_open_wrapper_follow( file, flags, mode );
	if( fd < 0 ) {
		dprintf( D_ALWAYS,
					"WriteUserLog::initialize: "
						"safe_open_wrapper(\"%s\") failed - errno %d (%s)\n",
					file,
					errno,
					strerror(errno) );
		return false;
	}

	// prepare to lock the file.
	if ( use_lock ) {
#if !defined(WIN32)
		bool new_locking = param_boolean("CREATE_LOCKS_ON_LOCAL_DISK", true);
			
		if (new_locking) {
			lock = new FileLock(file, true, false);
			if ( lock->initSucceeded() )
				return true;
			delete lock;
		}		
#endif	
		lock = new FileLock( fd, NULL, file );
	} else {
		lock = new FakeFileLock( );
	}

	return true;
}

bool
WriteUserLog::openGlobalLog( bool reopen )
{
	UserLogHeader	header;
	return openGlobalLog( reopen, header );
}

bool
WriteUserLog::openGlobalLog( bool reopen, const UserLogHeader &header )
{
	if ( m_global_disable || (NULL==m_global_path) ) {
		return true;
	}

	// Close it if it's already open
	if( reopen && m_global_fd >= 0 ) {
		closeGlobalLog();
	}
	else if ( m_global_fd >= 0 ) {
		return true;
	}

	bool ret_val = true;
	priv_state priv = set_condor_priv();
	ret_val = openFile( m_global_path, false, m_global_lock_enable, true,
						m_global_lock, m_global_fd);

	if ( ! ret_val ) {
		set_priv( priv );
		return false;
	}
	if (!m_global_lock->obtain(WRITE_LOCK) ) {
		dprintf( D_ALWAYS, "WARNING WriteUserLog::openGlobalLog failed to obtain global event log lock, an event will not be written to the global event log\n" );
		return false;
	}

	StatWrapper		statinfo;
	if (  ( !(statinfo.Stat(m_global_path))    )  &&
		  ( 0 == statinfo.GetBuf()->st_size )  )  {

		// Generate a header event
		WriteUserLogHeader writer( header );

		m_global_sequence = writer.incSequence( );

		std::string file_id;
		GenerateGlobalId( file_id );
		writer.setId( file_id );

		writer.addFileOffset( writer.getSize() );
		writer.setSize( 0 );

		writer.addEventOffset( writer.getNumEvents() );
		writer.setNumEvents( 0 );
		writer.setCtime( time(NULL) );

		writer.setMaxRotation( m_global_max_rotations );

		if ( m_creator_name ) {
			writer.setCreatorName( m_creator_name );
		}

		ret_val = writer.Write( *this );

		std::string	s;
		formatstr( s, "openGlobalLog: header: %s", m_global_path );
		writer.dprint( D_FULLDEBUG, s );

		// TODO: we should should add the number of events in the
		// previous file to the "first event number" of this one.
		// The problem is that we don't know the number of events
		// in the previous file, and we can't (other processes
		// could write to it, too) without manually counting them
		// all.

		if (!updateGlobalStat() ) {
			dprintf( D_ALWAYS,
					 "WriteUserLog Failed to update global stat after header write\n" );
		}
		else {
			m_global_state->Update( *m_global_stat );
		}
	}


	if (!m_global_lock->release() ) {
		dprintf( D_ALWAYS, "WARNING WriteUserLog::openGlobalLog failed to release global lock\n" );
	}

	set_priv( priv );
	return ret_val;
}

bool
WriteUserLog::closeGlobalLog( void )
{
	if (m_global_lock) {
		delete m_global_lock;
		m_global_lock = NULL;
	}
	if (m_global_fd >= 0) {
		close(m_global_fd);
		m_global_fd = -1;
	}
	return true;
}

	// This method is called from doWriteEvent() - we expect the file to
	// be locked, seeked to the end of the file, and in condor priv state.
	// return true if log was rotated, either by us or someone else.
bool
WriteUserLog::checkGlobalLogRotation( void )
{
	if (m_global_fd < 0) {
		return false;
	}
	if ( m_global_disable || (NULL==m_global_path) ) {
		return false;
	}

	// Don't rotate if max rotations is set to zero
	if ( 0 == m_global_max_rotations ) {
		return false;
	}

	// Check the size of the log file
	if ( !updateGlobalStat() ) {
		return false;			// What should we do here????
	}

	// Header reader for later use
	ReadUserLogHeader	header_reader;

	// New file?  Another process rotated it
	if ( m_global_state->isNewFile(*m_global_stat) ) {
		globalLogRotated( header_reader );
		return true;
	}
	m_global_state->Update( *m_global_stat );

	// Less than the size limit -- nothing to do
	if ( !m_global_state->isOverSize(m_global_max_filesize) ) {
		return false;
	}

	// Here, it appears that the file is over the limit
	// Grab the rotation lock and check again

	// Get the rotation lock
	if ( !m_rotation_lock->obtain( WRITE_LOCK ) ) {
		dprintf( D_ALWAYS, "WARNING WriteUserLog::checkGlobalLogRotation failed to get rotation lock, we may log to the wrong log for a period\n" );
		return false;
	}

	// Check the size of the log file
#if ROTATION_TRACE
	double stat_time = condor_gettimestamp_double();
#endif
	if ( !updateGlobalStat() ) {
		return false;			// What should we do here????
	}

	// New file?  Another process rotated it
	if ( m_global_state->isNewFile(*m_global_stat) ) {
		m_rotation_lock->release( );
		globalLogRotated( header_reader );
		return true;
	}
	m_global_state->Update( *m_global_stat );

	// Less than the size limit -- nothing to do
	// Note: This should never be true, but checking just in case
	if ( !m_global_state->isOverSize(m_global_max_filesize) ) {
		m_rotation_lock->release( );
		return false;
	}


	// Now, we have the rotation lock *and* the file is over the limit
	// Let's get down to the business of rotating it
	filesize_t	current_filesize = 0;
	StatWrapper	sbuf;
	if ( sbuf.Stat( m_global_fd ) ) {
		dprintf( D_ALWAYS, "WriteUserLog Failed to stat file handle\n" );
	}
	else {
		current_filesize = sbuf.GetBuf()->st_size;
	}


	// First, call the rotation starting callback
	if ( !globalRotationStarting( (unsigned long) current_filesize ) ) {
		m_rotation_lock->release( );
		return false;
	}

#if ROTATION_TRACE
	{
		StatWrapper	swrap( m_global_path );
		double start_time = condor_gettimestamp_double();
		dprintf( D_FULLDEBUG, "Rotating inode #%ld @ %.6f (stat @ %.6f)\n",
				 (long)swrap.GetBuf()->st_ino, start_time,
				 stat_time );
		m_global_lock->display();
	}
#endif

	// Read the old header, use it to write an updated one
	FILE *fp = safe_fopen_wrapper_follow( m_global_path, "r" );
	if ( !fp ) {
		dprintf( D_ALWAYS,
				 "WriteUserLog: "
				 "safe_fopen_wrapper_follow(\"%s\") failed - errno %d (%s)\n",
				 m_global_path, errno, strerror(errno) );
	}
	else {
		UserLogType is_xml = UserLogType::LOG_TYPE_NORMAL;
		// TODO: add support for JSON and CLASSAD formats
		if (m_global_format_opts & ULogEvent::formatOpt::XML) {
			is_xml = UserLogType::LOG_TYPE_XML;
		}
		ReadUserLog log_reader( fp, is_xml, false );
		if ( header_reader.Read( log_reader ) != ULOG_OK ) {
			dprintf( D_ALWAYS,
					 "WriteUserLog: Error reading header of \"%s\"\n",
					 m_global_path );
		}
		else {
			std::string s;
			formatstr( s, "read %s header:", m_global_path );
			header_reader.dprint( D_FULLDEBUG, s );
		}

		if ( m_global_count_events ) {
			int		events = 0;
#         if ROTATION_TRACE
			double time1 = condor_gettimestamp_double();
#         endif
			while( 1 ) {
				ULogEvent		*event = NULL;
				ULogEventOutcome outcome = log_reader.readEvent( event );
				if ( ULOG_OK != outcome ) {
					break;
				}
				events++;
				delete event;
			}
#         if ROTATION_TRACE
			double	time2 = condor_gettimestamp_double();
			double	elapsed = time2 - time1;
			double	eps = ( events / elapsed );
#         endif

			globalRotationEvents( events );
			header_reader.setNumEvents( events );
#         if ROTATION_TRACE
			dprintf( D_FULLDEBUG,
					 "WriteUserLog: Read %d events in %.4fs = %.0f/s\n",
					 events, elapsed, eps );
#         endif
		}
		fclose( fp );
	}
	header_reader.setSize( current_filesize );

	// Craft a header writer object from the header reader
	int				header_fd = -1;
	FileLockBase	*fake_lock = NULL;
	if( !openFile(m_global_path, false, false, false, fake_lock, header_fd) ) {
		dprintf( D_ALWAYS,
				 "WriteUserLog: "
				 "failed to open %s for header rewrite: %d (%s)\n",
				 m_global_path, errno, strerror(errno) );
	}
	WriteUserLogHeader	header_writer( header_reader );
	header_writer.setMaxRotation( m_global_max_rotations );
	if ( m_creator_name ) {
		header_writer.setCreatorName( m_creator_name );
	}

	std::string s;
	formatstr( s, "checkGlobalLogRotation(): %s", m_global_path );
	header_writer.dprint( D_FULLDEBUG, s );

	// And write the updated header
# if ROTATION_TRACE
	dprintf( D_FULLDEBUG, "WriteUserLog: Writing header to %s (%d) @ %.6f\n",
			 m_global_path, header_fd, condor_gettimstamp_double() );
# endif
	if ( header_fd >= 0 ) {
		lseek( header_fd, 0, SEEK_SET );
		header_writer.Write( *this, header_fd );
		close( header_fd );

		std::string tmps;
		formatstr( tmps, "WriteUserLog: Wrote header to %s", m_global_path );
		header_writer.dprint( D_FULLDEBUG, tmps );
	}
	if ( fake_lock ) {
		delete fake_lock;
	}

	// Now, rotate files
# if ROTATION_TRACE
	double time1 = condor_gettimestamp_double();
	dprintf( D_FULLDEBUG,
			 "WriteUserLog: Starting bulk rotation @ %.6f\n",
			 time1 );
# endif

	std::string rotated;
	int num_rotations = doRotation( m_global_path, m_global_fd,
									rotated, m_global_max_rotations );
	if ( num_rotations ) {
		dprintf(D_FULLDEBUG,
				"WriteUserLog: Rotated event log %s to %s at size %lu bytes\n",
				m_global_path, rotated.c_str(),
				(unsigned long) current_filesize);
	}

# if ROTATION_TRACE
	double end_time = condor_gettimestamp_double();
	if ( num_rotations ) {
		dprintf( D_FULLDEBUG,
				 "WriteUserLog: Done rotating files (inode = %ld) @ %.6f\n",
				 (long)swrap.GetBuf()->st_ino, end_time );
	}
	double	elapsed = end_time - time1;
	double	rps = ( num_rotations / elapsed );
	dprintf( D_FULLDEBUG,
			 "WriteUserLog: Rotated %d files in %.4fs = %.0f/s\n",
			 num_rotations, elapsed, rps );
# endif

	// OK, *I* did the rotation, initialize the header of the file, too
	globalLogRotated( header_reader );

	// Finally, call the rotation complete callback
	globalRotationComplete( num_rotations,
							header_reader.getSequence(),
							header_reader.getId() );

	// Finally, release the rotation lock
	m_rotation_lock->release( );

	return true;

}

bool
WriteUserLog::updateGlobalStat( void )
{
	if ( (NULL == m_global_stat) || (m_global_stat->Stat()) ) {
		return false;
	}
	if ( m_global_stat->IsBufValid() == false ) {
		return false;
	}
	return true;
}

bool
WriteUserLog::getGlobalLogSize( unsigned long &size, bool use_fd )
{
	StatWrapper	stat;
	if ( m_global_close && m_global_fd < 0 ) {
		use_fd = false;
	}
	if ( use_fd ) {
		if ( m_global_fd < 0 ) {
			return false;
		}
		if ( stat.Stat(m_global_fd) ) {
			return false;
		}
	}
	else {
		if ( stat.Stat(m_global_path) ) {
			return false;
		}
	}
	size = (unsigned long) stat.GetBuf()->st_size;
	return true;
}

bool
WriteUserLog::globalLogRotated( ReadUserLogHeader &reader )
{
	// log was rotated, so we need to reopen/create it and also
	// recreate our lock.

	// this will re-open and re-create locks
	openGlobalLog( true, reader );
	if ( m_global_lock ) {
		m_global_lock->obtain(WRITE_LOCK);
		if ( !updateGlobalStat() ) {
			m_global_state->Clear( );
		}
		else {
			m_global_state->Update( *m_global_stat );
		}
	}
	return true;
}

int
WriteUserLog::doRotation( const char *path, int &fd,
						  std::string &rotated, int max_rotations )
{

	int  num_rotations = 0;
	rotated = path;
	if ( 1 == max_rotations ) {
		rotated += ".old";
	}
	else {
		rotated += ".1";
		for( int i=max_rotations;  i>1;  i--) {
			std::string old1( path );
			formatstr_cat(old1, ".%d", i-1 );

			StatWrapper	s( old1 );
			if ( 0 == s.GetRc() ) {
				std::string old2( path );
				formatstr_cat(old2, ".%d", i );
				if (rename( old1.c_str(), old2.c_str() )) {
					dprintf(D_FULLDEBUG, "WriteUserLog failed to rotate old log from '%s' to '%s' errno=%d\n",
							old1.c_str(), old2.c_str(), errno);
				}
				num_rotations++;
			}
		}
	}

# ifdef WIN32
	// on win32, cannot rename an open file
	if ( fd >= 0 ) {
		close( fd );
		fd = -1;
	}
# else
	(void) fd;		// Quiet compiler warnings
# endif

	// Before time
	double before = condor_gettimestamp_double();

	if ( rotate_file( path, rotated.c_str()) == 0 ) {
		double after = condor_gettimestamp_double();
		dprintf(D_FULLDEBUG, "WriteUserLog before .1 rot: %.6f\n", before );
		dprintf(D_FULLDEBUG, "WriteUserLog after  .1 rot: %.6f\n", after );
		num_rotations++;
	}

	return num_rotations;
}


int
WriteUserLog::writeGlobalEvent( ULogEvent &event,
								int fd,
								bool is_header_event )
{
	if ( fd < 0 ) {
		fd = m_global_fd;
	}

	if ( is_header_event ) {
		lseek( fd, 0, SEEK_SET );
	}

	return doWriteEvent( fd, &event, m_global_format_opts );
}

bool
WriteUserLog::doWriteEvent( ULogEvent *event,
							const log_file& log,
							bool is_global_event,
							bool is_header_event,
							int  format_opts)
{
	int success;
	int fd;
	FileLockBase* lock;
	TemporaryPrivSentry temp_priv;

	if (is_global_event) {
		fd = m_global_fd;
		lock = m_global_lock;
		format_opts = m_global_format_opts;
		set_condor_priv();
	} else {
		fd = log.fd;
		lock = log.lock;
		if ( m_set_user_priv ) {
			set_user_priv();
		}
	}
	bool was_locked = lock->isLocked();

		// We're seeing sporadic test suite failures where a daemon
		// takes more than 10 seconds to write to the user log.
		// This will help narrow down where the delay is coming from.
	if (!was_locked) {
		time_t before = time(nullptr);

		lock->obtain(WRITE_LOCK);

		time_t after = time(NULL);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG,
					"UserLog::doWriteEvent(): locking file took %ld seconds\n",
					(after-before) );
		}
	}

	int			status = 0;
	const char	*whence;
	if ( is_header_event ) {
		time_t before = time(nullptr);

		status = lseek( fd, 0, SEEK_SET );
		whence = "SEEK_SET";

		time_t after  = time(nullptr);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG,
					"UserLog::doWriteEvent(): lseek() took %ld seconds\n",
					(after-before) );
		}
		if ( status ) {
			dprintf( D_ALWAYS,
					"WriteUserLog lseek(%s) failed in WriteUserLog::doWriteEvent - "
					"errno %d (%s)\n",
					whence, errno, strerror(errno) );
		}
	}

		// rotate the global event log if it is too big
	if ( is_global_event ) {
		if ( checkGlobalLogRotation() ) {
				// if we rotated the log, we have a new fd and lock
			fd = m_global_fd;
			lock = m_global_lock;
		}
	}

	{
		time_t before = time(nullptr);

		success = doWriteEvent( fd, event, format_opts );

		time_t after = time(nullptr);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG,
					"UserLog::doWriteEvent(): writing event took %ld seconds\n",
					(after-before) );
		}
	}

	// If this is called with WriteEventNoFsync, don't fsync any file
	// otherwise, if this is writen to the global evenlog, only
	// fsync if a knob is set
	// otherwise, it is either a bona-fide user log (which we don't
	// fsync by default, or the dagman nodes.dag which we DO fsync

	if (!m_skip_fsync_this_event &&
			((is_global_event  && m_global_fsync_enable) ||
			((!is_global_event) && log.get_should_fsync()))) {

		time_t before = time(nullptr);

		const char *fname;
		if ( is_global_event ) {
			fname = m_global_path;
		} else {
			fname = log.path.c_str();
		}

		if (condor_fdatasync(fd, fname) != 0 ) {
		  dprintf( D_ALWAYS,
				   "fsync() failed in WriteUserLog::writeEvent"
				   " - errno %d (%s)\n",
				   errno, strerror(errno) );
			// Note:  should we set success to false here?
		}

		time_t after = time(nullptr);

		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG,
					 "UserLog::doWriteEvent(): fsyncing file took %ld secs\n",
					 (after-before) );
		}
	}

	if (!was_locked) {
		time_t before = time(nullptr);

		lock->release();

		time_t after = time(nullptr);
		if ((after - before) > 5) {
			dprintf( D_FULLDEBUG,
					"UserLog::doWriteEvent(): unlocking file took %ld seconds\n",
					(after-before) );
		}
	}
	return success;
}

bool
WriteUserLog::doWriteEvent( int fd, ULogEvent *event, int format_opts )
{
	ClassAd* eventAd = NULL;
	bool success = true;

	if (format_opts & ULogEvent::formatOpt::CLASSAD) {

		eventAd = event->toClassAd((format_opts & ULogEvent::formatOpt::UTC) != 0);	// must delete eventAd eventually
		if (!eventAd) {
			dprintf( D_ALWAYS,
					 "WriteUserLog Failed to convert event type # %d to classAd.\n",
					 event->eventNumber);
			success = false;
		} else {
			std::string output;
			if (format_opts & ULogEvent::formatOpt::JSON) {
				classad::ClassAdJsonUnParser  unparser;
				unparser.Unparse(output, eventAd);
				if ( ! output.empty()) output += "\n";
			} else /*if (format_opts & ULogEvent::formatOpt::XML)*/ {
				eventAd->Delete(ATTR_TARGET_TYPE); // TJ 2019: I think this is no longer necessary
				classad::ClassAdXMLUnParser unparser;
				unparser.SetCompactSpacing(false);
				unparser.Unparse(output, eventAd);
			}

			if (output.empty()) {
				dprintf( D_ALWAYS,
						 "WriteUserLog Failed to convert event type # %d to %s.\n",
						 event->eventNumber,
						 (format_opts & ULogEvent::formatOpt::JSON) ? "JSON" : "XML");
			}
			if ( write( fd, output.data(), output.length() ) < (ssize_t)output.length() ) {
				success = false;
			} else {
				success = true;
			}
		}
	} else {
		std::string output;
		success = event->formatEvent( output, format_opts );
		output += SynchDelimiter;
		if ( success && write( fd, output.data(), output.length() ) < (ssize_t)output.length() ) {
			// TODO Should we print a '\n...\n' like in the older code?
			success = false;
		}
	}

	if ( eventAd ) {
		delete eventAd;
	}

	return success;
}

bool
WriteUserLog::doWriteGlobalEvent( ULogEvent* event )
{
	log_file log;
	return doWriteEvent(event, log, true, false, m_global_format_opts);
}

// Return false on error, true on goodness
bool
WriteUserLog::writeEvent ( ULogEvent *event,
						   const ClassAd *param_jobad,
						   bool *written )
{
	// By default, no event written
	if ( written ) {
		*written = false;
	}

	// the the log is not initialized, don't bother --- just return OK
	if ( !m_initialized ) {
		dprintf( D_FULLDEBUG,
				 "WriteUserLog: not initialized @ writeEvent()\n" );
		return true;
	}

	// make certain some parameters we will need are initialized
	if (!event) {
		return false;
	}

	// Open the global log
	bool globalOpenError = false;
	if ( !openGlobalLog(false) ) {
		dprintf( D_ALWAYS, "WARNING WriteUserLog::writeEvent failed to open global log! The global event log will be missing an event.\n" );
		// We *don't* want to return here, so we at least try to write
		// to the "normal" log (see gittrac #2858).
		globalOpenError = true;
	}

	// fill in event context
	event->cluster = m_cluster;
	event->proc = m_proc;
	event->subproc = m_subproc;

	// write global event
	//TEMPTEMP -- don't try if we got a global open error
	if ( !globalOpenError && !m_global_disable && m_global_path ) {
		if ( ! doWriteGlobalEvent(event) ) {
			dprintf( D_ALWAYS, "WARNING: WriteUserLog::writeEvent global doWriteEvent() failed on global log! The global event log will be missing an event.\n" );
			// We *don't* want to return here, so we at least try to write
			// to the "normal" log (see gittrac #2858).
		}
		//TEMPTEMP -- do we want to do this if the global write failed?
		//TEMPTEMP -- make sure to free attrsToWrite in all cases
		char *attrsToWrite = param("EVENT_LOG_JOB_AD_INFORMATION_ATTRS");
		if( attrsToWrite && *attrsToWrite ) {
			//TEMPTEMP -- what the hell *is* this?
			log_file log;
			writeJobAdInfoEvent( attrsToWrite, log, event, param_jobad, true,
				m_global_format_opts );
		}
		free( attrsToWrite );
	}

	//TEMPTEMP -- don't try if we got a global open error
	if ( !globalOpenError && m_global_close ) {
		closeGlobalLog( );
	}

	// write ulog event
	bool ret = true;
	if ( m_userlog_enable ) {
		for(std::vector<log_file*>::iterator p = logs.begin(); p != logs.end(); ++p) {
			if( (*p)->fd < 0 || !(*p)->lock) {
				if((*p)->fd >= 0) {
					dprintf( D_ALWAYS, "WriteUserLog: No user log lock!\n" );
				}
				continue;
			}
				// Check our mask vector for the event
				// If we have a mask, the event must be in the mask to write the event.
			if( (*p)->is_dag_log && !mask.empty()){
				std::vector<ULogEventNumber>::iterator pp =
					std::find(mask.begin(),mask.end(),event->eventNumber);	
				if(pp == mask.end()) {
					dprintf( D_FULLDEBUG, "Did not find %d in the mask, so do not write this event.\n",
						event->eventNumber );
					break; // We are done caring about this event
				}
			}
			int fmt_opts = m_format_opts;
			if ((*p)->is_dag_log) { fmt_opts &= ~(ULogEvent::formatOpt::XML); }
			if ( ! doWriteEvent(event, **p, false, false, fmt_opts) ) {
				dprintf( D_ALWAYS, "WARNING: WriteUserLog::writeEvent user doWriteEvent() failed on normal log %s!\n", (*p)->path.c_str() );
				ret = false;
			}
			if( !(*p)->is_dag_log && param_jobad ) {
					// The following should match ATTR_JOB_AD_INFORMATION_ATTRS
					// but cannot reference it directly because of what gets
					// linked in libcondorapi
				std::string attrsToWrite;
				param_jobad->LookupString("JobAdInformationAttrs",attrsToWrite);
				if (attrsToWrite.size() > 0) {
					writeJobAdInfoEvent(attrsToWrite.c_str(), **p, event, param_jobad, false, fmt_opts);
				}
			}
		}
	}

	if ( written ) {
		*written = ret;
	}
	return ret;
}

void
WriteUserLog::writeJobAdInfoEvent(char const *attrsToWrite, log_file& log, ULogEvent *event, const ClassAd *param_jobad, bool is_global_event, int format_opts)
{
	classad::Value result;

	ClassAd *eventAd = event->toClassAd((format_opts & ULogEvent::formatOpt::UTC) != 0);

	if (eventAd && param_jobad) {
		for (auto& curr: StringTokenIterator(attrsToWrite)) {
			if (param_jobad->EvaluateAttr(curr, result, classad::Value::ValueType::SCALAR_EX_VALUES)) {
					// now inserted evaluated expr
				bool bval = false;
				int ival;
				double dval;
				std::string sval;
				switch (result.GetType()) {
				case classad::Value::BOOLEAN_VALUE:
					result.IsBooleanValue( bval );
					eventAd->Assign( curr, bval );
					break;
				case classad::Value::INTEGER_VALUE:
					result.IsIntegerValue( ival );
					eventAd->Assign( curr, ival );
					break;
				case classad::Value::REAL_VALUE:
					result.IsRealValue( dval );
					eventAd->Assign( curr, dval );
					break;
				case classad::Value::STRING_VALUE:
					result.IsStringValue( sval );
					eventAd->Assign( curr, sval );
					break;
				default:
					break;
				}
			}
		}
	}

		// The EventTypeNumber will get overwritten to be a
		// JobAdInformationEvent, so preserve the event that triggered
		// us to write out the info in another attribute name called
		// TriggerEventTypeNumber.
	if ( eventAd  ) {
		eventAd->Assign("TriggerEventTypeNumber",event->eventNumber);
		eventAd->Assign("TriggerEventTypeName",event->eventName());
			// Now that the eventAd has everything we want, write it.
		JobAdInformationEvent info_event;
		eventAd->Assign("EventTypeNumber",info_event.eventNumber);
		info_event.initFromClassAd(eventAd);
		info_event.cluster = m_cluster;
		info_event.proc = m_proc;
		info_event.subproc = m_subproc;
		doWriteEvent(&info_event, log, is_global_event, false, format_opts);
		delete eventAd;
	}
}

bool
WriteUserLog::writeEventNoFsync (ULogEvent *event, const ClassAd *jobad,
								 bool *written )
{
	m_skip_fsync_this_event = true;
	bool retval = writeEvent( event, jobad, written );
	m_skip_fsync_this_event = false;
	return retval;
}

// Generate the uniq global ID "base"
const char *
WriteUserLog::GetGlobalIdBase( void )
{
	if ( m_global_id_base ) {
		return m_global_id_base;
	}
	std::string base;
	struct timeval now;
	condor_gettimestamp( now );

	formatstr( base, "%d.%d.%ld.%ld.", getuid(), getpid(), (long)now.tv_sec,
	           (long)now.tv_usec );

	m_global_id_base = strdup( base.c_str( ) );
	return m_global_id_base;
}

// Generates a uniq global file ID
void
WriteUserLog::GenerateGlobalId( std::string &id )
{
	struct timeval now;
	condor_gettimestamp( now );

	// First pass -- initialize the sequence #
	if ( m_global_sequence == 0 ) {
		m_global_sequence = 1;
	}

	id = "";

	// Add in the creator name
	if ( m_creator_name ) {
		id += m_creator_name;
		id += ".";
	}

	formatstr_cat( id, "%s%d.%ld.%ld", GetGlobalIdBase(), m_global_sequence,
	               (long)now.tv_sec, (long)now.tv_usec );
}

FileLockBase *
WriteUserLog::getLock(CondorError &err) {
	if (logs.empty()) {
		err.pushf("WriteUserLog", 1, "User log has no configured logfiles.\n");
		return nullptr;
	}
		// This interface returns a single file lock; for now, as we return a single lock, we
		// touch nothing.
	if (logs.size() != 1) {
		err.pushf("WriteUserLog", 1, "User log has multiple configured logfiles; cannot lock.\n");
		return nullptr;
	}
	for (auto log : logs) {
		if (log->lock) {
			return log->lock;
		}
	}
	return nullptr;
}

//
// WriteUserLogHeader methods
//

// Write a header event
int
WriteUserLogHeader::Write( WriteUserLog &writer, int fd )
{
	GenericEvent	event;

	if ( 0 == m_ctime ) {
		m_ctime = time( NULL );
	}
	if ( !GenerateEvent( event ) ) {
		return ULOG_UNK_ERROR;
	}
	return writer.writeGlobalEvent( event, fd, true );
}

// Generate a header event
bool
WriteUserLogHeader::GenerateEvent( GenericEvent &event )
{
	int len = snprintf( event.info, COUNTOF(event.info),
			  "Global JobLog:"
			  " ctime=%lld"
			  " id=%s"
			  " sequence=%d"
			  " size=" FILESIZE_T_FORMAT""
			  " events=%" PRId64""
			  " offset=" FILESIZE_T_FORMAT""
			  " event_off=%" PRId64""
			  " max_rotation=%d"
			  " creator_name=<%s>",
			  (long long) getCtime(),
			  getId().c_str(),
			  getSequence(),
			  getSize(),
			  getNumEvents(),
			  getFileOffset(),
			  getEventOffset(),
			  getMaxRotation(),
			  getCreatorName().c_str()
			  );
	if (len < 0 || len == sizeof(event.info)) {
		// not enough room in the buffer
		len = (int)COUNTOF(event.info)-1;
		event.info[len] = 0; // make sure it's null terminated.
		::dprintf( D_FULLDEBUG, "Generated (truncated) log header: '%s'\n", event.info );
	}  else {
		::dprintf( D_FULLDEBUG, "Generated log header: '%s'\n", event.info );
		while( len < 256 ) {
			event.info[len++] = ' ';
			event.info[len] = 0;
		}
	}

	return true;
}
