/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// the implementation methods for AviaryQueryService methods

//local includes
#include "Globals.h"
#include "JobServerObject.h"
#include "AviaryQueryServiceSkeleton.h"
#include <AviaryQuery_GetJobData.h>
#include <AviaryQuery_GetJobDataResponse.h>
#include <AviaryQuery_GetJobStatus.h>
#include <AviaryQuery_GetJobStatusResponse.h>
#include <AviaryQuery_GetSubmissionSummary.h>
#include <AviaryQuery_GetSubmissionSummaryResponse.h>
#include <AviaryQuery_GetJobDetails.h>
#include <AviaryQuery_GetJobDetailsResponse.h>

// condor includes
#include "stl_string_utils.h"
#include "proc.h"

using namespace std;
using namespace AviaryQuery;
using namespace AviaryCommon;
using namespace aviary::query;

typedef vector<JobID*> JobIdVectorType;
typedef vector<JobStatus*> JobStatusVectorType;
typedef vector<JobDetails*> JobDetailsVectorType;
typedef vector<JobData*> JobDataVectorType;

// NOTE #1: unfortunately the Axis2/C generated code is inconsistent in its
// internal checking of nillable (i.e., minOccurs=0) elements
// so we have to use default ctors and build the element object using setters
// otherwise segvs await us in generated code

// NOTE #2: using template functions since WSO2 codegen won't give us
// XSD extension->C++ inheritance
template <class JobBase>
void createFoundJob(JobBase& jb, const char* job_id) {
	SubmissionObject* so = NULL;
	JobServerObject* jso = JobServerObject::getInstance();

	JobID* jid = new JobID;
	jid->setJob(job_id);
	jid->setPool(jso->getPool());
	jid->setScheduler(jso->getName());
	jb.setId(jid);
	Status* jst = new Status;
	jst->setCode(new StatusCodeType("SUCCESS"));
	jb.setStatus(jst);
}

template <class JobBase>
void createMissedJob(JobBase& jb, const char* job_id) {
	string unfound;
	JobServerObject* jso = JobServerObject::getInstance();

	sprintf(unfound, "job '%s' not found at scheduler '%s:%s'",
			job_id, jso->getPool(),jso->getName());
	JobID* jid = new JobID;
	jid->setJob(job_id);
	jb.setId(jid);
	Status* jst = new Status(new StatusCodeType("NO_MATCH"),unfound);
	jb.setStatus(jst);
}

GetSubmissionSummaryResponse* AviaryQueryServiceSkeleton::getSubmissionSummary(wso2wsf::MessageContext* /*outCtx*/
	,GetSubmissionSummary* _getSubmissionSummary)
{
	GetSubmissionSummaryResponse* getSummaryResponse = new GetSubmissionSummaryResponse;

	SubmissionCollectionType::const_iterator element = g_submissions.begin();
	SubmissionObject *submission;

	if (_getSubmissionSummary->isIdsNil()) {
		// no ids to scan, so they get everything
		for (SubmissionCollectionType::iterator i = g_submissions.begin();
			 g_submissions.end() != i; i++) {
			submission = (*i).second;
			JobID* jobId = new JobID();
			//SubmissionID* subId = new SubmissionID;
		}
	}
	// check to see if we need to scope our search
	else if (_getSubmissionSummary->getPartialMatches()) {
		// we are partially matching so try to return submissions
		// with id or owner substrings based on the input
		// TODO: multimap?
	}
	else {
		// we search exactly for the provided search id

		if (element != g_submissions.end()) {
		}
	}

    return getSummaryResponse;
}

GetJobStatusResponse* AviaryQueryServiceSkeleton::getJobStatus(wso2wsf::MessageContext* /*outCtx*/
	,GetJobStatus* _getJobStatus)
{
	GetJobStatusResponse* jobStatusResponse = new GetJobStatusResponse;
	JobStatusVectorType* job_results = new JobStatusVectorType;

	JobIdVectorType* id_list = _getJobStatus->getIds();
	for (JobIdVectorType::iterator i = id_list->begin(); id_list->end() != i; i++) {
		JobCollectionType::const_iterator element = g_jobs.find ( (*i)->getJob().c_str() );
		JobStatus* js = new JobStatus;
		if (element != g_jobs.end()) {
			Job* job = (Job*) element->second;
			createFoundJob<JobStatus>(*js,job->getKey());
			JobStatusType* jst = new JobStatusType;
			jst->setJobStatusType(getJobStatusString(job->getStatus()));
			js->setJob_status(jst);
		}
		else {
			// couldn't find it...report to client
			createMissedJob<JobStatus>(*js,(*i)->getJob().c_str());
		}
		job_results->push_back(js);
	}

	jobStatusResponse->setJobs(job_results);
	Status* status = new Status;
	status->setCode(new StatusCodeType("SUCCESS"));
	jobStatusResponse->setStatus(status);

    return jobStatusResponse;
}

GetJobDetailsResponse* AviaryQueryServiceSkeleton::getJobDetails(wso2wsf::MessageContext* /* outCtx*/
	,GetJobDetails* _getJobDetails)
{
	GetJobDetailsResponse* jobDetailsResponse = new GetJobDetailsResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobDetailsVectorType* job_results = new JobDetailsVectorType;

	JobIdVectorType* id_list = _getJobDetails->getIds();
	for (JobIdVectorType::iterator i = id_list->begin(); id_list->end() != i; i++) {
		JobCollectionType::const_iterator element = g_jobs.find ( (*i)->getJob().c_str() );
		JobDetails* jd = new JobDetails;
		if (element != g_jobs.end()) {
			Job* job = (Job*) element->second;
			createFoundJob<JobDetails>(*jd,job->getKey());
			// TODO: get attributes
		}
		else {
			// couldn't find it...report to client
			createMissedJob<JobDetails>(*jd, (*i)->getJob().c_str());
		}
		job_results->push_back(jd);
	}

	jobDetailsResponse->setJobs(job_results);
	Status* status = new Status;
	status->setCode(new StatusCodeType("SUCCESS"));
	jobDetailsResponse->setStatus(status);
    return jobDetailsResponse;
}

GetJobDataResponse* AviaryQueryServiceSkeleton::getJobData(wso2wsf::MessageContext* /* outCtx */
	,GetJobData* _getJobData)
{
    GetJobDataResponse* jobDataResponse = new  GetJobDataResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobDataVectorType* job_results = new JobDataVectorType;

	JobIdVectorType* id_list = _getJobData->getIds();
	for (JobIdVectorType::iterator i = id_list->begin(); id_list->end() != i; i++) {
		JobCollectionType::const_iterator element = g_jobs.find ( (*i)->getJob().c_str() );
		JobData* jd = new JobData;
		if (element != g_jobs.end()) {
			Job* job = (Job*) element->second;
			createFoundJob<JobData>(*jd,job->getKey());
			// TODO: load requested file data
		}
		else {
			// couldn't find it...report to client
			createMissedJob<JobData>(*jd, (*i)->getJob().c_str());
		}
		job_results->push_back(jd);
	}

	jobDataResponse->setJobs(job_results);
	Status* status = new Status;
	status->setCode(new StatusCodeType("SUCCESS"));
	jobDataResponse->setStatus(status);
    return jobDataResponse;
	
    return jobDataResponse;
}
