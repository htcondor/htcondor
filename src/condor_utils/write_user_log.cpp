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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"

#if defined ( WIN32 )
/*	Unfortunately, the trick used above for *nix does not work on
	Windows, because we us "condor_common.h" as the pre-compiled
	header, so it is a static entity by the time it is referenced
	here.  Thus below we try to mimic the equivalent of the
	above.  If this happens again, then maybe this hack can be 
	extracted and generalized to look a little nicer */
# undef open
# undef _CONDOR_ALLOW_OPEN
# define _CONDOR_ALLOW_OPEN 1
# include "condor_macros.h"
#endif

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
#include <string>
#include <algorithm>
#include "condor_attributes.h"

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


// ***************************
//  WriteUserLog constructors
// ***************************
WriteUserLog::WriteUserLog( bool disable_event_log )
{
	Reset( );
	m_global_disable = disable_event_log;
}

/* This constructor is just like the constructor below, except
 * that it doesn't take a domain, and it passes NULL for the domain and
 * the globaljobid. Hopefully it's not called anywhere by the condor code...
 * It's a convenience function, requested by our friends in LCG. */
WriteUserLog::WriteUserLog (const char *owner,
							const char *file,
							int c,
							int p,
							int s,
							bool xml)
{
	Reset( );
	m_use_xml = xml;
	
	// For PrivSep:
#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif

	initialize (owner, NULL, file, c, p, s, NULL);
}
/* This constructor is just like the constructor below, except
 * that it doesn't take a domain, and it passes NULL for the domain and
 * the globaljobid. Hopefully it's not called anywhere by the condor code...
 * It's a convenience function, requested by our friends in LCG. */
WriteUserLog::WriteUserLog (const char *owner,
							const std::vector<const char*>& file,
							int c,
							int p,
							int s,
							bool xml)
{
	Reset( );
	m_use_xml = xml;
	
	// For PrivSep:
#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif

	initialize (owner, NULL, file, c, p, s, NULL);
}

WriteUserLog::WriteUserLog (const char *owner,
							const char *domain,
							const char *file,
							int c,
							int p,
							int s,
							bool xml,
							const char *gjid)
{
	Reset();
	m_use_xml = xml;

	// For PrivSep:
#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif

	initialize (owner, domain, file, c, p, s, gjid);
}
WriteUserLog::WriteUserLog (const char *owner,
							const char *domain,
							const std::vector<const char *>& file,
							int c,
							int p,
							int s,
							bool xml,
							const char *gjid)
{
	Reset();
	m_use_xml = xml;

	// For PrivSep:
#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif

	initialize (owner, domain, file, c, p, s, gjid);
}

// Destructor
WriteUserLog::~WriteUserLog()
{
	FreeGlobalResources( true );
	FreeLocalResources( );
}


// ***********************************
//  WriteUserLog initialize() methods
// ***********************************

bool
WriteUserLog::initialize( const char *owner, const char *domain,
						  const char *file,
						  int c, int p, int s, const char *gjid )
{
	return initialize(owner,domain,std::vector<const char*>(1,file),
		c,p,s,gjid);
}
bool
WriteUserLog::initialize( const char *owner, const char *domain,
						  const std::vector<const char *>& file,
						  int c, int p, int s, const char *gjid )
{
	priv_state		priv;

	uninit_user_ids();
	if (!  init_user_ids(owner, domain) ) {
		dprintf(D_ALWAYS,
				"WriteUserLog::initialize: init_user_ids() failed!\n");
		return false;
	}

		// switch to user priv, saving the current user
	priv = set_user_priv();

		// initialize log file
	bool res = initialize( file, c, p, s, gjid );

		// get back to whatever UID and GID we started with
	set_priv(priv);

	return res;
}

bool
WriteUserLog::initialize( const char *file, int c, int p, int s,
						  const char *gjid)
{
	return initialize(std::vector<const char*>(1,file),c,p,s,gjid);
}

