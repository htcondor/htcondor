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
#include "newclassad_stream.unix.h"
using namespace std;

#include <stdio.h>

// *** DCLeaseManagerLease (class to hold lease information) class methods ***

DCLeaseManagerLease::DCLeaseManagerLease( time_t now )
{
	m_lease_ad = NULL;
	m_lease_duration = 0;
	m_release_lease_when_done = true;
	m_mark = false;
	m_dead = false;
	setLeaseStart( now );
}

DCLeaseManagerLease::DCLeaseManagerLease( const DCLeaseManagerLease &lease,
										  time_t now )
{
	m_mark = false;
	m_dead = false;
	if ( lease.leaseAd( ) ) {
		this->m_lease_ad = new classad::ClassAd( *(lease.leaseAd( )) );
	} else {
		this->m_lease_ad = NULL;
	}
	setLeaseId( lease.leaseId() );
	setLeaseDuration( lease.leaseDuration( ) );
	this->m_release_lease_when_done = lease.releaseLeaseWhenDone( );
	setLeaseStart( now );
}

DCLeaseManagerLease::DCLeaseManagerLease(
	classad::ClassAd		*ad,
	time_t					now
	)
{
	m_mark = false;
	m_dead = false;
	m_lease_ad = NULL;
	initFromClassAd( ad, now );
}

DCLeaseManagerLease::DCLeaseManagerLease(
const classad::ClassAd	&ad,
	time_t				now
	)
{
	m_mark = false;
	m_dead = false;
	m_lease_ad = NULL;
	initFromClassAd( ad, now );
}

