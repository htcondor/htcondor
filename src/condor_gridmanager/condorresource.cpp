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

#include "condorresource.h"
#include "condorjob.h"
#include "gridmanager.h"

std::map <std::string, CondorResource *>
    CondorResource::ResourcesByName;

HashTable <std::string, CondorResource::ScheddPollInfo *>
    CondorResource::PollInfoByName( hashFunction );

std::string & CondorResource::HashName( const char *resource_name,
                                      const char *pool_name,
                                      const char *proxy_subject,
                                      const std::string &scitokens_file )
{
	static std::string hash_name;

	formatstr( hash_name, "condor %s %s#%s#%s", resource_name,
	           pool_name ? pool_name : "NULL",
	           proxy_subject ? proxy_subject : "NULL",
	           scitokens_file.c_str() );

	return hash_name;
}

CondorResource *CondorResource::FindOrCreateResource( const char * resource_name,
                                                      const char *pool_name,
                                                      const Proxy *proxy,
                                                      const std::string &scitokens_file )
{
	CondorResource *resource = NULL;
	std::string &key = HashName( resource_name, pool_name,
	                             proxy ? proxy->subject->fqan : NULL,
	                             scitokens_file );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new CondorResource( resource_name, pool_name,
		                               proxy, scitokens_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName[key] = resource;
	} else {
		resource = itr->second;
		ASSERT(resource);
	}

	return resource;
}

CondorResource::CondorResource( const char *resource_name, const char *pool_name,
                                const Proxy *proxy, const std::string &scitokens_file )
	: BaseResource( resource_name )
{
	hasLeases = true;

	if ( proxy != NULL ) {
		proxySubject = strdup( proxy->subject->subject_name );
		proxyFQAN = strdup( proxy->subject->fqan );
	} else {
		proxySubject = NULL;
		proxyFQAN = NULL;
	}
	scheddPollTid = TIMER_UNSET;
	scheddName = strdup( resource_name );
	gahp = NULL;
	ping_gahp = NULL;
	scheddStatusActive = false;
	submitter_constraint = "";
	m_scitokensFile = scitokens_file;

	if ( pool_name != NULL ) {
		poolName = strdup( pool_name );
	} else {
		poolName = NULL;
	}

	scheddPollTid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&CondorResource::DoScheddPoll,
							"CondorResource::DoScheddPoll", (Service*)this );

	char *gahp_path = param("CONDOR_GAHP");
	if ( gahp_path == NULL ) {
		EXCEPT( "CONDOR_GAHP not defined in condor config file" );
	} else {
		// TODO remove scheddName from the gahp server key if/when
		//   a gahp server can handle multiple schedds
		std::string buff;
		ArgList args;
		formatstr( buff, "CONDOR/%s/%s/%s#%s", poolName ? poolName : "NULL",
		           scheddName, proxyFQAN ? proxyFQAN : "NULL",
		           m_scitokensFile.c_str() );
		args.AppendArg("-f");
		args.AppendArg("-s");
		args.AppendArg(scheddName);
		if ( poolName != NULL ) {
			args.AppendArg("-P");
			args.AppendArg(poolName);
		}

		gahp = new GahpClient( buff.c_str(), gahp_path, &args );
		gahp->setNotificationTimerId( scheddPollTid );
		gahp->setMode( GahpClient::normal );
		gahp->setTimeout( CondorJob::gahpCallTimeout );

		ping_gahp = new GahpClient( buff.c_str(), gahp_path, &args );
		ping_gahp->setNotificationTimerId( pingTimerId );
		ping_gahp->setMode( GahpClient::normal );
		ping_gahp->setTimeout( CondorJob::gahpCallTimeout );

		lease_gahp = new GahpClient( buff.c_str(), gahp_path, &args );
		lease_gahp->setNotificationTimerId( updateLeasesTimerId );
		lease_gahp->setMode( GahpClient::normal );
		lease_gahp->setTimeout( CondorJob::gahpCallTimeout );

		free( gahp_path );
	}
}

