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
#include "user_log.c++.h"
#include <time.h>
#include "MyString.h"
#include "condor_config.h"
#include "stat_wrapper.h"
#include "user_log_header.h"

//
// UserLogHeader (base class) methods
//

// Simple constructor for the user log header
UserLogHeader::UserLogHeader( void )
{
	m_sequence = 0;
	m_ctime = 0;
	m_size = 0;
	m_num_events = 0;
	m_file_offset = 0;
	m_event_offset = 0;
	m_valid = false;
}

// Copy constructor for the user log header
UserLogHeader::UserLogHeader( const UserLogHeader &other )
{
	setId(          other.getId() );
	setSequence(    other.getSequence() );
	setCtime(       other.getCtime() );
	setSize(        other.getSize() );
	setNumEvents(   other.getNumEvents() );
	setFileOffset(  other.getFileOffset() );
	setEventOffset( other.getEventOffset() );

	m_valid = other.IsValid( );
}

// Extract info from an event
int
UserLogHeader::ExtractEvent( const ULogEvent *event )
{
	// Not a generic event -- ignore it
	if ( ULOG_GENERIC != event->eventNumber ) {
		return ULOG_NO_EVENT;
	}

	const GenericEvent	*generic = dynamic_cast <const GenericEvent*>( event );
	if ( ! generic ) {
		dprintf( D_ALWAYS, "Can't pointer cast generic event!\n" );
		return ULOG_UNK_ERROR;
	} else {
		char	*id;
		int		ctime;

		int n = sscanf( generic->info,
						"Global JobLog: "
						"ctime=%d "
						"id=%as "
						"sequence=%d "
						"size="FILESIZE_T_FORMAT" "
						"events=%"PRId64" "
						"offset="FILESIZE_T_FORMAT" "
						"event_off=%"PRId64" ",
						&ctime,
						&id,
						&m_sequence,
						&m_size,
						&m_num_events,
						&m_file_offset,
						&m_event_offset );
		if ( n >= 3 ) {
			if ( id ) {
				m_id = id;
				free( id );
			}
			m_ctime = ctime;
			m_valid = true;
			return ULOG_OK;
		}
		return ULOG_NO_EVENT;
	}
}

//
// ReadUserLogHeader methods
//
int
ReadUserLogHeader::Read(
	ReadUserLog	&reader )
{

	// Now, read the event itself
	ULogEvent			*event = NULL;
	ULogEventOutcome	outcome = reader.readEvent( event );

	if ( ULOG_OK != outcome ) {
		return outcome;
	}
	if ( ULOG_GENERIC != event->eventNumber ) {
		return ULOG_NO_EVENT;
	}

	int rval = ExtractEvent( event );
	delete event;

	return rval;
}

// Simplified read interface
#if 0
int
ReadUserLogHeader::Read(
	const char			*path )
{
	return Read( 0, path, NULL );
}

// Read the header from a file
int
ReadUserLogHeader::Read(
	int						 rot,
	const char				*path,
	const ReadUserLogState	*state )
{
	// Here, we have an indeterminate result
	// Read the file's header info

	// If no path provided, generate one
	MyString temp_path;
	if ( ( NULL == path ) && state ) {
		state->GeneratePath( rot, temp_path );
		path = temp_path.GetCStr( );
	}

	// Finally, no path -- give up
	if ( !path ) {
		return ULOG_RD_ERROR;
	}

	// We'll instantiate a new log reader to do this for us
	// Note: we disable rotation for this one, so we won't recurse infinitely
	ReadUserLog			 reader;

	// Initialize the reader
	if ( !reader.initialize( path, false, false ) ) {
		return ULOG_RD_ERROR;
	}

	// Now, read the event itself
	ULogEvent			*event;
	ULogEventOutcome	outcome;
	outcome = reader.readEvent( event );
	if ( ULOG_RD_ERROR == outcome ) {
		return outcome;
	}
	else if ( ULOG_OK != outcome ) {
		return outcome;
	}

	// Finally, if it's a generic event, let's see if we can parse it
	int status = ExtractEvent( event );
	delete event;

	return status;
}
#endif


//
// WriteUserLogHeader methods
//

// Write a header event
int
WriteUserLogHeader::Write( UserLog &writer, FILE *fp )
{
	GenericEvent	event;

	if ( !GenerateEvent( event ) ) {
		return ULOG_UNK_ERROR;
	}
	return writer.writeGlobalEvent( event, fp, true );
}

// Generate a header event
bool
WriteUserLogHeader::GenerateEvent( GenericEvent &event )
{
	snprintf( event.info, sizeof(event.info),
			  "Global JobLog: "
			  "ctime=%d "
			  "id=%s "
			  "sequence=%d "
			  "size="FILESIZE_T_FORMAT" "
			  "events=%"PRId64" "
			  "offset="FILESIZE_T_FORMAT" "
			  "event_off=%"PRId64" ",
			  (int) getCtime(),
			  getId().GetCStr(),
			  getSequence(),
			  getSize(),
			  getNumEvents(),
			  getFileOffset(),
			  getEventOffset() );
	dprintf( D_FULLDEBUG, "Generated log header: '%s'\n", event.info );
	int		len = strlen( event.info );
	while( len < 256 ) {
		strcat( event.info, " " );
		len++;
	}

	return true;
}
