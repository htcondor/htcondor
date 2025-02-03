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
#include "read_user_log.h"
#include "write_user_log.h"
#include <time.h>
#include "condor_config.h"
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
	m_max_rotation = -1;
	m_creator_name = "";
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
	setMaxRotation( other.getMaxRotation() );
	setCreatorName( other.getCreatorName() );

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
	}

	char		 id[256];
	char		 name[256];
	int			 ctime;

	id[0] = '\0';
	name[0] = '\0';
	int n = sscanf( generic->info,
					"Global JobLog:"
					" ctime=%d"
					" id=%255s"
					" sequence=%d"
					" size=" FILESIZE_T_FORMAT""
					" events=%" PRId64""
					" offset=" FILESIZE_T_FORMAT""
					" event_off=%" PRId64""
					" max_rotation=%d"
					" creator_name=<%255[^>]>",
					&ctime,
					id,
					&m_sequence,
					&m_size,
					&m_num_events,
					&m_file_offset,
					&m_event_offset,
					&m_max_rotation,
					name
					);
	if ( n >= 3 ) {
		m_ctime = ctime;
		m_id = id;
		m_valid = true;

		if ( n >= 8 ) {
			m_creator_name = name;
		}
		else {
			m_creator_name = "";
			m_max_rotation = -1;
		}

		if (IsFulldebug(D_FULLDEBUG)) {
			dprint( D_FULLDEBUG, "UserLogHeader::ExtractEvent(): parsed ->" );
		}
		return ULOG_OK;
	}
	else {
		::dprintf( D_FULLDEBUG,
				   "UserLogHeader::ExtractEvent(): can't parse '%s' => %d\n",
				   generic->info, n );
		return ULOG_NO_EVENT;
	}
}

// sprintf() method
void
UserLogHeader::sprint_cat( std::string &buf ) const
{
	if ( m_valid ) {
		formatstr_cat( buf, "id=%s"
						 " seq=%d"
						 " ctime=%lu"
						 " size=" FILESIZE_T_FORMAT
						 " num=%" PRIi64
						 " file_offset=" FILESIZE_T_FORMAT
						 " event_offset=%" PRIi64
						 " max_rotation=%d"
						 " creator_name=<%s>",
						 m_id.c_str(),
						 m_sequence,
						 (unsigned long) m_ctime,
						 m_size,
						 m_num_events,
						 m_file_offset,
						 m_event_offset,
						 m_max_rotation,
						 m_creator_name.c_str()
						 );
	}
	else {
		buf += "invalid";
	}
}

// dprint() method
void
UserLogHeader::dprint( int level, std::string &buf ) const
{
	if ( ! IsDebugCatAndVerbosity(level) ) {
		return;
	}

	sprint_cat( buf );
	::dprintf( level, "%s\n", buf.c_str() );
}

// dprint() method
void
UserLogHeader::dprint( int level, const char *label ) const
{
	if ( ! IsDebugCatAndVerbosity(level) ) {
		return;
	}

	if ( NULL == label ) {
		label = "";
	}

	std::string buf;
	formatstr( buf, "%s header:", label );
	this->dprint( level, buf );
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
		delete event;
		return outcome;
	}
	if ( ULOG_GENERIC != event->eventNumber ) {
		::dprintf( D_FULLDEBUG,
				   "ReadUserLogHeader::Read(): event #%d should be %d\n",
				   event->eventNumber, ULOG_GENERIC );
		delete event;
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

