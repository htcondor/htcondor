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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_lease_manager.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
#include "newclassad_stream.h"
using namespace std;

#include <stdio.h>

// *** DCLeaseManagerLease (class to hold lease information) class methods ***

DCLeaseManagerLease::DCLeaseManagerLease( time_t now )
{
	m_lease_ad = NULL;
	m_lease_duration = 0;
	m_release_lease_when_done = true;
	setLeaseStart( now );
}

DCLeaseManagerLease::DCLeaseManagerLease( const DCLeaseManagerLease &lease,
										  time_t now )
{
	if ( lease.LeaseAd( ) ) {
		this->m_lease_ad = new classad::ClassAd( *(lease.LeaseAd( )) );
	} else {
		this->m_lease_ad = NULL;
	}
	setLeaseId( lease.LeaseId() );
	setLeaseDuration( lease.LeaseDuration( ) );
	this->m_release_lease_when_done = lease.ReleaseLeaseWhenDone( );
	setLeaseStart( now );
}

DCLeaseManagerLease::DCLeaseManagerLease(
	classad::ClassAd		*ad,
	time_t					now
	)
{
	m_lease_ad = NULL;
	initFromClassAd( ad, now );
}

DCLeaseManagerLease::DCLeaseManagerLease(
const classad::ClassAd	&ad,
	time_t				now
	)
{
	m_lease_ad = NULL;
	initFromClassAd( ad, now );
}

DCLeaseManagerLease::DCLeaseManagerLease(
	const string		&lease_id,
	int					lease_duration,
	bool				release_when_done,
	time_t				now )
{
	this->m_lease_ad = NULL;
	setLeaseId( lease_id );
	setLeaseDuration( lease_duration );
	this->m_release_lease_when_done = release_when_done;
	setLeaseStart( now );
}

DCLeaseManagerLease::~DCLeaseManagerLease( void )
{
	if ( m_lease_ad ) {
		delete m_lease_ad;
	}
}

int
DCLeaseManagerLease::setLeaseStart( time_t now )
{
	if ( !now ) {
		now = time( NULL );
	}
	this->m_lease_time = now;
	return 0;
}

int
DCLeaseManagerLease::initFromClassAd(
	const classad::ClassAd	&ad,
	time_t					now
	)
{
	return initFromClassAd( new classad::ClassAd( ad ), now );
}

int
DCLeaseManagerLease::initFromClassAd(
	classad::ClassAd	*ad,
	time_t				now
	)
{
	int		status = 0;
	if ( m_lease_ad && (m_lease_ad != ad ) ) {
		delete m_lease_ad;
		m_lease_ad = NULL;
	}
	if ( !ad ) {
		return 0;
	}

	m_lease_ad = ad;
	if ( !m_lease_ad->EvaluateAttrString( "LeaseId", m_lease_id ) ) {
		status = 1;
		m_lease_id = "";
	}
	if ( !m_lease_ad->EvaluateAttrInt( "LeaseDuration", m_lease_duration ) ) {
		status = 1;
		m_lease_duration = 0;
	}
	if ( !m_lease_ad->EvaluateAttrBool( "ReleaseWhenDone",
										m_release_lease_when_done ) ) {
		status = 1;
		m_release_lease_when_done = true;
	}
	setLeaseStart( now );
	return status;
}

int
DCLeaseManagerLease::copyUpdates( const DCLeaseManagerLease &lease )
{

	// Don't touch the lease ID

	// Copy other attributes
	setLeaseDuration( lease.LeaseDuration( ) );
	this->m_release_lease_when_done = lease.ReleaseLeaseWhenDone( );
	setLeaseStart( lease.LeaseTime() );
	setMark( lease.getMark() );

	// If there's an ad in the lease to copy, free the old one & copy
	if ( lease.LeaseAd( ) ) {
		if ( this->m_lease_ad ) {
			delete m_lease_ad;
		}
		this->m_lease_ad = new classad::ClassAd( *(lease.LeaseAd( )) );
	}
	// Otherwise, if there is an old ad, update it
	else if ( this->m_lease_ad ) {
		this->m_lease_ad->InsertAttr( "LeaseDuration",
									  this->m_lease_duration );
		this->m_lease_ad->InsertAttr( "ReleaseWhenDone",
									  this->m_release_lease_when_done );
	}

	// All done
	return 0;
}

int
DCLeaseManagerLease::setLeaseId(
	const string		&lease_id )
{
	this->m_lease_id = lease_id;
	return 0;
}

int
DCLeaseManagerLease::setLeaseDuration(
	int					duration )
{
	this->m_lease_duration = duration;
	return 0;
}

// *** DCLeaseManagerLease list helper functions

