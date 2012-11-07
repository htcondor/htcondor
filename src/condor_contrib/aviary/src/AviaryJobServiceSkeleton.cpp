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
#include "condor_config.h"
#include "condor_attributes.h"

#include <time.h>

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
#include "stl_string_utils.h"

using namespace std;
using namespace AviaryJob;
using namespace AviaryCommon;
using namespace aviary::codec;
using namespace aviary::job;
using namespace compat_classad;

#define ATTR_COMP(X) 0==strcasecmp(attr_name,X)

const char* BASIC_REQ_FORMAT = 
"\
( TARGET.Arch %s ) && \
( %s ) && \
( TARGET.Disk %s ) && \
( ( TARGET.Memory * 1024 ) %s ) && \
( TARGET.FileSystemDomain %s )";

const char* BASIC_OS_FORMAT = "TARGET.OpSys == \"%s\"";
const char* BASIC_WINOS_FORMAT = "TARGET.OpSys==\"WINNT51\" || TARGET.OpSys==\"WINNT52\" || TARGET.OpSys==\"WINNT60\" || TARGET.OpSys==\"WINNT61\"";
const char* REQ_UNDEFINED = " =!= undefined ";
const char* REQ_GTE_ZERO = " >= 0 ";

//
// Utility methods START
//

typedef vector<AviaryCommon::Attribute*> CommonAttributeCollection;

void
checkForSchedulerID(AviaryCommon::JobID* _jobId, string& _text)
{
    SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    if (!_jobId->isSubmissionNil()) {
        if ((_jobId->getSubmission()->getPool() != schedulerObj->getPool()) ||
            (_jobId->getSubmission()->getName() != schedulerObj->getName())) {
        _text = "WARNING: the pool and scheduler names of the requested jobid were empty or did not match this scheduler!";
        }
    }
}

void
buildBasicRequirements(ResourceConstraintVectorType* _constraints, string& _reqs) {
	// scan through these and build TARGET.<constraint> like string
	string arch = REQ_UNDEFINED;
	string opsys = REQ_UNDEFINED;
	string disk = REQ_GTE_ZERO;
	string memory = REQ_GTE_ZERO;
	string filesystem = REQ_UNDEFINED;
	for ( ResourceConstraintVectorType::const_iterator it = _constraints->begin(); it != _constraints->end();
		 it++ ) {
		ResourceConstraint* rc = *it;
		ADBResourceConstraintTypeEnum rct = rc->getType()->getResourceConstraintTypeEnum();
		switch (rct) {
			case ResourceConstraintType_ARCH:
				arch = " == \"" + rc->getValue() + "\"";
				break;
			case ResourceConstraintType_OS:
				if (rc->getValue() == "WINDOWS") {
					opsys = BASIC_WINOS_FORMAT;
				}
				else {
					formatstr(opsys,BASIC_OS_FORMAT,rc->getValue().c_str());
				}
				break;
            case ResourceConstraintType_DISK:
				disk = " >= " + rc->getValue();
				break;
			case ResourceConstraintType_MEMORY:
				memory = " >= " + rc->getValue();
				break;
            case ResourceConstraintType_FILESYSTEM:
				filesystem = " == \"" + rc->getValue() + "\"";
				break;
			default:
				dprintf(D_ALWAYS,"Ignoring unknown resource constraint submitted: %s:%s\n",
						rc->getType()->getResourceConstraintType().c_str(),rc->getValue().c_str());
		}
	}
	// order is important! see BASIC_REQ_FORMAT above
	formatstr(_reqs, BASIC_REQ_FORMAT, arch.c_str(), opsys.c_str(), disk.c_str(), memory.c_str(), filesystem.c_str());
}

bool
isBasicAttribute(const char* attr_name) {
	return (
		ATTR_COMP(ATTR_JOB_CMD) ||
		ATTR_COMP(ATTR_REQUIREMENTS) ||
		ATTR_COMP(ATTR_OWNER) ||
		ATTR_COMP(ATTR_JOB_IWD) ||
		ATTR_COMP(ATTR_JOB_ARGUMENTS1)
	);
}

