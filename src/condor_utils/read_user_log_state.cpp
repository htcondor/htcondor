/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_uid.h"
#include "condor_config.h"
#include "read_user_log_state.h"

static const char FileStateSignature[] = "UserLogReader::FileState";


// ******************************
// ReadUserLogFileState methods
// ******************************

// C-tors and D-tors
ReadUserLogFileState::ReadUserLogFileState( void )
{
	m_rw_state = NULL;
	m_ro_state = NULL;
}

ReadUserLogFileState::ReadUserLogFileState(
	ReadUserLog::FileState &state )
{
	convertState( state, m_rw_state );
	m_ro_state = m_rw_state;
}

ReadUserLogFileState::ReadUserLogFileState(
	const ReadUserLog::FileState &state )
{
	m_rw_state = NULL;
	convertState( state, m_ro_state );
}

ReadUserLogFileState::~ReadUserLogFileState( void )
{
}

// Is the state initialized?
bool
ReadUserLogFileState::isInitialized( void ) const
{
	if ( NULL == m_ro_state ) {
		return false;
	}
	if ( strcmp( m_ro_state->internal.m_signature, FileStateSignature ) ) {
		return false;
	}
	return true;
}

// Is the state valid for use?
bool
ReadUserLogFileState::isValid( void ) const
{
	if ( !isInitialized() ) {
		return false;
	}
	if ( 0 == strlen(m_ro_state->internal.m_base_path) ) {
		return false;
	}
	return true;
}

bool
ReadUserLogFileState::getFileOffset( int64_t &pos ) const
{
	if ( NULL == m_ro_state ) {
		return false;
	}
	pos = m_ro_state->internal.m_offset.asint;
	return true;
}

bool
ReadUserLogFileState::getFileEventNum( int64_t &num ) const
{
	if ( NULL == m_ro_state ) {
		return false;
	}
	num = m_ro_state->internal.m_event_num.asint;
	return true;
}

bool
ReadUserLogFileState::getLogPosition( int64_t &pos ) const
{
	if ( NULL == m_ro_state ) {
		return false;
	}
	pos = m_ro_state->internal.m_log_position.asint;
	return true;
}

bool
ReadUserLogFileState::getLogRecordNo( int64_t &recno ) const
{
	if ( NULL == m_ro_state ) {
		return false;
	}
	recno = m_ro_state->internal.m_log_record.asint;
	return true;
}

bool
ReadUserLogFileState::getSequenceNo( int &seqno ) const
{
	if ( NULL == m_ro_state ) {
		return false;
	}
	seqno = m_ro_state->internal.m_sequence;
	return true;
}

bool
ReadUserLogFileState::getUniqId( char *buf, int len ) const
{
	if ( NULL == m_ro_state ) {
		return false;
	}
	strncpy( buf, m_ro_state->internal.m_uniq_id, len );
	buf[len-1] = '\0';
	return true;
}

// Static
bool
ReadUserLogFileState::InitState( ReadUserLog::FileState &state )
{
	state.buf  = (void *) new ReadUserLogState::FileStatePub;
	state.size = sizeof( ReadUserLogState::FileStatePub );

	ReadUserLogFileState::FileState	*istate;
	if ( !convertState(state, istate)  ) {
		return false;
	}

	memset( istate, 0, sizeof(ReadUserLogState::FileStatePub) );
	istate->m_log_type = LOG_TYPE_UNKNOWN;

	strncpy( istate->m_signature,
			 FileStateSignature,
			 sizeof(istate->m_signature) );
	istate->m_signature[sizeof(istate->m_signature) - 1] = '\0';
	istate->m_version = FILESTATE_VERSION;

	return true;
}

// Static
bool
ReadUserLogFileState::UninitState( ReadUserLog::FileState &state )
{
	ReadUserLogState::FileStatePub	*istate =
		(ReadUserLogState::FileStatePub *) state.buf;
	
	delete istate;
	state.buf = NULL;
	state.size = 0;

	return true;
}