bool
WriteUserLog::initialize( const std::vector<const char *>& file, int c, int p, int s,
						  const char *gjid)
{
		// Save parameter info
	FreeLocalResources( );
	Configure(false);
	bool ret = true;
	if ( m_userlog_enable ) {
		for(std::vector<const char*>::const_iterator p = file.begin();
				p != file.end(); ++p) {
			log_file log(*p);
			if(!openFile(log.path.c_str(), true, m_enable_locking, true,
					log.lock, log.fp) ) {
				dprintf(D_ALWAYS, "WriteUserLog::initialize: failed to open file %s\n",
					log.path.c_str() );
				ret = false;
				break;
			} else {
				dprintf(D_FULLDEBUG, "WriteUserLog::initialize: opened %s successfully\n",
					log.path.c_str());
				logs.push_back(log);
			}
		}
	}
	// At least one of our logs failed to be initialized
	if(!ret) {
		logs.clear();
	}
	return !logs.empty() && internalInitialize( c, p, s, gjid );
}

bool
WriteUserLog::initialize( int c, int p, int s, const char *gjid )
{
	Configure(false);
	return internalInitialize( c, p, s, gjid );
}

// Internal-only initializer, invoked by all of the others
bool
WriteUserLog::internalInitialize( int c, int p, int s, const char *gjid )
{

	m_cluster = c;
	m_proc = p;
	m_subproc = s;

		// Important for performance: We do not re-open the global log
		// if we already have done so (i.e. if m_global_fp is not NULL).
	if ( !m_global_disable && m_global_path && !m_global_fp ) {
		priv_state priv = set_condor_priv();
		openGlobalLog( true );
		set_priv( priv );
	}

	if(gjid) {
		m_gjid = strdup(gjid);
	}

	m_initialized = true;
	return true;
}

// Read in our configuration information
bool
WriteUserLog::Configure( bool force )
{
	// introduce a boolean variable for local locking -- I did never really
	// care for the goto 
	bool doLocalLocking = false;
	priv_state previous;
	// If we're already configured and not in "force" mode, do nothing
	if (  m_configured && ( !force )  ) {
		return true;
	}
	FreeGlobalResources( false );
	m_configured = true;

	m_enable_fsync = param_boolean( "ENABLE_USERLOG_FSYNC", true );
	m_enable_locking = param_boolean( "ENABLE_USERLOG_LOCKING", true );

	m_global_path = param( "EVENT_LOG" );
	if ( NULL == m_global_path ) {
		return true;
	}
	m_global_stat = new StatWrapper( m_global_path, StatWrapper::STATOP_NONE );
	m_global_state = new WriteUserLogState( );


	m_rotation_lock_path = param( "EVENT_LOG_ROTATION_LOCK" );
	if ( NULL == m_rotation_lock_path ) {
		
#if !defined(WIN32)	
		bool new_locking = param_boolean("CREATE_LOCKS_ON_LOCAL_DISK", true);
		if (new_locking){
			previous = set_priv(PRIV_CONDOR);
			m_rotation_lock = new FileLock(m_global_path, true, false);
			if (m_rotation_lock->initSucceeded()) {
				doLocalLocking = true;		
			} else {
				delete m_rotation_lock;
			}
			set_priv(previous);
		}
#endif	
		if (!doLocalLocking) {
			int len = strlen(m_global_path) + 6;
			char *tmp = (char*) malloc(len);
			snprintf( tmp, len, "%s.lock", m_global_path );
			m_rotation_lock_path = tmp;
		}
	}
	if (!doLocalLocking) {
		// Make sure the global lock exists
		previous = set_priv(PRIV_CONDOR);
		m_rotation_lock_fd = open( m_rotation_lock_path, O_WRONLY|O_CREAT, 0666 );
		if ( m_rotation_lock_fd < 0 ) {
			dprintf( D_ALWAYS,
				 "Warning: WriteUserLog Failed to open event rotation lock file %s:"
				 " %d (%s)\n",
				 m_rotation_lock_path, errno, strerror(errno) );
			m_rotation_lock = new FakeFileLock( );
		}
		else {
			m_rotation_lock = new FileLock( m_rotation_lock_fd,
										NULL,
										m_rotation_lock_path );
			dprintf( D_FULLDEBUG, "WriteUserLog Created rotation lock %s @ %p\n",
				 m_rotation_lock_path, m_rotation_lock );
		}
		set_priv(previous);
	}


	m_global_use_xml = param_boolean( "EVENT_LOG_USE_XML", false );
	m_global_count_events = param_boolean( "EVENT_LOG_COUNT_EVENTS", false );
	m_global_max_rotations = param_integer( "EVENT_LOG_MAX_ROTATIONS", 1, 0 );
	m_global_fsync_enable = param_boolean( "EVENT_LOG_FSYNC", false );
	m_global_lock_enable = param_boolean( "EVENT_LOG_LOCKING", true );
	m_global_max_filesize = param_integer( "EVENT_LOG_MAX_SIZE", -1 );
	if ( m_global_max_filesize < 0 ) {
		m_global_max_filesize = param_integer( "MAX_EVENT_LOG", 1000000, 0 );
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

	m_cluster = -1;
	m_proc = -1;
	m_subproc = -1;

	m_userlog_enable = true;
	logs.clear();
	m_enable_fsync = true;
	m_enable_locking = true;

	m_global_path = NULL;
	m_global_fp = NULL;
	m_global_lock = NULL;
	m_global_stat = NULL;
	m_global_state = NULL;

	m_rotation_lock = NULL;
	m_rotation_lock_fd = -1;
	m_rotation_lock_path = NULL;

	m_use_xml = XML_USERLOG_DEFAULT;
	m_gjid = NULL;

	m_creator_name = NULL;

	m_global_disable = false;
	m_global_use_xml = false;
	m_global_count_events = false;
	m_global_max_filesize = 1000000;
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

	// For PrivSep:
#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif

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
			if(fp != NULL) {
				if(fclose(fp) != 0) {
					dprintf( D_ALWAYS,
							 "WriteUserLog::FreeLocalResources(): "
							 "fclose() failed - errno %d (%s)\n",
							 errno, strerror(errno) );
				}
			}
			delete lock;
		}
		path = rhs.path;
		fp = rhs.fp;
		lock = rhs.lock;
		rhs.copied = true;
	}
	return *this;
}
WriteUserLog::log_file::log_file(const log_file& orig) : path(orig.path), fp(orig.fp),
	lock(orig.lock), copied(false) 
{
	orig.copied = true;
}

