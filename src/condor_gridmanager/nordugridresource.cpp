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



#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "nordugridresource.h"
#include "nordugridjob.h"
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
		resource->Reconfig();
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
	ResourcesByName.remove( HashKey( HashName( resourceName, proxySubject ) ) );
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

const char *NordugridResource::ResourceType()
{
	return "nordugrid";
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