CondorResource::~CondorResource()
{
	ResourcesByName.erase( HashName( resourceName, poolName, proxyFQAN, m_scitokensFile ) );

		// Make sure we don't leak a ScheddPollInfo. If there are other
		// CondorResources that still want to use it, they'll recreate it.
		// Don't delete it if we know another CondorResource is doing a
		// poll of the remote schedd right now.
		// TODO Track how many CondorResources are still using this
		//   ScheddPollInfo and delete it only if we're the last one.
	ScheddPollInfo *poll_info = NULL;
	PollInfoByName.lookup( HashName( scheddName, poolName, NULL, "" ), poll_info );
	if ( poll_info && ( poll_info->m_pollActive == false ||
		 scheddStatusActive == true ) ) {
		PollInfoByName.remove( HashName( scheddName, poolName, NULL, "" ) );
		delete poll_info;
	}
	if ( proxySubject != NULL ) {
		free( proxySubject );
	}
	free( proxyFQAN );
	if ( scheddPollTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( scheddPollTid );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( ping_gahp != NULL ) {
		delete ping_gahp;
	}
	if ( lease_gahp != NULL ) {
		delete lease_gahp;
	}
	if ( scheddName != NULL ) {
		free( scheddName );
	}
	if ( poolName != NULL ) {
		free( poolName );
	}
}

void CondorResource::Reconfig()
{
	BaseResource::Reconfig();
}

const char *CondorResource::ResourceType()
{
	return "condor";
}

const char *CondorResource::GetHashName()
{
	return HashName( resourceName, poolName, proxyFQAN, m_scitokensFile ).c_str();
}

void CondorResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	std::string buff;

	formatstr( buff, "condor %s %s", resourceName, poolName );
	resource_ad->Assign( ATTR_NAME, buff );
	if ( proxySubject ) {
		resource_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT, proxySubject );
	}
	if ( proxyFQAN ) {
		resource_ad->Assign( ATTR_X509_USER_PROXY_FQAN, proxyFQAN );
	}
	if ( !m_scitokensFile.empty() ) {
		resource_ad->Assign( ATTR_SCITOKENS_FILE, m_scitokensFile );
	}

	gahp->PublishStats( resource_ad );
}

void CondorResource::CondorRegisterJob( CondorJob *job, const char *submitter_id )
{
	BaseResource::RegisterJob( job );

	if ( submitter_ids.contains( submitter_id ) == false ) {
		submitter_ids.append( submitter_id );
		if ( submitter_constraint.empty() ) {
			formatstr( submitter_constraint, "(%s=?=\"%s\")",
										  ATTR_SUBMITTER_ID,
										  submitter_id );
		} else {
			formatstr_cat( submitter_constraint, "||(%s=?=\"%s\")",
											  ATTR_SUBMITTER_ID,
											  submitter_id );
		}
	}
}

void CondorResource::UnregisterJob( BaseJob *base_job )
{
	CondorJob *job = dynamic_cast<CondorJob*>( base_job );

	ScheddPollInfo *poll_info = NULL;
	PollInfoByName.lookup( HashName( scheddName, poolName, NULL, "" ), poll_info );
	if ( poll_info ) {
		poll_info->m_submittedJobs.Delete( job );
	}

		// This may call delete, so don't put anything after it!
	BaseResource::UnregisterJob( job );
}

// Return true if the given error message from the gahp indicates that
// the remote schedd isn't working. Authentication or authorization
// failures don't count.
bool CondorResource::GahpErrorResourceDown( const char *errmsg )
{
	if ( errmsg == NULL ) {
		return false;
	}
	if ( strstr( errmsg, "Failed to connect to" ) ||
		 strstr( errmsg, "Error locating schedd" ) ) {
		return true;
	} else {
		return false;
	}
}