WriteUserLog::log_file::~log_file()
{
	if(!copied) {
		if(fp != NULL) {
			if(fclose(fp) != 0) {
				dprintf( D_ALWAYS,
						 "WriteUserLog::FreeLocalResources(): "
						 "fclose() failed - errno %d (%s)\n",
						 errno, strerror(errno) );
			}
			fp = NULL;
		}
		delete lock;
		lock = NULL;
	}
}

void
WriteUserLog::FreeLocalResources( void )
{
	logs.clear();
	if (m_gjid) {
		free(m_gjid);
		m_gjid = NULL;
	}
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
	FILE		 *&fp )
{
	(void)  log_as_user;	// Quiet warning
	int 	fd = 0;

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
		fp = NULL;
		lock = NULL;
		return true;
	}

# if !defined(WIN32)
	// Unix
	int	flags = O_WRONLY | O_CREAT;
	if ( append ) {
		flags |= O_APPEND;
	}
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

		// attach it to stdio stream
	const char *fmode = append ? "a" : "w";
	fp = fdopen( fd, fmode );
	if( NULL == fp ) {
		dprintf( D_ALWAYS, "WriteUserLog::initialize: "
				 "fdopen(%i,%s) failed - errno %d (%s)\n",
				 fd, fmode, errno, strerror(errno) );
		close( fd );
		return false;
	}
# else
	// Windows (Visual C++)
	const char *fmode = append ? "a+tc" : "w+tc";
	fp = safe_fopen_wrapper_follow( file, fmode );
	if( NULL == fp ) {
		dprintf( D_ALWAYS, "WriteUserLog::initialize: "
				 "safe_fopen_wrapper_follow(\"%s\",%s) failed - errno %d (%s)\n",
				 file, fmode, errno, strerror(errno) );
		return false;
	}

	fd = _fileno(fp);
