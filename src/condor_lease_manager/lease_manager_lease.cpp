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

#include <list>
#include <string>
#include "lease_manager_lease.h"
using namespace std;

LeaseManagerLease::LeaseManagerLease(
	const string		&lease_id,
	int					lease_duration,
	bool				release_when_done )
{
	setLeaseId( lease_id );
	setLeaseDuration( lease_duration );
	m_release_when_done = release_when_done ;
}

LeaseManagerLease::~LeaseManagerLease( void )
{
}

void
LeaseManagerLease::setLeaseId(
	const string		&lease_id )
{
	m_id = lease_id;
}

void
LeaseManagerLease::setLeaseDuration(
	int					duration )
{
	m_duration = duration;
}

void
LeaseManagerLease::setReleaseWhenDone(
	bool				rel )
{
	m_release_when_done = rel;
}

void LeaseManagerLease_FreeList( list<LeaseManagerLease *> &lease_list )
{
	for( list<LeaseManagerLease *>::iterator iter = lease_list.begin();
		 iter != lease_list.end();
		 iter++ ) {
		delete *iter;
	}
}
