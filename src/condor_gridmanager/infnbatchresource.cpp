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

#include "infnbatchresource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE	500

HashTable <HashKey, INFNBatchResource *> 
	INFNBatchResource::ResourcesByName( HASH_TABLE_SIZE, hashFunction );

const char * INFNBatchResource::HashName( const char * batch_type,
		const char * resource_name )
{
	static std::string hash_name;
	formatstr( hash_name, "batch %s", batch_type );
	if ( resource_name && resource_name[0] ) {
		formatstr_cat( hash_name, " %s", resource_name );
	}
	return hash_name.c_str();
}


INFNBatchResource* INFNBatchResource::FindOrCreateResource(const char * batch_type, 
	const char * resource_name )
{
	int rc;
	INFNBatchResource *resource = NULL;

	if ( resource_name == NULL ) {
		resource_name = "";
	}

	rc = ResourcesByName.lookup( HashKey( HashName( batch_type, resource_name ) ), resource );
	if ( rc != 0 ) {
		resource = new INFNBatchResource( batch_type, resource_name );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName.insert( HashKey( HashName( batch_type, resource_name ) ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}


INFNBatchResource::INFNBatchResource( const char *batch_type, 
	const char * resource_name )
	: BaseResource( resource_name )
{
	m_batchType = batch_type;
	m_gahpIsRemote = false;

	m_remoteHostname = resource_name;
	size_t pos = m_remoteHostname.find( '@' );
	if ( pos != m_remoteHostname.npos ) {
		m_remoteHostname.erase( 0, pos + 1 );
	}
	
	gahp = NULL;

	std::string gahp_name = batch_type;
	if ( resource_name && *resource_name ) {
		formatstr_cat( gahp_name, "/%s", resource_name );
		m_gahpIsRemote = true;
	}

	gahp = new GahpClient( gahp_name.c_str() );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( INFNBatchJob::gahpCallTimeout );
}


INFNBatchResource::~INFNBatchResource()
{
	ResourcesByName.remove( HashKey( HashName( m_batchType.c_str(), resourceName ) ) );
	if ( gahp ) delete gahp;
}


void INFNBatchResource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( INFNBatchJob::gahpCallTimeout );
}

const char *INFNBatchResource::ResourceType()
{
	return "batch";
}

const char *INFNBatchResource::GetHashName()
{
	return HashName( m_batchType.c_str(), resourceName );
}

void INFNBatchResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );
}

// we will use ec2 command "status_all" to do the Ping work
void INFNBatchResource::DoPing( unsigned& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	// Since blah doesn't need a proxy, we should use Startup() to replace isInitialized()
	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	
	ping_delay = 0;
	
		// TODO Is there a meaning ping command we can perform?
	int rc = 0;

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} 
	else if ( rc != 0 ) {
		ping_complete = true;
		ping_succeeded = false;
	} 
	else {
		ping_complete = true;
		ping_succeeded = true;
	}

	return;
}
