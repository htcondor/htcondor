/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "user_log.c++.h"
#include <time.h>
#include "MyString.h"
#include "condor_uid.h"
#include "condor_xml_classads.h"
#include "condor_config.h"
#include "stat_wrapper.h"
#include "../condor_privsep/condor_privsep.h"
#include "read_user_log_state.h"

static const char FileStateSignature[] = "UserLogReader::FileState";

ReadUserLogState::ReadUserLogState(
	const char		*path,
	int				 max_rotations,
	int				 recent_thresh )
{
	Reset( RESET_INIT );
	m_max_rotations = max_rotations;
	m_recent_thresh = recent_thresh;
	if ( path ) {
		m_base_path = path;
	}
	m_initialized = true;
}

ReadUserLogState::ReadUserLogState(
	const ReadUserLog::FileState	&state,
	int								 recent_thresh )
{
	Reset( RESET_INIT );
	m_recent_thresh = recent_thresh;

	// Sets m_initialized and m_init_error as appropriate
	SetState( state );
}
ReadUserLogState::ReadUserLogState( void )
{
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
	}

	// Full reset: Free up memory
	else if ( RESET_FULL == type ) {
		m_base_path = "";
	}

	// Always: Reset everything else
	m_cur_path = "";
	m_cur_rot = -1;
	m_uniq_id = "";

	memset( &m_stat_buf, 0, sizeof(m_stat_buf) );
	m_status_size = -1;

	m_stat_valid = false;
	m_update_time = 0;

	m_offset = 0;
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
ReadUserLogState::Rotation( int rotation, StatStructType &statbuf,
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

	return StatFile( statbuf );
}

