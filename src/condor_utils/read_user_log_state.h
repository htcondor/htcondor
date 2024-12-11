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


#ifndef _READ_USER_LOG_STATE_H
#define _READ_USER_LOG_STATE_H

#include "condor_common.h"
#include <time.h>
#include "condor_event.h" // for UserLogType


	// ********************************************************************
	// This is the file state buffer that we generate for the init/get/set
	// methods
	// ********************************************************************
#define FILESTATE_VERSION		104

class ReadUserLogFileState
{
  public:

	// Make things 8 bytes
	union FileStateI64_t {
		char		bytes[8];
		int64_t		asint;
	};

	// The structure itself
	struct FileState {
		char			m_signature[64];	// File state signature
		int				m_version;			// Version #
		char			m_base_path[512];	// The log's base path
		char			m_uniq_id[128];		// File's uniq identifier
		int				m_sequence;			// File's sequence number
		int				m_rotation;			// 0 == the "current" file
		int				m_max_rotations;	// Max rotation level
		UserLogType		m_log_type;			// The log's type
		ino_t			m_inode;			// The log's inode #
		time_t			m_ctime;			// The log's creation time
		FileStateI64_t	m_size;				// The log's size (bytes)
		FileStateI64_t	m_offset;			// Current offset in current file
		FileStateI64_t	m_event_num;		// Current event # in the cur. file
		FileStateI64_t	m_log_position;		// Our position in the whole log
		FileStateI64_t	m_log_record;		// Cur record # in the whole log
		time_t			m_update_time;		// Time of last struct update
	};


	// "Public" view of the file state
	typedef union {
		FileState	internal;
		char		filler [2048];
	} FileStatePub;

	ReadUserLogFileState( void );
	ReadUserLogFileState( ReadUserLog::FileState &state );
	ReadUserLogFileState( const ReadUserLog::FileState &state );
	virtual ~ReadUserLogFileState( void );

	// Is the state buffer initialized?
	bool isInitialized( void ) const;

	// Is the state buffer valid for use?
	bool isValid( void ) const;

	static bool InitState( ReadUserLog::FileState &state );
	static bool UninitState( ReadUserLog::FileState &state );

	// Convert an application file state to an "internal" state pointer
	static bool convertState(
		const ReadUserLog::FileState				&state,
		const ReadUserLogFileState::FileStatePub	*&pub
		);
	static bool convertState(
		ReadUserLog::FileState						&state,
		ReadUserLogFileState::FileStatePub			*&pub
		);
	static bool convertState(
		const ReadUserLog::FileState				&state,
		const ReadUserLogFileState::FileState		*&internal
		);
	static bool convertState(
		ReadUserLog::FileState						&state,
		ReadUserLogFileState::FileState				*&internal
		);

	// Get the size of the internal data structure
	int getSize( void ) const
		{ return sizeof(FileState); };

	// Get the state (in various forms)
	const FileState *getState( void ) {
		if (!m_ro_state) return NULL;
		return &(m_ro_state->internal);
	};
	const FileStatePub *getPubState( void ) {
		return m_ro_state;
	};
	FileState *getRwState( void ) {
		if (!m_rw_state) return NULL; 
		return &(m_rw_state->internal);
	};
	FileStatePub *getPubRwState( void )
		{ return m_rw_state; };

	// General accessors
	bool getFileOffset( int64_t & ) const;
	bool getFileEventNum( int64_t & ) const;
	bool getLogPosition( int64_t & ) const;
	bool getLogRecordNo( int64_t & ) const;
	bool getSequenceNo( int & ) const;
	bool getUniqId( char *buf, int len ) const;

  private:
	FileStatePub		*m_rw_state;
	const FileStatePub	*m_ro_state;
};


// Internal verion of the file state
class ReadUserLogState : public ReadUserLogFileState
{
public:
	// Reset type
	enum ResetType {
		RESET_FILE, RESET_FULL, RESET_INIT,
	};

	ReadUserLogState( void );
	ReadUserLogState( const char *path,
					  int max_rotations,
					  int recent_thresh );
	ReadUserLogState( const ReadUserLog::FileState &state,
					  int recent_thresh );
	~ReadUserLogState( void );

	// Reset parameters about the current file (offset, stat info, etc.)
	void Reset( ResetType type = RESET_FILE );

	// Initialized?
	bool Initialized( void ) const { return m_initialized; };
	bool InitializeError( void ) const { return m_init_error; };

	// Accessors
	const char *BasePath( void ) const { return m_base_path.c_str(); };
	const char *CurPath( void ) const { return m_cur_path.c_str( ); };
	int Rotation( void ) const { return m_cur_rot; };
	filesize_t Offset( void ) const { return m_offset; };
	filesize_t EventNum( void ) const { return m_event_num; };
	bool IsValid( void ) const { return (m_cur_rot >= 0); };

	// Accessors for a "file state"
	const char *BasePath( const ReadUserLog::FileState &state ) const;
	const char *CurPath( const ReadUserLog::FileState &state ) const;
	int Rotation( const ReadUserLog::FileState &state ) const;
	filesize_t LogPosition( const ReadUserLog::FileState &state ) const;
	filesize_t LogRecordNo( const ReadUserLog::FileState &state ) const;
	filesize_t Offset( const ReadUserLog::FileState &state ) const;
	filesize_t EventNum( const ReadUserLog::FileState &state ) const;