// Static
bool
ReadUserLogFileState::convertState(
	const ReadUserLog::FileState				&state,
	const ReadUserLogFileState::FileStatePub	*&pub )
{
	pub = (const ReadUserLogFileState::FileStatePub *) state.buf;
	return true;
}
// Static
bool
ReadUserLogFileState::convertState(
	ReadUserLog::FileState				&state,
	ReadUserLogFileState::FileStatePub	*&pub )
{
	pub = (ReadUserLogFileState::FileStatePub *) state.buf;
	return true;
}
// Static
bool
ReadUserLogFileState::convertState(
	const ReadUserLog::FileState			&state,
	const ReadUserLogFileState::FileState	*&internal )
{
	const ReadUserLogFileState::FileStatePub	*pub;
	convertState(state, pub);
	internal = &(pub->internal);
	return true;
}
// Static
bool
ReadUserLogFileState::convertState(
	ReadUserLog::FileState			&state,
	ReadUserLogFileState::FileState	*&internal )
{
	ReadUserLogFileState::FileStatePub	*pub;
	convertState(state, pub);
	internal = &(pub->internal);
	return true;
}


// ******************************
// ReadUserLogState methods
// ******************************
ReadUserLogState::ReadUserLogState(
	const char		*path,
	int				 max_rotations,
	int				 recent_thresh )
	: ReadUserLogFileState( )
{
	Reset( RESET_INIT );
	m_max_rotations = max_rotations;
	m_recent_thresh = recent_thresh;
	if ( path ) {
		m_base_path = path;
	}
	m_initialized = true;
	m_update_time = 0;
}

ReadUserLogState::ReadUserLogState(
	const ReadUserLog::FileState	&state,
	int								 recent_thresh )
	: ReadUserLogFileState( state )
{
	Reset( RESET_INIT );
	m_recent_thresh = recent_thresh;

	// Sets m_initialized and m_init_error as appropriate
	if ( !SetState( state ) ) {
		dprintf( D_FULLDEBUG, "::ReadUserLogState: failed to set state from buffer\n" );
		m_init_error = true;
	}
}
ReadUserLogState::ReadUserLogState( void )
	: ReadUserLogFileState( )
{
	m_update_time = 0;
	Reset( RESET_INIT );
}


ReadUserLogState::~ReadUserLogState( void )
{
	Reset( RESET_FULL );
}

void
ReadUserLogState::Reset( ResetType type )
{
	// Initial reset: Set pointers to NULL
	if ( RESET_INIT == type ) {
		m_initialized = false;
		m_init_error = false;
		m_base_path = "";
		m_max_rotations = 0;

		m_recent_thresh = 0;
		m_score_fact_ctime = 0;
		m_score_fact_inode = 0;
		m_score_fact_same_size = 0;
		m_score_fact_grown = 0;
		m_score_fact_shrunk = 0;
	}

	// Full reset: Free up memory
	else if ( RESET_FULL == type ) {
		m_base_path = "";
	}

	// Always: Reset everything else
	m_cur_path = "";
	m_cur_rot = -1;
	m_uniq_id = "";
	m_sequence = 0;

	memset( &m_stat_buf, 0, sizeof(m_stat_buf) );
	m_status_size = -1;

	m_stat_valid = false;
	m_stat_time = 0;

	m_log_position = 0;
	m_log_record = 0;

	m_offset = 0;
	m_event_num = 0;

	m_log_type = LOG_TYPE_UNKNOWN;

}

int
ReadUserLogState::Rotation( int rotation, bool store_stat, bool initializing )
{
	// If we're not initializing and we're not initialized, something is wrong
	if ( !initializing && !m_initialized ) {
		return -1;
	}
	if ( rotation > m_max_rotations ) {
		return -1;
	}
	if ( store_stat ) {
		Reset( );
		int status = Rotation( rotation, m_stat_buf );
		if ( 0 == status ) {
			m_stat_valid = true;
		}
		return status;
	}
	else {
		StatStructType	statbuf;
		return Rotation( rotation, statbuf, initializing );
	}
}

