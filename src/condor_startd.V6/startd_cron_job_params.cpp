/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include <ctype.h>
#include "string_list.h"
#include "startd.h"
#include "classad_cron_job.h"
#include "startd_cron_job.h"
#include <list>
using namespace std;

// Override the lookup methods so that we can stuff in default values
StartdCronJobParams::StartdCronJobParams(
	const char			*job_name,
	const CronJobMgr	&mgr )
		: ClassAdCronJobParams( job_name, mgr )
{
}

bool
StartdCronJobParams::Initialize( void )
{
	if ( !ClassAdCronJobParams::Initialize() ) {
		return false;
	}

	MyString	slots_str;
	Lookup( "SLOTS", slots_str );

	m_slots.clear();
	StringList	slot_list( slots_str.Value() );
	slot_list.rewind();
	const char *slot;
	while( ( slot = slot_list.next()) != NULL ) {
		if( !isdigit(*slot)) {
			dprintf( D_ALWAYS,
					 "Cron Job '%s': INVALID slot # '%s': ignoring slot list",
					 GetName(), slot );
			m_slots.clear();
			break;
		}
		unsigned	slotno = atoi( slot );
		m_slots.push_back( slotno );
	}

	// Print out the slots for D_FULLDEBUG
	if ( IsFulldebug(D_FULLDEBUG) ) {
		MyString	s;
		s.formatstr( "CronJob '%s' slots: ", GetName() );
		if ( m_slots.empty() ) {
			s += "ALL";
		}
		else {
			list<unsigned>::iterator iter;
			for( iter = m_slots.begin(); iter != m_slots.end(); iter++ ) {
				s.formatstr_cat( "%u ", *iter );
			}
		}
		dprintf( D_ALWAYS, "%s\n", s.Value() );
	}

	return true;
};

bool
StartdCronJobParams::InSlotList( unsigned slot ) const
{
	if ( m_slots.empty() ) {
		return true;
	}

	list<unsigned>::const_iterator iter;
	for( iter = m_slots.begin(); iter != m_slots.end(); iter++ ) {
		if ( slot == *iter ) {
			return true;
		}
	}
	return false;
}
