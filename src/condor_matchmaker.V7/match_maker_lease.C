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

#include <list>
#include <string>
#include "match_maker_lease.h"
using namespace std;

MatchMakerLease::MatchMakerLease( void )
{
	m_duration = 0;
}

MatchMakerLease::MatchMakerLease(
	const string		&lease_id,
	int					lease_duration,
	bool				release_when_done )
{
	setLeaseId( lease_id );
	setLeaseDuration( lease_duration );
	m_release_when_done = release_when_done ;
}

MatchMakerLease::~MatchMakerLease( void )
{
}

void
MatchMakerLease::setLeaseId(
	const string		&lease_id )
{
	m_id = lease_id;
}

void
MatchMakerLease::setLeaseDuration(
	int					duration )
{
	m_duration = duration;
}

void
MatchMakerLease::setReleaseWhenDone(
	bool				rel )
{
	m_release_when_done = rel;
}

void MatchMakerLease_FreeList( list<MatchMakerLease *> &lease_list )
{
	for( list<MatchMakerLease *>::iterator iter = lease_list.begin();
		 iter != lease_list.end();
		 iter++ ) {
		delete *iter;
	}
}
