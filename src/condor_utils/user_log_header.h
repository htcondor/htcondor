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

#ifndef _USER_LOG_HEADER_H
#define _USER_LOG_HEADER_H

#include <time.h>
#include "read_user_log_state.h"
#include "write_user_log.h"

// User log header info

// Simple class to extract info from a log file header event
class UserLogHeader
{
public:
	UserLogHeader( void );
	UserLogHeader( const ULogEvent *event );
	UserLogHeader( const UserLogHeader & );
	~UserLogHeader( void ) { };

	// Valid?
	bool IsValid( void ) const { return m_valid; };

	// Get/set methods
	void getId( std::string &id ) const
		{ id = m_id; };
	const std::string &getId( void ) const
		{ return m_id; };
	void setId( const std::string &id )
		{ m_id = id; };

	int getSequence( void ) const
		{ return m_sequence; };
	int setSequence( int sequence )
		{ return m_sequence = sequence; };
	int incSequence( void )
		{ return ++m_sequence; };

	time_t getCtime( void ) const
		{ return m_ctime; };
	time_t setCtime( time_t ctime )
		{ return m_ctime = ctime; };

	filesize_t getSize( void ) const
		{ return m_size; };
	filesize_t setSize( filesize_t size )
		{ return m_size = size; };

	int64_t getNumEvents( void ) const
		{ return m_num_events; };
	int64_t setNumEvents( int64_t num_events )
		{ return m_num_events = num_events; };

	filesize_t getFileOffset( void ) const
		{ return m_file_offset; };
	filesize_t setFileOffset( filesize_t file_offset )
		{ return m_file_offset = file_offset; };
	filesize_t addFileOffset( filesize_t add )
		{ return m_file_offset += add; };

	int64_t getEventOffset( void ) const
		{ return m_event_offset; };
	int64_t setEventOffset( int64_t event_offset )
		{ return m_event_offset = event_offset; };
	int64_t addEventOffset( int64_t num_events )
		{ return m_event_offset += num_events; };

	int getMaxRotation( void ) const
		{ return m_max_rotation; };
	int setMaxRotation( int max_rotation )
		{ return m_max_rotation = max_rotation; };

	void getCreatorName( std::string &name ) const
		{ name = m_creator_name; };
	const std::string & getCreatorName( void ) const
		{ return m_creator_name; };
	void setCreatorName( const std::string &name )
		{ m_creator_name = name; };
	void setCreatorName( const char *name )
		{ m_creator_name = name; };
	
	// Extract data from an event
	int ExtractEvent( const ULogEvent *);

	// Debug
	void sprint_cat( std::string &s ) const;
	void dprint( int level, std::string &buf ) const;
	void dprint( int level, const char *label ) const;

protected:
	std::string	m_id;
	int			m_sequence;
	time_t		m_ctime;			// Creation time
	filesize_t	m_size;				// Size of this file
	int64_t		m_num_events;		// # events in this file
	filesize_t	m_file_offset;		// Offset in the "big file"
	int64_t		m_event_offset;		// Event offset in the "big file"
	int			m_max_rotation;		// Max rotation
	std::string	m_creator_name;		// Name of the file's creator

	bool		m_valid;
};


// Simple class to extract info from a log file header event
class ReadUserLogHeader : public UserLogHeader
{
public:
	ReadUserLogHeader( void )
		{ m_valid = false; };
	ReadUserLogHeader( const ULogEvent *event )
		{ m_valid = false; ExtractEvent( event ); };
	~ReadUserLogHeader( void ) { };

	// Read the header from a file
	int Read( ReadUserLog &reader );

private:
};

// Simple class to extract info from a log file header event
class WriteUserLogHeader : public UserLogHeader
{
public:
	WriteUserLogHeader( void )
		{ };
	WriteUserLogHeader( const UserLogHeader &other )
		: UserLogHeader( other )
		{ };
	~WriteUserLogHeader( void )
		{ };

	// Read the header from a file
	int Write( WriteUserLog &writer, int fd = -1 );
	bool GenerateEvent( GenericEvent &event );

private:
};

#endif
