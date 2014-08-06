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

#include "gceresource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE	13

HashTable <HashKey, GCEResource *> 
	GCEResource::ResourcesByName( HASH_TABLE_SIZE, hashFunction );

const char * GCEResource::HashName( const char *resource_name,
									const char *project,
									const char *zone,
									const char *auth_file )
{
	static std::string hash_name;
	formatstr( hash_name, "gce %s %s %s#%s", resource_name, project, zone, auth_file );
	return hash_name.c_str();
}


GCEResource* GCEResource::FindOrCreateResource( const char *resource_name,
												const char *project,
												const char *zone,
												const char *auth_file )
{
	int rc;
	GCEResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name, project, zone, auth_file ) ), resource );
	if ( rc != 0 ) {
		resource = new GCEResource( resource_name, project, zone, auth_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName.insert( HashKey( HashName( resource_name, project, zone, auth_file ) ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}


GCEResource::GCEResource( const char *resource_name,
						  const char *project,
						  const char *zone,
						  const char *auth_file ) :
		BaseResource( resource_name ),
		jobsByInstanceID( hashFunction ),
		m_hadAuthFailure( false )
{
	// although no one will use resource_name, we still keep it for base class constructor

	m_auth_file = strdup(auth_file);
	m_project = strdup(project);
	m_zone = strdup(zone);

	gahp = NULL;

	char * gahp_path = param( "GCE_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "GCE_GAHP not defined! \n");
		return;
	}

	ArgList args;
	args.AppendArg("-f");

	gahp = new GahpClient( GCE_RESOURCE_NAME, gahp_path, &args );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( GCEJob::gahpCallTimeout );

	status_gahp = new GahpClient( GCE_RESOURCE_NAME, gahp_path, &args );

	StartBatchStatusTimer();

	status_gahp->setNotificationTimerId( BatchPollTid() );
	status_gahp->setMode( GahpClient::normal );
	status_gahp->setTimeout( GCEJob::gahpCallTimeout );

	free(gahp_path);
}


GCEResource::~GCEResource()
{
	ResourcesByName.remove( HashKey( HashName( resourceName, m_project, m_zone, m_auth_file ) ) );
	delete gahp;
	free( m_auth_file );
	free( m_project );
	free( m_zone );
}


void GCEResource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( GCEJob::gahpCallTimeout );
}

const char *GCEResource::ResourceType()
{
	return "gce";
}

const char *GCEResource::GetHashName()
{
	return HashName( resourceName, m_project, m_zone, m_auth_file );
}

void GCEResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_GCE_AUTH_FILE, m_auth_file );
	resource_ad->Assign( ATTR_GCE_PROJECT, m_project );
	resource_ad->Assign( ATTR_GCE_ZONE, m_zone );
}