int
ReadUserLogState::Rotation( int rotation,
							StatStructType &statbuf,
							bool initializing )
{
	// If we're not initializing and we're not initialized, something is wrong
	if ( !initializing && !m_initialized ) {
		return -1;
	}

	// Check the rotation parameter
	if (  ( rotation < 0 ) || ( rotation > m_max_rotations )  ) {
		return -1;
	}

	// No change?  We're done
	if ( m_cur_rot == rotation ) {
		return 0;
	}

	m_uniq_id = "";
	GeneratePath( rotation, m_cur_path, initializing );
	m_cur_rot = rotation;
	m_log_type = LOG_TYPE_UNKNOWN;
	Update();

	return StatFile( statbuf );
}

bool
ReadUserLogState::GeneratePath( int rotation,
								std::string &path,
								bool initializing ) const
{
	// If we're not initializing and we're not initialized, something is wrong
	if ( !initializing && !m_initialized ) {
		return false;
	}

	// Check the rotation parameter
	if (  ( rotation < 0 ) || ( rotation > m_max_rotations )  ) {
		return false;
	}

	// No base path set???  Nothing we can do here.
	if ( !m_base_path.length() ) {
		path = "";
		return false;
	}

	// For starters, copy the base path
	path = m_base_path;
	if ( rotation ) {
		if ( m_max_rotations > 1 ) {
			formatstr_cat(path, ".%d", rotation );
		}
		else {
			path += ".old";
		}
	}

	return true;
}

int
ReadUserLogState::StatFile( void )
{
	int status = StatFile( CurPath(), m_stat_buf );
	if ( 0 == status ) {
		m_stat_time = time( NULL );
		m_stat_valid = true;
		Update();
	}
	return status;
}

int
ReadUserLogState::StatFile( StatStructType &statbuf ) const
{
	return StatFile( CurPath(), statbuf );
}

int
ReadUserLogState::StatFile( const char *path, StatStructType &statbuf ) const
{
	return stat(path, &statbuf);
}

// Special method to stat the current file from an open FD to it
//  *Assumes* that this FD points to this file!!
int
ReadUserLogState::StatFile( int fd )
{
	int rc = fstat(fd, &m_stat_buf);
	if (rc == 0) {
		m_stat_time = time( NULL );
		m_stat_valid = true;
		Update();
	}
	return rc;
}

int
ReadUserLogState::SecondsSinceStat( void ) const
{
	time_t	now = time( NULL );
	return (int) (now - m_stat_time);
}

// Score a file based on it's rotation #
int
ReadUserLogState::ScoreFile( int rot ) const
{
	if ( rot > m_max_rotations ) {
		return -1;
	}
	else if ( rot < 0 ) {
		rot = m_cur_rot;
	}

	std::string	path;
	if ( !GeneratePath( rot, path ) ) {
		return -1;
	}
	return ScoreFile( path.c_str(), rot );
}

int
ReadUserLogState::ScoreFile( const char *path, int rot ) const
{
	StatStructType	statbuf;

	if ( NULL == path ) {
		path = CurPath( );
	}
	if ( rot < 0 ) {
		rot = m_cur_rot;
	}
	if ( StatFile( path, statbuf ) ) {
		dprintf( D_FULLDEBUG, "ScoreFile: stat Error\n" );
		return -1;
	}

	return ScoreFile( statbuf, rot );
}

