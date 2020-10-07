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
#include "SchedDCommands.h"
#include "condor_attributes.h"


SchedDRequest * SchedDRequest::createRemoveRequest (const int request_id, int cluster_id, int proc_id, const char * reason) {
	SchedDRequest * req = new SchedDRequest;
	req->cluster_id = cluster_id;
	req->proc_id = proc_id;
	req->classad = NULL;
	req->constraint = NULL;
	req->reason = strdup(reason);

	req->command = SDC_REMOVE_JOB;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;
}

SchedDRequest * SchedDRequest::createStatusConstrainedRequest (const int request_id,
																const char * constraint) {
	SchedDRequest  * req = new SchedDRequest;
	req->cluster_id = -1;
	req->proc_id = -1;
	req->classad = NULL;
	req->constraint = (constraint != NULL) ? strdup (constraint) : NULL;

	req->command = SDC_STATUS_CONSTRAINED;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;
}

SchedDRequest * SchedDRequest::createUpdateConstrainedRequest (const int request_id,
															const char * constraint,
															const ClassAd * classad) {
	SchedDRequest  * req = new SchedDRequest;
	req->cluster_id = -1;
	req->proc_id = -1;
	req->constraint = (constraint != NULL) ? strdup (constraint) : NULL;
	req->classad = new ClassAd (*classad);

	req->command = SDC_UPDATE_CONSTRAINED;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;
}

SchedDRequest * SchedDRequest::createSubmitRequest (const int request_id,
													const ClassAd * classad) {
	SchedDRequest  * req = new SchedDRequest;
	req->cluster_id = -1;
	req->proc_id = -1;
	req->classad = NULL;
	req->constraint = NULL;
	req->classad = new ClassAd (*classad);

	req->command = SDC_SUBMIT_JOB;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;
}

SchedDRequest * SchedDRequest::createUpdateRequest (const int request_id,
													const int cluster_id,
													const int proc_id,
													const ClassAd * classad) {
	SchedDRequest  * req = new SchedDRequest;
	req->cluster_id = cluster_id;
	req->proc_id = proc_id;
	req->classad = NULL;
	req->constraint = NULL;
	req->classad = new ClassAd (*classad);

	req->command = SDC_UPDATE_JOB;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;

}

SchedDRequest * SchedDRequest::createHoldRequest (const int request_id, int cluster_id, int proc_id, const char * reason) {
	SchedDRequest * req = new SchedDRequest;
	req->cluster_id = cluster_id;
	req->proc_id = proc_id;
	req->classad = NULL;
	req->constraint = NULL;
	req->reason = strdup(reason);

	req->command = SDC_HOLD_JOB;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;
}

SchedDRequest * SchedDRequest::createReleaseRequest (const int request_id, int cluster_id, int proc_id, const char * reason) {
	SchedDRequest * req = new SchedDRequest;
	req->cluster_id = cluster_id;
	req->proc_id = proc_id;
	req->classad = NULL;
	req->constraint = NULL;
	req->reason = strdup(reason);

	req->command = SDC_RELEASE_JOB;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;
}

SchedDRequest * SchedDRequest::createJobStageInRequest (const int request_id,
														const ClassAd * classad) {

	SchedDRequest * req = new SchedDRequest;
	req->cluster_id = -1;
	req->proc_id = -1;
	req->classad = new ClassAd (*classad);
	req->constraint = NULL;
	req->reason = NULL;

	req->classad->LookupInteger( ATTR_CLUSTER_ID, req->cluster_id );
	req->classad->LookupInteger( ATTR_PROC_ID, req->proc_id );

	req->command = SDC_JOB_STAGE_IN;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;

}

SchedDRequest * SchedDRequest::createJobStageOutRequest (const int request_id,
														 const int cluster_id,
														 const int proc_id) {

	SchedDRequest * req = new SchedDRequest;
	req->cluster_id = cluster_id;
	req->proc_id = proc_id;
	req->classad = NULL;
	req->constraint = NULL;
	req->reason = NULL;

	req->command = SDC_JOB_STAGE_OUT;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;

}

SchedDRequest * SchedDRequest::createRefreshProxyRequest (const int request_id, int cluster_id, int proc_id, const char * proxy_file, time_t proxy_expiration) {
	SchedDRequest * req = new SchedDRequest;
	req->cluster_id = cluster_id;
	req->proc_id = proc_id;
	req->classad = NULL;
	req->constraint = NULL;
	req->proxy_file = strdup(proxy_file);
	req->proxy_expiration = proxy_expiration;

	req->command = SDC_JOB_REFRESH_PROXY;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	return req;
}

SchedDRequest * SchedDRequest::createUpdateLeaseRequest ( const int request_id,
													 const int num_jobs,
														job_expiration * &expirations) {
	SchedDRequest * req = new SchedDRequest;

	req->command = SDC_UPDATE_LEASE;
	req->status = SDCS_NEW;
	req->request_id = request_id;

	req->num_jobs = num_jobs;
	
	req->expirations = new job_expiration [num_jobs];
	int i;
	for (i=0; i < num_jobs; i++) {
		req->expirations[i].copy_from(expirations[i]);
	}

	return req;
}	
														 
