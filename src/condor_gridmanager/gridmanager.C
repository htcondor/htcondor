/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "daemon.h"
#include "dc_schedd.h"

#include "gridmanager.h"
#include "gahp-client.h"

#include "globusjob.h"

#if defined(ORACLE_UNIVERSE)
#include "oraclejob.h"
#endif

#if defined(NORDUGRID_UNIVERSE)
#include "nordugridjob.h"
#endif

#include "gt3job.h"

#define QMGMT_TIMEOUT 15

#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500

extern char *myUserName;

struct JobType
{
	char *Name;
	void(*InitFunc)();
	void(*ReconfigFunc)();
	const char *AdMatchConst;
	bool(*AdMustExpandFunc)(const ClassAd*);
	BaseJob *(*CreateFunc)(ClassAd*);
};

List<JobType> jobTypes;

// Stole these out of the schedd code
int procIDHash( const PROC_ID &procID, int numBuckets )
{
	//dprintf(D_ALWAYS,"procIDHash: cluster=%d proc=%d numBuck=%d\n",procID.cluster,procID.proc,numBuckets);
	return ( (procID.cluster+(procID.proc*19)) % numBuckets );
}

bool operator==( const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}

template class HashTable<PROC_ID, BaseJob *>;
template class HashBucket<PROC_ID, BaseJob *>;
template class List<BaseJob>;
template class Item<BaseJob>;
template class List<JobType>;
template class Item<JobType>;

HashTable <PROC_ID, BaseJob *> pendingScheddUpdates( HASH_TABLE_SIZE,
													 procIDHash );
bool addJobsSignaled = false;
bool removeJobsSignaled = false;
int contactScheddTid = TIMER_UNSET;
int contactScheddDelay;
time_t lastContactSchedd = 0;

char *ScheddAddr = NULL;
char *ScheddJobConstraint = NULL;
char *GridmanagerScratchDir = NULL;

HashTable <PROC_ID, BaseJob *> JobsByProcID( HASH_TABLE_SIZE,
											 procIDHash );

bool firstScheddContact = true;

char *Owner = NULL;

void RequestContactSchedd();
int doContactSchedd();

// handlers
int ADD_JOBS_signalHandler( int );
int REMOVE_JOBS_signalHandler( int );


bool JobMatchesConstraint( const ClassAd *jobad, const char *constraint )
{
	ExprTree *tree;
	EvalResult *val;

	val = new EvalResult;

	Parse( constraint, tree );
	if ( tree == NULL ) {
		dprintf( D_FULLDEBUG,
				 "Parse() returned a NULL tree on constraint '%s'\n",
				 constraint );
		return false;
	}
	tree->EvalTree(jobad, val);           // evaluate the constraint.
	if(!val || val->type != LX_INTEGER) {
		delete tree;
		delete val;
		dprintf( D_FULLDEBUG, "Constraint '%s' evaluated to wrong type\n",
				 constraint );
		return false;
	} else {
        if( !val->i ) {
			delete tree;
			delete val;
			return false; 
		}
	}

	delete tree;
	delete val;
	return true;
}

// Job objects should call this function when they have changes that need
// to be propagated to the schedd.
// return value of true means requested update has been committed to schedd.
// return value of false means requested update has been queued, but has not
//   been committed to the schedd yet
bool
requestScheddUpdate( BaseJob *job )
{
	BaseJob *hashed_job;

	// Check if there's anything that actually requires contacting the
	// schedd. If not, just return true (i.e. update is complete)
	job->ad->ResetExpr();
	if ( job->deleteFromGridmanager == false &&
		 job->deleteFromSchedd == false &&
		 job->ad->NextDirtyExpr() == NULL ) {
		return true;
	}

	// Check if the job is already in the hash table
	if ( pendingScheddUpdates.lookup( job->procID, hashed_job ) != 0 ) {

		pendingScheddUpdates.insert( job->procID, job );
		RequestContactSchedd();
	}

	return false;
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
												(TimerHandler)&doContactSchedd,
												"doContactSchedd", NULL );
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
		ScheddAddr = daemonCore->InfoCommandSinfulString( schedd_pid );
		if ( ScheddAddr == NULL ) {
			EXCEPT( "Failed to determine schedd's address" );
		} else {
			ScheddAddr = strdup( ScheddAddr );
		}
	}

	// read config file
	// initialize variables

	Owner = my_username();
	if ( Owner == NULL ) {
		EXCEPT( "Can't determine username" );
	}

	if ( GridmanagerScratchDir == NULL ) {
		EXCEPT( "Schedd didn't specify scratch dir with -S" );
	}

	if ( InitializeProxyManager( GridmanagerScratchDir ) == false ) {
		EXCEPT( "Failed to initialize Proxymanager" );
	}

	JobType *new_type;

