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


#ifndef SCHEDD_COMMANDS_H
#define SCHEDD_COMMANDS_H

#include "condor_common.h"
#include "condor_classad.h"

/**
 *
 * This class represents a simple data structure for
 * representing a job request to the SchedD
 *
 * Use one of the static methods to construct an instance
 *
 **/


struct job_expiration {
	int cluster;
	int proc;
	unsigned long expiration;

	void copy_from(const job_expiration & r) {
		cluster = r.cluster;
		proc = r.proc;
	    expiration = r.expiration;
	}

};
	
 
class SchedDRequest {
public:
	static SchedDRequest * createRemoveRequest (const int request_id,
												const int cluster_id,
												const int proc_id,
												const char * reason);

	static SchedDRequest * createHoldRequest (const int request_id,
												const int cluster_id,
												const int proc_id,
												const char * reason);

	static SchedDRequest * createReleaseRequest (const int request_id,
												const int cluster_id,
												const int proc_id,
												const char * reason);

	static SchedDRequest * createStatusConstrainedRequest (const int request_id,
															const char * constraint);

	static SchedDRequest * createUpdateConstrainedRequest (const int request_id,
															const char * constraint,
															const ClassAd * classad);


	static SchedDRequest * createUpdateRequest (const int request_id,
													const int cluster_id,
													const int proc_id,
													const ClassAd * classad);

	static SchedDRequest * createSubmitRequest (const int request_id,
													const ClassAd * classad);

	static SchedDRequest * createJobStageInRequest (const int request_id,
													const ClassAd * classad);

	static SchedDRequest * createJobStageOutRequest (const int request_id,
													 const int cluster_id,
													 const int proc_id);

	static SchedDRequest * createRefreshProxyRequest (const int request_id,
													  const int cluster_id,
													  const int proc_id,
													  const char * proxy_file,
													  time_t proxy_expiration);

	static SchedDRequest * createUpdateLeaseRequest (const int request_id,
													 const int num_jobs,
													 job_expiration* & expirations);

	~SchedDRequest() {
		if (classad)
			delete classad;
		if (constraint)
			free (constraint);
		if (reason)
			free (reason);
		if (proxy_file)
			free (proxy_file);
		if (expirations)
			delete [] expirations;

	}

	ClassAd * classad;
	char * constraint;
	int cluster_id;
	int proc_id;

	int request_id;

	char * reason;	// For release, remove, update
	char * proxy_file;	// For refresh_proxy
	time_t proxy_expiration;

	int num_jobs;
	job_expiration * expirations;

	// Status of the command
	enum {
		SDCS_NEW,
		SDCS_COMPLETED,
	} status;

	// Command being sent
typedef enum {
		SDC_REMOVE_JOB,
		SDC_HOLD_JOB,
		SDC_RELEASE_JOB,
		SDC_SUBMIT_JOB,
		SDC_COMPLETE_JOB,
		SDC_STATUS_CONSTRAINED,
		SDC_UPDATE_CONSTRAINED,
		SDC_UPDATE_JOB,
		SDC_JOB_STAGE_IN,
		SDC_JOB_STAGE_OUT,
		SDC_JOB_REFRESH_PROXY,
		SDC_UPDATE_LEASE,
} schedd_command_type;
	
	schedd_command_type command;

protected:
	SchedDRequest() {
		classad = NULL;
		constraint = NULL;
		cluster_id = -1;
		proc_id = -1;
		reason = NULL;
		proxy_file = NULL;
		proxy_expiration = 0;
		request_id = -1;
		expirations = NULL;
		num_jobs =0;
		status = SDCS_NEW;
		command = SDC_REMOVE_JOB;
	}

};

#endif
