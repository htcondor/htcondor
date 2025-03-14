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
#include "condor_debug.h"
#include <stdarg.h>
#include "read_user_log.h"
#include <time.h>
#include "condor_config.h"
#include "file_lock.h"
#include "read_user_log_state.h"
#include "user_log_header.h"

const char SynchDelimiter[] = "...\n";

// Values for min scores
const int SCORE_THRESH_RESTORE		= 10;
const int SCORE_THRESH_FWSEARCH		= 4;
const int SCORE_THRESH_NONROT		= 3;
const int SCORE_MIN_MATCH			= 1;

// Score factor values
// For Windoze:
//  st_ino is meaningless, so we ignore it; set it's factor to zero
// UNIX:
//  We look at the inode & ctime
//  Note: inodes are recycled; and thus an idential inode number
//  does *not* garentee the we have the same file
//  Also note that ctime is "change time" of the inode *not*
//  creation time, we can't completely rely on it, either  :(
const int SCORE_FACTOR_UNIQ_MATCH	= 100;
# if defined(WIN32)
const int SCORE_FACTOR_CTIME		= 2;
const int SCORE_FACTOR_INODE		= 0;
# else
const int SCORE_FACTOR_CTIME		= 1;
const int SCORE_FACTOR_INODE		= 2;
# endif
const int SCORE_FACTOR_SAME_SIZE	= 2;
const int SCORE_FACTOR_GROWN		= 1;
const int SCORE_FACTOR_SHRUNK		= -5;


// Threshold to consider file stat's as recent (seconds)
const int SCORE_RECENT_THRESH		= 60;



// Class to manage score the user logs
// This class was created because you can't pre-declare the
// StructStatType in the main header file, so I created this simple
// class to manage all related stuff
class ReadUserLogMatch
{
public:

	// Constructor & destructor
	ReadUserLogMatch( ReadUserLogState *state ) {
		m_state = state;
	};
	// Note: *Don't* delete m_state -- see the comment below
	~ReadUserLogMatch( void ) { };

	// Results of file compare
	enum MatchResult {
		MATCH_ERROR = -1, MATCH = 0, UNKNOWN, NOMATCH,
	};

	// Compare the specified file / file info with the cached info
	MatchResult Match(
		int				 rot,
		int				 match_thresh,
		int				*score = NULL ) const;
	MatchResult Match(
		const char		*path,
		int				 rot,
		int				 match_thresh,
		int				*score = NULL ) const;
	MatchResult Match(
		struct stat		&statbuf,
		int				 rot,
		int				 match_thresh,
		int				*score = NULL ) const;

	// Get a string to match the result value
	const char *MatchStr( MatchResult value ) const;

private:
	MatchResult MatchInternal(
		int				 rot,
		const char		*path,
		int				 match_thresh,
		const int				*state_score ) const;
	MatchResult EvalScore(
		int				 max_thresh,
		int				 score ) const;

	// NOTE: m_state is *not* owned by this object, but, rather,
	//  we store a pointer to it for easy access
	ReadUserLogState	*m_state;		// File state info
};


// **********************************
// ReadUserLog class methods
// **********************************
ReadUserLog::ReadUserLog ( bool isEventLog )
{
	clear();
	if ( isEventLog ) {
		initialize( );
	}
}

ReadUserLog::ReadUserLog (const char * filename, bool read_only )
{
	clear();

    if (!initialize(filename, false, false, read_only )) {
		dprintf(D_ALWAYS, "ReadUserLog: Failed to open %s\n", filename);
    }
}

// Constructor that takes a state
ReadUserLog::ReadUserLog (const FileState &state, bool read_only )
{
	clear();

    if (!initialize(state, read_only )) {
		dprintf(D_ALWAYS, "Failed to initialize from state\n");
    }
}

// Create a log reader with minimal functionality
// Only reads from the file, will not lock, write the header, etc.
ReadUserLog::ReadUserLog ( FILE *fp, UserLogType  log_type, bool enable_close )
{
	clear();
	if ( ! fp ) {
		return;
	}
	m_fp = fp;
	m_fd = fileno( fp );
	m_enable_close = enable_close;

	m_lock = new FakeFileLock( );

	m_state = new ReadUserLogState( );
	m_match = new ReadUserLogMatch( m_state );

	m_initialized = true;

	setLogType( log_type );
}

// ***************************************
// * Initializers which take a file name
// ***************************************


// Initialize to read the global event log
bool
ReadUserLog::initialize( void )
{
	char	*path = param( "EVENT_LOG" );
	if ( NULL == path ) {
		Error( LOG_ERROR_FILE_NOT_FOUND,__LINE__ );
		return false;
	}
	int max_rotations = param_integer( "EVENT_LOG_MAX_ROTATIONS", 1, 0 );
	bool status = initialize( path, max_rotations, true );
	free( path );
	return status;
}

// Default initializer
bool
ReadUserLog::initialize( const char *filename,
						 bool handle_rotation,
						 bool check_for_old,
						 bool read_only )
{
	return initialize( filename,
					   handle_rotation ? 1 : 0,
					   check_for_old,
					   read_only );
}

// Initializer which allows the specifying of the max rotation level
bool
ReadUserLog::initialize( const char *filename,
						 int max_rotations,
						 bool check_for_old,
						 bool read_only )
{
	if ( m_initialized ) {
		Error( LOG_ERROR_RE_INITIALIZE, __LINE__ );
		return false;
	}

	// the magic filename "-" means read from stdin
	if (YourStringNoCase("-") == filename) {

		m_fp = stdin;
		m_fd = 0;
		m_enable_close = false;
		m_handle_rot = false;

		m_lock = new FakeFileLock( );

		m_state = new ReadUserLogState( );
		m_match = new ReadUserLogMatch( m_state );

		m_initialized = true;

		// TODO: figure out how to auto detect without seeking...
		setLogType( UserLogType::LOG_TYPE_AUTO );
		return true;
	}

	bool handle_rotation = ( max_rotations > 0 );
	m_state = new ReadUserLogState( filename, max_rotations, SCORE_RECENT_THRESH );
	if ( ! m_state->Initialized() ) {
		Error( LOG_ERROR_NOT_INITIALIZED, __LINE__ );
		return false;
	}
	m_match = new ReadUserLogMatch( m_state );

	if (! InternalInitialize( max_rotations,
							  check_for_old,
							  false,
							  handle_rotation,
							  read_only ) ) {
		return false;
	}

	return true;
}

// ***************************************
// * Initializers which take a state
// ***************************************

