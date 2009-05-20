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
#include "MyString.h"
#include "stat_wrapper.h"

// Internal verion of the file state
class ReadUserLogState
{
public:

	// Log file type
	enum UserLogType {
		LOG_TYPE_UNKNOWN = 0, LOG_TYPE_OLD, LOG_TYPE_XML
	};
	struct FileState;
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
	const char *BasePath( void ) const { return m_base_path.GetCStr(); };
	const char *CurPath( void ) const { return m_cur_path.GetCStr( ); };
	int Rotation( void ) const { return m_cur_rot; };
	filesize_t Offset( void ) const { return m_offset; };
	bool IsValid( void ) const { return (m_cur_rot >= 0); };

	// Accessors for a "file state"
	const char *BasePath( const ReadUserLog::FileState &state ) const;
	const char *CurPath( const ReadUserLog::FileState &state ) const;
	int Rotation( const ReadUserLog::FileState &state ) const;
	filesize_t LogPosition( const ReadUserLog::FileState &state ) const;
	filesize_t LogRecordNo( const ReadUserLog::FileState &state ) const;
	filesize_t Offset( const ReadUserLog::FileState &state ) const;

	// Get/set maximum rotations
	int MaxRotations( void ) { return m_max_rotations; }
	int MaxRotations( int max_rotations )
		{ Update(); return m_max_rotations = max_rotations; }

	// Set methods
	int Rotation( int rotation, bool store_stat = false,
				  bool initializing = false );
	int Rotation( int rotation, StatStructType &statbuf,
				  bool initializing = false );
	filesize_t Offset( filesize_t offset )
		{ Update(); return m_offset = offset; };

	// Get / set the uniq identifier
	void UniqId( const MyString &id ) { Update(); m_uniq_id = id; };
	const char *UniqId( void ) const { return m_uniq_id.GetCStr(); };
	bool ValidUniqId( void ) const { return ( m_uniq_id.Length() != 0 ); };

	// Get / set the sequence number
	void Sequence( int seq ) { m_sequence = seq; };
	int Sequence( void ) const { return m_sequence; };

	// Get/set global fields
	filesize_t LogPosition( void ) const { return m_log_position; };
	filesize_t LogPosition( filesize_t pos )
		{ Update(); return m_log_position = pos; };
	filesize_t LogRecordNo( void ) const { return m_log_record; };
	filesize_t LogRecordNo( filesize_t num )
		{ Update(); return m_log_record = num; };

	// Compare the ID to the one stored
	// 0==one (or both) are empty, 1=same, -1=different
	int CompareUniqId( const MyString &id ) const;

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
	bool GeneratePath( int rotation, MyString &path,
					   bool initializing = false ) const;

	// Check the file for differences
	int ScoreFile( int rot = -1 ) const;
	int ScoreFile( const char *path = NULL, int rot = -1 ) const;
	int ScoreFile( const StatStructType &statbuf, int rot = -1 ) const;

	UserLogType LogType( void ) const { return m_log_type; };
	UserLogType LogType( UserLogType t ) { Update(); return m_log_type = t; };
	bool IsLogType( UserLogType t ) const { return m_log_type == t; };

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
	static bool InitState( ReadUserLog::FileState &state );
	static bool UninitState( ReadUserLog::FileState &state );
	bool GetState( ReadUserLog::FileState &state ) const;
	bool SetState( const ReadUserLog::FileState &state );

	// *** Testing API ***
	// Get file state into a formated string
	void GetStateString( MyString &str, const char *label = NULL ) const;
	void GetStateString( const ReadUserLog::FileState &state,
						 MyString &str, const char *label = NULL ) const;

	// Get the file state
	static const ReadUserLogState::FileState *
		GetFileStateConst( const ReadUserLog::FileState &state );


	// ********************************************************************
	// This is the file state buffer that we generate for the init/get/set
	// methods
	// ********************************************************************
#define FILESTATE_VERSION		103

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
		StatStructInode	m_inode;			// The log's inode #
		time_t			m_ctime;			// The log's creation time
		FileStateI64_t	m_size;				// The log's size (bytes)
		FileStateI64_t	m_offset;			// Our offset in current log
		FileStateI64_t	m_log_offset;		// UNUSED
		FileStateI64_t	m_log_position;		// Our position in the whole log
		FileStateI64_t	m_log_record;		// Cur record # in the whole log
		time_t			m_update_time;		// Time of last struct update
	};

	// "Public" view of the file state
	typedef union {
		FileState	actual_state;
		char		filler [2048];
	} FileStatePub;


private:
	// Private methods
	void Clear( bool init = false );
	int StatFile( StatStructType &statbuf ) const;
	int StatFile( const char *path, StatStructType &statbuf ) const;

	// Get the "internal" state pointer from the application file state
	static ReadUserLogState::FileState *
		GetFileState( ReadUserLog::FileState &state );

	// Private data
	bool			m_init_error;		// Error initializing?
	bool			m_initialized;		// Initialized OK?

	MyString		m_base_path;		// The log's base path
	MyString		m_cur_path;			// The current (reading) log's path
	int				m_cur_rot;			// Current file rotation number
	MyString		m_uniq_id;			// File's uniq identifier
	int				m_sequence;			// File's sequence number
	time_t			m_update_time;		// Time of last data update

	StatStructType	m_stat_buf;			// file stat data
	filesize_t		m_status_size;		// Size at last status check

	bool			m_stat_valid;		// Stat buffer valid?
	time_t			m_stat_time;		// Time of last stat

	filesize_t		m_log_position;		// Our position in the whole log
	filesize_t		m_log_record;		// Current record # in the whole log

	UserLogType		m_log_type;			// Type of this log
	filesize_t		m_offset;			// Our current offset

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
