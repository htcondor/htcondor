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
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "daemon.h"
#include "dc_schedd.h"

#include "gridmanager.h"
#include "gahp-client.h"

#include "arcjob.h"
#include "condorjob.h"
#include "infnbatchjob.h"
#include "condor_version.h"

#include "ec2job.h"
#include "gcejob.h"
#include "azurejob.h"

#include <algorithm>

#define QMGMT_TIMEOUT 15

#define UPDATE_SCHEDD_DELAY		5

struct JobType
{
	std::string Name;
	void(*InitFunc)();
	void(*ReconfigFunc)();
	bool(*AdMatchFunc)(const ClassAd*);
	BaseJob *(*CreateFunc)(ClassAd*);
};

std::vector<JobType> jobTypes;

std::vector<int> scheddUpdateNotifications;

struct ScheddUpdateRequest {
	BaseJob *m_job;
	bool m_notify;
};

std::map<PROC_ID, ScheddUpdateRequest *> pendingScheddUpdates;
bool addJobsSignaled = false;
bool updateJobsSignaled = false;
bool checkLeasesSignaled = false;
int contactScheddTid = TIMER_UNSET;
int contactScheddDelay;
time_t lastContactSchedd = 0;

char *ScheddAddr = NULL;
char *ScheddName = NULL;
char *ScheddJobConstraint = NULL;
char *GridmanagerScratchDir = NULL;
DCSchedd *ScheddObj = NULL;

bool firstScheddContact = true;
int scheddFailureCount = 0;
int maxScheddFailures = 10;	// Years of careful research...

void RequestContactSchedd();
void doContactSchedd(int);

std::map<std::string, BaseJob*> FetchProxyList;

// handlers
int ADD_JOBS_signalHandler(int );
int REMOVE_JOBS_signalHandler(int );
void CHECK_LEASES_signalHandler(int);
int UPDATE_JOBAD_signalHandler(int );
int FetchProxyDelegationHandler(int, Stream * );

static bool jobExternallyManaged(ClassAd * ad)
{
	ASSERT(ad);
	std::string job_managed;
	if( ! ad->LookupString(ATTR_JOB_MANAGED, job_managed) ) {
		return false;
	}
	return job_managed == MANAGED_EXTERNAL;
}

/**
TODO: Should use the version in qmgmt_common.C.  It's not
exposed as a remote call.  Perhaps link it in directly? 
*/
static int tSetAttributeString(int cluster, int proc, 
	const char * attr_name, const char * attr_value)
{
	std::string tmp;
	formatstr(tmp, "\"%s\"", attr_value);
	return SetAttribute( cluster, proc, attr_name, tmp.c_str());
}

// Job objects should call this function when they have changes that need
// to be propagated to the schedd.
// return value of true means requested update has been committed to schedd.
// return value of false means requested update has been queued, but has not
//   been committed to the schedd yet
void
requestScheddUpdate( BaseJob *job, bool notify )
{
	ScheddUpdateRequest *request;

	// Check if there's anything that actually requires contacting the
	// schedd. If not, just return true (i.e. update is complete)

	if ( job->deleteFromGridmanager == false &&
		 job->deleteFromSchedd == false &&
		 job->jobAd->dirtyBegin() == job->jobAd->dirtyEnd() ) {
		job->SetEvaluateState();
		return;
	}

	// Check if the job is already in the map
	auto it = pendingScheddUpdates.find(job->procID);
	if (it != pendingScheddUpdates.end()) {
		if (it->second->m_notify == false) {
			it->second->m_notify = notify;
		}
	} else {
		request = new ScheddUpdateRequest();
		request->m_job = job;
		request->m_notify = notify;
		pendingScheddUpdates[job->procID] = request;
		RequestContactSchedd();
	}
}

void
requestScheddUpdateNotification( int timer_id )
{
	if ( scheddUpdateNotifications.end() == std::find(
				scheddUpdateNotifications.begin(), scheddUpdateNotifications.end(), timer_id)) {
		// A new request; add it to the list
		scheddUpdateNotifications.push_back( timer_id );
		RequestContactSchedd();
	}
}

