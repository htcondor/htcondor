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


#include "SchedDCommands.h"


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
