/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
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

#include "azureresource.h"
#include "gridmanager.h"

std::map <std::string, AzureResource *>
    AzureResource::ResourcesByName;

std::string & AzureResource::HashName( const char *resource_name,
                                     const char *subscription,
                                     const char *auth_file )
{
	static std::string hash_name;
	formatstr( hash_name, "azure %s %s#%s", resource_name, subscription, auth_file );
	return hash_name;
}


AzureResource* AzureResource::FindOrCreateResource( const char *resource_name,
                                                    const char *subscription,
                                                    const char *auth_file )
{
	AzureResource *resource = NULL;
	std::string &key = HashName( resource_name, subscription, auth_file );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new AzureResource( resource_name, subscription, auth_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName[key] = resource;
	} else {
		resource = itr->second;
		ASSERT(resource);
	}

	return resource;
}


AzureResource::AzureResource( const char *resource_name,
                              const char *subscription,
                              const char *auth_file ) :
	BaseResource( resource_name ),
	m_hadAuthFailure( false )
{
	// although no one will use resource_name, we still keep it for base class constructor

	m_auth_file = strdup(auth_file);
	m_subscription = strdup(subscription);

	status_gahp = gahp = NULL;

	
	char * gahp_path = param( "AZURE_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "AZURE_GAHP not defined! \n");
		return;
	}

	gahp = new GahpClient( AZURE_RESOURCE_NAME, gahp_path );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( AzureJob::gahpCallTimeout );

	status_gahp = new GahpClient( AZURE_RESOURCE_NAME, gahp_path );

	StartBatchStatusTimer();

	status_gahp->setNotificationTimerId( BatchPollTid() );
	status_gahp->setMode( GahpClient::normal );
	status_gahp->setTimeout( AzureJob::gahpCallTimeout );

	free(gahp_path);
}


AzureResource::~AzureResource()
{
	ResourcesByName.erase( HashName( resourceName, m_subscription, m_auth_file ) );
	delete gahp;
	free( m_auth_file );
	free( m_subscription );
}


void AzureResource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( AzureJob::gahpCallTimeout );
}

const char *AzureResource::ResourceType()
{
	return "azure";
}

const char *AzureResource::GetHashName()
{
	return HashName( resourceName, m_subscription, m_auth_file ).c_str();
}

void AzureResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_AZURE_AUTH_FILE, m_auth_file );
	resource_ad->Assign( ATTR_AZURE_SUBSCRIPTION, m_subscription );

	gahp->PublishStats( resource_ad );
}

void AzureResource::DoPing( unsigned& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}

	ping_delay = 0;

	std::string error_code;
	int rc = gahp->azure_ping( m_auth_file, m_subscription );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc != 0 ) {
		ping_complete = true;

		// If the service returns an authorization failure, that means
		// the service is up, so return true.  Individual jobs with
		// invalid authentication tokens will then go on hold, which is
		// what we want (rather than saying idle).
		if( error_code != "" ) {
			// TODO JEF Update this parsing of error string
			if( strstr( error_code.c_str(), "(401)" ) != NULL ) {
				ping_succeeded = true;
				m_hadAuthFailure = true;
				formatstr( authFailureMessage, "(%s): '%s'", error_code.c_str(), gahp->getErrorString() );
			}
		} else {
			ping_succeeded = false;
		}
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}

	return;
}

AzureResource::BatchStatusResult AzureResource::StartBatchStatus() {
	ASSERT( status_gahp );

	StringList vm_name_list;
	StringList vm_status_list;
	int rc = status_gahp->azure_vm_list( m_auth_file,
	                                     m_subscription,
	                                     vm_name_list,
	                                     vm_status_list );

	if( rc == GAHPCLIENT_COMMAND_PENDING ) { return BSR_PENDING; }

	if( rc != 0 ) {
		dprintf( D_ALWAYS, "Error doing batched Azure status query: %s\n",
		         status_gahp->getErrorString() );
		return BSR_ERROR;
	}

	//
	// We have to let a job know if we can't find a status report for it.
	//
	List<AzureJob> myJobs;
	AzureJob * nextJob = NULL;
	BaseJob *nextBaseJob = NULL;
	registeredJobs.Rewind();
	while ( (nextBaseJob = registeredJobs.Next()) ) {
		nextJob = dynamic_cast< AzureJob * >( nextBaseJob );
		ASSERT( nextJob );
		if ( !nextJob->m_vmName.empty() ) {
			myJobs.Append( nextJob );
		}
	}

	int job_cnt = vm_name_list.number();
	vm_name_list.rewind();
	vm_status_list.rewind();
	ASSERT( vm_status_list.number() == job_cnt );
	for( int i = 0; i < job_cnt; i++ ) {
		std::string vm_name = vm_name_list.next();
		std::string vm_status = vm_status_list.next();

		// Resource names are case-insenstive. We generate lower-case
		// names, but the gahp returns upper-case names.
		lower_case( vm_name );

		// First, lookup the job using vm_name
		std::string remote_job_id;
		formatstr( remote_job_id, "azure %s", vm_name.c_str() );

		BaseJob * tmp = NULL;
		rc = BaseJob::JobsByRemoteId.lookup( remote_job_id, tmp );

		if( rc == 0 ) {
			ASSERT( tmp );
			AzureJob * job = dynamic_cast< AzureJob * >( tmp );
			if( job == NULL ) {
				EXCEPT( "Found non-AzureJob identified by '%s'.",
				        remote_job_id.c_str() );
			}

			dprintf( D_FULLDEBUG, "Found job object via vm name for '%s', updating status ('%s').\n", vm_name.c_str(), vm_status.c_str() );
			// TODO Can we get public DNS name?
			job->StatusUpdate( vm_status.c_str() );
			myJobs.Delete( job );
		} else {
			dprintf( D_FULLDEBUG, "Found unknown vm name='%s'; skipping.\n",
			         vm_name.c_str() );
		}
	}

	myJobs.Rewind();
	while( ( nextJob = myJobs.Next() ) ) {
		dprintf( D_FULLDEBUG, "Informing job %p it got no status.\n", nextJob );
		nextJob->StatusUpdate( NULL );
	}

	return BSR_DONE;

	// This should never happen (but the compiler hates you).
	return BSR_ERROR;
}

AzureResource::BatchStatusResult AzureResource::FinishBatchStatus() {
	return StartBatchStatus();
}