void
RequestContactSchedd()
{
	if ( contactScheddTid == TIMER_UNSET ) {
		time_t now = time(NULL);
		time_t delay = 0;
		if ( lastContactSchedd + contactScheddDelay > now ) {
			delay = (lastContactSchedd + contactScheddDelay) - now;
		}
		contactScheddTid = daemonCore->Register_Timer( delay,
												doContactSchedd,
												"doContactSchedd" );
	}
}

void
Init()
{
	pid_t schedd_pid;

	// schedd address may be overridden by a commandline option
	// only set it if it hasn't been set already
	if ( ScheddAddr == NULL ) {
		schedd_pid = daemonCore->getppid();
		char const *tmp = daemonCore->InfoCommandSinfulString( schedd_pid );
		if ( tmp == NULL ) {
			EXCEPT( "Failed to determine schedd's address" );
		} else {
			ScheddAddr = strdup( tmp );
		}
	}

	const char *val = getenv( "SCHEDD_NAME" );
	if ( val ) {
		ScheddName = strdup( val );
	} else {
		ScheddName = strdup( "" );
	}

	if ( ScheddObj == NULL ) {
		ScheddObj = new DCSchedd( ScheddAddr );
		ASSERT( ScheddObj );
		if ( ScheddObj->locate() == false ) {
			EXCEPT( "Failed to locate schedd: %s", ScheddObj->error() );
		}
	}

    // read config file
	// initialize variables

	if ( GridmanagerScratchDir == NULL ) {
		EXCEPT( "Schedd didn't specify scratch dir with -S" );
	}

	if ( InitializeProxyManager( GridmanagerScratchDir ) == false ) {
		EXCEPT( "Failed to initialize Proxymanager" );
	}

	JobType new_type;

	new_type.Name = "ARC";
	new_type.InitFunc = ArcJobInit;
	new_type.ReconfigFunc = ArcJobReconfig;
	new_type.AdMatchFunc = ArcJobAdMatch;
	new_type.CreateFunc = ArcJobCreate;
	jobTypes.push_back(new_type);

	new_type.Name = "EC2";
	new_type.InitFunc = EC2JobInit;
	new_type.ReconfigFunc = EC2JobReconfig;
	new_type.AdMatchFunc = EC2JobAdMatch;
	new_type.CreateFunc = EC2JobCreate;
	jobTypes.push_back(new_type);

	new_type.Name = "GCE";
	new_type.InitFunc = GCEJobInit;
	new_type.ReconfigFunc = GCEJobReconfig;
	new_type.AdMatchFunc = GCEJobAdMatch;
	new_type.CreateFunc = GCEJobCreate;
	jobTypes.push_back(new_type);

	new_type.Name = "Azure";
	new_type.InitFunc = AzureJobInit;
	new_type.ReconfigFunc = AzureJobReconfig;
	new_type.AdMatchFunc = AzureJobAdMatch;
	new_type.CreateFunc = AzureJobCreate;
	jobTypes.push_back(new_type);

	new_type.Name = "INFNBatch";
	new_type.InitFunc = INFNBatchJobInit;
	new_type.ReconfigFunc = INFNBatchJobReconfig;
	new_type.AdMatchFunc = INFNBatchJobAdMatch;
	new_type.CreateFunc = INFNBatchJobCreate;
	jobTypes.push_back(new_type);

	new_type.Name = "Condor";
	new_type.InitFunc = CondorJobInit;
	new_type.ReconfigFunc = CondorJobReconfig;
	new_type.AdMatchFunc = CondorJobAdMatch;
	new_type.CreateFunc = CondorJobCreate;
	jobTypes.push_back(new_type);

	for (auto job_type: jobTypes) {
		job_type.InitFunc();
	}

}