// Restore from state, use rotation parameters from state, too.
bool
ReadUserLog::initialize( const ReadUserLog::FileState &state,
						 bool read_only )
{
	return InternalInitialize( state, false, 0, read_only );
}

// Restore from state, setting the rotation parameters
bool
ReadUserLog::initialize( const ReadUserLog::FileState &state,
						 int max_rotations,
						 bool read_only )
{
	return InternalInitialize( state, true, max_rotations, read_only );
}

// Get / set rotation parameters
int
ReadUserLog::getMaxRot( bool handle_rotation )
{
	return (handle_rotation ? 1 : 0);
}

// can't put this into the header file on *nix for some reason.
bool ReadUserLog::Lock() {
	if (m_lock->isUnlocked()) { m_lock->obtain(WRITE_LOCK); }
	return m_lock->isLocked();
}
bool ReadUserLog::Unlock() {
	if (m_lock->isLocked()) { m_lock->release(); }
	return m_lock->isUnlocked();
}

void
ReadUserLog::initRotParms( int max_rotation )
{
	m_handle_rot = max_rotation ? true : false;
	m_max_rotations = max_rotation;
}

// ***************************************
// * Internal Initializers
// ***************************************

// Internal initialization from state
// In this case, we don't stat the file, but use the stat info
//  restored in "state" passed to us by the application
bool
ReadUserLog::InternalInitialize( const ReadUserLog::FileState &state,
								 bool set_rotations,
								 int max_rotations,
								 bool read_only )
{
	if ( m_initialized ) {
		Error( LOG_ERROR_RE_INITIALIZE, __LINE__ );
		return false;
	}

	m_state = new ReadUserLogState( state, SCORE_RECENT_THRESH );
	if (  ( m_state->InitializeError() ) || ( !m_state->Initialized() )  ) {
		Error( LOG_ERROR_STATE_ERROR, __LINE__ );
		return false;
	}

	// If max rotations specified, store it away
	if ( set_rotations ) {
		m_state->MaxRotations( max_rotations );
	}
	else {
		max_rotations = m_state->MaxRotations( );
	}

	m_match = new ReadUserLogMatch( m_state );
	return InternalInitialize( max_rotations, false, true, true, read_only );
}

// Internal only initialization
bool
ReadUserLog::InternalInitialize ( int max_rotations,
								  bool check_for_rotation,
								  bool restore,
								  bool enable_header_read,
								  bool read_only )
{
	if ( m_initialized ) {
		Error( LOG_ERROR_RE_INITIALIZE, __LINE__ );
		return false;
	}

	m_handle_rot = ( max_rotations > 0 );
	m_max_rotations = max_rotations;
	m_read_header = enable_header_read;
	m_lock = NULL;
	m_read_only = read_only;

	// Set the score factor in the file state manager
	m_state->SetScoreFactor( ReadUserLogState::SCORE_CTIME,
							 SCORE_FACTOR_CTIME );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_INODE,
							 SCORE_FACTOR_INODE );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_SAME_SIZE,
							 SCORE_FACTOR_SAME_SIZE );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_GROWN,
							 SCORE_FACTOR_GROWN );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_SHRUNK,
							 SCORE_FACTOR_SHRUNK );

	if ( restore ) {
		// Do nothing
	}
	else if ( m_handle_rot && check_for_rotation ) {
		if (! FindPrevFile( m_max_rotations, 0, true ) ) {
			releaseResources();
			Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
			return false;
		}
	}
	else {
		m_max_rotations = 0;
		if ( m_state->Rotation( 0, true ) ) {
			releaseResources();
			Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
			return false;
		}
	}

	// Should we be locking?
	if ( read_only ) {
		m_lock_enable = false;
	} else {
		m_lock_enable = param_boolean( "ENABLE_USERLOG_LOCKING", false );
	}

	// Should we close the file between operations?
	m_close_file = param_boolean( "ALWAYS_CLOSE_USERLOG", false );
# if defined(WIN32)
	if ( m_handle_rot ) {
		m_close_file = true;	// Can't rely on open FD with unlink / rename
	}
# endif

	// Now, open the file, setup locks, read the header, etc.
	if ( restore ) {
		dprintf( D_FULLDEBUG, "init: ReOpening file %s\n",
				 m_state->CurPath() );
		ULogEventOutcome status = ReopenLogFile( true );
		if ( ULOG_MISSED_EVENT == status ) {
			m_missed_event = true;	// We'll check this during readEvent()
			dprintf( D_FULLDEBUG,
					 "ReadUserLog::initialize: Missed event\n" );
		}
		else if ( status != ULOG_OK ) {
			dprintf( D_ALWAYS,
					 "ReadUserLog::initialize: error re-opening file: %d (%d @ %d)\n", status, m_error, m_line_num );
			releaseResources();
			Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
			return false;
		}
	}
	else {
		dprintf( D_FULLDEBUG, "init: Opening file %s\n", m_state->CurPath() );
		if ( ULOG_OK != OpenLogFile( restore ) ) {
			dprintf( D_ALWAYS,
					 "ReadUserLog::initialize: error opening file\n" );
			releaseResources();
			Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
			return false;
		}
	}

	// Close the file between operations
	CloseLogFile( false );
	m_initialized = true;
	return true;

}	// InternalInitialize()

void ReadUserLog::setLogType( UserLogType  log_type ) { m_state->LogType(log_type); }

ReadUserLog::FileStatus
ReadUserLog::CheckFileStatus( void )
{
	bool		is_empty;
	if ( !m_state ) {
		return ReadUserLog::LOG_STATUS_ERROR;
	}
	return m_state->CheckFileStatus( m_fd, is_empty );
}

ReadUserLog::FileStatus
ReadUserLog::CheckFileStatus( bool &is_empty )
{
	if ( !m_state ) {
		return ReadUserLog::LOG_STATUS_ERROR;
	}
	return m_state->CheckFileStatus( m_fd, is_empty );
}

bool
ReadUserLog::CloseLogFile( bool force )
{
	if ( force || m_close_file ) {

		if ( m_lock && m_lock->isLocked() ) {
			m_lock->release();
			m_lock_rot = -1;
		}

		if ( m_enable_close ) {
			if ( m_fp ) {
				fclose( m_fp );
				m_fp = NULL;
				m_fd = -1;
			}
			else if ( m_fd >= 0 ) {
				close(m_fd);
				m_fd = -1;
			}
		}
	}

	return true;
}

