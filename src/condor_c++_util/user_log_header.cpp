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
#include "read_user_log.h"
#include "write_user_log.h"
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
		char		 id[256];
		const char	*format;
		int			 ctime;

		int n = sscanf( generic->info,
						format = 
						"Global JobLog: "
						"ctime=%d "
						"id=%255s "
						"sequence=%d "
						"size="FILESIZE_T_FORMAT" "
						"events=%"PRId64" "
						"offset="FILESIZE_T_FORMAT" "
						"event_off=%"PRId64" ",
						&ctime,
						id,
						&m_sequence,
						&m_size,
						&m_num_events,
						&m_file_offset,
						&m_event_offset );
		if ( n >= 3 ) {
			m_ctime = ctime;
			m_valid = true;
			return ULOG_OK;
		}
		::dprintf( D_FULLDEBUG,
				   "UserLogHeader::ExtractEvent(): failed to parse '%s'"
				   "using format '%s' -> %d\n",
				   generic->info, format, n );
		return ULOG_NO_EVENT;
	}
}

// dprintf() metchod
void
UserLogHeader::dprintf( int level, const char *label ) const
{
	if ( NULL == label ) {
		label = "";
	}
	if ( m_valid ) {
		::dprintf( level,
				   "%s header:"
				   " id=%s"
				   " seq=%d"
				   " ctime=%lu"
				   " size="FILESIZE_T_FORMAT
				   " num=%"PRIi64
				   " file_offset="FILESIZE_T_FORMAT
				   " event_offset=%" PRIi64
				   "\n",
				   label,
				   m_id.GetCStr(),
				   m_sequence,
				   (unsigned long) m_ctime,
				   m_size,
				   m_num_events,
				   m_file_offset,
				   m_event_offset );
	}
	else {
		::dprintf( level, "%s header: not valid\n", label );
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
		::dprintf( D_FULLDEBUG,
				   "ReadUserLogHeader::Read(): readEvent() failed\n" );
		return outcome;
	}
	if ( ULOG_GENERIC != event->eventNumber ) {
		::dprintf( D_FULLDEBUG,
				   "ReadUserLogHeader::Read(): event #%d should be %d\n",
				   event->eventNumber, ULOG_GENERIC );
		return ULOG_NO_EVENT;
	}

	int rval = ExtractEvent( event );
	delete event;

	if ( rval != ULOG_OK) {
		::dprintf( D_FULLDEBUG,
				   "ReadUserLogHeader::Read(): failed to extract event\n" );
	}
	return rval;
}


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
	::dprintf( D_FULLDEBUG, "Generated log header: '%s'\n", event.info );
	int		len = strlen( event.info );
	while( len < 256 ) {
		strcat( event.info, " " );
		len++;
	}

	return true;
}