	// Get/set maximum rotations
	int MaxRotations( void ) const { return m_max_rotations; }
	int MaxRotations( int max_rotations )
		{ Update(); return m_max_rotations = max_rotations; }

	// Set methods
	int Rotation( int rotation, bool store_stat = false,
				  bool initializing = false );
	int Rotation( int rotation, struct stat &statbuf,
				  bool initializing = false );
	filesize_t Offset( filesize_t offset )
		{ Update(); return m_offset = offset; };
	filesize_t EventNum( filesize_t num )
		{ Update(); return m_event_num = num; };
	filesize_t EventNumInc( int num = 1 )
		{ Update(); m_event_num += num; return m_event_num; };

	// Get / set the uniq identifier
	void UniqId( const std::string &id ) { Update(); m_uniq_id = id; };
	const char *UniqId( void ) const { return m_uniq_id.c_str(); };
	bool ValidUniqId( void ) const { return ( m_uniq_id.length() != 0 ); };

	// Get / set the sequence number
	void Sequence( int seq ) { m_sequence = seq; };
	int Sequence( void ) const { return m_sequence; };

	// Get/set global fields
	filesize_t LogPosition( void ) const { return m_log_position; };
	filesize_t LogPosition( filesize_t pos )
		{ Update(); return m_log_position = pos; };
	filesize_t LogPositionAdd( filesize_t pos )
		{ Update(); return m_log_position += pos; };

	filesize_t LogRecordNo( void ) const { return m_log_record; };
	filesize_t LogRecordNo( filesize_t num )
		{ Update(); return m_log_record = num; };
	filesize_t LogRecordInc( int num = 1 )
		{ Update(); m_log_record += num; return m_log_record; };

	// Compare the ID to the one stored
	// 0==one (or both) are empty, 1=same, -1=different
	int CompareUniqId( const std::string &id ) const;

	// Get updated stat of the file
	int StatFile( void );
	int SecondsSinceStat( void ) const;

	// Special method to stat the current file from an open FD to it
	//  NOTE: *Assumes* that this FD points to CurPath()!!
	int StatFile( int fd );

	// Update the time
	void Update( void ) { m_update_time = time(NULL); };

	// Has the log file grown?
	ReadUserLog::FileStatus CheckFileStatus( int fd, bool &is_emtpy );

	// Get / set the log file type
	// Method to generate log path
	bool GeneratePath( int rotation, std::string &path,
					   bool initializing = false ) const;

	// Check the file for differences
	int ScoreFile( int rot = -1 ) const;
	int ScoreFile( const char *path = NULL, int rot = -1 ) const;
	int ScoreFile( const struct stat &statbuf, int rot = -1 ) const;

	UserLogType LogType( void ) const
		{ return m_log_type; };
	void LogType( UserLogType t )
		{ Update(); m_log_type = t; };
	bool IsClassadLogType() const
		{ return m_log_type >= UserLogType::LOG_TYPE_XML; };
	bool IsUnknownLogType() const
		{ return m_log_type < UserLogType::LOG_TYPE_NORMAL; };

	// Set the score factors
	enum ScoreFactors {
		SCORE_CTIME,		// ctime values match
		SCORE_INODE,		// inodes match
		SCORE_SAME_SIZE,	// File is the same size
		SCORE_GROWN,		// current file has grown
		SCORE_SHRUNK,		// File has shrunk
	};
	void SetScoreFactor( enum ScoreFactors which, int factor );


	// Generate an external file state structure
	bool GetState( ReadUserLog::FileState &state ) const;
	bool SetState( const ReadUserLog::FileState &state );

	// *** Testing API ***
	// Get file state into a formated string
	void GetStateString( std::string &str, const char *label = NULL ) const;
	void GetStateString( const ReadUserLog::FileState &state,
						 std::string &str, const char *label = NULL ) const;

private:
	// Private methods
	void Clear( bool init = false );
	int StatFile( struct stat &statbuf ) const;
	int StatFile( const char *path, struct stat &statbuf ) const;

	// Private data
	bool			m_init_error;		// Error initializing?
	bool			m_initialized;		// Initialized OK?

	std::string		m_base_path;		// The log's base path
	std::string		m_cur_path;			// The current (reading) log's path
	int				m_cur_rot;			// Current file rotation number
	std::string		m_uniq_id;			// File's uniq identifier
	int				m_sequence;			// File's sequence number
	time_t			m_update_time;		// Time of last data update

	struct stat		m_stat_buf;			// file stat data
	filesize_t		m_status_size;		// Size at last status check

	bool			m_stat_valid;		// Stat buffer valid?
	time_t			m_stat_time;		// Time of last stat

	filesize_t		m_log_position;		// Our position in the whole log
	filesize_t		m_log_record;		// Current record # in the whole log

	UserLogType		m_log_type;			// Type of this log
	filesize_t		m_offset;			// Our current offset
	filesize_t		m_event_num;		// Our current event #

	int				m_max_rotations;	// Max rot #
	int				m_recent_thresh;	// Max time for a stat to be "recent"

	// Score factors
	int				m_score_fact_ctime;		// ctime values match
	int				m_score_fact_inode;		// inodes match
	int				m_score_fact_same_size;	// File is the same size
	int				m_score_fact_grown;		// current file has grown
	int				m_score_fact_shrunk;	// File has shrunk
};

#endif