int
ReadUserLogState::ScoreFile( const StatStructType &statbuf, int rot ) const
{
	int		score = 0;

	if ( rot < 0 ) {
		rot = m_cur_rot;
	}

	bool	is_recent = ( time(NULL) < (m_update_time + m_recent_thresh) );
	bool	is_current = ( rot == m_cur_rot );
	bool	same_size = ( statbuf.st_size == m_stat_buf.st_size );
	bool 	has_grown = ( statbuf.st_size > m_stat_buf.st_size );

	std::string	MatchList = "";	// For debugging

	// Check inode match
	if ( m_stat_buf.st_ino == statbuf.st_ino ) {
		score += m_score_fact_inode;
		if ( IsFulldebug(D_FULLDEBUG) ) MatchList += "inode ";
	}

	// Check ctime match
	if ( m_stat_buf.st_ctime == statbuf.st_ctime ) {
		score += m_score_fact_ctime;
		if ( IsFulldebug(D_FULLDEBUG) ) MatchList += "ctime ";
	}

	// If it's the same size, it's a good sign..
	if ( same_size ) {
		score += m_score_fact_same_size;
		if ( IsFulldebug(D_FULLDEBUG) ) MatchList += "same-size ";
	}
	// If it's the current file and recently stat()ed, if it's grown
	// we're OK with that, too
	else if ( is_recent && is_current && has_grown ) {
		score += m_score_fact_grown;
		if ( IsFulldebug(D_FULLDEBUG) ) MatchList += "grown ";
	}

	// If the file has shrunk, that doesn't bode well, though, in *any* case
	if ( m_stat_buf.st_size > statbuf.st_size ) {
		score += m_score_fact_shrunk;
		if ( IsFulldebug(D_FULLDEBUG) ) MatchList += "shrunk ";
	}

	if ( IsFulldebug(D_FULLDEBUG) ) {
		dprintf( D_FULLDEBUG, "ScoreFile: match list: %s\n",
				 MatchList.c_str() );
	}

	// Min score is zero
	if ( score < 0 ) {
		score = 0;
	}

	return score;
}

void
ReadUserLogState::SetScoreFactor( enum ScoreFactors which, int factor )
{
	switch ( which )
	{
	case SCORE_CTIME:
		m_score_fact_ctime = factor;
		break;
	case SCORE_INODE:
		m_score_fact_inode = factor;
		break;
	case SCORE_SAME_SIZE:
		m_score_fact_same_size = factor;
		break;
	case SCORE_GROWN:
		m_score_fact_grown = factor;
		break;
	case SCORE_SHRUNK:
		m_score_fact_shrunk = factor;
		break;
	default:
		// Ignore
		break;
	}
	Update();
}

ReadUserLog::FileStatus
ReadUserLogState::CheckFileStatus( int fd, bool &is_empty )
{
	struct stat sb;
	int rc = 0;

	if (fd < 0 && m_cur_path.empty()) {
		dprintf(D_FULLDEBUG, "StatFile: no file to stat\n");
		return ReadUserLog::LOG_STATUS_ERROR;
	}

	if ( fd >= 0 ) {
		rc = fstat(fd, &sb);
	}

	if ( m_cur_path.length() && rc != 0 ) {
		rc = stat(m_cur_path.c_str(), &sb);
	}

	if ( rc ) {
		dprintf( D_FULLDEBUG, "StatFile: errno = %d\n", errno );
		return ReadUserLog::LOG_STATUS_ERROR;
	}

	filesize_t size = sb.st_size;
	int num_hard_links = sb.st_nlink;
	ReadUserLog::FileStatus status;

	// If there are no hard links to the file, it has been overwritten or 
	// deleted. Send back an error.
	if ( num_hard_links < 1 ) {
		dprintf( D_ALWAYS, "ERROR: log file %s has been deleted. "
			"Aborting.\n", m_cur_path.c_str() );
		return ReadUserLog::LOG_STATUS_ERROR;
	}

	// Special case for zero size file
	if ( 0 == size ) {
		is_empty = true;
		if ( m_status_size < 0 ) {
			m_status_size = 0;
		}
	}
	else {
		is_empty = false;
	}
	if ( (m_status_size < 0) || (size > m_status_size) ) {
		status = ReadUserLog::LOG_STATUS_GROWN;
	}
	else if ( size == m_status_size ) {
		status = ReadUserLog::LOG_STATUS_NOCHANGE;
	}
	else {
		status = ReadUserLog::LOG_STATUS_SHRUNK;
		dprintf( D_ALWAYS, "ERROR: log file %s has shrunk, probably due to being"
			" overwritten. Aborting.\n", m_cur_path.c_str() );
	}
	m_status_size = size;
	Update();
	return status;
}

