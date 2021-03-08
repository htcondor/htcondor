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

#ifndef INCLUDE_SUBMIT_JOB_H
#define INCLUDE_SUBMIT_JOB_H

#include "condor_classad.h"

namespace classad { class ClassAd; }

enum ClaimJobResult { 
	CJR_OK,       // Job claimed (put in managed state)
	CJR_BUSY,     // The job was not IDLE or is claimed by another manager
	CJR_ERROR     // Something went wrong, probably failure to connect to the schedd.
};

/*
	Assuming an active qmgr connection to the schedd holding the job,
	attempt to the put cluster.proc into the managed state (claiming it
	for our own management).  Before doing so, does a last minute check
	to confirm the job is IDLE.  error_details is optional. If non NULL,
	error_details will contain a message explaining why claim_job didn't
	return OK.  The message will be suitable for reporting to a log file.

	my_identity is an optional string.  If non-NULL, it will be inserted
	into the job's classad as 1. a useful debugging identifier and 
	2. a double check that we're the current owner.  The identity is an
	arbitrary string, but should as uniquely as possible identify
	this process.  "$(SUBSYS) /path/to/condor_config" is recommended.
*/
ClaimJobResult claim_job(int cluster, int proc, std::string * error_details,
	const char * my_identity, bool target_is_sandboxed);

/*
	As the above claim_job, but will attempt to create the qmgr connection
	itself.  pool_name can be null to indicate "whatever COLLECTOR_HOST"
	is set to.  schedd_name can be null to indicate "local schedd".

	Attempt to the put cluster.proc into the managed state (claiming it
	for our own management).  Before doing so, does a last minute check
	to confirm the job is IDLE.  error_details is optional. If non NULL,
	error_details will contain a message explaining why claim_job didn't
	return OK.  The message will be suitable for reporting to a log file.
    The ClassAd is used to set priv state appropriately for manipulating
    the job through qmgmt.
*/
ClaimJobResult claim_job(classad::ClassAd const &ad, const char * pool_name, const char * schedd_name, int cluster, int proc, std::string * error_details, const char * my_identity, bool target_is_sandboxed);


/*
	The opposite of claim_job.  Assuming the job is claimed (managed)
	yield it.

	done - true to indiciate the job is all done, the schedd should
	       take no more efforts to manage it (short of evaluating
		   LeaveJobInQueue)
           false to indicate that the job is unfinished, the schedd
		   is free to do what it likes, including probably trying to run it.
	my_identity - If non null, will be checked against the identity registered
		as having claimed the job.  If a different identity claimed the
		job, the yield attempt fails.  See claim_job for details
		on suggested identity strings.
*/
bool yield_job(bool done, int cluster, int proc, classad::ClassAd const &job_ad, std::string * error_details = 0, const char * my_identity = 0, bool target_is_sandboxed=true, bool release_on_hold = true, bool *keep_trying = 0);
bool yield_job(classad::ClassAd const &ad, const char * pool_name,
	const char * schedd_name, bool done, int cluster, int proc,
	std::string * error_details = 0, const char * my_identity = 0, bool target_is_sandboxed = true,
	bool release_on_hold = true, bool *keep_trying = 0);

/* 
- src - The classad to submit.  The ad will be modified based on the needs of
  the submission.  This will include adding QDate, ClusterId and ProcId as well as putting the job on hold (for spooling)

- schedd_name - Name of the schedd to send the job to, as specified in the Name
  attribute of the schedd's classad.  Can be NULL to indicate "local schedd"

- pool_name - hostname (and possible port) of collector where schedd_name can
  be found.  Can be NULL to indicate "whatever COLLECTOR_HOST in my
  condor_config file claims".

*/
bool submit_job( const std::string &owner, const std::string &domain, ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out = 0, int * proc_out = 0 );


/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Assumes the existance of an open qmgr connection (via ConnectQ).

	Likely usage:
	// original_vanilla_ad is the ad passed to VanillaToGrid
	//   (it can safely a more recent version pulled from the queue)
	// new_grid_ad is the resulting ad from VanillaToGrid, with any updates 
	//   (indeed, it should probably be the most recent version from the queue)
	original_vanilla_ad.ClearAllDirtyFlags();
	update_job_status(original_vanilla_ad, new_grid_ad);
	push_dirty_attributes(original_vanilla_ad);
*/
bool push_dirty_attributes(classad::ClassAd & src);

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes. 
	Establishes (and tears down) a qmgr connection.
	schedd_name and pool_name can be NULL to indicate "local".
*/
bool push_dirty_attributes(classad::ClassAd & src, const char * schedd_name, const char * pool_name);

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
	Establishes (and tears down) a qmgr connection.
	schedd_name and pool_name can be NULL to indicate "local".
*/
bool push_classad_diff(classad::ClassAd & src, classad::ClassAd & dest, const char * schedd_name, const char * pool_name);

/*

Pull a submit_job() job's results out of the sandbox and place them back where
they came from.  If successful, let the job leave the queue.

ad - ClassAd of job (for owner info when switching privs)
cluster.proc - ID of the grid (transformed) job.
schedd_name and pool_name can be NULL to indicate "local".

*/
bool finalize_job(const std::string &owner, const std::string &domain, classad::ClassAd const &ad, int cluster, int proc, const char * schedd_name, const char * pool_name, bool is_sandboxed);

/*

Remove a job from the schedd.

ad - ClassAd of job (for owner info when switching privs)
cluster.proc - ID of the job to remove
reason - description of reason for removal
schedd_name and pool_name can be NULL to indicate "local"

 */
bool remove_job(classad::ClassAd const &ad, int cluster, int proc, char const *reason, const char * schedd_name, const char * pool_name, std::string &error_desc);


bool WriteTerminateEventToUserLog( classad::ClassAd const &ad );
bool WriteAbortEventToUserLog( classad::ClassAd const &ad );
bool EmailTerminateEvent( classad::ClassAd const &ad );
bool WriteHoldEventToUserLog( classad::ClassAd const &ad );
bool WriteExecuteEventToUserLog( classad::ClassAd const &ad );
bool WriteEvictEventToUserLog( classad::ClassAd const &ad );


#endif /* INCLUDE_SUBMIT_JOB_H*/