#if defined(ORACLE_UNIVERSE)
	new_type = new JobType;
	new_type->Name = strdup( "Oracle" );
	new_type->InitFunc = OracleJobInit;
	new_type->ReconfigFunc = OracleJobReconfig;
	new_type->AdMatchConst = OracleJobAdConst;
	new_type->AdMustExpandFunc = OracleJobAdMustExpand;
	new_type->CreateFunc = OracleJobCreate;
	jobTypes.Append( new_type );
#endif

#if defined(NORDUGRID_UNIVERSE)
	new_type = new JobType;
	new_type->Name = strdup( "Nordugrid" );
	new_type->InitFunc = NordugridJobInit;
	new_type->ReconfigFunc = NordugridJobReconfig;
	new_type->AdMatchConst = NordugridJobAdConst;
	new_type->AdMustExpandFunc = NordugridJobAdMustExpand;
	new_type->CreateFunc = NordugridJobCreate;
	jobTypes.Append( new_type );
#endif

	new_type = new JobType;
	new_type->Name = strdup( "GT3" );
	new_type->InitFunc = GT3JobInit;
	new_type->ReconfigFunc = GT3JobReconfig;
	new_type->AdMatchConst = GT3JobAdConst;
	new_type->AdMustExpandFunc = GT3JobAdMustExpand;
	new_type->CreateFunc = GT3JobCreate;
	jobTypes.Append( new_type );

	new_type = new JobType;
	new_type->Name = strdup( "Globus" );
	new_type->InitFunc = GlobusJobInit;
	new_type->ReconfigFunc = GlobusJobReconfig;
	new_type->AdMatchConst = GlobusJobAdConst;
	new_type->AdMustExpandFunc = GlobusJobAdMustExpand;
	new_type->CreateFunc = GlobusJobCreate;
	jobTypes.Append( new_type );

	jobTypes.Rewind();
	while ( jobTypes.Next( new_type ) ) {
		new_type->InitFunc();
	}

}

void
Register()
{
	daemonCore->Register_Signal( GRIDMAN_ADD_JOBS, "AddJobs",
								 (SignalHandler)&ADD_JOBS_signalHandler,
								 "ADD_JOBS_signalHandler", NULL, WRITE );

	daemonCore->Register_Signal( GRIDMAN_REMOVE_JOBS, "RemoveJobs",
								 (SignalHandler)&REMOVE_JOBS_signalHandler,
								 "REMOVE_JOBS_signalHandler", NULL, WRITE );

	Reconfig();
}

void
Reconfig()
{
	// This method is called both at startup [from method Init()], and
	// when we are asked to reconfig.

	contactScheddDelay = param_integer("GRIDMANAGER_CONTACT_SCHEDD_DELAY", 5);

	GahpReconfig();

	JobType *job_type;
	jobTypes.Rewind();
	while ( jobTypes.Next( job_type ) ) {
		job_type->ReconfigFunc();
	}

	// Tell all the job objects to deal with their new config values
	BaseJob *next_job;

	JobsByProcID.startIterations();

	while ( JobsByProcID.iterate( next_job ) != 0 ) {
		next_job->Reconfig();
	}
}

int
ADD_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received ADD_JOBS signal\n");

	if ( !addJobsSignaled ) {
		RequestContactSchedd();
		addJobsSignaled = true;
	}

	return TRUE;
}

int
REMOVE_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received REMOVE_JOBS signal\n");

	if ( !removeJobsSignaled ) {
		RequestContactSchedd();
		removeJobsSignaled = true;
	}

	return TRUE;
}

