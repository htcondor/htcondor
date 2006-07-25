/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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


#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "nordugridresource.h"
#include "gridmanager.h"

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

	MyString buff;
	buff.sprintf( "NORDUGRID/%s", proxySubject );

	gahp = new GahpClient( buff.Value() );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( NordugridJob::gahpCallTimeout );
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