void
addExtraAttributes(const CommonAttributeCollection* extra_attrs, AttributeMapType& attr_map, bool override_basic) {
	// add in the extras
	for (CommonAttributeCollection::const_iterator i = extra_attrs->begin();i < extra_attrs->end();i++) {
		AviaryCommon::Attribute* attr = *i;
		const string& attr_key = attr->getName();

		// Are we overriding our basic attributes?
		if (!override_basic && isBasicAttribute(attr_key.c_str())) {
			// exclude this attribute from the submission map
			continue;
		}

		const char* attr_value = attr->getValue().c_str();
		switch (attr->getType()->getAttributeTypeEnum()) {
			case AviaryCommon::AttributeType_INTEGER:
				attr_map[attr_key.c_str()] =
					new AviaryAttribute(AviaryAttribute::INTEGER_TYPE,attr_value);
			break;
			case AviaryCommon::AttributeType_FLOAT:
				attr_map[attr_key.c_str()] =
					new AviaryAttribute(AviaryAttribute::FLOAT_TYPE,attr_value);
			break;
			case AviaryCommon::AttributeType_STRING:
				attr_map[attr_key.c_str()] =
					new AviaryAttribute(AviaryAttribute::STRING_TYPE,attr_value);
			break;
			case AviaryCommon::AttributeType_BOOLEAN:
			case AviaryCommon::AttributeType_EXPRESSION:
				attr_map[attr_key.c_str()] =
					new AviaryAttribute(AviaryAttribute::EXPR_TYPE,attr_value);
			break;
			// probably shouldn't get here unless axis2 fails us
			case AviaryCommon::AttributeType_ERROR:
			case AviaryCommon::AttributeType_UNDEFINED:
			default:
				dprintf(D_FULLDEBUG,"Unknown type supplied for attribute '%s=%s'\n",
						attr_key.c_str(),attr_value);
		}
	}
}

void
addDefaultHiddenAttributes(AttributeMapType& attr_map) {

    // need these for dynamic provisioning
    if (attr_map.end() == attr_map.find(ATTR_REQUEST_CPUS)) {
        attr_map[ATTR_REQUEST_CPUS] = new AviaryAttribute(AviaryAttribute::INTEGER_TYPE,"1");
    }
    if (attr_map.end() == attr_map.find(ATTR_DISK_USAGE)) {
        attr_map[ATTR_DISK_USAGE] = new AviaryAttribute(AviaryAttribute::INTEGER_TYPE,"1");
    }
    if (attr_map.end() == attr_map.find(ATTR_IMAGE_SIZE)) {
        attr_map[ATTR_IMAGE_SIZE] = new AviaryAttribute(AviaryAttribute::INTEGER_TYPE,"0");
    }
    if (attr_map.end() == attr_map.find(ATTR_REQUEST_DISK)) {
        attr_map[ATTR_REQUEST_DISK] = new AviaryAttribute(AviaryAttribute::EXPR_TYPE,ATTR_DISK_USAGE);
    }
    if (attr_map.end() == attr_map.find(ATTR_REQUEST_MEMORY)) {
        attr_map[ATTR_REQUEST_MEMORY] = new AviaryAttribute(AviaryAttribute::EXPR_TYPE,"ceiling(ifThenElse(JobVMMemory =!= undefined,JobVMMemory,ImageSize / 1024.000000))");
    }
}

//
// Utility methods END
//

//
// Interface implementation START
//