void CondorResource::DoScheddPoll()
{
	int rc;
	ScheddPollInfo *poll_info = NULL;

	if ( ( registeredJobs.IsEmpty() || resourceDown ) &&
		 scheddStatusActive == false ) {
			// No jobs or we can't talk to the schedd, so no point
			// in polling
		daemonCore->Reset_Timer( scheddPollTid, BatchStatusInterval() );
		return;
	}

	if ( gahp->isStarted() == false ) {
		// The gahp isn't started yet. Wait a few seconds for a CondorJob
		// object to start it (and possibly initialize x509 credentials).
		daemonCore->Reset_Timer( scheddPollTid, 5 );
		return;
	}

	PollInfoByName.lookup( HashName( scheddName, poolName, NULL, "" ), poll_info );

	daemonCore->Reset_Timer( scheddPollTid, TIMER_NEVER );

	if ( scheddStatusActive == false ) {

			// We share polls across all CondorResource objects going to
			// the same schedd. If another object has done a poll
			// recently, then don't bother doing one ourselves.
		if ( poll_info  == NULL ) {
			poll_info = new ScheddPollInfo;
			poll_info->m_lastPoll = 0;
			poll_info->m_pollActive = false;
			PollInfoByName.insert( HashName( scheddName, poolName, NULL, "" ),
								   poll_info );
		}

		if ( poll_info->m_pollActive == true ||
			 poll_info->m_lastPoll + BatchStatusInterval() > time(NULL) ) {
			daemonCore->Reset_Timer( scheddPollTid, BatchStatusInterval() );
			return;
		}

			// start schedd status command
		dprintf( D_FULLDEBUG, "Starting collective poll: %s\n",
				 scheddName );
		std::string constraint;

			// create a list of jobs we expect to hear about in our
			// status command
			// Since we're sharing the results of this status command with
			// all CondorResource objects going to the same schedd, look
			// for their jobs as well.
		poll_info->m_submittedJobs.Rewind();
		while ( poll_info->m_submittedJobs.Next() ) {
			poll_info->m_submittedJobs.DeleteCurrent();
		}
		BaseJob *job;
		std::string job_id;
		for (auto &elem : ResourcesByName) {
			CondorResource *next_resource = elem.second;
			if ( strcmp( scheddName, next_resource->scheddName ) ||
				 strcmp( poolName ? poolName : "",
						 next_resource->poolName ? next_resource->poolName : "" ) ) {
				continue;
			}

			next_resource->registeredJobs.Rewind();
			while ( ( job = next_resource->registeredJobs.Next() ) ) {
				if ( job->jobAd->LookupString( ATTR_GRID_JOB_ID, job_id ) ) {
					poll_info->m_submittedJobs.Append( (CondorJob *)job );
				}
			}
		}

		formatstr( constraint, "(%s)", submitter_constraint.c_str() );

		rc = gahp->condor_job_status_constrained( scheddName,
												  constraint.c_str(),
												  NULL, NULL );

		if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
			dprintf( D_ALWAYS,
					 "gahp->condor_job_status_constrained returned %d for remote schedd: %s\n",
					 rc, scheddName );
			EXCEPT( "condor_job_status_constrained failed!" );
		}
		scheddStatusActive = true;
		poll_info->m_pollActive = true;

	} else {

			// finish schedd status command
		int num_status_ads;
		ClassAd **status_ads = NULL;

		ASSERT( poll_info );

		rc = gahp->condor_job_status_constrained( NULL, NULL,
												  &num_status_ads,
												  &status_ads );

		if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
			return;
		} else if ( rc != 0 ) {
			dprintf( D_ALWAYS,
					 "gahp->condor_job_status_constrained returned %d for remote schedd %s\n",
					 rc, scheddName );
			dprintf( D_ALWAYS, "Requesting ping of resource\n" );
			RequestPing( NULL );
		}

		if ( rc == 0 ) {
			for ( int i = 0; i < num_status_ads; i++ ) {
				int cluster, proc;
				int rc2;
				std::string job_id_string;
				BaseJob *base_job = NULL;
				CondorJob *job;

				if( status_ads[i] == NULL ) {
					dprintf(D_ALWAYS, "DoScheddPoll was given null pointer for classad #%d\n", i);
					continue;
				}

				status_ads[i]->LookupInteger( ATTR_CLUSTER_ID, cluster );
				status_ads[i]->LookupInteger( ATTR_PROC_ID, proc );

				formatstr( job_id_string, "condor %s %s %d.%d", scheddName,
									   poolName, cluster, proc );

				rc2 = BaseJob::JobsByRemoteId.lookup( job_id_string, base_job );
				job = dynamic_cast<CondorJob*>( base_job );
				if ( rc2 == 0 ) {
					job->NotifyNewRemoteStatus( status_ads[i] );
					poll_info->m_submittedJobs.Delete( job );
				} else {
					delete status_ads[i];
				}
			}

			poll_info->m_lastPoll = time(NULL);
		}
		poll_info->m_pollActive = false;

		if ( status_ads != NULL ) {
			free( status_ads );
		}

			// Check if any jobs were missing from the status result
		if ( rc == 0 ) {
			CondorJob *job;
			std::string job_id;
			poll_info->m_submittedJobs.Rewind();
			while ( ( job = poll_info->m_submittedJobs.Next() ) ) {
				if ( job->jobAd->LookupString( ATTR_GRID_JOB_ID, job_id ) ) {
						// We should have gotten a status ad for this job,
						// but didn't. Tell the job that there may be
						// something wrong by giving it a NULL status ad.
					job->NotifyNewRemoteStatus( NULL );
				}
				poll_info->m_submittedJobs.DeleteCurrent();
			}
		}

		scheddStatusActive = false;

		dprintf( D_FULLDEBUG, "Collective poll complete: %s\n", scheddName );

		daemonCore->Reset_Timer( scheddPollTid, BatchStatusInterval() );
	}
}

