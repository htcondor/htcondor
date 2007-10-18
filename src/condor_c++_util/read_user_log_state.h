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

	ReadUserLogState( const char *path,
					  int max_rot,
					  int recent_thresh );
	ReadUserLogState( const ReadUserLog::FileState &state,
					  int max_rot,
					  int recent_thresh );
	~ReadUserLogState( void );

	// Reset parameters about the current file (offset, stat info, etc.)
	void Reset( ResetType type = RESET_FILE );

	// Accessors
	const char *BasePath( void ) const { return m_base_path; };
	const char *CurPath( void ) const { return m_cur_path.GetCStr( ); };
	int Rotation( void ) const { return m_cur_rot; };
	filesize_t Offset( void ) const { return m_offset; };
	bool IsValid( void ) const { return (m_cur_rot >= 0); };

	// Set methods
	int Rotation( int rotation, bool store_stat = false );
	int Rotation( int rotation, StatStructType &statbuf );
	filesize_t Offset( filesize_t offset )
		{ return m_offset = offset; };

	// Get / set the uniq identifier
	void UniqId( const MyString &id ) { m_uniq_id = id; };
	const char *UniqId( void ) const { return m_uniq_id.GetCStr(); };
	bool ValidUniqId( void ) const { return ( m_uniq_id.Length() != 0 ); };

	// Get / set the sequence number
	void Sequence( int seq ) { m_sequence = seq; };
	int Sequence( void ) const { return m_sequence; };

	// Compare the ID to the one stored
	// 0==one (or both) are empty, 1=same, -1=different
	int CompareUniqId( const MyString &id ) const;

	// Get updated stat of the file
	int StatFile( void );
	int SecondsSinceUpdate( void ) const;

	// Special method to stat the current file from an open FD to it
	//  NOTE: *Assumes* that this FD points to CurPath()!!
	int StatFile( int fd );

	// Method to generate log path
	bool GeneratePath( int rotation, MyString &path ) const;

	// Check the file for differences
	int ScoreFile( int rot = -1 ) const;
	int ScoreFile( const char *path = NULL, int rot = -1 ) const;
	int ScoreFile( const StatStructType &statbuf, int rot = -1 ) const;

	UserLogType LogType( void ) const { return m_log_type; };
	UserLogType LogType( UserLogType t ) { return m_log_type = t; };
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
	bool GetState( ReadUserLog::FileState &state ) const;
	bool SetState( const ReadUserLog::FileState &state );
	bool UninitState( ReadUserLog::FileState &state ) const;

	// Debugging: Get file state into a formated string
	void GetState( MyString &str, const char *label = NULL ) const;

	// This is the file state buffer that we generate for the init/get/set
	// methods
	struct FileState {
		char			signature[64];		// File state signature
		char			path[_POSIX_PATH_MAX]; // The log's path
		char			uniq_id[128];		// File's uniq identifier
		int				sequence;			// File's sequence number
		int				rotation;			// 0 == the "current" file
		UserLogType		log_type;			// The log's type
		StatStructInode	inode;				// The log's inode #
		time_t			ctime;				// The log's creation time
		union {								// Make these things 8 bytes
			char		bytes[8];
			filesize_t	asint;
		}				size, offset;		// log's size and our offset
		time_t			update_time;		// Time of last struct update
	};

private:
	// Private methods
	void Clear( bool init = false );
	int StatFile( StatStructType &statbuf ) const;
	int StatFile( const char *path, StatStructType &statbuf ) const;

	// Private data
	char			*m_base_path;		// The log's base path
	MyString		m_cur_path;			// The current (reading) log's path
	int				m_cur_rot;			// Current file rotation number
	MyString		m_uniq_id;			// File's uniq identifier
	int				m_sequence;			// File's sequence number

	StatStructType	m_stat_buf;			// file stat data
	bool			m_stat_valid;		// Stat buffer valid?
	time_t			m_update_time;		// Time of last stat

	UserLogType		m_log_type;			// Type of this log
	filesize_t		m_offset;			// Our current offset

	int				m_max_rot;			// Max rot #
	int				m_recent_thresh;	// Max time for a stat to be "recent"

	// Score factors
	int				m_score_fact_ctime;		// ctime values match
	int				m_score_fact_inode;		// inodes match
	int				m_score_fact_same_size;	// File is the same size
	int				m_score_fact_grown;		// current file has grown
	int				m_score_fact_shrunk;	// File has shrunk
};

#endif