int
ReadUserLogState::CompareUniqId( const std::string &id ) const
{
	if ( ( m_uniq_id == "" ) || ( id == "" ) ) {
		return 0;
	}
	else if ( m_uniq_id == id ) {
		return 1;
	}
	else {
		return -1;
	}
}

bool
ReadUserLogState::GetState( ReadUserLog::FileState &state ) const
{
	ReadUserLogFileState			 fstate( state );
	ReadUserLogFileState::FileState *istate = fstate.getRwState();
	if ( !istate ) {
		return false;
	}

	// Verify that the signature and version matches
	if ( strcmp( istate->m_signature, FileStateSignature ) ) {
		return false;
	}
	if ( istate->m_version != FILESTATE_VERSION ) {
		return false;
	}

	// The paths shouldn't change... copy them only the first time
	if( !strlen( istate->m_base_path ) ) {
		memset( istate->m_base_path, 0, sizeof(istate->m_base_path) );
		if ( m_base_path.c_str() ) {
			strncpy( istate->m_base_path,
					 m_base_path.c_str(),
					 sizeof(istate->m_base_path) - 1 );
		}
	}

	// The signature is set in the InitFileState()
	// method, so we don't need to do it here

	istate->m_rotation = m_cur_rot;

	istate->m_log_type = m_log_type;

	if( m_uniq_id.c_str() ) {
		strncpy( istate->m_uniq_id,
				 m_uniq_id.c_str(),
				 sizeof(istate->m_uniq_id) );
		istate->m_uniq_id[sizeof(istate->m_uniq_id) - 1] = '\0';
	}
	else {
		memset( istate->m_uniq_id, 0, sizeof(istate->m_uniq_id) );
	}
	istate->m_sequence      = m_sequence;
	istate->m_max_rotations = m_max_rotations;

	istate->m_inode         = m_stat_buf.st_ino;
	istate->m_ctime         = m_stat_buf.st_ctime;
	istate->m_size.asint    = m_stat_buf.st_size;

	istate->m_offset.asint  = m_offset;
	istate->m_event_num.asint = m_event_num;

	istate->m_log_position.asint = m_log_position;
	istate->m_log_record.asint   = m_log_record;

	istate->m_update_time = m_update_time;

	return true;
}

bool
ReadUserLogState::SetState( const ReadUserLog::FileState &state )
{
	const ReadUserLogFileState::FileState *istate;
	if ( !convertState(state, istate) ) {
		return false;
	}

	// Verify that the signature matches
	if ( strcmp( istate->m_signature, FileStateSignature ) ) {
		m_init_error = true;
		return false;
	}
	if ( istate->m_version != FILESTATE_VERSION ) {
		m_init_error = true;
		return false;
	}

	m_base_path = istate->m_base_path;

	// Set the rotation & path
	m_max_rotations = istate->m_max_rotations;
	Rotation( istate->m_rotation, false, true );

	m_log_type = istate->m_log_type;
	m_uniq_id  = istate->m_uniq_id;
	m_sequence = istate->m_sequence;

	m_stat_buf.st_ino   = istate->m_inode;
	m_stat_buf.st_ctime = istate->m_ctime;
	m_stat_buf.st_size  = istate->m_size.asint;
	m_stat_valid = true;

	m_offset = istate->m_offset.asint;
	m_event_num = istate->m_event_num.asint;

	m_log_position = istate->m_log_position.asint;
	m_log_record   = istate->m_log_record.asint;

	m_update_time  = istate->m_update_time;

	m_initialized  = true;

	std::string	str;
	GetStateString( str, "Restored reader state" );
	dprintf( D_FULLDEBUG, "%s", str.c_str() );

	return true;
}

