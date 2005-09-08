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
													  const char * proxy_file);

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

	int num_jobs;
	job_expiration * expirations;

	// Status of the command
	enum {
		SDCS_NEW,
		SDCS_PENDING,
		SDCS_COMPLETED,
		SDCS_ERROR,
	} status;

	// Command being sent
typedef enum {
		SDC_NOOP,
		SDC_ERROR,
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
		request_id = -1;
		expirations = NULL;
		num_jobs =0;
	}

};

#endif
