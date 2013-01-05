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

#include "ec2resource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE	500

HashTable <HashKey, EC2Resource *> 
	EC2Resource::ResourcesByName( HASH_TABLE_SIZE, hashFunction );

const char * EC2Resource::HashName( const char * resource_name,
		const char * public_key_file, const char * private_key_file )
{								 
	static std::string hash_name;
	formatstr( hash_name, "ec2 %s#%s#%s", resource_name, public_key_file, private_key_file );
	return hash_name.c_str();
}


EC2Resource* EC2Resource::FindOrCreateResource(const char * resource_name, 
	const char * public_key_file, const char * private_key_file )
{
	int rc;
	EC2Resource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name, public_key_file, private_key_file ) ), resource );
	if ( rc != 0 ) {
		resource = new EC2Resource( resource_name, public_key_file, private_key_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName.insert( HashKey( HashName( resource_name, public_key_file, private_key_file ) ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}


EC2Resource::EC2Resource( const char *resource_name, 
	const char * public_key_file, const char * private_key_file )
	: BaseResource( resource_name )
{
	// although no one will use resource_name, we still keep it for base class constructor
	
	m_public_key_file = strdup(public_key_file);
	m_private_key_file = strdup(private_key_file);
	
	gahp = NULL;

	char * gahp_path = param( "EC2_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "EC2_GAHP not defined! \n");
		return;
	}
	
	ArgList args;
	args.AppendArg("-f");

	gahp = new GahpClient( EC2_RESOURCE_NAME, gahp_path, &args );
	free(gahp_path);
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( EC2Job::gahpCallTimeout );
	
	m_hadAuthFailure = false;
}


EC2Resource::~EC2Resource()
{
	ResourcesByName.remove( HashKey( HashName( resourceName, m_public_key_file,
											   m_private_key_file ) ) );
	if ( gahp ) delete gahp;
	if (m_public_key_file) free(m_public_key_file);
	if (m_private_key_file) free(m_private_key_file);
}


void EC2Resource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( EC2Job::gahpCallTimeout );
}

const char *EC2Resource::ResourceType()
{
	return "ec2";
}

const char *EC2Resource::GetHashName()
{
	return HashName( resourceName, m_public_key_file, m_private_key_file );
}

void EC2Resource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_EC2_ACCESS_KEY_ID, m_public_key_file );
	resource_ad->Assign( ATTR_EC2_SECRET_ACCESS_KEY, m_private_key_file );
}

// we will use ec2 command "status_all" to do the Ping work
void EC2Resource::DoPing( time_t& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	// Since EC2 doesn't use proxy, we should use Startup() to replace isInitialized()
	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	
	ping_delay = 0;

	char * error_code = NULL;	
	int rc = gahp->ec2_ping( resourceName, m_public_key_file, m_private_key_file, error_code );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} 
	else if ( rc != 0 ) {
		ping_complete = true;
		
		// If the service returns an authorization failure, that means
		// the service is up, so return true.  Individual jobs with
		// invalid authentication tokens will then go on hold, which is
		// what we want (rather than saying idle).
		if( error_code != NULL ) {
			if( strstr( error_code, "(401)" ) != NULL ) {
				ping_succeeded = true;
				m_hadAuthFailure = true;
				formatstr( authFailureMessage, "(%s): '%s'", error_code, gahp->getErrorString() );
			}    		    
			free( error_code );
		} else {
		    ping_succeeded = false;
		}
	} 
	else {
		ping_complete = true;
		ping_succeeded = true;
	}

	return;
}
