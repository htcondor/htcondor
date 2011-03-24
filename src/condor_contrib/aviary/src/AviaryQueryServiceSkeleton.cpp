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
#include <AviaryQuery_GetJobSummary.h>
#include <AviaryQuery_GetJobSummaryResponse.h>

// condor includes
#include "stl_string_utils.h"
#include "proc.h"
#include "condor_attributes.h"

using namespace std;
using namespace AviaryQuery;
using namespace AviaryCommon;
using namespace aviary::query;

typedef vector<JobID*> JobIdVectorType;
typedef vector<JobStatus*> JobStatusVectorType;
typedef vector<JobDetails*> JobDetailsVectorType;
typedef vector<JobSummary*> JobSummaryVectorType;

// NOTE #1: unfortunately the Axis2/C generated code is inconsistent in its
// internal checking of nillable (i.e., minOccurs=0) elements
// so we have to use default ctors and build the element object using setters
// otherwise segvs await us in generated code

// NOTE #2: using template functions since WSO2 codegen won't give us
// XSD extension->C++ inheritance
template <class JobBase>
void createGoodJobResponse(JobBase& jb, const char* job_id) {
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
void createFailJobResponse(JobBase& jb, const char* job_id, const string error) {
	JobID* jid = new JobID;
	jid->setJob(job_id);
	jb.setId(jid);
	Status* jst = new Status(new StatusCodeType("FAIL"),error);
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
			//JobID* jobId = new JobID();
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
	JobServerObject* jso = JobServerObject::getInstance();
	JobStatusVectorType* job_results = new JobStatusVectorType;

	JobIdVectorType* id_list = _getJobStatus->getIds();
	for (JobIdVectorType::iterator i = id_list->begin(); id_list->end() != i; i++) {
		JobStatus* js = new JobStatus;
		const char* job = (*i)->getJob().c_str();
		string error;
		int job_status = JOB_STATUS_MIN;
		if (jso->getStatus((*i)->getJob().c_str(),job_status,error)) {
			createGoodJobResponse<JobStatus>(*js,job);
			JobStatusType* jst = new JobStatusType;
			jst->setJobStatusType(getJobStatusString(job_status));
			js->setJob_status(jst);
		}
		else {
			// problem...report to client
			createFailJobResponse<JobStatus>(*js,job,error);
		}
		job_results->push_back(js);
	}

	jobStatusResponse->setJobs(job_results);
	Status* status = new Status;
	status->setCode(new StatusCodeType("SUCCESS"));
	jobStatusResponse->setStatus(status);

    return jobStatusResponse;
}

void mapFieldsToSummary(const JobSummaryFields& fields, JobSummary* _summary) {

	// JobID should already been in our summary
	SubmissionID* sid = new SubmissionID;
	sid->setName(fields.submission_id);
	sid->setOwner(fields.owner);
	_summary->getId()->setSubmission(sid);
	_summary->setCmd(fields.cmd);
	_summary->setArgs1(fields.args1);
	_summary->setArgs2(fields.args2);
	_summary->setHeld(fields.hold_reason);
	_summary->setReleased(fields.release_reason);
	_summary->setRemoved(fields.remove_reason);
	JobStatusType* jst = new JobStatusType;
	jst->setJobStatusType(getJobStatusString(fields.status));
	_summary->setJob_status(jst);

	// TODO: need to figure out our date/time/conversion
}

GetJobSummaryResponse* AviaryQueryServiceSkeleton::getJobSummary(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobSummary* _getJobSummary)
{
	GetJobSummaryResponse* jobSummaryResponse = new GetJobSummaryResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobSummaryVectorType* job_results = new JobSummaryVectorType;

	JobIdVectorType* id_list = _getJobSummary->getIds();
	for (JobIdVectorType::iterator i = id_list->begin(); id_list->end() != i; i++) {
		JobSummary* js = new JobSummary;
		const char* job = (*i)->getJob().c_str();
		JobSummaryFields jsf;
		string error;
		if (jso->getSummary((*i)->getJob().c_str(),jsf,error)) {
			createGoodJobResponse<JobSummary>(*js,job);
			mapFieldsToSummary(jsf,js);
		}
		else {
			// problem...report to client
			createFailJobResponse<JobSummary>(*js, job, error);
		}
		job_results->push_back(js);
	}

	jobSummaryResponse->setJobs(job_results);
	Status* status = new Status;
	status->setCode(new StatusCodeType("SUCCESS"));
	jobSummaryResponse->setStatus(status);
    return jobSummaryResponse;
}

void mapToXsdAttributes(const aviary::codec::AttributeMapType& _map, AviaryCommon::Attributes* _attrs) {
	for (AttributeMapIterator i = _map.begin(); _map.end() != i; i++) {
		aviary::codec::Attribute* codec_attr = (aviary::codec::Attribute*)(*i).second;
		AviaryCommon::Attribute* attr = new AviaryCommon::Attribute;
		attr->setName((*i).first);
		AviaryCommon::AttributeType* attr_type = new AviaryCommon::AttributeType;
		switch (codec_attr->getType()) {
			case aviary::codec::Attribute::INTEGER_TYPE:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_INTEGER);
				break;
			case aviary::codec::Attribute::FLOAT_TYPE:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_FLOAT);
				break;
			case aviary::codec::Attribute::STRING_TYPE:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_STRING);
				break;
			case aviary::codec::Attribute::EXPR_TYPE:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_EXPRESSION);
				break;
			default:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_UNDEFINED);
		}
		attr->setType(attr_type);
		attr->setValue(codec_attr->getValue());
		_attrs->addAttrs(attr);
	}
}