AviaryJob::SubmitJobResponse*
AviaryJobServiceSkeleton::submitJob(wso2wsf::MessageContext* /*outCtx*/ ,AviaryJob::SubmitJob* _submitJob)
{
    AviaryJob::SubmitJobResponse* submitJobResponse = new AviaryJob::SubmitJobResponse();
	SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    AttributeMapType reqsMap, attrMap;
	const char* submissionName = NULL;

    // add the simple stuff first
    attrMap[ATTR_JOB_CMD] = new AviaryAttribute(AviaryAttribute::STRING_TYPE, _submitJob->getCmd().c_str());
    if (!(_submitJob->isArgsNil() || _submitJob->getArgs().empty())) {
        attrMap[ATTR_JOB_ARGUMENTS1] = new AviaryAttribute(AviaryAttribute::STRING_TYPE, _submitJob->getArgs().c_str());
    }
    attrMap[ATTR_OWNER] = new AviaryAttribute(AviaryAttribute::STRING_TYPE, _submitJob->getOwner().c_str());
    attrMap[ATTR_JOB_IWD] = new AviaryAttribute(AviaryAttribute::STRING_TYPE, _submitJob->getIwd().c_str());
	if (!(_submitJob->isSubmission_nameNil() || _submitJob->getSubmission_name().empty())) {
		submissionName = _submitJob->getSubmission_name().c_str();
        attrMap[ATTR_JOB_SUBMISSION] = new AviaryAttribute(AviaryAttribute::STRING_TYPE,submissionName);
    }

    // build a requirements string and add to it
    string reqBuilder;
    if (!(_submitJob->isRequirementsNil() || _submitJob->getRequirements()->empty())) {
        // build from resource constraints
		buildBasicRequirements(_submitJob->getRequirements(), reqBuilder);
    }
    else {
        // default
        reqBuilder = "TRUE";
    }
    attrMap[ATTR_REQUIREMENTS] = new AviaryAttribute(AviaryAttribute::EXPR_TYPE, reqBuilder.c_str());

    // need to add extras attrs also
    // wso2 doesn't seem to make true nil checking easy
    // might remove the Attributes element
    bool allow_overrides = !_submitJob->isAllowOverridesNil() && _submitJob->getAllowOverrides();
    CommonAttributeCollection* attrs = NULL;
    if (!_submitJob->isExtraNil()) {
		attrs = _submitJob->getExtra();
		if (attrs && !attrs->empty()) {			
			if (attrs && !attrs->empty()) {
				addExtraAttributes(attrs, attrMap,allow_overrides);
			}
		}
	}

	// finally we'll add defaults for dynamic provisioning
	addDefaultHiddenAttributes(attrMap);

    // invoke submit
    string jobId, error;
    // we need this since we don't have a trusted socket to the schedd,
    // without it basically the schedd won't accept whatever we claim
    // is Owner and the job will be pseudo-pruned
    qmgmt_all_users_trusted = true;
    if (!schedulerObj->submit(attrMap,jobId, error)) {
        submitJobResponse->setStatus(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
    }
    else {
        string submissionId;
        if (submissionName) {
			submissionId = submissionName;
		}
		else {
			submissionId = schedulerObj->getName();
			submissionId.append("#");
			submissionId.append(jobId);
		}
        submitJobResponse->setId(new AviaryCommon::JobID(jobId, schedulerObj->getPool(), schedulerObj->getName(),
				new AviaryCommon::SubmissionID(
                        submissionId,_submitJob->getOwner().c_str(),
                        time(NULL),schedulerObj->getPool(),
                        schedulerObj->getName()
                )));
        submitJobResponse->setStatus(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),""));
    }
    qmgmt_all_users_trusted = false;

    // clean up
    for (AttributeMapType::iterator i = attrMap.begin(); attrMap.end() != i; i++) {
        delete (*i).second;
    }
    for (AttributeMapType::iterator i = reqsMap.begin(); reqsMap.end() != i; i++) {
        delete (*i).second;
    }

    return submitJobResponse;
}


// TODO: would be nice to template these next 3
AviaryJob::HoldJobResponse*
AviaryJobServiceSkeleton::holdJob(wso2wsf::MessageContext* /*outCtx*/ ,AviaryJob::HoldJob* _holdJob)
{
	AviaryJob::HoldJobResponse* holdJobResponse = new HoldJobResponse;
	SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    string error;

	AviaryCommon::JobID* jobId = _holdJob->getHoldJob()->getId();
	string reason = _holdJob->getHoldJob()->getReason();
	string cluster_proc = jobId->getJob();
	ControlJobResponse* controlJobResponse = NULL;

	checkForSchedulerID(jobId, error);
	if (!schedulerObj->hold(cluster_proc,reason,error)) {
		dprintf(D_FULLDEBUG, "SchedulerObject Hold failed: %s\n", error.c_str());
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
	}
	else {
		// in this case, error may hve been the result of the pool/schedd check
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),error));
	}

	holdJobResponse->setHoldJobResponse(controlJobResponse);
    return holdJobResponse;
}


AviaryJob::ReleaseJobResponse*
AviaryJobServiceSkeleton::releaseJob(wso2wsf::MessageContext* /*outCtx*/ ,AviaryJob::ReleaseJob* _releaseJob)
{
	AviaryJob::ReleaseJobResponse* releaseJobResponse = new ReleaseJobResponse;
	SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    string error;

	AviaryCommon::JobID* jobId = _releaseJob->getReleaseJob()->getId();
	string reason = _releaseJob->getReleaseJob()->getReason();
	string cluster_proc = jobId->getJob();
	ControlJobResponse* controlJobResponse = NULL;

	checkForSchedulerID(jobId, error);
	if (!schedulerObj->release(cluster_proc,reason,error)) {
		dprintf(D_FULLDEBUG, "SchedulerObject Release failed: %s\n", error.c_str());
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
	}
	else {
		// in this case, error may hve been the result of the pool/schedd check
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),error));
	}

	releaseJobResponse->setReleaseJobResponse(controlJobResponse);
    return releaseJobResponse;
}