void
ReadUserLogState::GetStateString( std::string &str, const char *label ) const
{
	str = "";
	if ( NULL != label ) {
		formatstr(str, "%s:\n", label );
	}
	formatstr_cat (str,
		"  BasePath = %s\n"
		"  CurPath = %s\n"
		"  UniqId = %s, seq = %d\n"
		"  rotation = %d; max = %d; offset = %ld; event = %ld; type = %d\n"
		"  inode = %u; ctime = %d; size = %ld\n",
		m_base_path.c_str(), m_cur_path.c_str(),
		m_uniq_id.c_str(), m_sequence,
		m_cur_rot, m_max_rotations, (long) m_offset,
		(long) m_event_num, m_log_type,
		(unsigned)m_stat_buf.st_ino, (int)m_stat_buf.st_ctime,
		(long)m_stat_buf.st_size );
}

void
ReadUserLogState::GetStateString(
	const ReadUserLog::FileState	&state,
	std::string						&str,
	const char						*label
  ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		if ( label ) {
			formatstr(str, "%s: no state", label );
		}
		else {
			str = "no state\n";
		}
		return;
	}

	str = "";
	if ( NULL != label ) {
		formatstr(str, "%s:\n", label );
	}
	formatstr_cat (str,
		"  signature = '%s'; version = %d; update = %ld\n"
		"  base path = '%s'\n"
		"  cur path = '%s'\n"
		"  UniqId = %s, seq = %d\n"
		"  rotation = %d; max = %d; offset = " FILESIZE_T_FORMAT";"
		" event num = " FILESIZE_T_FORMAT"; type = %d\n"
		"  inode = %u; ctime = %ld; size = " FILESIZE_T_FORMAT"\n",
		istate->m_signature, istate->m_version, istate->m_update_time,
		istate->m_base_path,
		CurPath(state),
		istate->m_uniq_id, istate->m_sequence,
		istate->m_rotation, istate->m_max_rotations,
		istate->m_offset.asint,
		istate->m_event_num.asint,
		istate->m_log_type,
		(unsigned)istate->m_inode, istate->m_ctime, istate->m_size.asint );
}

// Get state information back from a "file state" structure
const char *
ReadUserLogState::BasePath( const ReadUserLog::FileState &state ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		return NULL;
	}
	return istate->m_base_path;
}

const char *
ReadUserLogState::CurPath( const ReadUserLog::FileState &state ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		return NULL;
	}

	static std::string	path;
	if ( !GeneratePath( istate->m_rotation, path, true ) ) {
		return NULL;
	}
	return path.c_str();
}

int
ReadUserLogState::Rotation( const ReadUserLog::FileState &state ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		return -1;
	}
	return istate->m_rotation;
}
filesize_t
ReadUserLogState::Offset( const ReadUserLog::FileState &state ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		return -1;
	}
	return (filesize_t) istate->m_offset.asint;
}
filesize_t
ReadUserLogState::EventNum( const ReadUserLog::FileState &state ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		return -1;
	}
	return (filesize_t) istate->m_event_num.asint;
}
filesize_t
ReadUserLogState::LogPosition( const ReadUserLog::FileState &state ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		return -1;
	}
	return (filesize_t) istate->m_log_position.asint;
}
filesize_t
ReadUserLogState::LogRecordNo( const ReadUserLog::FileState &state ) const
{
	const ReadUserLogFileState::FileState *istate;
	if ( ( !convertState(state, istate) ) || ( !istate->m_version ) ) {
		return -1;
	}
	return (filesize_t) istate->m_log_record.asint;
}


// **********************************
// ReadUserLogStateAccess methods
// **********************************

// Constructor
ReadUserLogStateAccess::ReadUserLogStateAccess(
	const ReadUserLog::FileState &state)
{
	m_state = new ReadUserLogFileState(state);
}

ReadUserLogStateAccess::~ReadUserLogStateAccess(void)
{
	delete m_state;
}

// Is the state buffer initialized?
bool
ReadUserLogStateAccess::isInitialized( void ) const
{
	return m_state->isInitialized( );
}

// Is the state buffer valid for use?
bool
ReadUserLogStateAccess::isValid( void ) const
{
	return m_state->isValid( );
}

