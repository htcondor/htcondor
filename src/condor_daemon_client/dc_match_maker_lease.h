/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef _CONDOR_DC_MATCH_MAKER_LEASE_H
#define _CONDOR_DC_MATCH_MAKER_LEASE_H

#include <list>
#include <string>
#include "condor_common.h"
#include "stream.h"
#include "daemon.h"


#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;


class DCMatchMakerLease {
  public:

	// Constructors / destructors
	DCMatchMakerLease( time_t now = 0 );
	DCMatchMakerLease( const DCMatchMakerLease &, time_t now = 0 );
	DCMatchMakerLease( classad::ClassAd *, time_t now = 0 );
	DCMatchMakerLease( const classad::ClassAd &, time_t now = 0 );
	DCMatchMakerLease( const string &lease_id,
					  int lease_duration = 0,
					  bool release_when_done = true,
					  time_t now = 0 );
	~DCMatchMakerLease( void );

	// Various initialization methods
	int initFromClassAd( classad::ClassAd *ad, time_t now = 0 );
	int initFromClassAd( const classad::ClassAd	&ad, time_t now = 0 );
	int copyUpdates( const DCMatchMakerLease & );

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
		{ return ( LeaseRemaining() > 0 ); };

  private:
	classad::ClassAd	*m_lease_ad;
	string				m_lease_id;
	int					m_lease_duration;
	int					m_lease_time;
	bool				m_release_lease_when_done;
	bool				m_mark;
};

// Free a list of leases
void DCMatchMakerLease_FreeList(
	list<DCMatchMakerLease *>		&lease_list
	);
int DCMatchMakerLease_RemoveLeases(
	list<DCMatchMakerLease *>		&lease_list,
	const list<const DCMatchMakerLease *> &remove_list
	);
int DCMatchMakerLease_UpdateLeases(
	list<DCMatchMakerLease *>		&lease_list,
	const list<const DCMatchMakerLease *> &update_list
	);
int DCMatchMakerLease_MarkLeases(
	list<DCMatchMakerLease *>		&lease_list,
	bool							mark
	);
int DCMatchMakerLease_RemoveMarkedLeases(
	list<DCMatchMakerLease *>		&lease_list,
	bool							mark
	);
int DCMatchMakerLease_CountMarkedLeases(
	const list<const DCMatchMakerLease *> &lease_list,
	bool							mark
	);

#endif /* _CONDOR_DC_MATCH_MAKER_LEASE_H */