void
Register()
{
	daemonCore->Register_Signal( GRIDMAN_ADD_JOBS, "AddJobs",
								 &ADD_JOBS_signalHandler,
								 "ADD_JOBS_signalHandler");

	daemonCore->Register_Signal( GRIDMAN_REMOVE_JOBS, "RemoveJobs",
								 &REMOVE_JOBS_signalHandler,
								 "REMOVE_JOBS_signalHandler");

	daemonCore->Register_Signal( UPDATE_JOBAD, "UpdateJobAd",
								 &UPDATE_JOBAD_signalHandler,
								 "UPDATE_JOBAD_signalHandler");
/*
	daemonCore->Register_Signal( GRIDMAN_CHECK_LEASES, "CheckLeases",
								 &CHECK_LEASES_signalHandler,
								 "CHECK_LEASES_signalHandler");
*/
	daemonCore->Register_Timer( 60, 60, CHECK_LEASES_signalHandler,
								"CHECK_LEASES_signalHandler" );

	daemonCore->Register_Command( FETCH_PROXY_DELEGATION,
								  "FETCH_PROXY_DELEGATION",
								  &FetchProxyDelegationHandler,
								  "FetchProxyDelegationHandler",
								  DAEMON, true );

	Reconfig();
}

void
Reconfig()
{
	// This method is called both at startup [from method Init()], and
	// when we are asked to reconfig.

	contactScheddDelay = param_integer("GRIDMANAGER_CONTACT_SCHEDD_DELAY", 5);

	ReconfigProxyManager();
	GahpReconfig();
	BaseJob::BaseJobReconfig();

	for (auto job_type: jobTypes) {
		job_type.ReconfigFunc();
	}

	// Tell all the job objects to deal with their new config values
	for (auto& itr: BaseJob::JobsByProcId) {
		itr.second->Reconfig();
	}
}

int
ADD_JOBS_signalHandler(int )
{
	dprintf(D_FULLDEBUG,"Received ADD_JOBS signal\n");

	if ( !addJobsSignaled ) {
		RequestContactSchedd();
		addJobsSignaled = true;
	}

	return TRUE;
}

int
REMOVE_JOBS_signalHandler(int )
{
	dprintf(D_FULLDEBUG,"Received REMOVE_JOBS signal\n");

	// For held jobs that are still submitted to remote resources
	// (i.e. GridJobId defined) which are then removed, we need
	// to trigger an add-jobs query so that we attempt to cancel
	// them before taking them out of the schedd's queue.
	// TODO This is an imperfect solution. The gridmanager may
	//   connect to the schedd after the removal but before the
	//   REMOVE_JOBS signal is sent.
	if ( !addJobsSignaled ) {
		RequestContactSchedd();
		addJobsSignaled = true;
	}

	return TRUE;
}

int
UPDATE_JOBAD_signalHandler(int )
{
	dprintf(D_FULLDEBUG,"Received UPDATE_JOBAD signal\n");
	if ( !updateJobsSignaled ) {
		RequestContactSchedd();
		updateJobsSignaled = true;
	}

	return TRUE;
}

// Call initJobExprs before using any of the expr_*
// variables.  It is safe to repeatedly call
// initJobExprs.
	// Job is being managed by an external process
	// (probably this gridmanager process)
static std::string expr_managed;
	// The job is not being managed by an external
	// process.  The schedd should be managing it.
	// It may be done.
static std::string expr_not_managed;
	// The job is not HELD
static std::string expr_not_held;
	// The constraint passed into the gridmanager
	// to filter all jobs by
static std::string expr_schedd_job_constraint;
	// The job is marked as completely done by
	// the gridmanager.  The gridmanager should
	// never try to manage the job again
static std::string expr_completely_done;
	// Opposite of expr_completely_done
static std::string expr_not_completely_done;

