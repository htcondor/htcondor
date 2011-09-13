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
#include <Axis2SoapProvider.h>

// condor includes
#include "stl_string_utils.h"
#include "proc.h"
#include "condor_attributes.h"

// axis includes
#include "axutil_date_time.h"

using namespace std;
using namespace AviaryQuery;
using namespace AviaryCommon;
using namespace aviary::query;

struct cmpid {
	bool operator()(const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

typedef vector<JobID*> JobIdCollection;
typedef vector<SubmissionID*> SubmissionIdCollection;
typedef vector<JobStatus*> JobStatusCollection;
typedef vector<JobDetails*> JobDetailsCollection;
typedef vector<JobSummary*> JobSummaryCollection;
typedef vector<SubmissionSummary*> SubmissionSummaryCollection;
typedef set<const char*, cmpid> IdCollection;

// TODO: singleton this...
extern aviary::soap::Axis2SoapProvider* provider;

//
// Utility section START
//

// Any key that begins with the '0' char is either the
// header or a cluster, i.e. not a job
#define IS_JOB(key) ((key) && '0' != (key)[0])

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
	Status* js = new Status;
	js->setCode(new StatusCodeType("OK"));
	jb.setStatus(js);
}

template <class JobBase>
void createBadJobResponse(JobBase& jb, const char* job_id, const AviaryStatus& error) {
	JobID* jid = new JobID;
	jid->setJob(job_id);
	jb.setId(jid);
	StatusCodeType* jst = new StatusCodeType;
	jst->setStatusCodeTypeEnum(ADBStatusCodeTypeEnum(error.type));
	Status* js = new Status(jst,error.text);
	jb.setStatus(js);
}

// unfortunately no convenience functions from WS02 for dateTime
axutil_date_time_t* encodeDateTime(const time_t& ts) {
	struct tm the_tm;

	// need the re-entrant version because axutil_date_time_create
	// calls time() again and overwrites static tm
	localtime_r(&ts,&the_tm);
	// get our Axis2 env for the allocator
	const axutil_env_t* env = provider->getEnv();

	axutil_date_time_t* time_value = NULL;
	time_value = axutil_date_time_create(env);

	if (!time_value)
    {
        AXIS2_ERROR_SET(env->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, "Out of memory");
        return NULL;
    }

	// play their game with adjusting the year and month offset
	axutil_date_time_set_date_time(time_value,env,
								   the_tm.tm_year+1900,
								   the_tm.tm_mon+1,
								   the_tm.tm_mday,
								   the_tm.tm_hour,
								   the_tm.tm_min,
								   the_tm.tm_sec,
								   0);
	return time_value;
};

void mapFieldsToSummary(const JobSummaryFields& fields, JobSummary* _summary) {

	// JobID should already been in our summary
	SubmissionID* sid = new SubmissionID;
	sid->setName(fields.submission_id);
	sid->setOwner(fields.owner);
	_summary->getId()->setSubmission(sid);
	// do date/time conversion
	_summary->setQueued(encodeDateTime(fields.queued));
	_summary->setLast_update(encodeDateTime(fields.last_update));
	JobStatusType* jst = new JobStatusType;
	jst->setJobStatusType(getJobStatusString(fields.status));
	_summary->setJob_status(jst);
	_summary->setCmd(fields.cmd);
	if (!fields.args1.empty()) {
		_summary->setArgs1(fields.args1);
	}
	if (!fields.args2.empty()) {
		_summary->setArgs2(fields.args2);
	}
	if (!fields.hold_reason.empty()) {
		_summary->setHeld(fields.hold_reason);
	}
	if (!fields.release_reason.empty()) {
		_summary->setReleased(fields.release_reason);
	}
	if (!fields.remove_reason.empty()) {
		_summary->setRemoved(fields.remove_reason);
	}
}

void mapToXsdAttributes(const aviary::codec::AttributeMapType& _map, AviaryCommon::Attributes* _attrs) {
	for (AttributeMapIterator i = _map.begin(); _map.end() != i; i++) {
		AviaryAttribute* codec_attr = (AviaryAttribute*)(*i).second;
		AviaryCommon::Attribute* attr = new AviaryCommon::Attribute;
		attr->setName((*i).first);
		AviaryCommon::AttributeType* attr_type = new AviaryCommon::AttributeType;
		switch (codec_attr->getType()) {
			case AviaryAttribute::INTEGER_TYPE:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_INTEGER);
				break;
			case AviaryAttribute::FLOAT_TYPE:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_FLOAT);
				break;
			case AviaryAttribute::STRING_TYPE:
				attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_STRING);
				break;
			case AviaryAttribute::EXPR_TYPE:
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

//
// Utility section END
//

//
// Interface implementation START
//
GetSubmissionSummaryResponse* AviaryQueryServiceSkeleton::getSubmissionSummary(wso2wsf::MessageContext* /*outCtx*/
	,GetSubmissionSummary* _getSubmissionSummary)
{
	GetSubmissionSummaryResponse* getSummaryResponse = new GetSubmissionSummaryResponse;

	SubmissionCollectionType::const_iterator element = g_submissions.begin();
	SubmissionSummaryCollection* submissions = new SubmissionSummaryCollection;

	SubmissionCollectionType sub_map;

	if (_getSubmissionSummary->isIdsNil() || _getSubmissionSummary->getIds()->size() == 0) {
		// no ids supplied...they get them all
		for (SubmissionCollectionType::iterator i = g_submissions.begin(); g_submissions.end() != i; i++) {
			sub_map[(*i).first] = (*i).second;
		}
	}
	else {
		// fast track...client has supplied ids to scan
		SubmissionIdCollection* id_list = _getSubmissionSummary->getIds();
		for (SubmissionIdCollection::iterator sic_it = id_list->begin(); id_list->end() != sic_it; sic_it++) {
			const char* sid_str = (*sic_it)->getName().c_str();
			SubmissionCollectionType::iterator sct_it = g_submissions.find(sid_str);
			if (sct_it != g_submissions.end()) {
				sub_map[(*sct_it).first] = (*sct_it).second;
			}
			else {
				// mark this as not matched when returning our results
				sub_map[(*sct_it).first] = NULL;
			}
		}
	}

	for (SubmissionCollectionType::iterator i = sub_map.begin(); sub_map.end() != i; i++) {
		SubmissionSummary* summary = new SubmissionSummary;
		SubmissionObject *submission = (*i).second;

		if (submission) {
			SubmissionID* sid = new SubmissionID;
			sid->setName(submission->getName());
			sid->setOwner(submission->getOwner());
			summary->setId(sid);
			// TODO: fully implement getOldest
			summary->setQdate(submission->getOldest());
			summary->setCompleted(submission->getCompleted().size());
			summary->setHeld(submission->getHeld().size());
			summary->setIdle(submission->getIdle().size());
			summary->setRemoved(submission->getRemoved().size());
			summary->setRunning(submission->getRunning().size());
			Status* ss = new Status;
			ss->setCode(new StatusCodeType("OK"));
			summary->setStatus(ss);

			if (!_getSubmissionSummary->isIncludeJobSummariesNil() && _getSubmissionSummary->getIncludeJobSummaries()) {
				// client wants the job summaries also
				JobSummaryPairCollection jobs;
				submission->getJobSummaries(jobs);
				for (JobSummaryPairCollection::const_iterator it = jobs.begin(); jobs.end() != it; it++) {
					JobSummary* js = new JobSummary;
					createGoodJobResponse<JobSummary>(*js,(*it).first);
					mapFieldsToSummary(*((*it).second),js);
					summary->addJobs(js);
				}
			}

		}
		else {
			SubmissionID* sid = new SubmissionID;
			summary->setId(sid);
			StatusCodeType* sst = new StatusCodeType;
			sst->setStatusCodeType("NO_MATCH");
			Status* ss = new Status(sst,"Unable to locate submission");
			summary->setStatus(ss);
		}
		submissions->push_back(summary);
	}

	getSummaryResponse->setSubmissions(submissions);

    return getSummaryResponse;
}

GetJobStatusResponse* AviaryQueryServiceSkeleton::getJobStatus(wso2wsf::MessageContext* /*outCtx*/
	,GetJobStatus* _getJobStatus)
{
	GetJobStatusResponse* jobStatusResponse = new GetJobStatusResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobStatusCollection* job_results = new JobStatusCollection;

	IdCollection id_set;

	if (_getJobStatus->isIdsNil() || _getJobStatus->getIds()->size() == 0) {
		// no ids supplied...they get them all
		for (JobCollectionType::iterator i = g_jobs.begin(); g_jobs.end() != i; i++) {
			const char* job_id = (*i).first;
			if (IS_JOB(job_id)) {
				id_set.insert(job_id);
			}
		}
	}
	else {
		// fast track...client has supplied ids to scan
		JobIdCollection* id_list = _getJobStatus->getIds();
		for (JobIdCollection::iterator i = id_list->begin(); id_list->end() != i; i++) {
			id_set.insert((*i)->getJob().c_str());
		}
	}

	for (IdCollection::const_iterator i = id_set.begin(); id_set.end() != i; i++) {
		JobStatus* js = new JobStatus;
		const char* job = *i;
		AviaryStatus status;
		int job_status = JOB_STATUS_MIN;
		if (jso->getStatus(job,job_status,status)) {
			createGoodJobResponse<JobStatus>(*js,job);
			JobStatusType* jst = new JobStatusType;
			jst->setJobStatusType(getJobStatusString(job_status));
			js->setJob_status(jst);
		}
		else {
			// problem...report to client
			createBadJobResponse<JobStatus>(*js,job,status);
		}
		job_results->push_back(js);
	}

	jobStatusResponse->setJobs(job_results);

    return jobStatusResponse;
}

GetJobSummaryResponse* AviaryQueryServiceSkeleton::getJobSummary(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobSummary* _getJobSummary)
{
	GetJobSummaryResponse* jobSummaryResponse = new GetJobSummaryResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobSummaryCollection* job_results = new JobSummaryCollection;

	IdCollection id_set;

	if (_getJobSummary->isIdsNil() || _getJobSummary->getIds()->size() == 0) {
		// no ids supplied...they get them all
		for (JobCollectionType::iterator i = g_jobs.begin(); g_jobs.end() != i; i++) {
			const char* job_id = (*i).first;
			if (IS_JOB(job_id)) {
				id_set.insert(job_id);
			}
		}
	}
	else {
		// fast track...client has supplied ids to scan
		JobIdCollection* id_list = _getJobSummary->getIds();
		for (JobIdCollection::iterator i = id_list->begin(); id_list->end() != i; i++) {
			id_set.insert((*i)->getJob().c_str());
		}
	}

	for (IdCollection::const_iterator i = id_set.begin(); id_set.end() != i; i++) {
		JobSummary* js = new JobSummary;
		const char* job = *i;
		JobSummaryFields jsf;
		AviaryStatus status;
		if (jso->getSummary(job,jsf,status)) {
			createGoodJobResponse<JobSummary>(*js,job);
			mapFieldsToSummary(jsf,js);
		}
		else {
			// problem...report to client
			createBadJobResponse<JobSummary>(*js, job, status);
		}
		job_results->push_back(js);
	}

	jobSummaryResponse->setJobs(job_results);

    return jobSummaryResponse;
}

GetJobDetailsResponse* AviaryQueryServiceSkeleton::getJobDetails(wso2wsf::MessageContext* /* outCtx*/
	,GetJobDetails* _getJobDetails)
{
	GetJobDetailsResponse* jobDetailsResponse = new GetJobDetailsResponse;
	JobServerObject* jso = JobServerObject::getInstance();
	JobDetailsCollection* job_results = new JobDetailsCollection;

	IdCollection id_set;

	if (_getJobDetails->isIdsNil() || _getJobDetails->getIds()->size() == 0) {
		// no ids supplied...they get them all
		for (JobCollectionType::iterator i = g_jobs.begin(); g_jobs.end() != i; i++) {
			const char* job_id = (*i).first;
			if (IS_JOB(job_id)) {
				id_set.insert(job_id);
			}
		}
	}
	else {
		// fast track...client has supplied ids to scan
		JobIdCollection* id_list = _getJobDetails->getIds();
		for (JobIdCollection::iterator i = id_list->begin(); id_list->end() != i; i++) {
			id_set.insert((*i)->getJob().c_str());
		}
	}

	for (IdCollection::const_iterator i = id_set.begin(); id_set.end() != i; i++) {
		JobDetails* jd = new JobDetails;
		const char* job = *i;
		aviary::codec::AttributeMapType attr_map;
		AviaryStatus status;
		if (jso->getJobAd(job,attr_map,status)) {
			createGoodJobResponse<JobDetails>(*jd,job);
			// load attributes
			AviaryCommon::Attributes* attrs = new AviaryCommon::Attributes;
			mapToXsdAttributes(attr_map,attrs);
			jd->setDetails(attrs);
		}
		else {
			// problem...report to client
			createBadJobResponse<JobDetails>(*jd, job, status);
		}
		job_results->push_back(jd);

        for (aviary::codec::AttributeMapType::iterator i = attr_map.begin();attr_map.end() != i; i++) {
            delete (*i).second;
        }
	}

	jobDetailsResponse->setJobs(job_results);

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
    JobDataType* jdt = new JobDataType(_getJobData->getData()->getType()->getJobDataType());
	AviaryStatus status;
	status.type = AviaryStatus::FAIL;
	string fname, content;
	int fsize;
	if (jso->fetchJobData(job,UserFileType(file_type),fname,_getJobData->getMax_bytes(),_getJobData->getFrom_end(),fsize,content,status)) {
		JobID* jid = new JobID;
		jid->setJob(job);
		jid->setPool(jso->getPool());
		jid->setScheduler(jso->getName());
		JobData* jd = new JobData;
		jd->setId(jid);
		jd->setType(jdt);
		jobDataResponse->setData(jd);
		Status* js = new Status;
		js->setCode(new StatusCodeType("OK"));
		jobDataResponse->setStatus(js);

		// load requested file data
		jobDataResponse->setContent(content);
		jobDataResponse->setFile_name(fname);
		jobDataResponse->setFile_size(fsize);
	}
	else {
		// problem...report to client
		JobID* jid = new JobID;
		jid->setJob(job);
		JobData* jd = new JobData;
		jd->setId(jid);
		jd->setType(jdt);
		jobDataResponse->setData(jd);
		StatusCodeType* jst = new StatusCodeType;
		jst->setStatusCodeTypeEnum(ADBStatusCodeTypeEnum(status.type));
		Status* js = new Status(jst,status.text);
		jobDataResponse->setStatus(js);
	}

    return jobDataResponse;
}