void GCEResource::DoPing( unsigned& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	// Since GCE doesn't use proxy, we should use Startup() to replace isInitialized()
	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}

	ping_delay = 0;

	std::string error_code;
	int rc = gahp->gce_ping( resourceName, m_auth_file, m_project, m_zone );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc != 0 ) {
		ping_complete = true;

		// If the service returns an authorization failure, that means
		// the service is up, so return true.  Individual jobs with
		// invalid authentication tokens will then go on hold, which is
		// what we want (rather than saying idle).
		if( error_code != "" ) {
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

GCEResource::BatchStatusResult GCEResource::StartBatchStatus() {
	ASSERT( status_gahp );

	StringList instance_id_list;
	StringList instance_name_list;
	StringList instance_status_list;
	StringList instance_status_msg_list;
	int rc = status_gahp->gce_instance_list( resourceName, m_auth_file,
											 m_project, m_zone,
											 instance_id_list,
											 instance_name_list,
											 instance_status_list,
											 instance_status_msg_list );

	if( rc == GAHPCLIENT_COMMAND_PENDING ) { return BSR_PENDING; }

	if( rc != 0 ) {
		dprintf( D_ALWAYS, "Error doing batched GCE status query: %s\n",
				 status_gahp->getErrorString() );
		return BSR_ERROR;
	}

	//
	// We have to let a job know if we can't find a status report for it.
	//
	List<GCEJob> myJobs;
	GCEJob * nextJob = NULL;
	BaseJob *nextBaseJob = NULL;
	registeredJobs.Rewind();
	while ( (nextBaseJob = registeredJobs.Next()) ) {
		nextJob = dynamic_cast< GCEJob * >( nextBaseJob );
		ASSERT( nextJob );
		if ( !nextJob->m_instanceName.empty() ) {
			myJobs.Append( nextJob );
		}
	}

	int job_cnt = instance_id_list.number();
	instance_id_list.rewind();
	instance_name_list.rewind();
	instance_status_list.rewind();
	instance_status_msg_list.rewind();
	ASSERT( instance_name_list.number() == job_cnt );
	ASSERT( instance_status_list.number() == job_cnt );
	ASSERT( instance_status_msg_list.number() == job_cnt );
	for( int i = 0; i < job_cnt; i++ ) {
		std::string instance_id = instance_id_list.next();
		std::string instance_name = instance_name_list.next();
		std::string instance_status = instance_status_list.next();
		std::string instance_status_msg = instance_status_msg_list.next();

		// First, lookup the job using both instace_id and instance_name
		std::string remote_job_id;
		formatstr( remote_job_id, "gce %s %s %s", resourceName,
				   instance_name.c_str(), instance_id.c_str() );

		BaseJob * tmp = NULL;
		rc = BaseJob::JobsByRemoteId.lookup( HashKey( remote_job_id.c_str() ), tmp );

		if( rc == 0 ) {
			ASSERT( tmp );
			GCEJob * job = dynamic_cast< GCEJob * >( tmp );
			if( job == NULL ) {
				EXCEPT( "Found non-GCEJob identified by '%s'.",
						remote_job_id.c_str() );
			}

			dprintf( D_FULLDEBUG, "Found job object via instance id for '%s', updating status ('%s').\n", instance_id.c_str(), instance_status.c_str() );
			// TODO Can we get public DNS name?
			job->StatusUpdate( instance_id.c_str(), instance_status.c_str(),
							   instance_status_msg.c_str(), NULL );
			myJobs.Delete( job );
			continue;
		}

		// We may not know the job's instance_id. Look it up using just the
		// instance_name
		formatstr( remote_job_id, "gce %s %s", resourceName,
				   instance_name.c_str() );

		tmp = NULL;
		rc = BaseJob::JobsByRemoteId.lookup( HashKey( remote_job_id.c_str() ), tmp );

		if( rc == 0 ) {
			ASSERT( tmp );
			GCEJob * job = dynamic_cast< GCEJob * >( tmp );
			if( job == NULL ) {
				EXCEPT( "Found non-GCEJob identified by '%s'.",
						remote_job_id.c_str() );
			}

			dprintf( D_FULLDEBUG, "Found job object via instance name for '%s', updating status ('%s').\n", instance_name.c_str(), instance_status.c_str() );
			// TODO Can we get public DNS name?
			job->StatusUpdate( instance_id.c_str(), instance_status.c_str(),
							   instance_status_msg.c_str(), NULL );
			myJobs.Delete( job );
			continue;
		}

		dprintf( D_FULLDEBUG, "Found unknown instance id='%s' name='%s'; skipping.\n",
				 instance_id.c_str(), instance_name.c_str() );
		continue;
	}

	myJobs.Rewind();
	while( ( nextJob = myJobs.Next() ) ) {
		dprintf( D_FULLDEBUG, "Informing job %p it got no status.\n", nextJob );
		nextJob->StatusUpdate( NULL, NULL, NULL, NULL );
	}

	return BSR_DONE;

	// This should never happen (but the compiler hates you).
	return BSR_ERROR;
}

GCEResource::BatchStatusResult GCEResource::FinishBatchStatus() {
	return StartBatchStatus();
}
