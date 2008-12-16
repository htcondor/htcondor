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
#include <stdio.h>


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
	const string &leaseId( void ) const
		{ return m_lease_id; };
	int leaseDuration( void ) const
		{ return m_lease_duration; };
	classad::ClassAd *leaseAd( void ) const
		{ return m_lease_ad; };
	bool releaseLeaseWhenDone( void ) const
		{ return m_release_lease_when_done; };
	int leaseTime( void ) const
		{ return m_lease_time; };

	// Lease ID matching methods
	bool idMatch( const char *id ) const
		{ return m_lease_id == id; };
	bool idMatch( const string &id ) const
		{ return m_lease_id == id; };
	bool idMatch( const DCLeaseManagerLease &other ) const
		{ return m_lease_id == other.leaseId(); };

	// "Dead" accessors
	bool isDead( void ) const
		{ return m_dead; };
	bool setDead( bool dead )
		{ return m_dead = dead; };

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
	int leaseExpiration( void ) const
		{ return m_lease_time + m_lease_duration; };
	int secondsRemaining( time_t now = 0 ) const
		{ return getNow( now ) - ( m_lease_time + m_lease_duration ); };
	bool isExpired( time_t now = 0 ) const
		{ return ( secondsRemaining(now) > 0 ); };

	// Read/write leases from/to a stdio file
	bool fwrite( FILE *fp ) const;
	bool fread(  FILE *fp );

  private:
	classad::ClassAd	*m_lease_ad;
	string				 m_lease_id;
	int					 m_lease_duration;
	int					 m_lease_time;
	bool				 m_release_lease_when_done;
	bool				 m_mark;
	bool				 m_dead;
};


//
// Helper free functions to manipulate lists of leases
//

// Copy a list of leases -- returns count
int
DCLeaseManagerLease_copyList(
	const list<const DCLeaseManagerLease *>	&source_list,
	list<const DCLeaseManagerLease *>		&dest_list
	);
int
DCLeaseManagerLease_copyList(
	const list<DCLeaseManagerLease *>		&source_list,
	list<DCLeaseManagerLease *>				&dest_list
	);

// Free a list of leases -- returns count
int
DCLeaseManagerLease_freeList(
	list<DCLeaseManagerLease *>				&lease_list
	);

// Remove leases from a list
int
DCLeaseManagerLease_removeLeases(
	list<DCLeaseManagerLease *>				&lease_list,
	const list<const DCLeaseManagerLease *> &remove_list
	);

// Update a list of leases
int
DCLeaseManagerLease_updateLeases(
	list<DCLeaseManagerLease *>				&lease_list,
	const list<const DCLeaseManagerLease *> &update_list
	);

// Lease mark operations
int
DCLeaseManagerLease_markLeases(
	list<DCLeaseManagerLease *>				&lease_list,
	bool									mark
	);
int
DCLeaseManagerLease_removeMarkedLeases(
	list<DCLeaseManagerLease *>				&lease_list,
	bool									mark
	);
int
DCLeaseManagerLease_getMarkedLeases(
	const list<const DCLeaseManagerLease *> &lease_list,
	bool									mark,
	list<const DCLeaseManagerLease *>	 	&marked_lease_list
	);
int
DCLeaseManagerLease_countMarkedLeases(
	const list<const DCLeaseManagerLease *> &lease_list,
	bool									mark
	);

// Get a 'const' list of leases from a list of leases
list<const DCLeaseManagerLease *> &
DCLeaseManagerLease_getConstList(
	const list<DCLeaseManagerLease *>		&non_const_list
	);

// Write out a list of leases to a file (returns count)
int
DCLeaseManagerLease_fwriteList(
	const list<const DCLeaseManagerLease *> &lease_list,
	FILE									*fp
	);

// Read a list of leases from a file (returns count)
int
DCLeaseManagerLease_freadList(
	list<DCLeaseManagerLease *>				&lease_list,
	FILE									*fp
	);


#endif /* _CONDOR_DC_LEASE_MANAGER_LEASE_H */