int
doContactSchedd()
{
	int rc;
	Qmgr_connection *schedd;
	BaseJob *curr_job;
	ClassAd *next_ad;
	char expr_buf[12000];
	bool schedd_updates_complete = false;
	bool schedd_deletes_complete = false;
	bool add_remove_jobs_complete = false;
	bool commit_transaction = true;
	List<BaseJob> successful_deletes;
	int failure_line_num = 0;

	dprintf(D_FULLDEBUG,"in doContactSchedd()\n");

	contactScheddTid = TIMER_UNSET;

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}


	// AddJobs
	/////////////////////////////////////////////////////
	if ( addJobsSignaled || firstScheddContact ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for new jobs\n" );

		// Make sure we grab all Globus Universe jobs (except held ones
		// that we previously indicated we were done with)
		// when we first start up in case we're recovering from a
		// shutdown/meltdown.
		// Otherwise, grab all jobs that are unheld and aren't marked as
		// currently being managed and aren't marked as not matched.
		// If JobManaged is undefined, equate it with false.
		// If Matched is undefined, equate it with true.
		// NOTE: Schedds from Condor 6.6 and earlier don't include
		//   "(Universe==9)" in the constraint they give to the gridmanager,
		//   so this gridmanager will pull down non-globus-universe ads,
		//   although it won't use them. This is inefficient but not
		//   incorrect behavior.
		if ( firstScheddContact ) {
			// Grab all jobs for us to manage. This expression is a
			// derivative of the expression below for new jobs. We add
			// "|| Managed =?= TRUE" to also get jobs our previous
			// incarnation was in the middle of managing when it died
			// (if it died unexpectedly). With the new term, the
			// "&& Managed =!= TRUE" from the new jobs expression becomes
			// superfluous (by boolean logic), so we drop it.
			sprintf( expr_buf,
					 "(%s) && ((%s =!= FALSE && %s != %d) || %s =?= TRUE)",
					 ScheddJobConstraint, ATTR_JOB_MATCHED,
					 ATTR_JOB_STATUS, HELD, ATTR_JOB_MANAGED );
		} else {
			// Grab new jobs for us to manage
			sprintf( expr_buf,
					 "(%s) && %s =!= FALSE && %s =!= TRUE && %s != %d",
					 ScheddJobConstraint, ATTR_JOB_MATCHED, ATTR_JOB_MANAGED,
					 ATTR_JOB_STATUS, HELD );
		}
		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			BaseJob *old_job;
			int job_is_managed = 0;		// default to false if not in ClassAd
			int job_is_matched = 1;		// default to true if not in ClassAd

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			next_ad->LookupBool(ATTR_JOB_MANAGED,job_is_managed);
			next_ad->LookupBool(ATTR_JOB_MATCHED,job_is_matched);

			if ( JobsByProcID.lookup( procID, old_job ) != 0 ) {

				int rc;
				JobType *job_type = NULL;
				BaseJob *new_job = NULL;

				// job had better be either managed or matched! (or both)
				ASSERT( job_is_managed || job_is_matched );

				// Search our job types for one that'll handle this job
				jobTypes.Rewind();
				while ( jobTypes.Next( job_type ) ) {
dprintf(D_FULLDEBUG,"***Trying job type %s\n",job_type->Name);
					if ( JobMatchesConstraint( next_ad, job_type->AdMatchConst ) ) {

						// Found one!
						dprintf( D_FULLDEBUG, "Using job type %s for job %d.%d\n",
								 job_type->Name, procID.cluster, procID.proc );
						break;
					}
				}

				if ( job_type != NULL ) {
					if ( job_type->AdMustExpandFunc( next_ad ) ) {
						// Get the expanded ClassAd from the schedd, which
						// has the globus resource filled in with info from
						// the matched ad.
						delete next_ad;
						next_ad = NULL;
						next_ad = GetJobAd(procID.cluster,procID.proc);
						if ( next_ad == NULL && errno == ETIMEDOUT ) {
							failure_line_num = __LINE__;
							commit_transaction = false;
							goto contact_schedd_disconnect;
						}
						ASSERT(next_ad);
					}
					new_job = job_type->CreateFunc( next_ad );
				} else {
					dprintf( D_ALWAYS, "No handlers for job %d.%d",
							 procID.cluster, procID.proc );
					new_job = new BaseJob( next_ad );
				}

				ASSERT(new_job);
				new_job->SetEvaluateState();
				dprintf(D_ALWAYS,"Found job %d.%d --- inserting\n",
						new_job->procID.cluster,new_job->procID.proc);
				JobsByProcID.insert( new_job->procID, new_job );
				num_ads++;

				if ( !job_is_managed ) {
					rc = SetAttribute( new_job->procID.cluster,
									   new_job->procID.proc,
									   ATTR_JOB_MANAGED,
									   "TRUE" );
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
				rc = SetAttribute( procID.cluster, procID.proc,
								   ATTR_JOB_MANAGED, "TRUE" );
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
	if ( removeJobsSignaled ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for removed/held jobs\n" );

		// Grab jobs marked as REMOVED or marked as HELD that we haven't
		// previously indicated that we're done with (by setting JobManaged
		// to FALSE. If JobManaged is undefined, equate it with false.
		sprintf( expr_buf, "(%s) && (%s == %d || (%s == %d && %s =?= TRUE))",
				 ScheddJobConstraint, ATTR_JOB_STATUS, REMOVED,
				 ATTR_JOB_STATUS, HELD, ATTR_JOB_MANAGED );

		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			BaseJob *next_job;
			int curr_status;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			next_ad->LookupInteger( ATTR_JOB_STATUS, curr_status );

			if ( JobsByProcID.lookup( procID, next_job ) == 0 ) {
				// Should probably skip jobs we already have marked as
				// held or removed

				next_job->JobAdUpdateFromSchedd( next_ad );
				num_ads++;

			} else if ( curr_status == REMOVED ) {

				// If we don't know about the job, remove it immediately
				// I don't think this can happen in the normal case,
				// but I'm not sure.
				dprintf( D_ALWAYS, 
						 "Don't know about removed job %d.%d. "
						 "Deleting it immediately\n", procID.cluster,
						 procID.proc );
				// Log the removal of the job from the queue
				WriteAbortEventToUserLog( next_ad );
				rc = DestroyProc( procID.cluster, procID.proc );
				if ( rc < 0 ) {
					failure_line_num = __LINE__;
					delete next_ad;
					commit_transaction = false;
					goto contact_schedd_disconnect;
				}

			} else {

				dprintf( D_ALWAYS, "Don't know about held job %d.%d. "
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

	if ( CloseConnection() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	add_remove_jobs_complete = true;

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
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_job ) != 0 ) {

		dprintf(D_FULLDEBUG,"Updating classad values for %d.%d:\n",
				curr_job->procID.cluster, curr_job->procID.proc);
		char attr_name[1024];
		char attr_value[1024];
		ExprTree *expr;
		bool fake_job_in_queue = false;
		curr_job->ad->ResetExpr();
		while ( (expr = curr_job->ad->NextDirtyExpr()) != NULL &&
				fake_job_in_queue == false ) {
			attr_name[0] = '\0';
			attr_value[0] = '\0';
			expr->LArg()->PrintToStr(attr_name);
			expr->RArg()->PrintToStr(attr_value);

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
				} else {
						// The job is not in the schedd's job queue. This
						// probably means that the user did a condor_rm -f,
						// so pretend that all updates for the job succeed.
						// Otherwise, we'll never make forward progress on
						// the job.
						// TODO We should also fake a job status of REMOVED
						//   to the job, so it can do what cleanup it can.
					fake_job_in_queue = true;
					break;
				}
			}
		}

	}

	if ( CloseConnection() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	schedd_updates_complete = true;


	// Delete existing jobs
	/////////////////////////////////////////////////////
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_job ) != 0 ) {

		if ( curr_job->deleteFromSchedd ) {
			dprintf(D_FULLDEBUG,"Deleting job %d.%d from schedd\n",
					curr_job->procID.cluster, curr_job->procID.proc);
			BeginTransaction();
			if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			rc = DestroyProc(curr_job->procID.cluster,
							 curr_job->procID.proc);
			if ( rc < 0 ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			if ( CloseConnection() < 0 ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			successful_deletes.Append( curr_job );
		}

	}

	schedd_deletes_complete = true;


 contact_schedd_disconnect:
	DisconnectQ( schedd, commit_transaction );

	if ( add_remove_jobs_complete == true ) {
		firstScheddContact = false;
		addJobsSignaled = false;
		removeJobsSignaled = false;
	} else {
		dprintf( D_ALWAYS, "Schedd connection error during Add/RemoveJobs at line %d! Will retry\n", failure_line_num );
		RequestContactSchedd();
		return TRUE;
	}

	if ( schedd_updates_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error during updates at line %d! Will retry\n", failure_line_num );
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}

	// Wake up jobs that had schedd updates pending and delete job
	// objects that wanted to be deleted
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_job ) != 0 ) {

		curr_job->ad->ClearAllDirtyFlags();

		if ( curr_job->deleteFromGridmanager ) {

				// If the Job object wants to delete the job from the
				// schedd but we failed to do so, don't delete the job
				// object yet; wait until we successfully delete the job
				// from the schedd.
			if ( curr_job->deleteFromSchedd == true &&
				 successful_deletes.Delete( curr_job ) == false ) {
				continue;
			}

			JobsByProcID.remove( curr_job->procID );
				// If wantRematch is set, send a reschedule now
			if ( curr_job->wantRematch ) {
				static DCSchedd* schedd_obj = NULL;
				if ( !schedd_obj ) {
					schedd_obj = new DCSchedd(NULL,NULL);
					ASSERT(schedd_obj);
				}
				schedd_obj->reschedule();
			}
			pendingScheddUpdates.remove( curr_job->procID );
			delete curr_job;

		} else {
			pendingScheddUpdates.remove( curr_job->procID );

			curr_job->SetEvaluateState();
		}

	}

	// Check if we have any jobs left to manage. If not, exit.
	if ( JobsByProcID.getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	lastContactSchedd = time(NULL);

	if ( schedd_deletes_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error! Will retry\n" );
		RequestContactSchedd();
	}

dprintf(D_FULLDEBUG,"leaving doContactSchedd()\n");
	return TRUE;
}