ULogEventOutcome
ReadUserLog::OpenLogFile( bool do_seek, bool read_header )
{
	// Note: For whatever reason, we obtain a WRITE lock in method
	// readEvent.  On Linux, if the file is opened O_RDONLY, then a
	// WRITE_LOCK never blocks.  Thus open the file RDWR so the
	// WRITE_LOCK below works correctly.
	//
	// NOTE: we tried changing this to O_READONLY once and things
	// stopped working right, so don't try it again, smarty-pants!

	// Is the lock current?
	bool	is_lock_current = ( m_state->Rotation() == m_lock_rot );

	dprintf( D_FULLDEBUG, "Opening log file #%d '%s' "
			 "(is_lock_cur=%s,seek=%s,read_header=%s)\n",
			 m_state->Rotation(), m_state->CurPath(),
			 is_lock_current ? "true" : "false",
			 do_seek ? "true" : "false",
			 read_header ? "true" : "false" );
	if ( m_state->Rotation() < 0 ) {
		if ( m_state->Rotation(-1) < 0 ) {
			return ULOG_RD_ERROR;
		}
	}

	m_fd = safe_open_wrapper_follow( m_state->CurPath(),
							  m_read_only ? (O_RDONLY | _O_BINARY) : (O_RDWR | _O_BINARY),
							  0 );
	if ( m_fd < 0 ) {
		dprintf(D_ALWAYS, "ReadUserLog::OpenLogFile safe_open_wrapper on %s returns %d: error %d(%s)\n", m_state->CurPath(), m_fd, errno, strerror(errno));
		return ULOG_RD_ERROR;
	}

	m_fp = fdopen( m_fd, "rb" );
	if ( m_fp == NULL ) {
		CloseLogFile( true );
		dprintf(D_ALWAYS, "ReadUserLog::OpenLogFile fdopen returns NULL\n");
	    return ULOG_RD_ERROR;
	}

	// Seek to the previous location
	if ( do_seek && m_state->Offset() ) {
		if( fseek( m_fp, m_state->Offset(), SEEK_SET) ) {
			CloseLogFile( true );
			dprintf(D_ALWAYS, "ReadUserLog::OpenLogFile fseek returns NULL\n");
			return ULOG_RD_ERROR;
		}
	}

	// Prepare to lock the file
	if ( m_lock_enable ) {

		// If the lock isn't for the current file (rotation #), destroy it
		if ( ( !is_lock_current ) && m_lock ) {
			delete m_lock;
			m_lock = NULL;
			m_lock_rot = -1;
		}

		// Create a lock if none exists
		// otherwise, update the lock's fd & fp
		if ( ! m_lock ) {
			dprintf( D_FULLDEBUG, "Creating file lock(%d,%p,%s)\n",
					 m_fd, m_fp, m_state->CurPath() );
			bool new_locking = param_boolean("CREATE_LOCKS_ON_LOCAL_DISK", true);
#if defined(WIN32)
			new_locking = false;
#endif
			if (new_locking) {
				m_lock = new FileLock(m_state->CurPath(), true, false);
				if (! m_lock->initSucceeded() ) {
					delete m_lock;
					m_lock = new FileLock( m_fd, m_fp, m_state->CurPath() );
				}
			} else {
				m_lock = new FileLock( m_fd, m_fp, m_state->CurPath() );
			}
			if( ! m_lock ) {
				CloseLogFile( true );
				dprintf(D_ALWAYS, "ReadUserLog::OpenLogFile FileLock returns NULL\n");
				return ULOG_RD_ERROR;
			}
			m_lock_rot = m_state->Rotation( );
		}
		else {
			m_lock->SetFdFpFile( m_fd, m_fp, m_state->CurPath() );
		}
	}
	else {
		if ( m_lock ) {
			delete m_lock;
			m_lock = NULL;
			m_lock_rot = -1;
		}
		m_lock = new FakeFileLock;
	}

	// Determine the type of the log file (if needed)
	if ( m_state->IsUnknownLogType() ) {
		if ( !determineLogType() ) {
			dprintf( D_ALWAYS,
					 "ReadUserLog::OpenLogFile(): Can't log type\n" );
			releaseResources();
			return ULOG_RD_ERROR;
		}
	}

	// Read the file's header event
	if ( read_header && m_read_header && ( !m_state->ValidUniqId()) ) {
		const char *path = m_state->CurPath( );

		// If no path provided, generate one
		std::string temp_path;
		if (  NULL == path ) {
			m_state->GeneratePath( m_state->Rotation(), temp_path );
			path = temp_path.c_str( );
		}

		// Now, try read the header
		ReadUserLog			log_reader;
		ReadUserLogHeader	header_reader;
		if (  ( path ) &&
			  ( log_reader.initialize( path, false, false, true ) ) &&
			  ( header_reader.Read( log_reader ) == ULOG_OK )  ) {
			m_state->UniqId( header_reader.getId() );
			m_state->Sequence( header_reader.getSequence() );
			m_state->LogPosition( header_reader.getFileOffset() );
			if ( header_reader.getEventOffset() ) {
				m_state->LogRecordNo( header_reader.getEventOffset() );
			}
			dprintf( D_FULLDEBUG,
					 "%s: Set UniqId to '%s', sequence to %d\n",
					 m_state->CurPath(),
					 header_reader.getId().c_str(),
					 header_reader.getSequence() );
		}

		// Finally, no path -- give up
		else {
			dprintf( D_FULLDEBUG, "%s: Failed to read file header\n",
					 m_state->CurPath() );
		}
	}

	return ULOG_OK;
}

bool
ReadUserLog::determineLogType( void )
{
	// now determine if the log file is XML and skip over the header (if
	// there is one) if it is XML

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	Lock();

	// store file position so we can rewind to this location
	long filepos = ftell( m_fp );
	if( filepos < 0 ) {
		dprintf(D_ALWAYS, "ftell failed in ReadUserLog::determineLogType\n");
		Unlock();
		Error( LOG_ERROR_FILE_OTHER, __LINE__ );
		return false;
	}
	m_state->Offset( filepos );

	if ( fseek( m_fp, 0, SEEK_SET ) < 0 ) {
		dprintf(D_ALWAYS,
				"fseek(0) failed in ReadUserLog::determineLogType\n");
		Unlock();
		Error( LOG_ERROR_FILE_OTHER, __LINE__ );
		return false;
	}
	char intro[2] = { 0,0 };
	int scanf_result = fscanf(m_fp, " %1[<{01]", intro);

	if (scanf_result <= 0) {
		// what sort of log is this???
		dprintf(D_FULLDEBUG, "Error, apparently invalid user log file\n");
		m_state->LogType(UserLogType::LOG_TYPE_UNKNOWN);
	} else if ('<' == intro[0]) {
		m_state->LogType( UserLogType::LOG_TYPE_XML );

		int afterangle = fgetc(m_fp);

		// If we're at the start of the file, skip the header
		if ( filepos == 0 ) {
			if( !skipXMLHeader(afterangle, filepos) ) {
				m_state->LogType( UserLogType::LOG_TYPE_UNKNOWN );
				Unlock();
				Error( LOG_ERROR_FILE_OTHER, __LINE__ );
				return false;
			}
		}

		// File type set, we're all done.
		Unlock();
		return true;
	} else if ('{' == intro[0]) {
		m_state->LogType(UserLogType::LOG_TYPE_JSON);
	} else {
		m_state->LogType(UserLogType::LOG_TYPE_NORMAL);
	}

	if( fseek( m_fp, filepos, SEEK_SET ) ) {
		dprintf( D_ALWAYS,
				 "fseek failed in ReadUserLog::determineLogType\n");
		Unlock();
		Error( LOG_ERROR_FILE_OTHER, __LINE__ );
		return false;
	}

	Unlock();
	return true;
}