void CondorResource::DoPing( unsigned& ping_delay, bool& ping_complete,
							 bool& ping_succeeded )
{
	int rc;
	int num_status_ads = 0;
	ClassAd **status_ads = NULL;

dprintf(D_FULLDEBUG,"*** DoPing called\n");
	if ( ping_gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}

	ping_delay = 0;

	rc = ping_gahp->condor_job_status_constrained( scheddName, "False",
												   &num_status_ads,
												   &status_ads );
	ASSERT( num_status_ads == 0 );
	ASSERT( status_ads == NULL );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc != 0 ) {
		ping_complete = true;
		if ( GahpErrorResourceDown( ping_gahp->getErrorString() ) ) {
			ping_succeeded = false;
		} else {
			ping_succeeded = true;
		}
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}
}

void CondorResource::DoUpdateLeases( unsigned& update_delay,
									 bool& update_complete,
									 SimpleList<PROC_ID>& update_succeeded )
{
	int rc;
	BaseJob *curr_job;
	SimpleList<PROC_ID> jobs;
	SimpleList<int> expirations;
	SimpleList<PROC_ID> updated;

dprintf(D_FULLDEBUG,"*** DoUpdateLeases called\n");
	if ( lease_gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying lease update\n" );
		update_delay = 5;
		return;
	}

	update_delay = 0;

	if ( leaseUpdates.IsEmpty() ) {
		dprintf( D_FULLDEBUG, "*** Job lease list empty, returning success immediately\n" );
		update_complete = true;
		return;
	}

	if ( updateLeasesCmdActive == false ) {
		leaseUpdates.Rewind();
		while ( leaseUpdates.Next( curr_job ) ) {
				// TODO When remote-job-id is homogenized and stored in 
				//   BaseJob, BaseResource can skip jobs that don't have a
				//   a remote-job-id yet
			if ( ((CondorJob*)curr_job)->remoteJobId.cluster != 0 ) {
				jobs.Append( ((CondorJob*)curr_job)->remoteJobId );
				expirations.Append( m_sharedLeaseExpiration );
			}
		}
	}

	rc = lease_gahp->condor_job_update_lease( scheddName, jobs, expirations,
											  updated );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		update_complete = false;
	} else if ( rc != 0 ) {
dprintf( D_FULLDEBUG, "*** Lease update failed!\n" );
		update_complete = true;
	} else {
dprintf( D_FULLDEBUG, "*** Lease udpate succeeded!\n" );
		update_complete = true;

		PROC_ID curr_id;
		std::string id_str;
		updated.Rewind();
		while ( updated.Next( curr_id ) ) {
			formatstr( id_str, "condor %s %s %d.%d", scheddName, poolName,
							curr_id.cluster, curr_id.proc );
			if ( BaseJob::JobsByRemoteId.lookup( id_str, curr_job ) == 0 ) {
				update_succeeded.Append( curr_job->procID );
			}
		}
	}
}
