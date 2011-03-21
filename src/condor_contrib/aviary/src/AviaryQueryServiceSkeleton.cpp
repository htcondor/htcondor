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

using namespace AviaryQuery;
using namespace AviaryCommon;
using namespace aviary::query;

typedef std::vector<JobID*> JobIdVectorType;
typedef std::vector<AviaryCommon::JobStatus*> JobStatusVectorType;

GetSubmissionSummaryResponse* AviaryQueryServiceSkeleton::getSubmissionSummary(wso2wsf::MessageContext *outCtx ,GetSubmissionSummary* _getSubmissionSummary)
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
//	else if (_getSubmissionSummary->getAllowPartialMatching()) {
		// we are partially matching so try to return submissions
		// with id or owner substrings based on the input
		// TODO: multimap?
//	}
	else {
		// we search exactly for the provided search id

		if (element != g_submissions.end()) {
		}
	}

    return getSummaryResponse;
}

GetJobStatusResponse* AviaryQueryServiceSkeleton::getJobStatus(wso2wsf::MessageContext *outCtx ,GetJobStatus* _getJobStatus)
{
	GetJobStatusResponse* jobStatusResponse = new GetJobStatusResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobStatusVectorType* job_results = new JobStatusVectorType;

	JobIdVectorType* id_list = _getJobStatus->getIds();
	for (JobIdVectorType::iterator i = id_list->begin(); id_list->end() != i; i++) {
		JobCollectionType::const_iterator element = g_jobs.find ( (*i)->getJob().c_str() );
		if (element != g_jobs.end()) {
			Job* job = (Job*) element->second;
			JobStatusType* jst = new JobStatusType;
			jst->setJobStatusTypeEnum(ADBJobStatusTypeEnum(job->getStatus()));
			JobStatus* js = new JobStatus(
								new JobID(string(""),string(""),(*i)->getJob(),
									new SubmissionID("","")),
							  jst,
							  new Status(new StatusCodeType("SUCCESS"),""));
			job_results->push_back(js);
		}
		else {
			// couldn't find it...report to client
			JobStatus* js = new JobStatus(new JobID(string(""),string(""),(*i)->getJob(),
									new SubmissionID("","")), NULL,
								  new Status(new StatusCodeType("NO_MATCH"),"job not found"));
			job_results->push_back(js);
		}
	}

	jobStatusResponse->setJobs(job_results);
	jobStatusResponse->setStatus(new Status(new StatusCodeType("SUCCESS"),""));

    return jobStatusResponse;
}

GetJobDetailsResponse* AviaryQueryServiceSkeleton::getJobDetails(wso2wsf::MessageContext *outCtx ,GetJobDetails* _getJobDetails)
{
    /* TODO fill this with the necessary business logic */
    return (GetJobDetailsResponse*)NULL;
}

GetJobDataResponse* AviaryQueryServiceSkeleton::getJobData(wso2wsf::MessageContext *outCtx ,GetJobData* _getJobData)
{
    GetJobDataResponse* getDataResponse = new  GetJobDataResponse;
	JobServerObject* jso = JobServerObject::getInstance();

    return getDataResponse;
}