bool
ReadUserLog::skipXMLHeader(int afterangle, long filepos)
{
	// now figure out if there is a header, and if so, advance _fp past
	// it - this is really ugly
	int nextchar = afterangle;
	if( nextchar == '?' || nextchar == '!' ) {
		// we're probably in the document prolog
		while( nextchar == '?' || nextchar == '!' ) {
			// skip until we get out of this tag
			nextchar = fgetc(m_fp);
			while( nextchar != EOF && nextchar != '>' ) {
				nextchar = fgetc(m_fp);
			}

			if( nextchar == EOF ) {
				Error( LOG_ERROR_FILE_OTHER, __LINE__ );
				return false;
			}

			// skip until we get to the next tag, saving our location as
			// we go so we can skip back two chars later
			while( nextchar != EOF && nextchar != '<' ) {
				filepos = ftell(m_fp);
				if (filepos < 0) {
					Error( LOG_ERROR_FILE_OTHER, __LINE__ );
					return false;
				}
				nextchar = fgetc(m_fp);
			}
			if( nextchar == EOF ) {
				Error( LOG_ERROR_FILE_OTHER, __LINE__ );
				return false;
			}
			nextchar = fgetc(m_fp);
		}

		// now we are in a tag like <[^?!]*>, so go back two chars and
		// we're all set
		if( fseek(m_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::skipXMLHeader\n");
			Error( LOG_ERROR_FILE_OTHER, __LINE__ );
			return false;
		}
	} else {
		// there was no prolog, so go back to the beginning
		if( fseek(m_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::skipXMLHeader\n");
			Error( LOG_ERROR_FILE_OTHER, __LINE__ );
			return false;
		}
	}

	m_state->Offset( filepos );

	return true;
}

bool
ReadUserLog::FindPrevFile( int start, int num, bool store_stat )
{
	if ( !m_handle_rot ) {
		return true;
	}

	// Deterine the range to look at
	int				end;
	if ( 0 == num ) {
		end = 0;
	} else {
		end = start - num + 1;
		if ( end < 0 ) {
			end = 0;
		}
	}

	// Search for the previous file
	for ( int rot = start;  rot >= end;  rot-- ) {
		if ( 0 == m_state->Rotation( rot, store_stat )  ) {
			dprintf( D_FULLDEBUG, "Found: '%s'\n", m_state->CurPath() );
			return true;
		}
	}
	Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
	return false;
}

ULogEventOutcome
ReadUserLog::ReopenLogFile( bool restore )
{

	// First, if the file's open, we're done.  :)
	if ( m_fp ) {
		return ULOG_OK;
	}

	// If we're not handling rotation, just try to reopen the file
	if ( ! m_handle_rot ) {
		return OpenLogFile( true );
	}

	// If we don't have valid info, do a new file search, just like init
	if ( ! m_state->IsValid() ) {
		if ( m_handle_rot ) {
			dprintf( D_FULLDEBUG, "reopen: looking for previous file...\n" );
			if (! FindPrevFile( m_max_rotations, 0, true ) ) {
				Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
				return ULOG_NO_EVENT;
			}
		}
		else {
			if ( m_state->Rotation( 0, true ) ) {
				Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
				return ULOG_NO_EVENT;
			}
		}
		return OpenLogFile( false );
	}

	// Search forward, starting with the "current" file, looking
	// for a file with the same "signature", or the highest score.
	int new_rot = -1;
	int max_score = -1;
	int max_score_rot = -1;
	int *scores = new int[m_max_rotations+1];
	int	start = m_state->Rotation();
	int thresh = restore ? SCORE_THRESH_RESTORE : SCORE_THRESH_FWSEARCH;
	for( int rot = start; (rot <= m_max_rotations) && (new_rot < 0); rot++ ) {
		int		score;

		ReadUserLogMatch::MatchResult result =
			m_match->Match( rot, thresh, &score );
		if ( ReadUserLogMatch::MATCH_ERROR == result ) {
			scores[rot] = -1;
		}
		else if ( ReadUserLogMatch::MATCH == result ) {
			new_rot = rot;
		}
		else if ( ReadUserLogMatch::UNKNOWN == result ) {
			scores[rot] = score;
			if ( score > max_score ) {
				max_score_rot = rot;
				max_score = score;
			}
		}
	}
	delete [] scores;

	// No good match found, fall back to highest score
	if ( ( new_rot < 0 )  &&  ( max_score > 0 )  ) {
		if ( restore ) {
			return ULOG_MISSED_EVENT;
		}
		new_rot = max_score_rot;
	}

	// If we've found a good match, or a high score, do it
	if ( new_rot >= 0 ) {
		if ( m_state->Rotation( new_rot ) ) {
			Error( LOG_ERROR_FILE_NOT_FOUND, __LINE__ );
			return ULOG_RD_ERROR;
		}
		return OpenLogFile( true );
	}

	// If we got here, no match found.  :(
	m_state->Reset( );			// We know nothing about the state now!
	return ULOG_MISSED_EVENT;
}

ULogEventOutcome
ReadUserLog::internalReadEvent (ULogEvent *& event, bool store_state )
{
	if ( !m_initialized ) {
		Error( LOG_ERROR_NOT_INITIALIZED, __LINE__ );
		return ULOG_RD_ERROR;
	}

	// Previous operation (initialization) detected a missed event
	// but couldn't report it to the application (the API doesn't
	// allow us to reliably return that type of info).
	if ( m_missed_event ) {
		m_missed_event = false;
		return ULOG_MISSED_EVENT;
	}

	int starting_seq = m_state->Sequence( );
	int starting_event = (int) m_state->EventNum( );
	filesize_t starting_recno = m_state->LogRecordNo();


	// If the file was closed on us, try to reopen it
	if ( !m_fp ) {
		ULogEventOutcome	status = ReopenLogFile( );
		if ( ULOG_OK != status ) {
			return status;
		}
	} else {
		// If the file wasn't closed, and the file is on AFS, we need
		// to stat() it to make sure that AFS notices if some other
		// machine wrote to the file since we last looked at it.
		//
		// See HTCONDOR-463 for details.  If it ends up mattering, we
		// could check at object construction time if this file is on
		// AFS (using statfs) and skip this system call if not.
		struct stat statbuf;
		std::ignore = fstat(m_fd, &statbuf);
	}

	if ( !m_fp ) {
		return ULOG_NO_EVENT;
	}
	
	/*
		09/27/2010 (cweiss): Added this check because so far the reader could get stuck
		in a non-recoverable state when ending up in feof. (Example scenario: XML writer 
		and reader on the same file, reader reads in 
			while (reader.readEvent(event) == ULOG_OK) ...
		mode, locks are *not* on the file but on  designated local disk lock files, which
		means that the reader's file pointer is not tampered with. Then XML writer
		writes another event -- this will never be discovered by the reader.)
	*/
	if ( feof(m_fp) ) {
		clearerr(m_fp);
	}
	
	ULogEventOutcome	outcome = ULOG_OK;
	bool try_again = false;
	if( m_state->IsUnknownLogType() ) {
	    if( !determineLogType() ) {
			outcome = ULOG_RD_ERROR;
			Error( LOG_ERROR_FILE_OTHER, __LINE__ );
			goto CLEANUP;
		}
	}

	// Now, read the actual event (depending on the file type)
	outcome = rawReadEvent( event, &try_again );
	if ( ! m_handle_rot ) {
		try_again = false;
	}

	// If we hit the end of a rotated file, try the previous one
	if ( try_again ) {

		// We've hit the end of file and file has been closed
		// This means that we've missed an event :(
		if ( m_state->Rotation() < 0 ) {
			return ULOG_MISSED_EVENT;
		}

		// We've hit the end of a non-rotated file
		// (a file that isn't a ".old" or ".1", etc.)
		else if ( m_state->Rotation() == 0 ) {
			// Same file?
			ReadUserLogMatch::MatchResult result;
			result = m_match->Match( m_state->CurPath(),
									 m_state->Rotation(),
									 SCORE_THRESH_NONROT );
			dprintf( D_FULLDEBUG,
					 "readEvent: checking to see if file (%s) matches: %s\n",
					 m_state->CurPath(), m_match->MatchStr(result) );
			if ( result == ReadUserLogMatch::NOMATCH ) {
				CloseLogFile( true );
			}
			else {
				try_again = false;
			}
		}

		// We've hit the end of a ".old" or ".1", ".2" ... file
		else {
			CloseLogFile( true );

			bool found = FindPrevFile( m_state->Rotation() - 1, 1, true );
			dprintf( D_FULLDEBUG,
					 "readEvent: checking for previous file (# %d): %s\n",
					 m_state->Rotation(), found ? "Found" : "Not found" );
			if ( found ) {
				CloseLogFile( true );
			}
			else {
				try_again = false;
			}
		}
	}

	// Finally, one more attempt to read an event
	if ( try_again ) {
		outcome = ReopenLogFile();
		if ( ULOG_OK == outcome ) {
			outcome = rawReadEvent( event, nullptr );
		}
	}

	// Store off our current offset
	if (  ( ULOG_OK == outcome ) && ( store_state )  )  {
		long	pos = ftell( m_fp );
		if ( pos > 0 ) {
			m_state->Offset( pos );
		}

		if ( ( m_state->Sequence() != starting_seq ) &&
			 ( 0 == m_state->LogRecordNo() ) ) {
			// Don't count the header record in the count below
			m_state->LogRecordNo( starting_recno + starting_event - 1 );
		}
		m_state->EventNumInc();
		m_state->StatFile( m_fd );
	}

	// Close the file between operations
  CLEANUP:
	CloseLogFile( false );

	return outcome;

}

ULogEventOutcome
ReadUserLog::rawReadEvent( ULogEvent *& event, bool *try_again )
{
	ULogEventOutcome	outcome;

	if( m_state->IsClassadLogType() ) {
		outcome = readEventClassad( event, m_state->LogType() );
		if ( try_again ) {
			*try_again = (outcome == ULOG_NO_EVENT );
		}
	} else if(m_state->LogType() == UserLogType::LOG_TYPE_AUTO) {
		outcome = readEventNormal( event );
		if ( try_again ) {
			*try_again = (outcome == ULOG_NO_EVENT );
		}
	} else if(m_state->LogType() == UserLogType::LOG_TYPE_NORMAL) {
		outcome = readEventNormal( event );
		if ( try_again ) {
			*try_again = (outcome == ULOG_NO_EVENT );
		}
	} else {
		outcome = ULOG_NO_EVENT;
		if ( try_again ) {
			*try_again = false;
		}
	}
	return outcome;
}

ULogEventOutcome
ReadUserLog::readEventClassad( ULogEvent *& event, int log_type )
{

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	ASSERT(m_initialized);
	Lock();

	// store file position so that if we are unable to read the event, we can
	// rewind to this location
	long     filepos;
	if (!m_fp || ((filepos = ftell(m_fp)) == -1L))
	{
		Unlock();
		event = NULL;
		return ULOG_UNK_ERROR;
	}

	ClassAd* eventad = new ClassAd();
	if (log_type == UserLogType::LOG_TYPE_JSON) {
		classad::ClassAdJsonParser jsonp;
		if (!jsonp.ParseClassAd(m_fp, *eventad)) {
			delete eventad;
			eventad = NULL;
		}
	} else {
		classad::ClassAdXMLParser xmlp;
		if ( !xmlp.ParseClassAd(m_fp, *eventad) ) {
			delete eventad;
			eventad = NULL;
		}
	}

	Unlock();

	if( !eventad ) {
		// we don't have the full event in the stream yet; restore file
		// position and return
		if( fseek(m_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent\n");
			return ULOG_UNK_ERROR;
		}
		clearerr(m_fp);
		event = NULL;
		return ULOG_NO_EVENT;
	}

	int enmbr;
	if( !eventad->LookupInteger("EventTypeNumber", enmbr) ) {
		event = NULL;
		delete eventad;
		return ULOG_NO_EVENT;
	}

	if( !(event = instantiateEvent((ULogEventNumber) enmbr)) ) {
		event = NULL;
		delete eventad;
		return ULOG_UNK_ERROR;
	}

	event->initFromClassAd(eventad);

	delete eventad;
	return ULOG_OK;
}

/*
 *  This is the heart of the user log reader.  The precondition on entry is
 *  that the fp in the class is pointing at the very start of a "native" user
 *  log event, or at end of file (assuming the end of file is immediately after
 *  a completely written event).  The writers must guarantee that events are
 *  not interleaved.  They do this by gathering up all the bytes in a single
 *  event, and writing them in one write(2) call, to an fd opened in O_APPEND
 *  mode.  If somehow the fp is pointing in the middle of an event, we don't go
 *  out of our way to detect this, but if we do, because we can't parse the
 *  event, we will consume what's left of this event by reading and throwing
 *  away all lines up to the next "^...", and returning ULOG_UNK_ERROR.
 *
 *  However, even if the writers are writing in O_APPEND with a single write,
 *  we may still see a short write.  This is because POSIX only guarantees that
 *  readers will see the full write if the read operation starts after the
 *  write call has returned to userspace.  We make no attempt to enforce this.
 *  However, we do know where the event ends, at the "^..." terminator.  If we
 *  don't see this when we read, we know we have a short read.  In this case,
 *  we fseek back to the where we started (hopefully, the beginning of an
 *  event!), and return ULOG_NO_EVENT, which tells the caller to call us again
 *  (perhaps after a delay) to try again.  ULOG_NO_EVENT is also returned when
 *  EOF is hit.
 *
 *  When the code was originally written, it used file locking in the writer to
 *  try to enforce both no short reads and no iterleaved events.  This was
 *  problematic because it would not be reliable on shared filesystems like
 *  NFS.  Switching to single write(2) calls fixes the interleaving problems.
 *
 *  We've left the read locking in here for historical reasons.  I think
 *  it can be removed, except perhaps for the case of file rotation.
 *  
 *
 */

ULogEventOutcome ReadUserLog::readEventNormal( ULogEvent *& event )
{
	long   filepos;
	//int    eventnumber;
	int    retval1, retval2;
	bool   got_sync_line = false;
	ULogFile ulf;
	ULogEventNumber eventnumber = ULOG_NO;
	char headbuf[1024]; // same size buf that readLine uses, and plenty big enough for the event header line

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	Lock();

	// store file position so that if we are unable to read the event, we can
	// rewind to this location
	if (!m_fp || ((filepos = ftell(m_fp)) == -1L))
	{
		dprintf( D_ALWAYS,
				 "ReadUserLog: invalid m_fp, or ftell() failed\n" );
		Unlock();
		return ULOG_UNK_ERROR;
	}

#if 1
	event = NULL;  // To prevent FMR: Free memory read
	ulf.attach(m_fp);
	eventnumber = ULogEvent::readEventNumber(ulf, headbuf, sizeof(headbuf));
	retval1 = (eventnumber != ULOG_NO) ? 1 : 0;
	if (eventnumber == ULOG_NO) {  // was it the 'no' event type? (i.e. -1 ?)
		int save_errno = errno;
		// check for end of file -- why this is needed has been
		// lost, but it was removed once and everything went to
		// hell, so don't touch it...
		// Note: this is needed because if this method is called and
		// you're at the end of the file, fscanf returns EOF (-1) and
		// you get here.  If you're at EOF you had better bail out...
		// (This is not uncommon -- any time you try to read an event
		// and there aren't any events to read you get here.)
		// If fscanf returns 0, you're probably *really* in trouble.
		// wenger 2004-10-07.
		if (ulf.atEnd()) {
			ulf.clearEof();
			ulf.attach(nullptr);
			Unlock();
			return ULOG_NO_EVENT;
		}
		ulf.attach(nullptr);
		Unlock();
		// if unable to read the event number, but we did read something and logtype is AUTO
		// look for the indications of XML or JSON format
		if (m_state->LogType() == UserLogType::LOG_TYPE_AUTO) {
			if (headbuf[0] == '<' && headbuf[1] == 'c') {
				m_state->LogType(UserLogType::LOG_TYPE_XML);
				// TODO: instantiate the first XML event without seeking back
				return ULOG_NO_EVENT;
			} else if (headbuf[0] == '{') {
				m_state->LogType(UserLogType::LOG_TYPE_JSON);
				// TODO: instantiate the first JSON event without seeking back
				return ULOG_NO_EVENT;
			}
		}
		dprintf( D_ALWAYS, "ReadUserLog: error %d (not EOF) reading event number at position %ld\n", save_errno, filepos);
		return ULOG_NO_EVENT;
	} else if (m_state->LogType() == UserLogType::LOG_TYPE_AUTO) {
		// we just auto detected a normal logtype
		m_state->LogType(UserLogType::LOG_TYPE_NORMAL);
	}

	event = instantiateEvent(eventnumber);
	if ( ! event) {
		dprintf( D_ALWAYS, "ReadUserLog: unable to instantiate event\n" );
		Unlock();
		return ULOG_UNK_ERROR;
	}
	// now populate the event, we pass in the header line at offset 3, because we already read the event number
	// and the code below needs to parse the rest of the header.  This is a milestone along the way
	// to reading the whole block of event text into a buffer before we start parsing.
	got_sync_line = false;
	retval2 = event->getEvent (ulf, headbuf+3, got_sync_line);
	ulf.attach(nullptr); // detach
#else
	retval1 = fscanf (m_fp, "%d", &eventnumber);

	// so we don't dump core if the above fscanf failed
	if (retval1 != 1) {
		int save_errno = errno;
		eventnumber = 1;
		// check for end of file -- why this is needed has been
		// lost, but it was removed once and everything went to
		// hell, so don't touch it...
			// Note: this is needed because if this method is called and
			// you're at the end of the file, fscanf returns EOF (-1) and
			// you get here.  If you're at EOF you had better bail out...
			// (This is not uncommon -- any time you try to read an event
			// and there aren't any events to read you get here.)
			// If fscanf returns 0, you're probably *really* in trouble.
			// wenger 2004-10-07.
		if( feof( m_fp ) ) {
			event = NULL;  // To prevent FMR: Free memory read
			clearerr( m_fp );
			Unlock();
			return ULOG_NO_EVENT;
		}
		dprintf( D_ALWAYS, "ReadUserLog: error %d (not EOF) reading event number\n", save_errno);
	}

	// allocate event object; check if allocated successfully
	event = instantiateEvent ((ULogEventNumber) eventnumber);
	if (!event) {
		dprintf( D_ALWAYS, "ReadUserLog: unable to instantiate event\n" );
		Unlock();
		return ULOG_UNK_ERROR;
	}

	// read event from file; check for result
	got_sync_line = false;
	retval2 = event->getEvent (m_fp, got_sync_line);
#endif

	// check if error in reading event
	if (!retval1 || !retval2)
	{
		dprintf( D_ALWAYS,
				 "ReadUserLog: error reading event; re-trying\n" );

		// we could end up here if file locking did not work for
		// whatever reason (usual NFS bugs, whatever).  so here
		// try to wait a second until the current partially-written
		// event has benn completely written.  the algorithm is
		// wait a second, rewind to our initial position (in case a
		// buggy getEvent() slurped up more than one event), then
		// again try to synchronize the log
		//
		// NOTE: this code is important, so don't remove or "fix"
		// it unless you *really* know what you're doing and test it
		// extermely well
		Unlock();
		sleep( 1 );
		Lock();
		if( fseek( m_fp, filepos, SEEK_SET)) {
			dprintf( D_ALWAYS, "fseek() failed in %s:%d\n", __FILE__, __LINE__ );
			Unlock();
			return ULOG_UNK_ERROR;
		}
		if( synchronize() )
		{
			// if synchronization was successful, reset file position and ...
			if (fseek (m_fp, filepos, SEEK_SET))
			{
				dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent\n");
				Unlock();
				return ULOG_UNK_ERROR;
			}
			got_sync_line = false;

			// ... attempt to read the event again
			clearerr (m_fp);
		#if 1
			ULogEventNumber oldeventnumber = eventnumber;
			eventnumber = ULOG_NO;
			ulf.attach(m_fp);
			eventnumber = ULogEvent::readEventNumber(ulf, headbuf, sizeof(headbuf));
			retval1 = (eventnumber != ULOG_NO) ? 1 : 0;
		#else
			int oldeventnumber = eventnumber;
			eventnumber = -1;
			retval1 = fscanf (m_fp, "%d", &eventnumber);
		#endif
			if( retval1 == 1 ) {
				if( eventnumber != oldeventnumber ) {
					if( event ) {
						delete event;
					}

					// allocate event object; check if allocated
					// successfully
					event = instantiateEvent( (ULogEventNumber)eventnumber );
					if( !event ) {
						dprintf( D_FULLDEBUG, "ReadUserLog: unable to "
								 "instantiate event\n" );
						Unlock();
						return ULOG_UNK_ERROR;
					}
				}
		#if 1
				retval2 = event->getEvent (ulf, headbuf+3, got_sync_line);
				ulf.attach(nullptr); // detach
		#else
				retval2 = event->getEvent( m_fp, got_sync_line );
		#endif
			}

			// if failed again, we have a parse error
			if (retval1 != 1 || !retval2)
			{
				dprintf( D_ALWAYS, "ReadUserLog: error reading event "
							"on second try\n");
				delete event;
				event = NULL;  // To prevent FMR: Free memory read
				if ( ! got_sync_line) { synchronize (); }
				Unlock();
				return ULOG_RD_ERROR;
			}
			else
			{
				// finally got the event successfully --
				// synchronize the log
				if( got_sync_line || synchronize() ) {
					Unlock();
					return ULOG_OK;
				}
				else
				{
					// got the event, but could not synchronize!!
					// treat as incomplete event
					dprintf( D_ALWAYS,
							 "ReadUserLog: got event on second try "
							 "but synchronize() failed\n");
					delete event;
					event = NULL;  // To prevent FMR: Free memory read
					clearerr( m_fp );
					if (fseek (m_fp, filepos, SEEK_SET))
					{
						dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent\n");
						Unlock();
						return ULOG_UNK_ERROR;
					}
					Unlock();
					return ULOG_NO_EVENT;
				}
			}
		}
		else
		{
			// if we could not synchronize the log, we don't have the full
			// event in the stream yet; restore file position and return
			dprintf( D_ALWAYS, "ReadUserLog: synchronize() failed\n");
			if (fseek (m_fp, filepos, SEEK_SET))
			{
				dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent\n");
				Unlock();
				return ULOG_UNK_ERROR;
			}
			clearerr (m_fp);
			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			Unlock();
			return ULOG_NO_EVENT;
		}
	}
	else
	{
		// got the event successfully -- synchronize the log
		if (got_sync_line || synchronize ())
		{
			Unlock();
			return ULOG_OK;
		}
		else
		{
			// got the event, but could not synchronize!!  treat as incomplete
			// event
			dprintf( D_ALWAYS, "ReadUserLog: got event on first try "
					"but synchronize() failed\n");

			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			clearerr (m_fp);
			if (fseek (m_fp, filepos, SEEK_SET))
			{
				dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent\n");
				Unlock();
				return ULOG_UNK_ERROR;
			}
			Unlock();
			return ULOG_NO_EVENT;
		}
	}

	// will not reach here
}

// Static method for initializing a file state
bool
ReadUserLog::InitFileState( ReadUserLog::FileState &state )
{
	return ReadUserLogState::InitState( state );
}

// Static method for un-initializing a file state
bool
ReadUserLog::UninitFileState( ReadUserLog::FileState &state )
{
	return ReadUserLogState::UninitState( state );
}

bool
ReadUserLog::GetFileState( ReadUserLog::FileState &state ) const
{
	if ( !m_initialized ) {
		Error( LOG_ERROR_NOT_INITIALIZED, __LINE__ );
		return false;
	}
	return m_state->GetState( state );
}

bool
ReadUserLog::SetFileState( const ReadUserLog::FileState &state )
{
	if ( !m_initialized ) {
		Error( LOG_ERROR_NOT_INITIALIZED, __LINE__ );
		return false;
	}
	return m_state->SetState( state );
}

bool
ReadUserLog::synchronize ( void )
{
	if ( !m_initialized ) {
		Error( LOG_ERROR_NOT_INITIALIZED, __LINE__ );
		return false;
	}

	const int ixN = sizeof(SynchDelimiter)-2; // back up to point at the \n (assuming size includes a trailing \0)
	const int bufSize = 512;
    char buffer[bufSize];
    while( fgets( buffer, bufSize, m_fp ) != NULL ) {
		if (buffer[0] != '.') continue;
		// this is a bit hacky, but if we have ...\r\n\0, it will covert it to ...\n\0.
		// so that the strcmp will match it.
		if (buffer[ixN] == '\r') { buffer[ixN] = buffer[ixN+1]; buffer[ixN+1] = buffer[ixN+2]; }
		if( strcmp( buffer, SynchDelimiter) == 0 ) {
            return true;
        }
    }
    return false;
}

void
ReadUserLog::outputFilePos( const char *pszWhereAmI )
{
	ASSERT ( m_initialized );
	dprintf(D_ALWAYS, "Filepos: %ld, context: %s\n", ftell(m_fp), pszWhereAmI);
}

void
ReadUserLog::clear( void )
{
	m_initialized = false;
	m_missed_event = false;
	m_state = NULL;
	m_match = NULL;
    m_fd = -1;
	m_fp = NULL;
	m_lock = NULL;
	m_lock_rot = -1;

	m_close_file = false;
	m_read_only = false;
	m_enable_close = true;
	m_handle_rot = false;
	m_lock_enable = false;
	m_max_rotations = 0;
	m_read_header = false;

	m_error = LOG_ERROR_NONE;
	m_line_num = 0;
}

void
ReadUserLog::releaseResources( void )
{
	if ( m_match ) {
		delete m_match;
		m_match = NULL;
	}

	if ( m_state ) {
		delete m_state;
		m_state = NULL;
	}

	CloseLogFile( true );

	delete m_lock;
	m_lock = NULL;
}

void
ReadUserLog:: getErrorInfo( ErrorType &error,
							const char *& error_str,
							unsigned &line_num ) const
{
	const char *strings[] = {
		"None",
		"Reader not initialized",
		"Attempt to re-initialize reader",
		"File not found",
		"Other file error",
		"Invalid state buffer",
	};
	error = m_error;
	line_num = m_line_num;

	unsigned	eint = (unsigned) m_error;
	unsigned	num = ( sizeof(strings) / sizeof(const char *) );
	if ( eint >= num ) {
		error_str = "Unknown";
	}
	else {
		error_str = strings[eint];
	}

};


// **********************************
// ReadUserLogMatch methods
// **********************************

// Compare a file by rotation # to the cached info
ReadUserLogMatch::MatchResult
ReadUserLogMatch::Match(
	int				 rot,
	int				 match_thresh,
	int				*score_ptr ) const
{
	// Get the initial score from the state
	int	local_score;
	if ( NULL == score_ptr ) {
		score_ptr = &local_score;
	}
	*score_ptr = m_state->ScoreFile( rot );

	// Generate the final score using the internal logic
	return MatchInternal( rot, NULL, match_thresh, score_ptr );
}

// Compare the "current" file to the cached info
ReadUserLogMatch::MatchResult
ReadUserLogMatch::Match(
	const char		*path,
	int				 rot,
	int				 match_thresh,
	int				*score_ptr ) const
{
	// Get the initial score from the state
	int	local_score;
	if ( NULL == score_ptr ) {
		score_ptr = &local_score;
	}
	*score_ptr = m_state->ScoreFile( path, rot );

	// Generate the final score using the internal logic
	return MatchInternal( rot, path, match_thresh, score_ptr );
}

// Compare the stat info passed in to the cached info
ReadUserLogMatch::MatchResult
ReadUserLogMatch::Match(
	struct stat	 	&statbuf,
	int				 rot,
	int				 match_thresh,
	int				*score_ptr ) const
{
	// Get the initial score from the state
	int	local_score;
	if ( NULL == score_ptr ) {
		score_ptr = &local_score;
	}
	*score_ptr = m_state->ScoreFile( statbuf, rot );

	// Generate the final score using the internal logic
	return MatchInternal( rot, NULL, match_thresh, score_ptr );
}

// Score analysis
ReadUserLogMatch::MatchResult
ReadUserLogMatch::MatchInternal(
	int				 rot,
	const char		*path,
	int				 match_thresh,
	const int				*score_ptr ) const
{
	int		score = *score_ptr;

	// If no path provided, generate one
	std::string path_str;
	if ( NULL == path ) {
		m_state->GeneratePath( rot, path_str );
	} else {
		path_str = path;
	}
	dprintf( D_FULLDEBUG, "Match: score of '%s' = %d\n",
			 path_str.c_str(), score );

	// Quick look at the score passed in from the state comparison
	// We can return immediately in some cases

	MatchResult result = EvalScore( match_thresh, score );
	if ( UNKNOWN != result ) {
		return result;
	}


	// Here, we have an indeterminate result
	// Read the file's header info

	// We'll instantiate a new log reader to do this for us
	// Note: we disable rotation for this one, so we won't recurse infinitely
	ReadUserLog			 log_reader;
	dprintf( D_FULLDEBUG, "Match: reading file %s\n", path_str.c_str() );

	// Initialize the reader
	if ( !log_reader.initialize( path_str.c_str(), false, false ) ) {
		return MATCH_ERROR;
	}

	// Read the file's header info
	ReadUserLogHeader	header_reader;
	int status = header_reader.Read( log_reader );
	if ( ULOG_OK == status ) {
		// Do nothing
	}
	else if ( ULOG_NO_EVENT == status ) {
		return EvalScore( match_thresh, score );
	}
	else {
		return MATCH_ERROR;
	}

	// Finally, extract the ID & store it
	int	id_result = m_state->CompareUniqId( header_reader.getId() );
	const char *result_str = "unknown";
	if ( id_result > 0 ) {
		score += SCORE_FACTOR_UNIQ_MATCH;
		result_str = "match";
	}
	else if ( id_result < 0 ) {
		score = 0;
		result_str = "no match";
	}
	dprintf( D_FULLDEBUG, "Read ID from '%s' as '%s': %d (%s)\n",
			 path_str.c_str(), header_reader.getId().c_str(),
			 id_result, result_str );

	// And, last but not least, re-evaluate the score
	dprintf( D_FULLDEBUG, "Match: Final score is %d\n", score );
	return EvalScore( match_thresh, score );
}

ReadUserLogMatch::MatchResult
ReadUserLogMatch::EvalScore(
	int				match_thresh,
	int				score ) const
{

	// < 0 is an error
	if ( score < 0 ) {
		return MATCH_ERROR;
	}
	// Less than the min threshold - give up, declare "no match"
	else if ( score < SCORE_MIN_MATCH ) {
		return NOMATCH;
	}
	// Or, if it's above the min match threshold,
	else if ( score >= match_thresh ) {
		return MATCH;
	}
	else {
		return UNKNOWN;
	}
}

const char *
ReadUserLogMatch::MatchStr( ReadUserLogMatch::MatchResult value ) const
{
	switch( value ) {
	case MATCH_ERROR:
		return "ERROR";
	case MATCH:
		return "MATCH";
	case UNKNOWN:
		return "UNKNOWN";
	case NOMATCH:
		return "NOMATCH";
	default:
		return "<invalid>";
	};

}
