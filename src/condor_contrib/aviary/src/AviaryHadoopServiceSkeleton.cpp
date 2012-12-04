/*
 * Copyright 2009-2012 Red Hat, Inc.
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

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"

#include <time.h>

// local includes
#include "AviaryHadoopServiceSkeleton.h"
#include <AviaryHadoop_StartTaskTracker.h>
#include <AviaryHadoop_StartTaskTrackerResponse.h>
#include <AviaryHadoop_StartDataNode.h>
#include <AviaryHadoop_StartDataNodeResponse.h>
#include <AviaryHadoop_GetTaskTracker.h>
#include <AviaryHadoop_GetTaskTrackerResponse.h>
#include <AviaryHadoop_StopJobTracker.h>
#include <AviaryHadoop_StopJobTrackerResponse.h>
#include <AviaryHadoop_GetJobTracker.h>
#include <AviaryHadoop_GetJobTrackerResponse.h>
#include <AviaryHadoop_StopTaskTracker.h>
#include <AviaryHadoop_StopTaskTrackerResponse.h>
#include <AviaryHadoop_StartNameNode.h>
#include <AviaryHadoop_StartNameNodeResponse.h>
#include <AviaryHadoop_GetDataNode.h>
#include <AviaryHadoop_GetDataNodeResponse.h>
#include <AviaryHadoop_StopNameNode.h>
#include <AviaryHadoop_StopNameNodeResponse.h>
#include <AviaryHadoop_GetNameNode.h>
#include <AviaryHadoop_GetNameNodeResponse.h>
#include <AviaryHadoop_StartJobTracker.h>
#include <AviaryHadoop_StartJobTrackerResponse.h>
#include <AviaryHadoop_StopDataNode.h>
#include <AviaryHadoop_StopDataNodeResponse.h>
#include "Codec.h"

using namespace std;
using namespace wso2wsf;
using namespace AviaryHadoop;
using namespace AviaryCommon;
using namespace aviary::codec;
using namespace compat_classad;


// TODO: ditch this when we're done
Status* makeUnimplemented() {
    Status* status = new Status(new StatusCodeType(StatusCodeType_UNIMPLEMENTED),NULL);
    return status;
}

StartNameNodeResponse* AviaryHadoopServiceSkeleton::startNameNode(MessageContext* /*outCtx*/ , StartNameNode* _startNameNode)
{
    /* TODO fill this with the necessary business logic */
    StartNameNodeResponse* response = new StartNameNodeResponse;
    HadoopStartResponse* hresp = new HadoopStartResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStartNameNodeResponse(hresp);
    return response;
}

StopNameNodeResponse* AviaryHadoopServiceSkeleton::stopNameNode(MessageContext* /*outCtx*/ , StopNameNode* _stopNameNode)
{
    /* TODO fill this with the necessary business logic */
    StopNameNodeResponse* response = new StopNameNodeResponse;
    HadoopStopResponse* hresp = new HadoopStopResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStopNameNodeResponse(hresp);
    return response;
}

GetNameNodeResponse* AviaryHadoopServiceSkeleton::getNameNode(MessageContext* /*outCtx*/ , GetNameNode* _getNameNode)
{
    /* TODO fill this with the necessary business logic */
    GetNameNodeResponse* response = new GetNameNodeResponse;
    HadoopQueryResponse* hresp = new HadoopQueryResponse;
    hresp->setStatus(makeUnimplemented());
    response->setGetNameNodeResponse(hresp);
    return response;
}

StartDataNodeResponse* AviaryHadoopServiceSkeleton::startDataNode(MessageContext* /*outCtx*/ , StartDataNode* _startDataNode)
{
    /* TODO fill this with the necessary business logic */
    StartDataNodeResponse* response = new StartDataNodeResponse;
    HadoopStartResponse* hresp = new HadoopStartResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStartDataNodeResponse(hresp);
    return response;
}

StopDataNodeResponse* AviaryHadoopServiceSkeleton::stopDataNode(MessageContext* /*outCtx*/ , StopDataNode* _stopDataNode)
{
    /* TODO fill this with the necessary business logic */
    StopDataNodeResponse* response = new StopDataNodeResponse;
    HadoopStopResponse* hresp = new HadoopStopResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStopDataNodeResponse(hresp);
    return response;
}

GetDataNodeResponse* AviaryHadoopServiceSkeleton::getDataNode(MessageContext* /*outCtx*/ , GetDataNode* _getDataNode)
{
    /* TODO fill this with the necessary business logic */
    GetDataNodeResponse* response = new GetDataNodeResponse;
    HadoopQueryResponse* hresp = new HadoopQueryResponse;
    hresp->setStatus(makeUnimplemented());
    response->setGetDataNodeResponse(hresp);
    return response;
}

StartJobTrackerResponse* AviaryHadoopServiceSkeleton::startJobTracker(MessageContext* /*outCtx*/ , StartJobTracker* _startJobTracker)
{
    /* TODO fill this with the necessary business logic */
    StartJobTrackerResponse* response = new StartJobTrackerResponse;
    HadoopStartResponse* hresp = new HadoopStartResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStartJobTrackerResponse(hresp);
    return response;
}

StopJobTrackerResponse* AviaryHadoopServiceSkeleton::stopJobTracker(MessageContext* /*outCtx*/ , StopJobTracker* _stopJobTracker)
{
    /* TODO fill this with the necessary business logic */
    StopJobTrackerResponse* response = new StopJobTrackerResponse;
    HadoopStopResponse* hresp = new HadoopStopResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStopJobTrackerResponse(hresp);
    return response;
}

GetJobTrackerResponse* AviaryHadoopServiceSkeleton::getJobTracker(MessageContext* /*outCtx*/ , GetJobTracker* _getJobTracker)
{
    /* TODO fill this with the necessary business logic */
    GetJobTrackerResponse* response = new GetJobTrackerResponse;
    HadoopQueryResponse* hresp = new HadoopQueryResponse;
    hresp->setStatus(makeUnimplemented());
    response->setGetJobTrackerResponse(hresp);
    return response;
}

StartTaskTrackerResponse* AviaryHadoopServiceSkeleton::startTaskTracker(MessageContext* /*outCtx*/ , StartTaskTracker* _startTaskTracker)
{
    /* TODO fill this with the necessary business logic */
    StartTaskTrackerResponse* response = new StartTaskTrackerResponse;
    HadoopStartResponse* hresp = new HadoopStartResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStartTaskTrackerResponse(hresp);
    return response;
}

StopTaskTrackerResponse* AviaryHadoopServiceSkeleton::stopTaskTracker(MessageContext* /*outCtx*/ , StopTaskTracker* _stopTaskTracker)
{
    /* TODO fill this with the necessary business logic */
    StopTaskTrackerResponse* response = new StopTaskTrackerResponse;
    HadoopStopResponse* hresp = new HadoopStopResponse;
    hresp->setStatus(makeUnimplemented());
    response->setStopTaskTrackerResponse(hresp);
    return response;
}

GetTaskTrackerResponse* AviaryHadoopServiceSkeleton::getTaskTracker(MessageContext* /*outCtx*/ , GetTaskTracker* _getTaskTracker)
{
    /* TODO fill this with the necessary business logic */
    GetTaskTrackerResponse* response = new GetTaskTrackerResponse;
    HadoopQueryResponse* hresp = new HadoopQueryResponse;
    hresp->setStatus(makeUnimplemented());
    response->setGetTaskTrackerResponse(hresp);
    return response;
}