DCLeaseManagerLease::DCLeaseManagerLease(
	const string		&lease_id,
	int					lease_duration,
	bool				release_when_done,
	time_t				now )
{
	m_mark = false;
	m_dead = false;
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
	setLeaseDuration( lease.leaseDuration( ) );
	this->m_release_lease_when_done = lease.releaseLeaseWhenDone( );
	setLeaseStart( lease.leaseTime() );
	setMark( lease.getMark() );
	setDead( lease.isDead() );

	// If there's an ad in the lease to copy, free the old one & copy
	if ( lease.leaseAd( ) ) {
		if ( this->m_lease_ad ) {
			delete m_lease_ad;
		}
		this->m_lease_ad = new classad::ClassAd( *(lease.leaseAd( )) );
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

union LeaseIoBuf
{
	struct
	{
		char	m_lease_id[256];
		char	m_lease_ad[2048];
		int		m_lease_duration;
		int		m_lease_time;
		bool	m_release_lease_when_done;
		bool	m_mark;
		bool	m_dead;
	} 		m_fields;
	char	m_static[4096];
};

bool
DCLeaseManagerLease::fwrite( FILE *fp ) const
{
	classad::ClassAdUnParser	unparser;
	LeaseIoBuf					buf;
	string						str;

	memset( &buf, 0, sizeof(buf) );

	strncpy( buf.m_fields.m_lease_id,
			 this->m_lease_id.c_str(),
			 sizeof(buf.m_fields.m_lease_id)-1 );

	unparser.Unparse( str, this->m_lease_ad );
	strncpy( buf.m_fields.m_lease_ad,
			 str.c_str(),
			 sizeof(buf.m_fields.m_lease_ad)-1 );

	buf.m_fields.m_lease_duration			= this->m_lease_duration;
	buf.m_fields.m_lease_time				= this->m_lease_time;
	buf.m_fields.m_release_lease_when_done	= this->m_release_lease_when_done;
	buf.m_fields.m_mark						= this->m_mark;
	buf.m_fields.m_dead						= this->m_dead;

	return ( ::fwrite( &buf, sizeof(buf), 1, fp ) == 1 );
}

bool
DCLeaseManagerLease::fread( FILE *fp )
{
	classad::ClassAdParser	parser;
	LeaseIoBuf				buf;
	string					str;

	if ( ::fread( &buf, sizeof(buf), 1, fp ) != 1 ) {
		return false;
	}

	this->m_lease_id = buf.m_fields.m_lease_id;
	this->m_lease_ad = parser.ParseClassAd( buf.m_fields.m_lease_ad, true );
	if ( NULL == this->m_lease_ad ) {
		return false;
	}

	this->m_lease_duration			= buf.m_fields.m_lease_duration;
	this->m_lease_time				= buf.m_fields.m_lease_time;
	this->m_release_lease_when_done	= buf.m_fields.m_release_lease_when_done;
	this->m_mark					= buf.m_fields.m_mark;
	this->m_dead					= buf.m_fields.m_dead;

	return true;
}


// *** DCLeaseManagerLease list helper functions

int
DCLeaseManagerLease_freeList( list<DCLeaseManagerLease *> &lease_list )
{
	int		count = 0;
	while( lease_list.size() ) {
		DCLeaseManagerLease *lease = *(lease_list.begin( ));
		delete lease;
		lease_list.pop_front( );
		count++;
	}
	return count;
}

int
DCLeaseManagerLease_copyList(
	const list<const DCLeaseManagerLease *>	&source_list,
	list<const DCLeaseManagerLease *>		&dest_list )
{
	int		count = 0;

	list<const DCLeaseManagerLease *>::const_iterator iter;
	for( iter = source_list.begin(); iter != source_list.end(); iter++ ) {
		const DCLeaseManagerLease *lease = *iter;
		dest_list.push_back( lease );
		count++;
	}

	return count;
}

int
DCLeaseManagerLease_copyList(
	const list<DCLeaseManagerLease *>	&source_list,
	list<DCLeaseManagerLease *>			&dest_list )
{
	int		count = 0;

	list<DCLeaseManagerLease *>::const_iterator iter;
	for( iter = source_list.begin(); iter != source_list.end(); iter++ ) {
		DCLeaseManagerLease *lease = *iter;
		dest_list.push_back( lease );
		count++;
	}

	return count;
}

int
DCLeaseManagerLease_removeLeases(
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
			if ( remove->leaseId() == lease->leaseId() ) {
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
DCLeaseManagerLease_updateLeases(
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
			if ( update->idMatch(*lease) ) {
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
DCLeaseManagerLease_markLeases(
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
DCLeaseManagerLease_removeMarkedLeases(
	list<DCLeaseManagerLease *>		&lease_list,
	bool							mark
	)
{
	list<const DCLeaseManagerLease *> remove_list;
	list<const DCLeaseManagerLease *> const_list =
		DCLeaseManagerLease_getConstList( lease_list );
	DCLeaseManagerLease_getMarkedLeases( const_list, mark, remove_list );

	list<const DCLeaseManagerLease *>::iterator iter;
	for( iter = remove_list.begin(); iter != remove_list.end(); iter++ ) {
		DCLeaseManagerLease *lease = const_cast<DCLeaseManagerLease*>( *iter );
		lease_list.remove( lease );
		delete lease;
	}

	return 0;
}

int
DCLeaseManagerLease_countMarkedLeases(
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
DCLeaseManagerLease_getMarkedLeases(
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

list<const DCLeaseManagerLease *> &
DCLeaseManagerLease_getConstList(
	const list<DCLeaseManagerLease *>	&non_const_list
	)
{
	typedef list<DCLeaseManagerLease *>			LeaseList;
	typedef list<const DCLeaseManagerLease *>	ConstList;

	const ConstList *const_list = (const ConstList *) &non_const_list;
	return *(const_cast<ConstList *>(const_list));
}

int
DCLeaseManagerLease_fwriteList(
	const list<const DCLeaseManagerLease *>	&lease_list,
	FILE									*fp
	)
{
	int		count = 0;
	list<const DCLeaseManagerLease *>::const_iterator iter;
	for( iter  = lease_list.begin();
		 iter != lease_list.end();
		 iter ++ ) {
		const DCLeaseManagerLease	*lease = *iter;
		if ( ! lease->fwrite( fp ) ) {
			break;
		}
		count++;
	}
	return count;
}

int
DCLeaseManagerLease_freadList(
	list<DCLeaseManagerLease *>		&lease_list,
	FILE							*fp
	)
{
	int		count = 0;
	while( 1 ) {
		DCLeaseManagerLease	*lease = new DCLeaseManagerLease( );
		if ( lease->fread( fp ) ) {
			lease_list.push_back( lease );
			count++;
		}
		else {
			delete lease;
			break;
		}
	}
	return count;
}