// Log position of a state
bool
ReadUserLogStateAccess::getFileOffset(
	unsigned long		&pos ) const
{
	int64_t	my_pos;
	if ( !m_state->getFileOffset(my_pos) ) {
		return false;
	}

	pos = (unsigned long) my_pos;
	return true;
}

// Event number of a state
bool
ReadUserLogStateAccess::getFileEventNum(
	unsigned long		&num ) const
{
	int64_t	my_num;
	if ( !m_state->getFileEventNum(my_num) ) {
		return false;
	}

	num = (unsigned long) my_num;
	return true;
}

// Event # difference between to states
bool
ReadUserLogStateAccess::getFileEventNumDiff(
	const ReadUserLogStateAccess	&other,
	long							&diff ) const
{
	const	ReadUserLogFileState	*ostate;
	if ( !other.getState( ostate ) ) {
		return false;
	}
	int64_t	my_num, other_num;
	if ( !m_state->getFileEventNum(my_num) ||
		 ! ostate->getFileEventNum(other_num) ) {
		return false;
	}

	int64_t	idiff = my_num - other_num;
	diff = (long) idiff;
	return true;
}

// Positional difference between to states
bool
ReadUserLogStateAccess::getFileOffsetDiff(
	const ReadUserLogStateAccess	&other,
	long							&diff ) const
{
	const	ReadUserLogFileState	*ostate;
	if ( !other.getState( ostate ) ) {
		return false;
	}
	int64_t	my_pos, other_pos;
	if ( !m_state->getFileOffset(my_pos) ||
		 ! ostate->getFileOffset(other_pos) ) {
		return false;
	}

	int64_t	idiff = my_pos - other_pos;
	diff = (long) idiff;
	return true;
}

// Log position of a state
bool
ReadUserLogStateAccess::getLogPosition(
	unsigned long		&pos ) const
{
	int64_t	my_pos;
	if ( !m_state->getLogPosition(my_pos) ) {
		return false;
	}

	pos = (unsigned long) my_pos;
	return true;
}

// Positional difference between to states
bool
ReadUserLogStateAccess::getLogPositionDiff(
	const ReadUserLogStateAccess	&other,
	long							&diff ) const
{
	const	ReadUserLogFileState	*ostate;
	if ( !other.getState( ostate ) ) {
		return false;
	}
	int64_t	my_pos, other_pos;
	if ( !m_state->getLogPosition(my_pos) ||
		 ! ostate->getLogPosition(other_pos) ) {
		return false;
	}

	int64_t	idiff = my_pos - other_pos;
	diff = (long) idiff;
	return true;
}

// Event number of a state
bool
ReadUserLogStateAccess::getEventNumber(
	unsigned long		&event_no ) const
{
	int64_t	my_event_no;
	if ( !m_state->getLogRecordNo(my_event_no) ) {
		return false;
	}

	event_no = (unsigned long) my_event_no;
	return true;
}

// # of events between to states
bool
ReadUserLogStateAccess::getEventNumberDiff(
	const ReadUserLogStateAccess	&other,
	long							&diff) const
{
	const	ReadUserLogFileState	*ostate;
	if ( !other.getState( ostate ) ) {
		return false;
	}
	int64_t	my_recno, other_recno;
	if ( !m_state->getLogRecordNo(my_recno) ||
		 ! ostate->getLogRecordNo(other_recno) ) {
		return false;
	}

	int64_t	idiff = my_recno - other_recno;
	diff = (long) idiff;
	return true;
}

// Get the unique ID and sequence # of the associated state file
bool
ReadUserLogStateAccess::getUniqId( char *buf, int len ) const
{
	return m_state->getUniqId( buf, len );
}

bool
ReadUserLogStateAccess::getSequenceNumber( int &seqno ) const
{
	return m_state->getSequenceNo( seqno );
}

bool
ReadUserLogStateAccess::getState( const ReadUserLogFileState *&state ) const
{
	state = m_state;
	return true;
}
