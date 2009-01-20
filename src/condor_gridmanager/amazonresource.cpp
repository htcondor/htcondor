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

#include "amazonresource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE	500

HashTable <HashKey, AmazonResource *> 
	AmazonResource::ResourcesByName( HASH_TABLE_SIZE, hashFunction );

const char * AmazonResource::HashName( const char * resource_name,
		const char * public_key_file, const char * private_key_file )
{								 
	static MyString hash_name;
	hash_name.sprintf( "%s#%s#%s", resource_name, public_key_file, private_key_file );
	return hash_name.Value();
}


AmazonResource* AmazonResource::FindOrCreateResource(const char * resource_name, 
	const char * public_key_file, const char * private_key_file )
{
	int rc;
	MyString resource_key;
	AmazonResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name, public_key_file, private_key_file ) ), resource );
	if ( rc != 0 ) {
		resource = new AmazonResource( resource_name, public_key_file, private_key_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName.insert( HashKey( HashName( resource_name, public_key_file, private_key_file ) ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}


AmazonResource::AmazonResource( const char *resource_name, 
	const char * public_key_file, const char * private_key_file )
	: BaseResource( resource_name )
{
	// although no one will use resource_name, we still keep it for base class constructor
	
	m_public_key_file = strdup(public_key_file);
	m_private_key_file = strdup(private_key_file);
	
	gahp = NULL;

	MyString buff;
	buff.sprintf( AMAZON_RESOURCE_NAME );
	
	char * gahp_path = param( "AMAZON_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "AMAZON_GAHP not defined! \n");
		return;
	}
	
	ArgList args;
	args.AppendArg("-f");

	gahp = new GahpClient( buff.Value(), gahp_path, &args );
	free(gahp_path);
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( AmazonJob::gahpCallTimeout );
}


AmazonResource::~AmazonResource()
{
	ResourcesByName.remove( HashKey( HashName( resourceName, m_public_key_file,
											   m_private_key_file ) ) );
	if ( gahp ) delete gahp;
	if (m_public_key_file) free(m_public_key_file);
	if (m_private_key_file) free(m_private_key_file);
}


void AmazonResource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( AmazonJob::gahpCallTimeout );
}

const char *AmazonResource::ResourceType()
{
	return "amazon";
}

const char *AmazonResource::GetHashName()
{
	return HashName( resourceName, m_public_key_file, m_private_key_file );
}

void AmazonResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_AMAZON_PUBLIC_KEY, m_public_key_file );
	resource_ad->Assign( ATTR_AMAZON_PRIVATE_KEY, m_private_key_file );
}

// we will use amazon command "status_all" to do the Ping work
void AmazonResource::DoPing( time_t& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	// Since Amazon doesn't use proxy, we should use Startup() to replace isInitialized()
	if ( gahp->Startup() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;		
		return;
	}
	
	ping_delay = 0;
	
	int rc = gahp->amazon_ping( m_public_key_file, m_private_key_file );

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
