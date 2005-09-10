/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "condorresource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE			500

HashTable <HashKey, CondorResource *>
    CondorResource::ResourcesByName( HASH_TABLE_SIZE,
									 hashFunction );

int CondorResource::scheddPollInterval = 300;		// default value

const char *CondorResource::HashName( const char *resource_name,
									  const char *pool_name,
									  const char *proxy_subject )
{
	static MyString hash_name;

	hash_name.sprintf( "%s#%s#%s", resource_name, 
					   pool_name ? pool_name : "NULL",
					   proxy_subject ? proxy_subject : "NULL" );

	return hash_name.Value();
}

CondorResource *CondorResource::FindOrCreateResource( const char * resource_name,
													  const char *pool_name,
													  const char *proxy_subject )
{
	int rc;
	MyString resource_key;
	CondorResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name,
													pool_name,
													proxy_subject ) ),
								 resource );
	if ( rc != 0 ) {
		resource = new CondorResource( resource_name, pool_name,
									   proxy_subject );
		ASSERT(resource);
		ResourcesByName.insert( HashKey( HashName( resource_name,
												   pool_name,
												   proxy_subject ) ),
								resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}

CondorResource::CondorResource( const char *resource_name, const char *pool_name,
								const char *proxy_subject )
	: BaseResource( resource_name )
{
	hasLeases = true;

	if ( proxy_subject != NULL ) {
		proxySubject = strdup( proxy_subject );
	} else {
		proxySubject = 0;
	}
	scheddPollTid = TIMER_UNSET;
	scheddName = strdup( resource_name );
	gahp = NULL;
	ping_gahp = NULL;
	scheddStatusActive = false;
	submitter_constraint = "";

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
		MyString buff;
		MyString buff2;
		buff.sprintf( "CONDOR/%s/%s/%s", poolName ? poolName : "NULL",
					  scheddName, proxySubject ? proxySubject : "NULL" );
		buff2.sprintf( "-f -s %s", scheddName );
		if ( poolName != NULL ) {
			buff2.sprintf_cat( " -P %s", poolName );
		}

		gahp = new GahpClient( buff.Value(), gahp_path, buff2.Value() );
		gahp->setNotificationTimerId( scheddPollTid );
		gahp->setMode( GahpClient::normal );
		gahp->setTimeout( CondorJob::gahpCallTimeout );

		ping_gahp = new GahpClient( buff.Value(), gahp_path, buff2.Value() );
		ping_gahp->setNotificationTimerId( pingTimerId );
		ping_gahp->setMode( GahpClient::normal );
		ping_gahp->setTimeout( CondorJob::gahpCallTimeout );

		lease_gahp = new GahpClient( buff.Value(), gahp_path, buff2.Value() );
		lease_gahp->setNotificationTimerId( updateLeasesTimerId );
		lease_gahp->setMode( GahpClient::normal );
		lease_gahp->setTimeout( CondorJob::gahpCallTimeout );

		free( gahp_path );
	}
}

CondorResource::~CondorResource()
{
	ResourcesByName.remove( HashKey( HashName( resourceName,
											   poolName,
											   proxySubject ) ) );
	if ( proxySubject != NULL ) {
		free( proxySubject );
	}
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
}

void CondorResource::Reconfig()
{
	BaseResource::Reconfig();
}

void CondorResource::RegisterJob( CondorJob *job, const char *submitter_id )
{
	BaseResource::RegisterJob( job );

	if ( submitter_ids.contains( submitter_id ) == false ) {
		submitter_ids.append( submitter_id );
		if ( submitter_constraint.Length() == 0 ) {
			submitter_constraint.sprintf( "(%s=?=\"%s\")",
										  ATTR_SUBMITTER_ID,
										  submitter_id );
		} else {
			submitter_constraint.sprintf_cat( "||(%s=?=\"%s\")",
											  ATTR_SUBMITTER_ID,
											  submitter_id );
		}
	}
}

