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

// the implementation class for AviaryJob methods

// condor includes
#include "condor_common.h"
#include "condor_attributes.h"

extern bool qmgmt_all_users_trusted;

// local includes
#include "AviaryJobServiceSkeleton.h"
#include <AviaryJob_RemoveJob.h>
#include <AviaryJob_RemoveJobResponse.h>
#include <AviaryJob_ReleaseJob.h>
#include <AviaryJob_ReleaseJobResponse.h>
#include <AviaryJob_SubmitJob.h>
#include <AviaryJob_SubmitJobResponse.h>
#include <AviaryJob_HoldJob.h>
#include <AviaryJob_HoldJobResponse.h>
#include <AviaryJob_SetJobAttribute.h>
#include <AviaryJob_SetJobAttributeResponse.h>
#include "Codec.h"
#include "SchedulerObject.h"

using namespace AviaryJob;
using namespace aviary::codec;
using namespace aviary::job;
using namespace compat_classad;

AviaryJob::RemoveJobResponse*
AviaryJobServiceSkeleton::removeJob(wso2wsf::MessageContext *outCtx ,AviaryJob::RemoveJob* _removeJob)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::RemoveJobResponse*)NULL;
}

AviaryJob::ReleaseJobResponse*
AviaryJobServiceSkeleton::releaseJob(wso2wsf::MessageContext *outCtx ,AviaryJob::ReleaseJob* _releaseJob)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::ReleaseJobResponse*)NULL;
}

AviaryJob::SubmitJobResponse*
AviaryJobServiceSkeleton::submitJob(wso2wsf::MessageContext *outCtx ,AviaryJob::SubmitJob* _submitJob)
{
    AviaryJob::SubmitJobResponse* submitJobResponse = new AviaryJob::SubmitJobResponse();
    SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    // TODO: would like to templatize the codec stuff
    // save for the linking issues
    //DefaultCodecFactory codecFactory;
	//codecFactory.createCodec();
    Codec* codec = new BaseCodec();
    AttributeMapType attrMap;

    // add the simple stuff first
    attrMap[ATTR_JOB_CMD] = new Attribute(Attribute::STRING_TYPE, _submitJob->getCmd().c_str());
    if (!(_submitJob->isArgsNil() || _submitJob->getArgs().empty())) {
        attrMap[ATTR_JOB_ARGUMENTS1] = new Attribute(Attribute::STRING_TYPE, _submitJob->getArgs().c_str());
    }
    attrMap[ATTR_OWNER] = new Attribute(Attribute::STRING_TYPE, _submitJob->getOwner().c_str());
    attrMap[ATTR_JOB_IWD] = new Attribute(Attribute::STRING_TYPE, _submitJob->getIwd().c_str());

    // build a requirements string and add to it
    string reqBuilder;
    if (!_submitJob->isRequirementsNil()) {
        // TODO: iterate through resource constraints
    }
    else {
        // default
        reqBuilder = "TRUE";
    }
    attrMap[ATTR_REQUIREMENTS] = new Attribute(Attribute::EXPR_TYPE, reqBuilder.c_str());
    // TODO: need to add extras attrs also

    // invoke submit
    string jobId, error;
    // TODO: temporary hack for testing
    // until ws-security or something gets turned on
    qmgmt_all_users_trusted = true;
    if (!schedulerObj->submit(codec,attrMap,jobId, error)) {
        submitJobResponse->setStatus(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
    }
    else {
        // TODO: fix up args
        string submissionId = schedulerObj->getName();
		submissionId.append("#");
		submissionId.append(jobId);
        submitJobResponse->setId(new AviaryCommon::JobID(
				schedulerObj->getPool(),schedulerObj->getName(),jobId,
				new AviaryCommon::SubmissionID(submissionId,_submitJob->getOwner().c_str())));
        submitJobResponse->setStatus(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("SUCCESS"),""));
    }
    qmgmt_all_users_trusted = false;

	// TODO: should be an instance member
	// but for header headaches
	delete codec;

    return submitJobResponse;
}

AviaryJob::HoldJobResponse*
AviaryJobServiceSkeleton::holdJob(wso2wsf::MessageContext *outCtx ,AviaryJob::HoldJob* _holdJob)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::HoldJobResponse*)NULL;
}

AviaryJob::SetJobAttributeResponse*
AviaryJobServiceSkeleton::setJobAttribute(wso2wsf::MessageContext *outCtx ,AviaryJob::SetJobAttribute* _setJobAttribute)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::SetJobAttributeResponse*)NULL;
}