static void 
initJobExprs()
{
	static bool done = false;
	if(done) { return; }

	formatstr(expr_managed, "(%s =?= \"%s\")", ATTR_JOB_MANAGED, MANAGED_EXTERNAL);
	formatstr(expr_not_managed, "(%s =!= \"%s\")", ATTR_JOB_MANAGED, MANAGED_EXTERNAL);
	formatstr(expr_not_held, "(%s != %d)", ATTR_JOB_STATUS, HELD);
	formatstr(expr_schedd_job_constraint, "(%s)", ScheddJobConstraint);
	// The gridmanager never wants to see this job again.
	// It should be in the process of leaving the queue.
	formatstr(expr_completely_done, "(%s =?= \"%s\")", ATTR_JOB_MANAGED, MANAGED_DONE);
	formatstr(expr_not_completely_done, "(%s =!= \"%s\")", ATTR_JOB_MANAGED, MANAGED_DONE);

	done = true;
}

void
CHECK_LEASES_signalHandler(int /* tid */)
{
	dprintf(D_FULLDEBUG,"Received CHECK_LEASES signal\n");

	if ( !checkLeasesSignaled ) {
		RequestContactSchedd();
		checkLeasesSignaled = true;
	}
}

void
doContactSchedd(int /* tid */)
{
	int rc;
	Qmgr_connection *schedd;
	BaseJob *curr_job;
	ClassAd *next_ad;
	char expr_buf[12000];
	bool schedd_updates_complete = false;
	bool schedd_deletes_complete = false;
	bool add_remove_jobs_complete = false;
	bool update_jobs_complete = false;
	bool commit_transaction = true;
	int failure_line_num = 0;
	bool send_reschedule = false;
	std::string error_str = "";
	std::vector<std::string> dirty_job_ids;
	PROC_ID job_id;
	CondorError errstack;

	dprintf(D_FULLDEBUG,"in doContactSchedd()\n");

	initJobExprs();

	contactScheddTid = TIMER_UNSET;

	schedd = ConnectQ( *ScheddObj, QMGMT_TIMEOUT, false, NULL, myUserName );
	if ( !schedd ) {
		error_str = "Failed to connect to schedd!";
		goto contact_schedd_failure;
	}


	// CheckLeases
	/////////////////////////////////////////////////////
	if ( checkLeasesSignaled ) {

		dprintf( D_FULLDEBUG, "querying for renewed leases\n" );

		// Grab the lease attributes of all the jobs in our global hashtable.

		for (auto& itr: BaseJob::JobsByProcId) {
			curr_job = itr.second;
			int new_expiration;

			rc = GetAttributeInt( curr_job->procID.cluster,
								  curr_job->procID.proc,
								  ATTR_TIMER_REMOVE_CHECK,
								  &new_expiration );
			if ( rc < 0 ) {
				if ( errno == ETIMEDOUT ) {
					failure_line_num = __LINE__;
					commit_transaction = false;
					goto contact_schedd_disconnect;
				} else {
						// This job doesn't have doesn't have a lease from
						// the submitter. Skip it.
					continue;
				}
			}
			curr_job->UpdateJobLeaseReceived( new_expiration );
		}

		checkLeasesSignaled = false;
	}	// end of handling check leases


	// AddJobs
	/////////////////////////////////////////////////////
	if ( addJobsSignaled || firstScheddContact ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for new jobs\n" );

		// Make sure we grab all Grid Universe jobs (except held ones
		// that we previously indicated we were done with)
		// when we first start up in case we're recovering from a
		// shutdown/meltdown.
		// Otherwise, grab all jobs that are unheld and aren't marked as
		// currently being managed.
		// If JobManaged is undefined, equate it with false.
		if ( firstScheddContact ) {
			// Grab all jobs for us to manage. This expression is a
			// derivative of the expression below for new jobs. We add
			// "|| Managed =?= TRUE" to also get jobs our previous
			// incarnation was in the middle of managing when it died
			// (if it died unexpectedly). With the new term, the
			// "&& Managed =!= TRUE" from the new jobs expression becomes
			// superfluous (by boolean logic), so we drop it.
			snprintf( expr_buf, sizeof(expr_buf),
					 "%s && %s && (%s || %s)",
					 expr_schedd_job_constraint.c_str(), 
					 expr_not_completely_done.c_str(),
					 expr_not_held.c_str(),
					 expr_managed.c_str()
					 );
		} else {
			// Grab new jobs for us to manage
			snprintf( expr_buf, sizeof(expr_buf),
					 "%s && %s && %s && %s",
					 expr_schedd_job_constraint.c_str(), 
					 expr_not_completely_done.c_str(),
					 expr_not_held.c_str(),
					 expr_not_managed.c_str()
					 );
		}
		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			bool job_is_managed = jobExternallyManaged(next_ad);

			auto itr = BaseJob::JobsByProcId.find(procID);
			if (itr == BaseJob::JobsByProcId.end()) {
				JobType *job_type = NULL;
				BaseJob *new_job = NULL;

				// Search our job types for one that'll handle this job
				for (auto & next_type: jobTypes) {
					if (next_type.AdMatchFunc(next_ad)) {

						// Found one!
						dprintf( D_FULLDEBUG, "Using job type %s for job %d.%d\n",
								 next_type.Name.c_str(), procID.cluster, procID.proc );
						job_type = &next_type;
						break;
					}
				}

				if ( job_type != NULL ) {
					new_job = job_type->CreateFunc( next_ad );
				} else {
					dprintf( D_ALWAYS, "No handlers for job %d.%d\n",
							 procID.cluster, procID.proc );
					new_job = new BaseJob( next_ad );
				}

				ASSERT(new_job);
				new_job->SetEvaluateState();
				dprintf(D_ALWAYS,"Found job %d.%d --- inserting\n",
						new_job->procID.cluster,new_job->procID.proc);
				num_ads++;

				if ( !job_is_managed ) {
					rc = tSetAttributeString( new_job->procID.cluster,
									   new_job->procID.proc,
									   ATTR_JOB_MANAGED,
									   MANAGED_EXTERNAL);
					if ( rc < 0 ) {
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
				}

			} else {

				// We already know about this job, skip
				// But also set Managed=true on the schedd so that it won't
				// keep signalling us about it
				delete next_ad;
				rc = tSetAttributeString( procID.cluster, procID.proc,
								   ATTR_JOB_MANAGED, MANAGED_EXTERNAL );
				if ( rc < 0 ) {
					failure_line_num = __LINE__;
					commit_transaction = false;
					goto contact_schedd_disconnect;
				}

			}

			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}	// end of while next_ad
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			commit_transaction = false;
			goto contact_schedd_disconnect;
		}

		dprintf(D_FULLDEBUG,"Fetched %d new job ads from schedd\n",num_ads);
	}	// end of handling add jobs


	// RemoveJobs
	/////////////////////////////////////////////////////

	// We always want to perform this check. Otherwise, we may overwrite a
	// REMOVED/HELD/COMPLETED status with something else below.
	{
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for removed/held jobs\n" );

		// Grab jobs marked as REMOVED/COMPLETED or marked as HELD that we
		// haven't previously indicated that we're done with (by setting
		// JobManaged to "Schedd".
		snprintf( expr_buf, sizeof(expr_buf), "(%s) && (%s) && (%s == %d || %s == %d || (%s == %d && %s =?= \"%s\"))",
				 ScheddJobConstraint, expr_not_completely_done.c_str(),
				 ATTR_JOB_STATUS, REMOVED,
				 ATTR_JOB_STATUS, COMPLETED, ATTR_JOB_STATUS, HELD,
				 ATTR_JOB_MANAGED, MANAGED_EXTERNAL );

		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			int curr_status;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			next_ad->LookupInteger( ATTR_JOB_STATUS, curr_status );

			auto itr = BaseJob::JobsByProcId.find(procID);
			if (itr != BaseJob::JobsByProcId.end()) {
				// Should probably skip jobs we already have marked as
				// held or removed

				itr->second->JobAdUpdateFromSchedd( next_ad, true );
				num_ads++;

			} else if ( curr_status == REMOVED ) {

				// If we don't know about the job, act like we got an
				// ADD_JOBS signal from the schedd the next time we
				// connect, so that we'll create a Job object for it
				// and decide how it needs to be handled.
				// TODO The AddJobs and RemoveJobs queries shoule be
				//   combined into a single query.
				dprintf( D_ALWAYS, 
						 "Don't know about removed job %d.%d. "
						 "Will treat it as a new job to manage\n",
						 procID.cluster, procID.proc );
				addJobsSignaled = true;

			} else {

				dprintf( D_ALWAYS, "Don't know about held/completed job %d.%d. "
						 "Ignoring it\n",
						 procID.cluster, procID.proc );

			}

			delete next_ad;
			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			commit_transaction = false;
			goto contact_schedd_disconnect;
		}

		dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);
	}

	if ( RemoteCommitTransaction() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	add_remove_jobs_complete = true;


	// Retrieve dirty attributes
	/////////////////////////////////////////////////////
	if ( updateJobsSignaled ) {
		dprintf( D_FULLDEBUG, "querying for jobs with attribute updates\n" );

		snprintf( expr_buf, sizeof(expr_buf), "%s && %s && %s && %s",
				 expr_schedd_job_constraint.c_str(), 
				 expr_not_completely_done.c_str(),
				 expr_not_held.c_str(),
				 expr_managed.c_str()
				 );
		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextDirtyJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			ClassAd updates;
			char str[PROC_ID_STR_BUFLEN];
			next_ad->LookupInteger( ATTR_CLUSTER_ID, job_id.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, job_id.proc );
			if ( GetDirtyAttributes( job_id.cluster, job_id.proc, &updates ) < 0 ) {
				dprintf( D_ALWAYS, "Failed to retrieve dirty attributes for job %d.%d\n", job_id.cluster, job_id.proc );
				failure_line_num = __LINE__;
				delete next_ad;
				goto contact_schedd_disconnect;
		        }
			else {
				dprintf (D_FULLDEBUG, "Retrieved updated attributes for job %d.%d\n", job_id.cluster, job_id.proc);
				dPrintAd(D_JOB, updates);
			}
			auto itr = BaseJob::JobsByProcId.find(job_id);
			if (itr != BaseJob::JobsByProcId.end()) {
				itr->second->JobAdUpdateFromSchedd( &updates, false );
				ProcIdToStr( job_id, str );
				dirty_job_ids.emplace_back( str );
			}
			else {
				dprintf( D_ALWAYS, "Don't know about updated job %d.%d. "
						 "Ignoring it\n",
						 job_id.cluster, job_id.proc );
			}
			delete next_ad;
			next_ad = GetNextDirtyJobByConstraint( expr_buf, 0 );
		}
	}
	update_jobs_complete = true;