int CondorResource::DoScheddPoll()
{
	int rc;

	if ( registeredJobs.IsEmpty() && scheddStatusActive == false ) {
			// No jobs, so nothing to poll/update
		daemonCore->Reset_Timer( scheddPollTid, scheddPollInterval );
		return 0;
	}

	if ( gahp->isStarted() == false ) {
		if ( gahp->Startup() == false ) {
				// Failed to start the gahp server. Don't do anything
				// about it. The job objects will also fail on this call
				// and should go on hold as a result.
			daemonCore->Reset_Timer( scheddPollTid, scheddPollInterval );
			return 0;
		}
	}

	daemonCore->Reset_Timer( scheddPollTid, TIMER_NEVER );

	if ( scheddStatusActive == false ) {

			// start schedd status command
		dprintf( D_FULLDEBUG, "Starting collective poll: %s\n",
				 scheddName );
		MyString constraint;

		constraint.sprintf( "(%s)", submitter_constraint.Value() );

		rc = gahp->condor_job_status_constrained( scheddName,
												  constraint.Value(),
												  NULL, NULL );

		if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
			dprintf( D_ALWAYS,
					 "gahp->condor_job_status_constrained returned %d for remote schedd: %s\n",
					 rc, scheddName );
			EXCEPT( "condor_job_status_constrained failed!" );
		}
		scheddStatusActive = true;

	} else {

			// finish schedd status command
		int num_status_ads;
		ClassAd **status_ads = NULL;

		rc = gahp->condor_job_status_constrained( NULL, NULL,
												  &num_status_ads,
												  &status_ads );

		if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
			return 0;
		} else if ( rc != 0 ) {
			dprintf( D_ALWAYS,
					 "gahp->condor_job_status_constrained returned %d for remote schedd %s\n",
					 rc, scheddName );
			dprintf( D_ALWAYS, "Requesting ping of resource\n" );
			RequestPing( NULL );
		}

		if ( rc == 0 ) {
			for ( int i = 0; i < num_status_ads; i++ ) {
				int rc;
				int cluster, proc;
				MyString job_id_string;
				CondorJob *job;

				if( status_ads[i] == NULL ) {
					dprintf(D_ALWAYS, "DoScheddPoll was given null pointer for classad #%d\n", i);
					continue;
				}

				status_ads[i]->LookupInteger( ATTR_CLUSTER_ID, cluster );
				status_ads[i]->LookupInteger( ATTR_PROC_ID, proc );

				job_id_string.sprintf( "condor %s %s %d.%d", poolName,
									   scheddName, cluster, proc );

				rc = BaseJob::JobsByRemoteId.lookup( HashKey( job_id_string.Value() ),
													 (BaseJob*&)job );
				if ( rc == 0 ) {
					job->NotifyNewRemoteStatus( status_ads[i] );
				} else {
					delete status_ads[i];
				}
			}
		}

		if ( status_ads != NULL ) {
			free( status_ads );
		}

		scheddStatusActive = false;

		dprintf( D_FULLDEBUG, "Collective poll complete: %s\n", scheddName );

		daemonCore->Reset_Timer( scheddPollTid, scheddPollInterval );
	}

	return 0;
}

void CondorResource::DoPing( time_t& ping_delay, bool& ping_complete,
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
	} else if ( rc != 0 ) 
	{
		ping_complete = true;
		ping_succeeded = false;
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}
}

void CondorResource::DoUpdateLeases( time_t& update_delay,
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

	if ( updateLeasesCmdActive == false ) {
		leaseUpdates.Rewind();
		while ( leaseUpdates.Next( curr_job ) ) {
				// TODO When remote-job-id is homogenized and stored in 
				//   BaseJob, BaseResource can skip jobs that don't have a
				//   a remote-job-id yet
			if ( ((CondorJob*)curr_job)->remoteJobId.cluster != 0 ) {
				int exp = 0;
				jobs.Append( ((CondorJob*)curr_job)->remoteJobId );
				curr_job->jobAd->LookupInteger( ATTR_TIMER_REMOVE_CHECK_SENT,
												exp );
				expirations.Append( exp );
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
		MyString id_str;
		updated.Rewind();
		while ( updated.Next( curr_id ) ) {
			id_str.sprintf( "condor %s %s %d.%d", poolName, scheddName,
							curr_id.cluster, curr_id.proc );
			if ( BaseJob::JobsByRemoteId.lookup( HashKey( id_str.Value() ),
												 curr_job ) == 0 ) {
				update_succeeded.Append( curr_job->procID );
			}
		}
	}
}
