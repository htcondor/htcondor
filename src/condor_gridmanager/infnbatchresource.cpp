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

std::map <std::string, INFNBatchResource *>
    INFNBatchResource::ResourcesByName;

std::string & INFNBatchResource::HashName( const char * batch_type,
		const char * gahp_args )
{
	static std::string hash_name;
	formatstr( hash_name, "batch %s", batch_type );
	if ( gahp_args && gahp_args[0] ) {
		formatstr_cat( hash_name, " %s", gahp_args );
	}
	return hash_name;
}


INFNBatchResource* INFNBatchResource::FindOrCreateResource(const char * batch_type, 
	const char * resource_name,
	const char * gahp_args )
{
	INFNBatchResource *resource = NULL;

	if ( resource_name == NULL ) {
		resource_name = "";
	}
    if ( gahp_args == NULL ) {
		gahp_args = "";
    }

	std::string &key = HashName( batch_type, gahp_args );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new INFNBatchResource( batch_type, resource_name, gahp_args );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName[key] = resource;
	} else {
		resource = itr->second;
		ASSERT(resource);
	}

	return resource;
}


INFNBatchResource::INFNBatchResource( const char *batch_type, 
	const char *resource_name, const char *gahp_args )
	: BaseResource( resource_name ),
	  m_xfer_gahp( NULL ),
	  m_gahpCanRefreshProxy( false ),
	  m_gahpRefreshProxyChecked( false )
{
	m_batchType = batch_type;
	m_gahpArgs = gahp_args;
	m_gahpIsRemote = false;

	m_remoteHostname = resource_name;
	size_t pos = m_remoteHostname.find( '@' );
	if ( pos != m_remoteHostname.npos ) {
		m_remoteHostname.erase( 0, pos + 1 );
	}
	
	gahp = NULL;

	std::string gahp_name = batch_type;
	if ( *gahp_args ) {
		formatstr_cat( gahp_name, "/%s", gahp_args );
		m_gahpIsRemote = true;
	}

	gahp = new GahpClient( gahp_name.c_str() );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( INFNBatchJob::gahpCallTimeout );

	if ( m_gahpIsRemote ) {
		gahp_name.insert( 0, "xfer/" );
		m_xfer_gahp = new GahpClient( gahp_name.c_str() );
		m_xfer_gahp->setNotificationTimerId( pingTimerId );
		m_xfer_gahp->setMode( GahpClient::normal );
		m_xfer_gahp->setTimeout( INFNBatchJob::gahpCallTimeout );
	} else {
		m_gahpCanRefreshProxy = true;
		m_gahpRefreshProxyChecked = true;
	}
}


INFNBatchResource::~INFNBatchResource()
{
	ResourcesByName.erase( HashName( m_batchType.c_str(), m_gahpArgs.c_str() ) );
	if ( gahp ) delete gahp;
	delete m_xfer_gahp;
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
	return HashName( m_batchType.c_str(), m_gahpArgs.c_str() ).c_str();
}

void INFNBatchResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	gahp->PublishStats( resource_ad );
}

bool INFNBatchResource::GahpCanRefreshProxy()
{
	if ( !m_gahpRefreshProxyChecked && m_xfer_gahp->isStarted() ) {
		m_gahpCanRefreshProxy = m_xfer_gahp->getCommands()->contains_anycase( "DOWNLOAD_PROXY" );
		m_gahpRefreshProxyChecked = true;
	}
	return m_gahpCanRefreshProxy;
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
/* comment out to calm coverity
	int rc = 0;

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} 
	else if ( rc != 0 ) {
		ping_complete = true;
		ping_succeeded = false;
	} 
	else {
*/
		ping_complete = true;
		ping_succeeded = true;
//	}

	return;
}
