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

#include "dcloudresource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE	500

HashTable <HashKey, DCloudResource *>
	DCloudResource::ResourcesByName( HASH_TABLE_SIZE, hashFunction );

const char * DCloudResource::HashName( const char *resource_name,
									   const char *username,
									   const char *password )
{
	static MyString hash_name;
	hash_name.formatstr( "%s#%s#%s", resource_name, username, password );
	return hash_name.Value();
}


DCloudResource* DCloudResource::FindOrCreateResource(const char *resource_name,
													 const char *username,
													 const char *password )
{
	int rc;
	MyString resource_key;
	DCloudResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name, username, password ) ), resource );
	if ( rc != 0 ) {
		resource = new DCloudResource( resource_name, username, password );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName.insert( HashKey( HashName( resource_name, username, password ) ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}


DCloudResource::DCloudResource( const char *resource_name,
								const char *username, const char *password )
	: BaseResource( resource_name )
{
	// although no one will use resource_name, we still keep it for base class constructor

	m_username = strdup( username );
	m_password = strdup( password );

	gahp = NULL;
	status_gahp = NULL;

	char * gahp_path = param( "DELTACLOUD_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "DELTACLOUD_GAHP not defined! \n");
		return;
	}

	gahp = new GahpClient( DCLOUD_RESOURCE_NAME, gahp_path );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( DCloudJob::gahpCallTimeout );

	status_gahp = new GahpClient( DCLOUD_RESOURCE_NAME, gahp_path );

	StartBatchStatusTimer();

	status_gahp->setNotificationTimerId( BatchPollTid() );
	status_gahp->setMode( GahpClient::normal );
	status_gahp->setTimeout( DCloudJob::gahpCallTimeout );

	free(gahp_path);
}


DCloudResource::~DCloudResource()
{
	ResourcesByName.remove( HashKey( HashName( resourceName, m_username,
											   m_password ) ) );
	delete gahp;
	free( m_username );
	free( m_password );
}


void DCloudResource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( DCloudJob::gahpCallTimeout );
}

const char *DCloudResource::ResourceType()
{
	return "deltacloud";
}

const char *DCloudResource::GetHashName()
{
	return HashName( resourceName, m_username, m_password );
}

void DCloudResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( "DeltacloudUserName", m_username );
}

// we will use deltacloud command "status_all" to do the Ping work
void DCloudResource::DoPing( unsigned& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}

	ping_delay = 0;

	StringList ids;
	StringList statuses;
	int rc = gahp->dcloud_status_all( resourceName, m_username, m_password,
									  ids, statuses );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc != 0 ) {
		ping_complete = true;
		ping_succeeded = false;
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}

	return;
}

DCloudResource::BatchStatusResult DCloudResource::StartBatchStatus()
{
	ASSERT(status_gahp);

	StringList instance_ids;
	StringList statuses;
	const char *instance_id;
	const char *status;

	int rc = status_gahp->dcloud_status_all( ResourceName(), m_username,
											 m_password, instance_ids,
											 statuses );
	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		return BSR_PENDING;
	}
	if ( rc != 0 ) {
		dprintf( D_ALWAYS, "Error attempting a Deltacloud batch status query: %s\n", status_gahp->getErrorString() );
		return BSR_ERROR;
	}

	DCloudJob *next_job;
	List<DCloudJob> my_jobs;
	registeredJobs.Rewind();
	while ( (next_job = (DCloudJob *)registeredJobs.Next()) ) {
		my_jobs.Insert( next_job );
	}

	instance_ids.rewind();
	statuses.rewind();
	while ( (instance_id = instance_ids.next()) &&
			(status = statuses.next()) ) {

		MyString hashname;
		hashname.formatstr( "%s#%s", ResourceName(), instance_id );
		DCloudJob *job = NULL;

		// TODO We can get rid of the hashtable.
		rc = DCloudJob::JobsByInstanceId.lookup(
										HashKey( hashname.Value() ), job );
		if ( rc != 0 ) {
			// Job not found. Probably okay; we might see jobs
			// submitted via other means, or jobs we've abandoned.
			dprintf( D_FULLDEBUG, "Job %s on remote host is unknown. Skipping.\n", hashname.Value() );
			continue;
		}
		ASSERT( job );

		job->StatusUpdate( status );

		my_jobs.Delete( job );
	}

	my_jobs.Rewind();
	while ( (next_job = my_jobs.Next()) ) {
		next_job->StatusUpdate( NULL );
	}

	return BSR_DONE;
}

DCloudResource::BatchStatusResult DCloudResource::FinishBatchStatus()
{
	// As it happens, we can use the same code path
	// for starting and finishing a batch status for
	// DCloud jobs.  Indeed, doing so is simpler.
	return StartBatchStatus();
}

GahpClient *DCloudResource::BatchGahp() { return status_gahp; }
