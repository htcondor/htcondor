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

using namespace AviaryJob;

/**
* "removeJob|http://grid.redhat.com/aviary-job/" operation.
*
* @param _removeJob of the AviaryJob::RemoveJob
*
* @return AviaryJob::RemoveJobResponse*
*/
AviaryJob::RemoveJobResponse* AviaryJobServiceSkeleton::removeJob(wso2wsf::MessageContext *outCtx ,AviaryJob::RemoveJob* _removeJob)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::RemoveJobResponse*)NULL;
}

/**
* "releaseJob|http://grid.redhat.com/aviary-job/" operation.
*
* @param _releaseJob of the AviaryJob::ReleaseJob
*
* @return AviaryJob::ReleaseJobResponse*
*/
AviaryJob::ReleaseJobResponse* AviaryJobServiceSkeleton::releaseJob(wso2wsf::MessageContext *outCtx ,AviaryJob::ReleaseJob* _releaseJob)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::ReleaseJobResponse*)NULL;
}

/**
* "submitJob|http://grid.redhat.com/aviary-job/" operation.
*
* @param _submitJob of the AviaryJob::SubmitJob
*
* @return AviaryJob::SubmitJobResponse*
*/
AviaryJob::SubmitJobResponse* AviaryJobServiceSkeleton::submitJob(wso2wsf::MessageContext *outCtx ,AviaryJob::SubmitJob* _submitJob)

{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::SubmitJobResponse*)NULL;
}

/**
* "holdJob|http://grid.redhat.com/aviary-job/" operation.
*
* @param _holdJob of the AviaryJob::HoldJob
*
* @return AviaryJob::HoldJobResponse*
*/
AviaryJob::HoldJobResponse* AviaryJobServiceSkeleton::holdJob(wso2wsf::MessageContext *outCtx ,AviaryJob::HoldJob* _holdJob)

{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::HoldJobResponse*)NULL;
}

/**
* "setJobAttribute|http://grid.redhat.com/aviary-job/" operation.
*
* @param _setJobAttribute of the AviaryJob::SetJobAttribute
*
* @return AviaryJob::SetJobAttributeResponse*
*/
AviaryJob::SetJobAttributeResponse* AviaryJobServiceSkeleton::setJobAttribute(wso2wsf::MessageContext *outCtx ,AviaryJob::SetJobAttribute* _setJobAttribute)

{
    /* TODO fill this with the necessary business logic */
    return (AviaryJob::SetJobAttributeResponse*)NULL;
}
