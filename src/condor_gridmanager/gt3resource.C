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

#include "gt3resource.h"
#include "gridmanager.h"

#define DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE	5
#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100

#define LOG_FILE_TIMEOUT		300

template class List<GT3Job>;
template class Item<GT3Job>;

int GT3Resource::probeInterval = 300;	// default value
int GT3Resource::probeDelay = 15;		// default value
int GT3Resource::gahpCallTimeout = 300;	// default value

//////////////from gridmanager.C
#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, GT3Resource *>;
template class HashBucket<HashKey, GT3Resource *>;

HashTable <HashKey, GT3Resource *> GT3ResourcesByName( HASH_TABLE_SIZE,
													   hashFunction );
////////////////////////////////

GT3Resource::GT3Resource( const char *resource_name )
	: BaseResource( resource_name )
{
	resourceDown = false;
	firstPingDone = false;
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GT3Resource::DoPing,
								"GT3Resource::DoPing", (Service*)this );
	lastPing = 0;
	lastStatusChange = 0;
		// TODO This assumes that at least one GT3Job has already
		// initialized the gahp server. Need a better solution.
	gahp = new GahpClient( "GT3", GAHPCLIENT_DEFAULT_SERVER_PATH );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );
	submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;

	Reconfig();
}

GT3Resource::~GT3Resource()
{
	daemonCore->Cancel_Timer( pingTimerId );
	if ( gahp != NULL ) {
		delete gahp;
	}
}

void GT3Resource::Reconfig()
{
	char *param_value;

	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );

	submitLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS_PER_RESOURCE" );
	if ( param_value == NULL ) {
		// Check old parameter name
		param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS" );
	}
	if ( param_value != NULL ) {
		char *tmp1;
		char *tmp2;
		StringList limits( param_value );
		limits.rewind();
		if ( limits.number() > 0 ) {
			submitLimit = atoi( limits.next() );
			while ( (tmp1 = limits.next()) && (tmp2 = limits.next()) ) {
				if ( strcmp( tmp1, resourceName ) == 0 ) {
					submitLimit = atoi( tmp2 );
				}
			}
		}
		free( param_value );
	}
	if ( submitLimit <= 0 ) {
		submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	}

	jobLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE" );
	if ( param_value != NULL ) {
		char *tmp1;
		char *tmp2;
		StringList limits( param_value );
		limits.rewind();
		if ( limits.number() > 0 ) {
			jobLimit = atoi( limits.next() );
			while ( (tmp1 = limits.next()) && (tmp2 = limits.next()) ) {
				if ( strcmp( tmp1, resourceName ) == 0 ) {
					jobLimit = atoi( tmp2 );
				}
			}
		}
		free( param_value );
	}
	if ( jobLimit <= 0 ) {
		jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;
	}

	// If the jobLimit was widened, move jobs from Wanted to Allowed and
	// add them to Queued
	while ( submitsAllowed.Length() < jobLimit &&
			submitsWanted.Length() > 0 ) {
		GT3Job *wanted_job = submitsWanted.Head();
		submitsWanted.Delete( wanted_job );
		submitsAllowed.Append( wanted_job );
//		submitsQueued.Append( wanted_job );
		wanted_job->SetEvaluateState();
	}

	// If the submitLimit was widened, move jobs from Queued to In-Progress
	while ( submitsInProgress.Length() < submitLimit &&
			submitsQueued.Length() > 0 ) {
		GT3Job *queued_job = submitsQueued.Head();
		submitsQueued.Delete( queued_job );
		submitsInProgress.Append( queued_job );
		queued_job->SetEvaluateState();
	}
}

const char *GT3Resource::CanonicalName( const char *name )
{
/*
	static MyString canonical;
	char *host;
	char *port;

	parse_resource_manager_string( name, &host, &port, NULL, NULL );

	canonical.sprintf( "%s:%s", host, *port ? port : "2119" );

	free( host );
	free( port );

	return canonical.Value();
*/
	return name;
}

void GT3Resource::RegisterJob( GT3Job *job, bool already_submitted )
{
	registeredJobs.Append( job );

	if ( already_submitted ) {
		submitsAllowed.Append( job );
	}

	if ( firstPingDone == true ) {
		if ( resourceDown ) {
			job->NotifyResourceDown();
		} else {
			job->NotifyResourceUp();
		}
	}
}

void GT3Resource::UnregisterJob( GT3Job *job )
{
	CancelSubmit( job );
	registeredJobs.Delete( job );
	pingRequesters.Delete( job );
}

void GT3Resource::RequestPing( GT3Job *job )
{
	pingRequesters.Append( job );

	daemonCore->Reset_Timer( pingTimerId, 0 );
}