AviaryJob::RemoveJobResponse*
AviaryJobServiceSkeleton::removeJob(wso2wsf::MessageContext* /*outCtx*/ ,AviaryJob::RemoveJob* _removeJob)
{
	AviaryJob::RemoveJobResponse* removeJobResponse = new RemoveJobResponse;
	SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    string error;

	AviaryCommon::JobID* jobId = _removeJob->getRemoveJob()->getId();
	string reason = _removeJob->getRemoveJob()->getReason();
	string cluster_proc = jobId->getJob();
	ControlJobResponse* controlJobResponse = NULL;

	checkForSchedulerID(jobId, error);
	if (!schedulerObj->remove(cluster_proc,reason,error)) {
		dprintf(D_FULLDEBUG, "SchedulerObject Remove failed: %s\n", error.c_str());
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
	}
	else {
		// in this case, error may hve been the result of the pool/schedd check
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),error));
	}

	removeJobResponse->setRemoveJobResponse(controlJobResponse);
    return removeJobResponse;
}

AviaryJob::SetJobAttributeResponse*
AviaryJobServiceSkeleton::setJobAttribute(wso2wsf::MessageContext* /*outCtx*/ ,AviaryJob::SetJobAttribute* _setJobAttribute)
{
	AviaryJob::SetJobAttributeResponse* setAttrResponse = new SetJobAttributeResponse;
	SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    string error;

	AviaryCommon::JobID* jobId = _setJobAttribute->getId();
	AviaryCommon::Attribute* attr = _setJobAttribute->getAttribute();
	string cluster_proc = jobId->getJob();
	ControlJobResponse* controlJobResponse = NULL;

	checkForSchedulerID(jobId, error);
	if (!schedulerObj->setAttribute(cluster_proc,attr->getName(),attr->getValue(),error)) {
		dprintf(D_FULLDEBUG, "SchedulerObject SetAttribute failed: %s\n", error.c_str());
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
	}
	else {
		// in this case, error may have been the result of the pool/schedd check
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),error));
	}

	setAttrResponse->setSetJobAttributeResponse(controlJobResponse);
    return setAttrResponse;
}

AviaryJob::SuspendJobResponse*
AviaryJobServiceSkeleton::suspendJob(wso2wsf::MessageContext* /*outCtx*/ ,AviaryJob::SuspendJob* _suspendJob)
{
	AviaryJob::SuspendJobResponse* suspendJobResponse = new SuspendJobResponse;
	SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    string error;

	AviaryCommon::JobID* jobId = _suspendJob->getSuspendJob()->getId();
	string reason = _suspendJob->getSuspendJob()->getReason();
	string cluster_proc = jobId->getJob();
	ControlJobResponse* controlJobResponse = NULL;

	checkForSchedulerID(jobId, error);
	if (!schedulerObj->suspend(cluster_proc,reason,error)) {
		dprintf(D_FULLDEBUG, "SchedulerObject Suspend failed: %s\n", error.c_str());
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
	}
	else {
		// in this case, error may have been the result of the pool/schedd check
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),error));
	}

	suspendJobResponse->setSuspendJobResponse(controlJobResponse);
    return suspendJobResponse;
}

AviaryJob::ContinueJobResponse*
AviaryJobServiceSkeleton::continueJob(wso2wsf::MessageContext* /*outCtx*/ ,AviaryJob::ContinueJob* _continueJob)
{
	AviaryJob::ContinueJobResponse* continueJobResponse = new ContinueJobResponse;
	SchedulerObject* schedulerObj = SchedulerObject::getInstance();
    string error;

	AviaryCommon::JobID* jobId = _continueJob->getContinueJob()->getId();
	string reason = _continueJob->getContinueJob()->getReason();
	string cluster_proc = jobId->getJob();
	ControlJobResponse* controlJobResponse = NULL;

	checkForSchedulerID(jobId, error);
	if (!schedulerObj->_continue(cluster_proc,reason,error)) {
		dprintf(D_FULLDEBUG, "SchedulerObject Continue failed: %s\n", error.c_str());
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),error));
	}
	else {
		// in this case, error may have been the result of the pool/schedd check
		controlJobResponse = new ControlJobResponse(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),error));
	}

	continueJobResponse->setContinueJobResponse(controlJobResponse);
    return continueJobResponse;
}
