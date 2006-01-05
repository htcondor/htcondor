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

#include "mirrorresource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE			500

HashTable <HashKey, MirrorResource *>
    MirrorResource::ResourcesByName( HASH_TABLE_SIZE,
									 hashFunction );

int MirrorResource::scheddPollInterval = 300;		// default value

MirrorResource *MirrorResource::FindOrCreateResource( const char * resource_name )
{
	int rc;
	MirrorResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( resource_name ), resource );
	if ( rc != 0 ) {
		resource = new MirrorResource( resource_name );
		ASSERT(resource);
		ResourcesByName.insert( HashKey( resource_name ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}

MirrorResource::MirrorResource( const char *resource_name )
	: BaseResource( resource_name )
{
	scheddPollTid = TIMER_UNSET;
	mirrorScheddName = strdup( resource_name );
	gahp = NULL;
	scheddUpdateActive = false;
	scheddStatusActive = false;
	submitter_constraint = "";

	scheddPollTid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&MirrorResource::DoScheddPoll,
							"MirrorResource::DoScheddPoll", (Service*)this );

	char *gahp_path = param("MIRROR_GAHP");
	if ( gahp_path == NULL ) {
		gahp_path = param( "CONDOR_GAHP" );
		if ( gahp_path == NULL ) {
			EXCEPT( "CONDOR_GAHP not defined in condor config file" );
		}
	} else {
		// TODO remove mirrorScheddName from the gahp server key if/when
		//   a gahp server can handle multiple schedds
		MyString buff;
		ArgList args;
		buff.sprintf( "MIRRORRESOURCE/%s", mirrorScheddName );
		args.AppendArg("-f");
		args.AppendArg("-s");
		args.AppendArg(mirrorScheddName);
		gahp = new GahpClient( buff.Value(), gahp_path, &args );
		gahp->setNotificationTimerId( scheddPollTid );
		gahp->setMode( GahpClient::normal );
		gahp->setTimeout( MirrorJob::gahpCallTimeout );
		free( gahp_path );
	}
}

MirrorResource::~MirrorResource()
{
	ResourcesByName.remove( HashKey( resourceName ) );
	if ( scheddPollTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( scheddPollTid );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( mirrorScheddName != NULL ) {
		free( mirrorScheddName );
	}
}

void MirrorResource::Reconfig()
{
	BaseResource::Reconfig();
}

void MirrorResource::RegisterJob( MirrorJob *job, const char *submitter_id )
{
	BaseResource::RegisterJob( job );

	if ( submitter_ids.contains( submitter_id ) == false ) {
		submitter_ids.append( submitter_id );
		if ( submitter_constraint.Length() == 0 ) {
			submitter_constraint.sprintf( "(%s=?=\"%s\")",
										  ATTR_MIRROR_SUBMITTER_ID,
										  submitter_id );
		} else {
			submitter_constraint.sprintf_cat( "||(%s=?=\"%s\")",
											  ATTR_MIRROR_SUBMITTER_ID,
											  submitter_id );
		}
	}
}

int MirrorResource::DoScheddPoll()
{
	int rc;

	if ( registeredJobs.IsEmpty() && scheddUpdateActive == false &&
		 scheddStatusActive == false ) {
			// No jobs, so nothing to poll/update
		daemonCore->Reset_Timer( scheddPollTid, scheddPollInterval );
		return 0;
	}

	if ( gahp->isStarted() == false ) {
		if ( gahp->Startup() == false ) {
			EXCEPT( "Failed to start gahp server" );
		}
	}

	daemonCore->Reset_Timer( scheddPollTid, TIMER_NEVER );

	if ( scheddUpdateActive == false  && scheddStatusActive == false ) {

			// start schedd update command
		MyString buff;
		MyString constraint;
		int new_lease;

dprintf(D_FULLDEBUG,"***starting schedd-update\n");
		constraint.sprintf( "(%s)", submitter_constraint.Value() );

		new_lease = time(NULL) + MirrorJob::leaseInterval;

		ClassAd update_ad;
		buff.sprintf( "%s = %d", ATTR_MIRROR_LEASE_TIME, new_lease );
		update_ad.Insert( buff.Value() );

		rc = gahp->condor_job_update_constrained( mirrorScheddName,
												  constraint.Value(),
												  &update_ad );

		if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
			dprintf( D_ALWAYS, "gahp->condor_job_update_constrained returned %d\n",
					 rc );
			EXCEPT( "condor_job_update_constrained failed!" );
		}
		scheddUpdateActive = true;

	} else if ( scheddUpdateActive == true ) {

			// finish schedd update command
dprintf(D_FULLDEBUG,"***finishing(hopefully) schedd-update\n");
		rc = gahp->condor_job_update_constrained( NULL, NULL, NULL );

		if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
dprintf(D_FULLDEBUG,"***schedd-update still pending\n");
			return 0;
		} else if ( rc != 0 ) {
			dprintf( D_ALWAYS, "gahp->condor_job_update_constrained returned %d\n",
					 rc );
		}
		scheddUpdateActive = false;

			// start schedd status command
dprintf(D_FULLDEBUG,"***starting schedd-status\n");
		MyString constraint;

		constraint.sprintf( "(%s)", submitter_constraint.Value() );

		rc = gahp->condor_job_status_constrained( mirrorScheddName,
												  constraint.Value(),
												  NULL, NULL );

		if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
			dprintf( D_ALWAYS, "gahp->condor_job_status_constrained returned %d\n",
					 rc );
			EXCEPT( "condor_job_status_constrained failed!" );
		}
		scheddStatusActive = true;

	} else if ( scheddStatusActive == true ) {

			// finish schedd status command
dprintf(D_FULLDEBUG,"***finishing(hopefully) schedd-status\n");
		int num_status_ads;
		ClassAd **status_ads = NULL;

		rc = gahp->condor_job_status_constrained( NULL, NULL,
												  &num_status_ads,
												  &status_ads );

		if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
dprintf(D_FULLDEBUG,"***schedd-status still pending\n");
			return 0;
		} else if ( rc != 0 ) {
			dprintf( D_ALWAYS, "gahp->condor_job_status_constrained returned %d\n",
					 rc );
		}

		if ( rc == 0 ) {
			for ( int i = 0; i < num_status_ads; i++ ) {
				int rc;
				int cluster, proc;
				MyString job_id_string;
				MirrorJob *job;

				status_ads[i]->LookupInteger( ATTR_CLUSTER_ID, cluster );
				status_ads[i]->LookupInteger( ATTR_PROC_ID, proc );

				job_id_string.sprintf( "%s/%d.%d", mirrorScheddName, cluster,
									   proc );

				rc = MirrorJobsById.lookup( HashKey( job_id_string.Value() ),
											job );
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

dprintf(D_FULLDEBUG,"***schedd-poll cycle complete\n");
		daemonCore->Reset_Timer( scheddPollTid, scheddPollInterval );
	}

	return 0;
}
