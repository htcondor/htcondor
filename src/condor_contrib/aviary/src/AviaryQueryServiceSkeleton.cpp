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

/**
 * "getJobData|http://grid.redhat.com/aviary-query/" operation.
 *
 * @param _getJobData of the AviaryQuery::GetJobData
 *
 * @return AviaryQuery::GetJobDataResponse*
 */
AviaryQuery::GetJobDataResponse* AviaryQueryServiceSkeleton::getJobData(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobData* _getJobData)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryQuery::GetJobDataResponse*)NULL;
}

/**
 * "getJobStatus|http://grid.redhat.com/aviary-query/" operation.
 *
 * @param _getJobStatus of the AviaryQuery::GetJobStatus
 *
 * @return AviaryQuery::GetJobStatusResponse*
 */
AviaryQuery::GetJobStatusResponse* AviaryQueryServiceSkeleton::getJobStatus(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobStatus* _getJobStatus)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryQuery::GetJobStatusResponse*)NULL;
}

/**
 * "getSubmissionSummary|http://grid.redhat.com/aviary-query/" operation.
 *
 * @param _getSubmissionSummary of the AviaryQuery::GetSubmissionSummary
 *
 * @return AviaryQuery::GetSubmissionSummaryResponse*
 */
AviaryQuery::GetSubmissionSummaryResponse* AviaryQueryServiceSkeleton::getSubmissionSummary(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetSubmissionSummary* _getSubmissionSummary)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryQuery::GetSubmissionSummaryResponse*)NULL;
}

/**
 * "getJobDetails|http://grid.redhat.com/aviary-query/" operation.
 *
 * @param _getJobDetails of the AviaryQuery::GetJobDetails
 *
 * @return AviaryQuery::GetJobDetailsResponse*
 */
AviaryQuery::GetJobDetailsResponse* AviaryQueryServiceSkeleton::getJobDetails(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobDetails* _getJobDetails)
{
    /* TODO fill this with the necessary business logic */
    return (AviaryQuery::GetJobDetailsResponse*)NULL;
}