GetJobDetailsResponse* AviaryQueryServiceSkeleton::getJobDetails(wso2wsf::MessageContext* /* outCtx*/
	,GetJobDetails* _getJobDetails)
{
	GetJobDetailsResponse* jobDetailsResponse = new GetJobDetailsResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobDetailsVectorType* job_results = new JobDetailsVectorType;

	JobIdVectorType* id_list = _getJobDetails->getIds();
	for (JobIdVectorType::iterator i = id_list->begin(); id_list->end() != i; i++) {
		JobDetails* jd = new JobDetails;
		const char* job = (*i)->getJob().c_str();
		aviary::codec::AttributeMapType attr_map;
		string error;
		if (jso->getJobAd((*i)->getJob().c_str(),attr_map,error)) {
			createGoodJobResponse<JobDetails>(*jd,job);
			// TODO: load attributes
			AviaryCommon::Attributes* attrs = new AviaryCommon::Attributes;
			mapToXsdAttributes(attr_map,attrs);
			jd->setDetails(attrs);
		}
		else {
			// problem...report to client
			createFailJobResponse<JobDetails>(*jd, job, error);
		}
		job_results->push_back(jd);
	}

	jobDetailsResponse->setJobs(job_results);
	Status* status = new Status;
	status->setCode(new StatusCodeType("SUCCESS"));
	jobDetailsResponse->setStatus(status);
    return jobDetailsResponse;
}

// NOTE: getJobData is the rare case (?) where someone wants to pull the job output
// thus, we don't batch this - just one at a time
GetJobDataResponse* AviaryQueryServiceSkeleton::getJobData(wso2wsf::MessageContext* /* outCtx */
	,GetJobData* _getJobData)
{
    GetJobDataResponse* jobDataResponse = new  GetJobDataResponse;
	JobServerObject* jso = JobServerObject::getInstance();

	const char* job = _getJobData->getData()->getId()->getJob().c_str();
	ADBJobDataTypeEnum file_type = _getJobData->getData()->getType()->getJobDataTypeEnum();
	string error, fname, content;
	int fsize;
	if (jso->fetchJobData(job,UserFileType(file_type),fname,_getJobData->getMax_bytes(),_getJobData->getFrom_end(),fsize,content,error)) {
		JobID* jid = new JobID;
		jid->setJob(job);
		jid->setPool(jso->getPool());
		jid->setScheduler(jso->getName());
		JobData* jd = new JobData;
		jd->setId(jid);
		jobDataResponse->setData(jd);
		Status* jst = new Status;
		jst->setCode(new StatusCodeType("SUCCESS"));
		jobDataResponse->setStatus(jst);

		// TODO: load requested file data
	}
	else {
		// couldn't find it...report to client
		JobID* jid = new JobID;
		jid->setJob(job);
		Status* jst = new Status(new StatusCodeType("FAIL"),error);
		jobDataResponse->setStatus(jst);
	}

    return jobDataResponse;
}
