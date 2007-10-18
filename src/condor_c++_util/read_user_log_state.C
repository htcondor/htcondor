/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


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
	int				 max_rot,
	int				 recent_thresh )
{
	Reset( RESET_INIT );
	m_max_rot = max_rot;
	m_recent_thresh = recent_thresh;
	if ( path ) {
		int		len = strlen( path );

		m_base_path = new char[len+1];
		strcpy(m_base_path, path );
	}
	else {
		m_base_path = NULL;
	}
}

ReadUserLogState::ReadUserLogState(
	const ReadUserLog::FileState	&state,
	int								 max_rot,
	int								 recent_thresh )
{
	Reset( RESET_INIT );
	m_max_rot = max_rot;
	m_recent_thresh = recent_thresh;
	SetState( state );
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
		m_base_path = NULL;
	}

	// Full reset: Free up memory
	else if ( RESET_FULL == type ) {
		if ( m_base_path ) {
			delete [] m_base_path;
			m_base_path = NULL;
		}
	}

	// Always: Reset everything else
	m_cur_path = "";
	m_cur_rot = -1;
	m_uniq_id = "";

	memset( &m_stat_buf, 0, sizeof(m_stat_buf) );
	m_stat_valid = false;
	m_update_time = 0;

	m_offset = 0;
	m_log_type = LOG_TYPE_UNKNOWN;
}

int
ReadUserLogState::Rotation( int rotation, bool store_stat )
{
	if ( rotation > m_max_rot ) {
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
		return Rotation( rotation, statbuf );
	}
}

int
ReadUserLogState::Rotation( int rotation, StatStructType &statbuf ) 
{
	// Check the rotation parameter
	if (  ( rotation < 0 ) || ( rotation > m_max_rot )  ) {
		return -1;
	}

	// No change?  We're done
	if ( m_cur_rot == rotation ) {
		return 0;
	}

	m_uniq_id = "";
	GeneratePath( rotation, m_cur_path );
	m_cur_rot = rotation;
	m_log_type = LOG_TYPE_UNKNOWN;

	return StatFile( statbuf );
}

bool
ReadUserLogState::GeneratePath( int rotation, MyString &path ) const
{
	// Check the rotation parameter
	if (  ( rotation < 0 ) || ( rotation > m_max_rot )  ) {
		return false;
	}

	// No base path set???  Nothing we can do here.
	if ( ! m_base_path ) {
		path = "";
		return false;
	}

	// For starters, copy the base path
	path = m_base_path;
	if ( rotation ) {
		path += ".old";
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
	StatWrapper	statwrap( path );
	if ( statwrap.GetStatus( )  ) {
		return statwrap.GetStatus( );
	}

	statwrap.GetStatBuf( statbuf );

	return 0;
}

// Special method to stat the current file from an open FD to it
//  *Assumes* that this FD points to this file!!
int
ReadUserLogState::StatFile( int fd )
{
	StatWrapper	statwrap( fd );
	if ( statwrap.GetStatus( )  ) {
		dprintf( D_FULLDEBUG, "StatFile: errno = %d\n", statwrap.GetErrno() );
		return statwrap.GetStatus( );
	}

	statwrap.GetStatBuf( m_stat_buf );
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
	if ( rot > m_max_rot ) {
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
	ReadUserLogState::FileState	*istate = new ReadUserLogState::FileState;

	memset( istate, 0, sizeof( FileState ) );
	strncpy( istate->signature,
			 FileStateSignature,
			 sizeof(istate->signature) );
	istate->signature[sizeof(istate->signature) - 1] = '\0';

	state.buf = (void *) istate;
	state.size = sizeof( FileState );
	return true;
}

bool
ReadUserLogState::UninitState( ReadUserLog::FileState &state ) const
{
	ReadUserLogState::FileState	*istate =
		(ReadUserLogState::FileState *) state.buf;
	
	delete istate;
	state.buf = NULL;
	state.size = 0;

	return true;
}

bool
ReadUserLogState::GetState( ReadUserLog::FileState &state ) const
{
	ReadUserLogState::FileState	*istate =
		(ReadUserLogState::FileState *) state.buf;

	// Verify that the signature matches
	if ( strcmp( istate->signature, FileStateSignature ) ) {
		return false;
	}

	// The paths shouldn't change... copy them only the first time
	if( ! istate->path[0] ) {
		strncpy( istate->path, m_base_path, _POSIX_PATH_MAX );
		istate->path[_POSIX_PATH_MAX-1] = '\0';
	}

	// The signature is set in the InitFileState()
	// method, so we don't need to do it here

	istate->rotation = m_cur_rot;

	istate->log_type = m_log_type;

	if( m_uniq_id.GetCStr() ) {
		strncpy( istate->uniq_id,
				 m_uniq_id.GetCStr(),
				 sizeof(istate->uniq_id) );
		istate->uniq_id[sizeof(istate->uniq_id) - 1] = '\0';
	}
	else {
		memset( istate->uniq_id, 0, sizeof(istate->uniq_id) );
	}
	istate->sequence = m_sequence;

	istate->inode = m_stat_buf.st_ino;
	istate->ctime = m_stat_buf.st_ctime;
	istate->size.asint = m_stat_buf.st_size;

	istate->offset.asint = m_offset;

	return true;
}

bool
ReadUserLogState::SetState( const ReadUserLog::FileState &state )
{
	ReadUserLogState::FileState	*istate =
		(ReadUserLogState::FileState *) state.buf;

	// Verify that the signature matches
	if ( strcmp( istate->signature, FileStateSignature ) ) {
		return false;
	}

	int		len;

	len = strlen( istate->path );
	m_base_path = new char[ len+1 ];
	strcpy( m_base_path, istate->path );

	// Set the rotation & path
	Rotation( istate->rotation, false );

	m_log_type = istate->log_type;
	m_uniq_id = istate->uniq_id;
	m_sequence = istate->sequence;

	m_stat_buf.st_ino = istate->inode;
	m_stat_buf.st_ctime = istate->ctime;
	m_stat_buf.st_size = istate->size.asint;
	m_stat_valid = true;

	m_offset = istate->offset.asint;

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
		"  rotation = %d; offset = %ld; type = %d\n"
		"  inode = %d; ctime = %d; size = %ld\n",
		m_base_path, m_cur_path.GetCStr(),
		m_uniq_id.GetCStr() ? m_uniq_id.GetCStr() : "", m_sequence,
		m_cur_rot, (long) m_offset, m_log_type,
		(int)m_stat_buf.st_ino, (int)m_stat_buf.st_ctime,
		(long)m_stat_buf.st_size );
}
