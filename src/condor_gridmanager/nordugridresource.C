/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "nordugridresource.h"
#include "gridmanager.h"

template class List<NordugridJob>;
template class Item<NordugridJob>;
template class HashTable<HashKey, NordugridResource *>;
template class HashBucket<HashKey, NordugridResource *>;

#define HASH_TABLE_SIZE			500

HashTable <HashKey, NordugridResource *>
    NordugridResource::ResourcesByName( HASH_TABLE_SIZE,
										hashFunction );

const char *NordugridResource::HashName( const char *resource_name,
										 const char *proxy_subject )
{
	static MyString hash_name;

	hash_name.sprintf( "%s#%s", resource_name, 
					   proxy_subject ? proxy_subject : "NULL" );

	return hash_name.Value();
}

NordugridResource *NordugridResource::FindOrCreateResource( const char * resource_name,
															const char *proxy_subject )
{
	int rc;
	MyString resource_key;
	NordugridResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name,
													proxy_subject ) ),
								 resource );
	if ( rc != 0 ) {
		resource = new NordugridResource( resource_name, proxy_subject );
		ASSERT(resource);
		ResourcesByName.insert( HashKey( HashName( resource_name,
												   proxy_subject ) ),
								resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}

NordugridResource::NordugridResource( const char *resource_name,
									  const char *proxy_subject )
	: BaseResource( resource_name )
{
	proxySubject = strdup( proxy_subject );

	gahp = NULL;

	char *gahp_path = param("NORDUGRID_GAHP");
	if ( gahp_path == NULL ) {
		EXCEPT( "NORDUGRID_GAHP not defined in condor config file" );
	} else {
		MyString buff;
		buff.sprintf( "NORDUGRID/%s", proxySubject );

		gahp = new GahpClient( buff.Value(), gahp_path );
		gahp->setNotificationTimerId( pingTimerId );
		gahp->setMode( GahpClient::normal );
		gahp->setTimeout( NordugridJob::gahpCallTimeout );

		free( gahp_path );
	}
}

NordugridResource::~NordugridResource()
{
	if ( proxySubject ) {
		free( proxySubject );
	}
	if ( gahp ) {
		delete gahp;
	}
}

void NordugridResource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( NordugridJob::gahpCallTimeout );
}

void NordugridResource::DoPing( time_t& ping_delay, bool& ping_complete,
								bool& ping_succeeded )
{
	int rc;

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	gahp->setNormalProxy( gahp->getMasterProxy() );
	if ( PROXY_IS_EXPIRED( gahp->getMasterProxy() ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		ping_delay = TIMER_NEVER;
		return;
	}

	ping_delay = 0;

	rc = gahp->nordugrid_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc != 0 ) {
		ping_complete = true;
		ping_succeeded = false;
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}
}