# endif

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
		lock = new FileLock( fd, fp, file );
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
	if( reopen && m_global_fp ) {
		closeGlobalLog();
	}
	else if ( m_global_fp ) {
		return true;
	}

	bool ret_val = true;
	priv_state priv = set_condor_priv();
	ret_val = openFile( m_global_path, false, m_global_lock_enable, true,
						m_global_lock, m_global_fp);

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

		MyString file_id;
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

		MyString	s;
		s.formatstr( "openGlobalLog: header: %s", m_global_path );
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
	if (m_global_fp != NULL) {
		fclose(m_global_fp);
		m_global_fp = NULL;
	}
	return true;
}

	// This method is called from doWriteEvent() - we expect the file to
	// be locked, seeked to the end of the file, and in condor priv state.
	// return true if log was rotated, either by us or someone else.
bool
WriteUserLog::checkGlobalLogRotation( void )
{
	if (!m_global_fp) {
		return false;
	}
	if ( m_global_disable || (NULL==m_global_path) ) {
		return false;
	}
	if ( !m_global_lock ||
		 m_global_lock->isFakeLock() ||
		 m_global_lock->isUnlocked() ) {
		dprintf( D_ALWAYS, "WriteUserLog checking for event log rotation, but no lock\n" );
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
	UtcTime	stat_time( true );
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
	if ( sbuf.Stat( fileno(m_global_fp) ) ) {
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
		UtcTime	start_time( true );
		dprintf( D_FULLDEBUG, "Rotating inode #%ld @ %.6f (stat @ %.6f)\n",
				 (long)swrap.GetBuf()->st_ino, start_time.combined(),
				 stat_time.combined() );
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
		ReadUserLog	log_reader( fp, m_global_use_xml, false );
		if ( header_reader.Read( log_reader ) != ULOG_OK ) {
			dprintf( D_ALWAYS,
					 "WriteUserLog: Error reading header of \"%s\"\n",
					 m_global_path );
		}
		else {
			MyString	s;
			s.formatstr( "read %s header:", m_global_path );
			header_reader.dprint( D_FULLDEBUG, s );
		}

		if ( m_global_count_events ) {
			int		events = 0;
#         if ROTATION_TRACE
			UtcTime	time1( true );
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
			UtcTime	time2( true );
			double	elapsed = time2.difference( time1 );
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
	FILE			*header_fp = NULL;
	FileLockBase	*fake_lock = NULL;
	if( !openFile(m_global_path, false, false, false, fake_lock, header_fp) ) {
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

	MyString	s;
	s.formatstr( "checkGlobalLogRotation(): %s", m_global_path );
	header_writer.dprint( D_FULLDEBUG, s );

	// And write the updated header
# if ROTATION_TRACE
	UtcTime	now( true );
	dprintf( D_FULLDEBUG, "WriteUserLog: Writing header to %s (%p) @ %.6f\n",
			 m_global_path, header_fp, now.combined() );
# endif
	if ( header_fp ) {
		rewind( header_fp );
		header_writer.Write( *this, header_fp );
		fclose( header_fp );

		MyString	tmps;
		tmps.formatstr( "WriteUserLog: Wrote header to %s", m_global_path );
		header_writer.dprint( D_FULLDEBUG, tmps );
	}
	if ( fake_lock ) {
		delete fake_lock;
	}

	// Now, rotate files
# if ROTATION_TRACE
	UtcTime	time1( true );
	dprintf( D_FULLDEBUG,
			 "WriteUserLog: Starting bulk rotation @ %.6f\n",
			 time1.combined() );
# endif

	MyString	rotated;
	int num_rotations = doRotation( m_global_path, m_global_fp,
									rotated, m_global_max_rotations );
	if ( num_rotations ) {
		dprintf(D_FULLDEBUG,
				"WriteUserLog: Rotated event log %s to %s at size %lu bytes\n",
				m_global_path, rotated.Value(),
				(unsigned long) current_filesize);
	}

# if ROTATION_TRACE
	UtcTime	end_time( true );
	if ( num_rotations ) {
		dprintf( D_FULLDEBUG,
				 "WriteUserLog: Done rotating files (inode = %ld) @ %.6f\n",
				 (long)swrap.GetBuf()->st_ino, end_time.combined() );
	}
	double	elapsed = end_time.difference( time1 );
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
	if ( NULL == m_global_stat->GetBuf() ) {
		return false;
	}
	return true;
}

bool
WriteUserLog::getGlobalLogSize( unsigned long &size, bool use_fp )
{
	StatWrapper	stat;
	if ( m_global_close && !m_global_fp ) {
		use_fp = false;
	}
	if ( use_fp ) {
		if ( !m_global_fp ) {
			return false;
		}
		if ( stat.Stat(fileno(m_global_fp)) ) {
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
WriteUserLog::doRotation( const char *path, FILE *&fp,
						  MyString &rotated, int max_rotations )
{

	int  num_rotations = 0;
	rotated = path;
	if ( 1 == max_rotations ) {
		rotated += ".old";
	}
	else {
		rotated += ".1";
		for( int i=max_rotations;  i>1;  i--) {
			MyString old1( path );
			old1.formatstr_cat(".%d", i-1 );

			StatWrapper	s( old1, StatWrapper::STATOP_STAT );
			if ( 0 == s.GetRc() ) {
				MyString old2( path );
				old2.formatstr_cat(".%d", i );
				if (rename( old1.Value(), old2.Value() )) {
					dprintf(D_FULLDEBUG, "WriteUserLog failed to rotate old log from '%s' to '%s' errno=%d\n",
							old1.Value(), old2.Value(), errno);
				}
				num_rotations++;
			}
		}
	}

# ifdef WIN32
	// on win32, cannot rename an open file
	if ( fp) {
		fclose( fp );
		fp = NULL;
	}
# else
	(void) fp;		// Quiet compiler warnings
# endif

	// Before time
	UtcTime before(true);

	if ( rotate_file( path, rotated.Value()) == 0 ) {
		UtcTime after(true);
		dprintf(D_FULLDEBUG, "WriteUserLog before .1 rot: %.6f\n", before.combined() );
		dprintf(D_FULLDEBUG, "WriteUserLog after  .1 rot: %.6f\n", after.combined() );
		num_rotations++;
	}

	return num_rotations;
}


int
WriteUserLog::writeGlobalEvent( ULogEvent &event,
								FILE *fp,
								bool is_header_event )
{
	if ( NULL == fp ) {
		fp = m_global_fp;
	}

	if ( is_header_event ) {
		rewind( fp );
	}

	return doWriteEvent( fp, &event, m_global_use_xml );
}

bool
WriteUserLog::doWriteEvent( ULogEvent *event,
							log_file& log,
							bool is_global_event,
							bool is_header_event,
							bool use_xml,
							ClassAd *)
{
	int success;
	FILE* fp;
	FileLockBase* lock;
	priv_state priv;

	if (is_global_event) {
		fp = m_global_fp;
		lock = m_global_lock;
		use_xml = m_global_use_xml;
		priv = set_condor_priv();
	} else {
		fp = log.fp;
		lock = log.lock;
		priv = set_user_priv();
	}

		// We're seeing sporadic test suite failures where a daemon
		// takes more than 10 seconds to write to the user log.
		// This will help narrow down where the delay is coming from.
	time_t before = time(NULL);
	lock->obtain (WRITE_LOCK);
	time_t after = time(NULL);
	if ( (after - before) > 5 ) {
		dprintf( D_FULLDEBUG,
				 "UserLog::doWriteEvent(): locking file took %ld seconds\n",
				 (after-before) );
	}

	before = time(NULL);
	int			status;
	const char	*whence;
	if ( is_header_event ) {
		status = fseek( fp, 0, SEEK_SET );
		whence = "SEEK_SET";
	}
	else {
		status = fseek (fp, 0, SEEK_END);
		whence = "SEEK_END";
	}
	after = time(NULL);
	if ( (after - before) > 5 ) {
		dprintf( D_FULLDEBUG,
				 "UserLog::doWriteEvent(): fseek() took %ld seconds\n",
				 (after-before) );
	}
	if ( status ) {
		dprintf( D_ALWAYS,
				 "WriteUserLog fseek(%s) failed in WriteUserLog::doWriteEvent - "
				 "errno %d (%s)\n",
				 whence, errno, strerror(errno) );
	}

		// rotate the global event log if it is too big
	if ( is_global_event ) {
		if ( checkGlobalLogRotation() ) {
				// if we rotated the log, we have a new fp and lock
			fp = m_global_fp;
			lock = m_global_lock;
		}
	}

	before = time(NULL);
	success = doWriteEvent( fp, event, use_xml );
	after = time(NULL);
	if ( (after - before) > 5 ) {
		dprintf( D_FULLDEBUG,
				 "UserLog::doWriteEvent(): writing event took %ld seconds\n",
				 (after-before) );
	}

	before = time(NULL);
	if ( fflush(fp) != 0 ) {
		dprintf( D_ALWAYS, "fflush() failed in WriteUserLog::doWriteEvent - "
				"errno %d (%s)\n", errno, strerror(errno) );
		// Note:  should we set success to false here?
	}
	after = time(NULL);
	if ( (after - before) > 5 ) {
		dprintf( D_FULLDEBUG,
				 "UserLog::doWriteEvent(): flushing event took %ld seconds\n",
				 (after-before) );
	}

	// Now that we have flushed the stdio stream, sync to disk
	// *before* we release our write lock!
	// For now, for performance, do not sync the global event log.
	if ( (   is_global_event  && m_global_fsync_enable ) ||
		 ( (!is_global_event) && m_enable_fsync ) ) {
		before = time(NULL);
		const char *fname;
		if ( is_global_event ) fname = m_global_path;
		else fname = log.path.c_str();
		if ( condor_fsync( fileno( fp ), fname ) != 0 ) {
		  dprintf( D_ALWAYS,
				   "fsync() failed in WriteUserLog::writeEvent"
				   " - errno %d (%s)\n",
				   errno, strerror(errno) );
			// Note:  should we set success to false here?
		}
		after = time(NULL);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG,
					 "UserLog::doWriteEvent(): fsyncing file took %ld secs\n",
					 (after-before) );
		}
	}
	before = time(NULL);
	lock->release ();
	after = time(NULL);
	if ( (after - before) > 5 ) {
		dprintf( D_FULLDEBUG,
				 "UserLog::doWriteEvent(): unlocking file took %ld seconds\n",
				 (after-before) );
	}
	set_priv( priv );
	return success;
}

bool
WriteUserLog::doWriteEvent( FILE *fp, ULogEvent *event, bool use_xml )
{
	ClassAd* eventAd = NULL;
	bool success = true;

	if( use_xml ) {

		eventAd = event->toClassAd();	// must delete eventAd eventually
		if (!eventAd) {
			dprintf( D_ALWAYS,
					 "WriteUserLog Failed to convert event type # %d to classAd.\n",
					 event->eventNumber);
			success = false;
		} else {
			std::string adXML;
			classad::ClassAdXMLUnParser xmlunp;

			eventAd->Delete( ATTR_TARGET_TYPE );
			xmlunp.SetCompactSpacing(false);
			xmlunp.Unparse(adXML, eventAd);
			if ( adXML.length() < 1 ) {
				dprintf( D_ALWAYS,
						 "WriteUserLog Failed to convert event type # %d to XML.\n",
						 event->eventNumber);
			}
			if (fprintf ( fp, "%s", adXML.c_str()) < 0) {
				success = false;
			} else {
				success = true;
			}
		}
	} else {
		success = event->putEvent ( fp);
		if (!success) {
			fputc ('\n', fp);
		}
		if (fprintf ( fp, "%s", SynchDelimiter) < 0) {
			success = false;
		}
	}

	if ( eventAd ) {
		delete eventAd;
	}

	return success;
}

bool
WriteUserLog::doWriteGlobalEvent( ULogEvent* event, ClassAd *ad) 
{
	log_file log;
	return doWriteEvent(event, log, true, false, m_global_use_xml, ad);
}

// Return false on error, true on goodness
bool
WriteUserLog::writeEvent ( ULogEvent *event,
						   ClassAd *param_jobad,
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
	event->setGlobalJobId(m_gjid);

	// write global event
	//TEMPTEMP -- don't try if we got a global open error
	if ( !globalOpenError && !m_global_disable && m_global_path ) {
		if ( ! doWriteGlobalEvent(event, param_jobad)  ) {
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
				m_global_use_xml );
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
		for(std::vector<log_file>::iterator p = logs.begin(); p != logs.end(); ++p) {
			if( !p->fp || !p->lock) {
				if(p->fp) {
					dprintf( D_ALWAYS, "WriteUserLog: No user log lock!\n" );
				}
				continue;
			}
				// Check our mask vector for the event
				// If we have a mask, the event must be in the mask to write the event.
			if( p != logs.begin() && !mask.empty()){
				std::vector<ULogEventNumber>::iterator pp =
					std::find(mask.begin(),mask.end(),event->eventNumber);	
				if(pp == mask.end()) {
					dprintf( D_FULLDEBUG, "Did not find %d in the mask, so do not write this event.\n",
						event->eventNumber );
					break; // We are done caring about this event
				}
			}
			if ( ! doWriteEvent(event, *p, false, false, (p == logs.begin()) && m_use_xml,
					param_jobad) ) {
				dprintf( D_ALWAYS, "WARNING: WriteUserLog::writeEvent user doWriteEvent() failed on normal log %s!\n", p->path.c_str() );
				ret = false;
			}
			if( (p == logs.begin()) && param_jobad ) {
					// The following should match ATTR_JOB_AD_INFORMATION_ATTRS
					// but cannot reference it directly because of what gets
					// linked in libcondorapi
				char *attrsToWrite = NULL;
				param_jobad->LookupString("JobAdInformationAttrs",&attrsToWrite);
				if( attrsToWrite && *attrsToWrite ) {
					writeJobAdInfoEvent( attrsToWrite, *p, event, param_jobad, false,
						(p == logs.begin()) && m_use_xml);
				}
				free( attrsToWrite );
			}
		}
	}

	if ( written ) {
		*written = ret;
	}
	return ret;
}

void
WriteUserLog::writeJobAdInfoEvent(char const *attrsToWrite, log_file& log, ULogEvent *event, ClassAd *param_jobad, bool is_global_event, bool use_xml )
{
	ExprTree *tree;
	classad::Value result;
	char *curr;

	ClassAd *eventAd = event->toClassAd();

	StringList attrs(attrsToWrite);
	attrs.rewind();
	while ( eventAd && param_jobad && (curr=attrs.next()) )
	{
		if ( (tree=param_jobad->LookupExpr(curr)) ) {
				// found the attribute.  now evaluate it before
				// we put it into the eventAd.
			if ( EvalExprTree(tree,param_jobad,NULL,result) ) {
					// now inserted evaluated expr
				bool bval;
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
		doWriteEvent(&info_event, log, is_global_event, false, use_xml, param_jobad);
		delete eventAd;
	}
}

bool
WriteUserLog::writeEventNoFsync (ULogEvent *event, ClassAd *jobad,
								 bool *written )
{
	bool saved_fsync_setting = getEnableFsync();
	setEnableFsync( false );
	bool retval = writeEvent( event, jobad, written );
	setEnableFsync( saved_fsync_setting );
	return retval;
}

// Generate the uniq global ID "base"
const char *
WriteUserLog::GetGlobalIdBase( void )
{
	if ( m_global_id_base ) {
		return m_global_id_base;
	}
	MyString	base;
	base = "";
	base += getuid();
	base += '.';
	base += getpid();
	base += '.';

	UtcTime	utc;
	utc.getTime();
	base += utc.seconds();
	base += '.';
	base += utc.microseconds();
	base += '.';

	m_global_id_base = strdup( base.Value( ) );
	return m_global_id_base;
}

// Generates a uniq global file ID
void
WriteUserLog::GenerateGlobalId( MyString &id )
{
	UtcTime	utc;
	utc.getTime();

	id = "";

	// Add in the creator name
	if ( m_creator_name ) {
		id += m_creator_name;
		id += ".";
	}

	id += GetGlobalIdBase( );

	// First pass -- initialize the sequence #
	if ( m_global_sequence == 0 ) {
		m_global_sequence = 1;
	}
	id += m_global_sequence;
	id += '.';
	id += utc.seconds();
	id += '.';
	id += utc.microseconds();
}
/*
### Local Variables: ***
### mode:c++ ***
### tab-width:4 ***
### End: ***
*/

void
WriteUserLog::setEnableFsync(bool enabled) {
	m_enable_fsync = enabled;
}

bool
WriteUserLog::getEnableFsync() {
	return m_enable_fsync;
}