bool
ReadUserLogState::GeneratePath( int rotation, MyString &path,
								bool initializing ) const
{
	// If we're not initializing and we're not initialized, something is wrong
	if ( !initializing && !m_initialized ) {
		return -1;
	}

	// Check the rotation parameter
	if (  ( rotation < 0 ) || ( rotation > m_max_rotations )  ) {
		return false;
	}

	// No base path set???  Nothing we can do here.
	if ( !m_base_path.Length() ) {
		path = "";
		return false;
	}

	// For starters, copy the base path
	path = m_base_path;
	if ( rotation ) {
		if ( m_max_rotations > 1 ) {
			path.sprintf_cat( ".%d", rotation );
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
		m_update_time = time( NULL );
		m_stat_valid = true;
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
	StatWrapper	statwrap;
	if ( statwrap.Stat( path, StatWrapper::STATOP_STAT ) ) {
		return statwrap.GetRc( );
	}

	statwrap.GetBuf( statbuf );

	return 0;
}

// Special method to stat the current file from an open FD to it
//  *Assumes* that this FD points to this file!!
int
ReadUserLogState::StatFile( int fd )
{
	StatWrapper	statwrap;
	if ( statwrap.Stat( fd )  ) {
		dprintf( D_FULLDEBUG, "StatFile: errno = %d\n", statwrap.GetErrno() );
		return statwrap.GetRc( );
	}

	statwrap.GetBuf( m_stat_buf );
	m_update_time = time( NULL );
	m_stat_valid = true;

	return 0;
}

int
ReadUserLogState::SecondsSinceUpdate( void ) const
{
	time_t	now = time( NULL );
	return (int) (now - m_update_time);
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

	MyString	path;
	if ( !GeneratePath( rot, path ) ) {
		return -1;
	}
	return ScoreFile( path.GetCStr(), rot );
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

	MyString	MatchList = "";	// For debugging

	// Check inode match
	if ( m_stat_buf.st_ino == statbuf.st_ino ) {
		score += m_score_fact_inode;
		if ( DebugFlags & D_FULLDEBUG ) MatchList += "inode ";
	}

	// Check ctime match
	if ( m_stat_buf.st_ctime == statbuf.st_ctime ) {
		score += m_score_fact_ctime;
		if ( DebugFlags & D_FULLDEBUG ) MatchList += "ctime ";
	}

	// If it's the same size, it's a good sign..
	if ( same_size ) {
		score += m_score_fact_same_size;
		if ( DebugFlags & D_FULLDEBUG ) MatchList += "same-size ";
	}
	// If it's the current file and recently stat()ed, if it's grown
	// we're OK with that, too
	else if ( is_recent && is_current && has_grown ) {
		score += m_score_fact_grown;
		if ( DebugFlags & D_FULLDEBUG ) MatchList += "grown ";
	}

	// If the file has shrunk, that doesn't bode well, though, in *any* case
	if ( m_stat_buf.st_size > statbuf.st_size ) {
		score += m_score_fact_shrunk;
		if ( DebugFlags & D_FULLDEBUG ) MatchList += "shrunk ";
	}

	if ( DebugFlags & D_FULLDEBUG ) {
		dprintf( D_FULLDEBUG, "ScoreFile: match list: %s\n",
				 MatchList.GetCStr() );
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
}

ReadUserLog::FileStatus
ReadUserLogState::CheckFileStatus( int fd )
{
	StatWrapper	sb;

	if ( fd >= 0 ) {
		sb.Stat( fd );
	}

	if ( m_cur_path.Length() && !sb.IsBufValid() ) {
		sb.Stat( m_cur_path.GetCStr() );
	}

	if ( sb.GetRc() ) {
		dprintf( D_FULLDEBUG, "StatFile: errno = %d\n", sb.GetErrno() );
		return ReadUserLog::LOG_STATUS_ERROR;
	}

	filesize_t				size = sb.GetBuf()->st_size;
	ReadUserLog::FileStatus status;

	if ( (m_status_size < 0) || (size > m_status_size) ) {
		status = ReadUserLog::LOG_STATUS_GROWN;
	}
	else if ( size == m_status_size ) {
		status = ReadUserLog::LOG_STATUS_NOCHANGE;
	}
	else {
		status = ReadUserLog::LOG_STATUS_SHRUNK;
	}
	m_status_size = size;
	return status;
}

int
ReadUserLogState::CompareUniqId( const MyString &id ) const
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
ReadUserLogState::InitState( ReadUserLog::FileState &state )
{
	state.buf  = (void *) new ReadUserLogState::FileStatePub;
	state.size = sizeof( ReadUserLogState::FileStatePub );

	ReadUserLogState::FileState	*istate = GetFileState( state );

	memset( istate, 0, sizeof(ReadUserLogState::FileStatePub) );
	istate->m_log_type = LOG_TYPE_UNKNOWN;

	strncpy( istate->m_signature,
			 FileStateSignature,
			 sizeof(istate->m_signature) );
	istate->m_signature[sizeof(istate->m_signature) - 1] = '\0';
	istate->m_version = FILESTATE_VERSION;

	return true;
}

bool
ReadUserLogState::UninitState( ReadUserLog::FileState &state )
{
	ReadUserLogState::FileStatePub	*istate =
		(ReadUserLogState::FileStatePub *) state.buf;
	
	delete istate;
	state.buf = NULL;
	state.size = 0;

	return true;
}

bool
ReadUserLogState::GetState( ReadUserLog::FileState &state ) const
{
	ReadUserLogState::FileState *istate = GetFileState( state );

	// Verify that the signature and version matches
	if ( strcmp( istate->m_signature, FileStateSignature ) ) {
		return false;
	}
	if ( istate->m_version != FILESTATE_VERSION ) {
		return false;
	}

	// The paths shouldn't change... copy them only the first time
	if( !strlen( istate->m_path ) ) {
		int	size = sizeof(istate->m_path) - 1;
		strncpy( istate->m_path, m_base_path.GetCStr(), size );
		istate->m_path[size] = '\0';
	}

	// The signature is set in the InitFileState()
	// method, so we don't need to do it here

	istate->m_rotation = m_cur_rot;

	istate->m_log_type = m_log_type;

	if( m_uniq_id.GetCStr() ) {
		strncpy( istate->m_uniq_id,
				 m_uniq_id.GetCStr(),
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

	istate->m_log_offset.asint   = m_log_offset;
	istate->m_log_position.asint = m_log_position;
	istate->m_log_record.asint   = m_log_record;

	return true;
}

bool
ReadUserLogState::SetState( const ReadUserLog::FileState &state )
{
	const ReadUserLogState::FileState *istate = GetFileStateConst( state );

	// Verify that the signature matches
	if ( strcmp( istate->m_signature, FileStateSignature ) ) {
		m_init_error = true;
		return false;
	}
	if ( istate->m_version != FILESTATE_VERSION ) {
		m_init_error = true;
		return false;
	}

	m_base_path = istate->m_path;

	// Set the rotation & path
	m_max_rotations = istate->m_max_rotations;
	Rotation( istate->m_rotation, false, true );

	m_log_type = istate->m_log_type;
	m_uniq_id = istate->m_uniq_id;
	m_sequence = istate->m_sequence;

	m_stat_buf.st_ino = istate->m_inode;
	m_stat_buf.st_ctime = istate->m_ctime;
	m_stat_buf.st_size = istate->m_size.asint;
	m_stat_valid = true;

	m_offset = istate->m_offset.asint;

	m_log_offset   = istate->m_log_offset.asint;
	m_log_position = istate->m_log_position.asint;
	m_log_record   = istate->m_log_record.asint;

	m_initialized = true;

	MyString	str;
	GetState( str, "Restored reader state" );
	dprintf( D_FULLDEBUG, str.GetCStr() );

	return true;
}

void
ReadUserLogState::GetState( MyString &str, const char *label ) const
{
	str = "";
	if ( NULL != label ) {
		str.sprintf( "%s:\n", label );
	}
	str.sprintf_cat (
		"  BasePath = %s\n"
		"  CurPath = %s\n"
		"  UniqId = %s, seq = %d\n"
		"  rotation = %d; max = %d; offset = %ld; type = %d\n"
		"  inode = %u; ctime = %d; size = %ld\n",
		m_base_path.GetCStr(), m_cur_path.GetCStr(),
		m_uniq_id.GetCStr() ? m_uniq_id.GetCStr() : "", m_sequence,
		m_cur_rot, m_max_rotations, (long) m_offset, m_log_type,
		(unsigned)m_stat_buf.st_ino, (int)m_stat_buf.st_ctime,
		(long)m_stat_buf.st_size );
}

const ReadUserLogState::FileState *
ReadUserLogState::GetFileStateConst( const ReadUserLog::FileState &state )
{
	const ReadUserLogState::FileStatePub *buf =
		(const ReadUserLogState::FileStatePub *) state.buf;
	return &buf->actual_state;
}
ReadUserLogState::FileState *
ReadUserLogState::GetFileState( ReadUserLog::FileState &state )
{
	ReadUserLogState::FileStatePub *buf =
		(ReadUserLogState::FileStatePub *) state.buf;
	return &buf->actual_state;
}

void
ReadUserLogState::GetState(
	const ReadUserLog::FileState	&state,
	MyString						&str,
	const char						*label
  ) const
{
	const ReadUserLogState::FileState *istate = GetFileStateConst( state );
	if ( ( !istate ) || ( !istate->m_version ) ) {
		if ( label ) {
			str.sprintf( "%s: no state", label );
		}
		else {
			str = "no state\n";
		}
		return;
	}

	str = "";
	if ( NULL != label ) {
		str.sprintf( "%s:\n", label );
	}
	str.sprintf_cat (
		"  signature = '%s'; version = %d; update = %ld\n"
		"  path = '%s'\n"
		"  UniqId = %s, seq = %d\n"
		"  rotation = %d; max = %d; offset = "FILESIZE_T_FORMAT"; type = %d\n"
		"  inode = %u; ctime = %ld; size = "FILESIZE_T_FORMAT"\n",
		istate->m_signature, istate->m_version, istate->m_update_time,
		istate->m_path,
		istate->m_uniq_id, istate->m_sequence,
		istate->m_rotation, istate->m_max_rotations,
		istate->m_offset.asint, istate->m_log_type,
		(unsigned)istate->m_inode, istate->m_ctime, istate->m_size.asint );
}
