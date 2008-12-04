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
#ifndef _CONDOR_DC_LEASE_MANAGER_LEASE_H
#define _CONDOR_DC_LEASE_MANAGER_LEASE_H

#include <list>
#include <string>
#include "condor_common.h"
#include "stream.h"
#include "daemon.h"


#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;


class DCLeaseManagerLease {
  public:

	// Constructors / destructors
	DCLeaseManagerLease( time_t now = 0 );
	DCLeaseManagerLease( const DCLeaseManagerLease &, time_t now = 0 );
	DCLeaseManagerLease( classad::ClassAd *, time_t now = 0 );
	DCLeaseManagerLease( const classad::ClassAd &, time_t now = 0 );
	DCLeaseManagerLease( const string &lease_id,
						 int lease_duration = 0,
						 bool release_when_done = true,
						 time_t now = 0 );
	~DCLeaseManagerLease( void );

	// Various initialization methods
	int initFromClassAd( classad::ClassAd *ad, time_t now = 0 );
	int initFromClassAd( const classad::ClassAd	&ad, time_t now = 0 );
	int copyUpdates( const DCLeaseManagerLease & );

	// Accessors
	const string &LeaseId( void ) const
		{ return m_lease_id; };
	int LeaseDuration( void ) const
		{ return m_lease_duration; };
	classad::ClassAd *LeaseAd( void ) const
		{ return m_lease_ad; };
	bool ReleaseLeaseWhenDone( void ) const
		{ return m_release_lease_when_done; };
	int LeaseTime( void ) const
		{ return m_lease_time; };

	// Set methods
	int setLeaseId( const string & );
	int setLeaseDuration( int );
	int setLeaseStart( time_t now = 0 );

	// Mark / getmark methods
	bool setMark( bool mark )
		{ this->m_mark = mark; return m_mark; };
	bool getMark( void ) const
		{ return m_mark; };

	// Helper methods to help determine how much time is left on a lease
	time_t getNow( time_t now = 0 ) const
		{ if ( !now ) { now = time( NULL ); } return now; };
	int LeaseExpiration( void ) const
		{ return m_lease_time + m_lease_duration; };
	int LeaseRemaining( time_t now = 0 ) const
		{ return getNow( now ) - ( m_lease_time + m_lease_duration ); };
	bool LeaseExpired( time_t now = 0 ) const
		{ return ( LeaseRemaining(now) > 0 ); };

  private:
	classad::ClassAd	*m_lease_ad;
	string				m_lease_id;
	int					m_lease_duration;
	int					m_lease_time;
	bool				m_release_lease_when_done;
	bool				m_mark;
};

// Free a list of leases
void
DCLeaseManagerLease_FreeList(
	list<DCLeaseManagerLease *>		&lease_list
	);
int
DCLeaseManagerLease_RemoveLeases(
	list<DCLeaseManagerLease *>				&lease_list,
	const list<const DCLeaseManagerLease *> &remove_list
	);
int
DCLeaseManagerLease_UpdateLeases(
	list<DCLeaseManagerLease *>		&lease_list,
	const list<const DCLeaseManagerLease *> &update_list
	);
int
DCLeaseManagerLease_MarkLeases(
	list<DCLeaseManagerLease *>		&lease_list,
	bool							mark
	);
int
DCLeaseManagerLease_RemoveMarkedLeases(
	list<DCLeaseManagerLease *>		&lease_list,
	bool							mark
	);
int
DCLeaseManagerLease_GetMarkedLeases(
	const list<const DCLeaseManagerLease *> &lease_list,
	bool									mark,
	list<const DCLeaseManagerLease *>	 	&marked_lease_list
	);
int
DCLeaseManagerLease_CountMarkedLeases(
	const list<const DCLeaseManagerLease *> &lease_list,
	bool									mark
	);
list<const DCLeaseManagerLease *> *
DCLeaseManagerLease_GetConstList(
	list<DCLeaseManagerLease *>		*non_const_list
	);

#endif /* _CONDOR_DC_LEASE_MANAGER_LEASE_H */