bool GT3Resource::RequestSubmit( GT3Job *job )
{
	bool already_allowed = false;
	GT3Job *jobptr;

	submitsQueued.Rewind();
	while ( submitsQueued.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsInProgress.Rewind();
	while ( submitsInProgress.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return true;
		}
	}

	submitsWanted.Rewind();
	while ( submitsWanted.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsAllowed.Rewind();
	while ( submitsAllowed.Next( jobptr ) ) {
		if ( jobptr == job ) {
			already_allowed = true;
			break;
		}
	}

	if ( already_allowed == false ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			EXCEPT("In GT3Resource for %s, SubmitsWanted is not empty and SubmitsAllowed is not full\n",resourceName);
		}
		if ( submitsAllowed.Length() < jobLimit ) {
			submitsAllowed.Append( job );
			// proceed to see if submitLimit applies
		} else {
			submitsWanted.Append( job );
			return false;
		}
	}

	if ( submitsInProgress.Length() < submitLimit &&
		 submitsQueued.Length() > 0 ) {
		EXCEPT("In GT3Resource for %s, SubmitsQueued is not empty and SubmitsToProgress is not full\n",resourceName);
	}
	if ( submitsInProgress.Length() < submitLimit ) {
		submitsInProgress.Append( job );
		return true;
	} else {
		submitsQueued.Append( job );
		return false;
	}
}

void GT3Resource::SubmitComplete( GT3Job *job )
{
	if ( submitsInProgress.Delete( job ) ) {
		if ( submitsInProgress.Length() < submitLimit &&
			 submitsQueued.Length() > 0 ) {
			GT3Job *queued_job = submitsQueued.Head();
			submitsQueued.Delete( queued_job );
			submitsInProgress.Append( queued_job );
			queued_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsQueued if the job wasn't in
		// submitsInProgress.
		submitsQueued.Delete( job );
	}

	return;
}

void GT3Resource::CancelSubmit( GT3Job *job )
{
	if ( submitsAllowed.Delete( job ) ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			GT3Job *wanted_job = submitsWanted.Head();
			submitsWanted.Delete( wanted_job );
			submitsAllowed.Append( wanted_job );
//			submitsQueued.Append( wanted_job );
			wanted_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsWanted if the job wasn't in
		// submitsAllowed.
		submitsWanted.Delete( job );
	}

	SubmitComplete( job );

	return;
}

bool GT3Resource::IsEmpty()
{
	return registeredJobs.IsEmpty();
}

bool GT3Resource::IsDown()
{
	return resourceDown;
}

int GT3Resource::DoPing()
{
	int rc;
	bool ping_failed = false;
	GT3Job *job;

	// Don't perform a ping if we have no requesters and the resource is up
	if ( pingRequesters.IsEmpty() && resourceDown == false &&
		 firstPingDone == true ) {
		daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );
		return TRUE;
	}

	// Don't start a new ping too soon after the previous one. If the
	// resource is up, the minimum time between pings is probeDelay. If the
	// resource is down, the minimum time between pings is probeInterval.
	int delay;
	if ( resourceDown == false ) {
		delay = (lastPing + probeDelay) - time(NULL);
	} else {
		delay = (lastPing + probeInterval) - time(NULL);
	}
	if ( delay > 0 ) {
		daemonCore->Reset_Timer( pingTimerId, delay );
		return TRUE;
	}

	daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		daemonCore->Reset_Timer( pingTimerId, 5 );
		return TRUE;
	}
	gahp->setNormalProxy( gahp->getMasterProxy() );
	if ( PROXY_NEAR_EXPIRED( gahp->getMasterProxy() ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		return TRUE;
	}

	rc = gahp->gt3_gram_client_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		return 0;
	}

	lastPing = time(NULL);

	if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
		ping_failed = true;
	}

	if ( ping_failed == resourceDown && firstPingDone == true ) {
		// State of resource hasn't changed. Notify ping requesters only.
		dprintf(D_ALWAYS,"resource %s is still %s\n",resourceName,
				ping_failed?"down":"up");

		pingRequesters.Rewind();
		while ( pingRequesters.Next( job ) ) {
			pingRequesters.DeleteCurrent();
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}
	} else {
		// State of resource has changed. Notify every job.
		dprintf(D_ALWAYS,"resource %s is now %s\n",resourceName,
				ping_failed?"down":"up");

		resourceDown = ping_failed;
		lastStatusChange = lastPing;

		firstPingDone = true;

		registeredJobs.Rewind();
		while ( registeredJobs.Next( job ) ) {
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}

		pingRequesters.Rewind();
		while ( pingRequesters.Next( job ) ) {
			pingRequesters.DeleteCurrent();
		}
	}

	if ( resourceDown ) {
		daemonCore->Reset_Timer( pingTimerId, probeInterval );
	}

	return 0;
}