//	if ( BeginTransaction() < 0 ) {
	errno = 0;
	BeginTransaction();
	if ( errno == ETIMEDOUT ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}


	// Update existing jobs
	/////////////////////////////////////////////////////
	for (auto& [key, curr_request]: pendingScheddUpdates) {

		curr_job = curr_request->m_job;
		dprintf(D_FULLDEBUG,"Updating classad values for %d.%d:\n",
				curr_job->procID.cluster, curr_job->procID.proc);
		const char *attr_name;
		const char *attr_value;
		ExprTree *expr;
		bool fake_job_in_queue = false;
		for ( auto itr = curr_job->jobAd->dirtyBegin(); itr != curr_job->jobAd->dirtyEnd() && fake_job_in_queue == false; itr++ ) {
			attr_name = itr->c_str();
			expr = curr_job->jobAd->LookupExpr( attr_name );
			if ( expr ) {
				attr_value = ExprTreeToString( expr );
			} else {
				attr_value = "Undefined";
			}

			dprintf(D_FULLDEBUG,"   %s = %s\n",attr_name,attr_value);
			rc = SetAttribute( curr_job->procID.cluster,
							   curr_job->procID.proc,
							   attr_name,
							   attr_value);
			if ( rc < 0 ) {
				if ( errno == ETIMEDOUT ) {
					failure_line_num = __LINE__;
					commit_transaction = false;
					goto contact_schedd_disconnect;
				} else if ( errno == ENOENT ) {
						// The job is not in the schedd's job queue. This
						// probably means that the user did a condor_rm -f,
						// so pretend that all updates for the job succeed.
						// Otherwise, we'll never make forward progress on
						// the job.
						// TODO We should also fake a job status of REMOVED
						//   to the job, so it can do what cleanup it can.
					fake_job_in_queue = true;
					break;
				} else {
					dprintf( D_FULLDEBUG, "     (Schedd rejected)\n" );
				}
			}
		}

	}

	if ( RemoteCommitTransaction() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	schedd_updates_complete = true;


	// Delete existing jobs
	/////////////////////////////////////////////////////
	errno = 0;
	BeginTransaction();
	if ( errno == ETIMEDOUT ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	for (auto& [key, curr_request]: pendingScheddUpdates) {

		curr_job = curr_request->m_job;
		if ( curr_job->deleteFromSchedd ) {
			dprintf(D_FULLDEBUG,"Deleting job %d.%d from schedd\n",
					curr_job->procID.cluster, curr_job->procID.proc);
			rc = DestroyProc(curr_job->procID.cluster,
							 curr_job->procID.proc);
				// NOENT means the job doesn't exist.  Good enough for us.
			if ( rc < 0 && rc != DESTROYPROC_ENOENT) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
		}

	}

	if ( RemoteCommitTransaction() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	schedd_deletes_complete = true;


 contact_schedd_disconnect:
	DisconnectQ( schedd, commit_transaction );

	if ( add_remove_jobs_complete == true ) {
		firstScheddContact = false;
		addJobsSignaled = false;
	} else {
		formatstr( error_str, "Schedd connection error during Add/RemoveJobs at line %d!", failure_line_num );
		goto contact_schedd_failure;
	}

	if ( update_jobs_complete == true ) {
		updateJobsSignaled = false;
	} else {
		formatstr( error_str, "Schedd connection error during dirty attribute update at line %d!", failure_line_num );
		goto contact_schedd_failure;
	}

	if ( schedd_updates_complete == false ) {
		formatstr( error_str, "Schedd connection error during updates at line %d!", failure_line_num );
		goto contact_schedd_failure;
	}

	// Clear dirty bits for all jobs updated
	if ( !dirty_job_ids.empty() ) {
		ClassAd *rval;
		dprintf( D_FULLDEBUG, "Calling clearDirtyAttrs on %zu jobs\n",
				 dirty_job_ids.size() );
		rval = ScheddObj->clearDirtyAttrs( dirty_job_ids, &errstack );
		if ( rval == NULL ) {
			dprintf(D_ALWAYS, "Failed to notify schedd to clear dirty attributes.  CondorError: %s\n", errstack.getFullText().c_str() );
		}
		delete rval;
	}

	// Wake up jobs that had schedd updates pending and delete job
	// objects that wanted to be deleted
	for (auto it = pendingScheddUpdates.begin(); it != pendingScheddUpdates.end();) {
		ScheddUpdateRequest* curr_request = it->second;
		curr_job = curr_request->m_job;
		curr_job->jobAd->ClearAllDirtyFlags();

		if ( curr_job->deleteFromGridmanager ) {

				// If the Job object wants to delete the job from the
				// schedd but we failed to do so, don't delete the job
				// object yet; wait until we successfully delete the job
				// from the schedd.
			if ( curr_job->deleteFromSchedd == true &&
				 schedd_deletes_complete == false ) {
				it++;
				continue;
			}

			it = pendingScheddUpdates.erase(it);
			delete curr_job;

		} else {
			it = pendingScheddUpdates.erase(it);

			if ( curr_request->m_notify ) {
				curr_job->SetEvaluateState();
			}
		}

		delete curr_request;
	}

	// Poke objects that wanted to be notified when a schedd update completed
	// successfully (possibly minus deletes)
	for (int timer_id : scheddUpdateNotifications) {
		daemonCore->Reset_Timer( timer_id, 0 );
	}
	scheddUpdateNotifications.clear();

	if ( send_reschedule == true ) {
		ScheddObj->reschedule();
	}

	// Check if we have any jobs left to manage. If not, exit.
	if ( BaseJob::JobsByProcId.size() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	lastContactSchedd = time(NULL);

	if ( schedd_deletes_complete == false ) {
		error_str = "Problem using DestroyProc to delete jobs!";
		goto contact_schedd_failure;
	}

	scheddFailureCount = 0;

	// For each job that had dirty attributes, re-evaluate the policy
	for (auto& job_id_str : dirty_job_ids) {
		StrToProcIdFixMe(job_id_str.c_str(), job_id);
		auto itr = BaseJob::JobsByProcId.find(job_id);
		if (itr != BaseJob::JobsByProcId.end()) {
			itr->second->EvalPeriodicJobExpr();
		}
	}

dprintf(D_FULLDEBUG,"leaving doContactSchedd()\n");
	return;

 contact_schedd_failure:
	scheddFailureCount++;
	if ( error_str == "" ) {
		error_str = "Failure in doContactSchedd";
	}
	if ( scheddFailureCount >= maxScheddFailures ) {
		dprintf( D_ALWAYS, "%s\n", error_str.c_str() );
		EXCEPT( "Too many failures connecting to schedd!" );
	}
	dprintf( D_ALWAYS, "%s Will retry\n", error_str.c_str() );
	lastContactSchedd = time(NULL);
	RequestContactSchedd();
	return;
}

int FetchProxyDelegationHandler(int, Stream *sock )
{
	ReliSock* rsock = (ReliSock*)sock;
	std::string xfer_id;
	INFNBatchJob *job = NULL;
	filesize_t file_size = 0;
	time_t expiration_time = 0;
	time_t result_expiration_time = 0;
	int rc = 0;
	std::map<std::string, BaseJob*>::iterator xfer_ptr;

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.
	rsock->timeout( 10 );
	rsock->decode();

	dprintf( D_FULLDEBUG, "FetchProxyDelegationHandler(): client: %s\n",
			 rsock->getAuthenticatedName() );

		// read the xfer id from the client
	rsock->decode();
	if ( !rsock->code( xfer_id ) || !rsock->end_of_message() ) {
		dprintf( D_ERROR, "FetchProxyDelegationHandler(): failed to read xfer id\n" );
		goto refuse;
	}
	dprintf( D_FULLDEBUG, "FetchProxyDelegationHandler(): xfer id: %s\n", xfer_id.c_str() );

	xfer_ptr = FetchProxyList.find( xfer_id );
	if ( xfer_ptr == FetchProxyList.end() ) {
		dprintf( D_ERROR, "FetchProxyDelegationHandler(): unknown transfer id: %s\n",
				 xfer_id.c_str() );
		goto refuse;
	}

	job = dynamic_cast<INFNBatchJob*>(xfer_ptr->second);
	if ( job == NULL ) {
		dprintf( D_ERROR, "FetchProxyDelegationHandler(): cast failed\n" );
		goto refuse;
	}

	dprintf( D_FULLDEBUG, "Delegating proxy %s\n", job->jobProxy->proxy_filename );

	// Delegate the proxy to the client
	rsock->encode();
	rsock->put( OK );
	rsock->end_of_message();

	expiration_time = GetDesiredDelegatedJobCredentialExpiration( job->jobAd );

	rc = rsock->put_x509_delegation( &file_size, job->jobProxy->proxy_filename,
									 expiration_time, &result_expiration_time );
	if ( rc < 0 ) {
			// transfer failed
		dprintf( D_ALWAYS, "FetchProxyDelegationHandler(): delegation failed\n" );
	}

	return TRUE;

 refuse:
	rsock->encode();
	rsock->put( NOT_OK );
	rsock->end_of_message();
	return FALSE;
}