void
DCLeaseManagerLease_FreeList( list<DCLeaseManagerLease *> &lease_list )
{
	while( lease_list.size() ) {
		DCLeaseManagerLease *lease = *(lease_list.begin( ));
		delete lease;
		lease_list.pop_front( );
	}
}

int
DCLeaseManagerLease_RemoveLeases(
	list<DCLeaseManagerLease *>				&lease_list,
	const list<const DCLeaseManagerLease *>	&remove_list
	)
{
	int		errors = 0;

	list<const DCLeaseManagerLease *>::const_iterator iter;
	for( iter = remove_list.begin( ); iter != remove_list.end( ); iter++ ) {
		const DCLeaseManagerLease	*remove = *iter;
		bool matched = false;
		for( list<DCLeaseManagerLease *>::iterator iter2 = lease_list.begin();
			 iter2 != lease_list.end();
			 iter2++ ) {
			DCLeaseManagerLease	*lease = *iter2;
			if ( remove->LeaseId() == lease->LeaseId() ) {
				matched = true;
				lease_list.erase( iter2 );	// Note: invalidates iter
				delete lease;
				break;
			}
		}
		if ( !matched ) {
			errors++;
		}
	}
	return errors;
}

int
DCLeaseManagerLease_UpdateLeases(
	list<DCLeaseManagerLease *>				&lease_list,
	const list<const DCLeaseManagerLease *>	&update_list
	)
{
	int		errors = 0;

	list<const DCLeaseManagerLease *>::const_iterator iter;
	for( iter = update_list.begin( ); iter != update_list.end( ); iter++ ) {
		const DCLeaseManagerLease	*update = *iter;
		bool matched = false;
		for( list<DCLeaseManagerLease *>::iterator iter2 = lease_list.begin();
			 iter2 != lease_list.end();
			 iter2++ ) {
			DCLeaseManagerLease	*lease = *iter2;
			if ( update->LeaseId() == lease->LeaseId() ) {
				matched = true;
				lease->copyUpdates( *update );
				break;
			}
		}
		if ( !matched ) {
			errors++;
		}
	}
	return errors;
}

int
DCLeaseManagerLease_MarkLeases(
	list<DCLeaseManagerLease *>		&lease_list,
	bool							mark
	)
{
	for( list<DCLeaseManagerLease *>::iterator iter = lease_list.begin();
		 iter != lease_list.end();
		 iter++ ) {
		DCLeaseManagerLease	*lease = *iter;
		lease->setMark( mark );
	}
	return 0;
}

int
DCLeaseManagerLease_RemoveMarkedLeases(
	list<DCLeaseManagerLease *>		&lease_list,
	bool							mark
	)
{
	list<const DCLeaseManagerLease *> remove_list;
	list<const DCLeaseManagerLease *> const_list =
		*DCLeaseManagerLease_GetConstList( &lease_list );
	DCLeaseManagerLease_GetMarkedLeases( const_list, mark, remove_list );

	for( list<const DCLeaseManagerLease *>::iterator iter = remove_list.begin();
		 iter != remove_list.end();
		 iter++ ) {
		DCLeaseManagerLease *lease = const_cast<DCLeaseManagerLease*>( *iter );
		lease_list.remove( lease );
		delete lease;
	}

	return 0;
}

int
DCLeaseManagerLease_CountMarkedLeases(
	const list<const DCLeaseManagerLease *> &lease_list,
	bool							mark
	)
{
	int		count = 0;
	list<const DCLeaseManagerLease *>::const_iterator iter;
	for( iter = lease_list.begin();
		 iter != lease_list.end();
		 iter++ ) {
		const DCLeaseManagerLease	*lease = *iter;
		if ( mark == lease->getMark( ) ) {
			count++;
		}
	}
	return count;
}

int
DCLeaseManagerLease_GetMarkedLeases(
	const list<const DCLeaseManagerLease *>	&lease_list,
	bool									mark,
	list<const DCLeaseManagerLease *>			&marked_lease_list
	)
{
	int		count = 0;
	list<const DCLeaseManagerLease *>::const_iterator iter;
	for( iter  = lease_list.begin();
		 iter != lease_list.end();
		 iter ++ ) {
		const DCLeaseManagerLease	*lease = *iter;
		if ( lease->getMark() == mark ) {
			marked_lease_list.push_back( lease );
			count++;
		}
	}
	return count;
}

list<const DCLeaseManagerLease *> *
DCLeaseManagerLease_GetConstList(
	list<DCLeaseManagerLease *>		*non_const_list
	)
{
	list<const DCLeaseManagerLease *> *const_list =
		(list<const DCLeaseManagerLease *>*) non_const_list;
	return const_list;
}
