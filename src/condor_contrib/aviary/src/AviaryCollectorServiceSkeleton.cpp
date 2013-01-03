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

// axis2 includes
#include "Environment.h"

// local includes
#include "CollectorObject.h"
#include "CollectableCodec.h"
#include "AviaryCollectorServiceSkeleton.h"
#include "Axis2SoapProvider.h"
#include <AviaryCollector_GetSlotID.h>
#include <AviaryCollector_GetSlotIDResponse.h>
#include <AviaryCollector_GetNegotiator.h>
#include <AviaryCollector_GetNegotiatorResponse.h>
#include <AviaryCollector_GetSubmitter.h>
#include <AviaryCollector_GetSubmitterResponse.h>
#include <AviaryCollector_GetSlot.h>
#include <AviaryCollector_GetSlotResponse.h>
#include <AviaryCollector_GetAttributes.h>
#include <AviaryCollector_GetAttributesResponse.h>
#include <AviaryCollector_GetScheduler.h>
#include <AviaryCollector_GetSchedulerResponse.h>
#include <AviaryCollector_GetCollector.h>
#include <AviaryCollector_GetCollectorResponse.h>
#include <AviaryCollector_GetMasterID.h>
#include <AviaryCollector_GetMasterIDResponse.h>
#include <AviaryCollector_GetMaster.h>
#include <AviaryCollector_GetMasterResponse.h>

using namespace std;
using namespace wso2wsf;
using namespace AviaryCommon;
using namespace AviaryCollector;
using namespace aviary::collector;

extern aviary::soap::Axis2SoapProvider* provider;

// generic load of results
template <class AviaryCollectableT, class CollectableMapT, class ResponseT>
void loadAllResults(CollectableMapT& cmt, ResponseT* response, string& error)
{
    for (typename CollectableMapT::iterator it = cmt.begin(); cmt.end() != it; it++) {
        CollectableCodec codec(provider->getEnv());
        AviaryCollectableT* _collectable = codec.encode(it->second);
        _collectable->setStatus(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),error));
        response->addResults(_collectable);
    }
}

GetAttributesResponse* AviaryCollectorServiceSkeleton::getAttributes(MessageContext *outCtx ,GetAttributes* _getAttributes)
{
    /* TODO fill this with the necessary business logic */
    return (GetAttributesResponse*)NULL;
}

GetCollectorResponse* AviaryCollectorServiceSkeleton::getCollector(MessageContext *outCtx ,GetCollector* _getCollector)
{
    /* TODO fill this with the necessary business logic */
    GetCollectorResponse* response = new GetCollectorResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();

    loadAllResults<AviaryCommon::Collector,CollectorMapType,GetCollectorResponse>(co->collectors,response,error);

    return response;
}

GetMasterResponse* AviaryCollectorServiceSkeleton::getMaster(MessageContext *outCtx ,GetMaster* _getMaster)
{
    /* TODO fill this with the necessary business logic */
    GetMasterResponse* response = new GetMasterResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadAllResults<AviaryCommon::Master,MasterMapType,GetMasterResponse>(co->masters,response,error);
    
    return response;
}

GetMasterIDResponse* AviaryCollectorServiceSkeleton::getMasterID(MessageContext *outCtx ,GetMasterID* _getMasterID)
{
    /* TODO fill this with the necessary business logic */
    return (GetMasterIDResponse*)NULL;
}

GetNegotiatorResponse* AviaryCollectorServiceSkeleton::getNegotiator(MessageContext *outCtx ,GetNegotiator* _getNegotiator)
{
    /* TODO fill this with the necessary business logic */
    GetNegotiatorResponse* response = new GetNegotiatorResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadAllResults<AviaryCommon::Negotiator,NegotiatorMapType,GetNegotiatorResponse>(co->negotiators,response,error);
    
    return response;
}

GetSlotResponse* AviaryCollectorServiceSkeleton::getSlot(MessageContext *outCtx ,GetSlot* _getSlot)
{
    /* TODO fill this with the necessary business logic */
    GetSlotResponse* response = new GetSlotResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadAllResults<AviaryCommon::Slot,SlotMapType,GetSlotResponse>(co->slots,response,error);

    return response;
}

GetSlotIDResponse* AviaryCollectorServiceSkeleton::getSlotID(MessageContext *outCtx ,GetSlotID* _getSlotID)
{
    /* TODO fill this with the necessary business logic */
    return (GetSlotIDResponse*)NULL;
}

GetSchedulerResponse* AviaryCollectorServiceSkeleton::getScheduler(MessageContext *outCtx ,GetScheduler* _getScheduler)
{
    /* TODO fill this with the necessary business logic */
    GetSchedulerResponse* response = new GetSchedulerResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadAllResults<AviaryCommon::Scheduler,SchedulerMapType,GetSchedulerResponse>(co->schedulers,response,error);

    return response;
}


GetSubmitterResponse* AviaryCollectorServiceSkeleton::getSubmitter(MessageContext *outCtx ,GetSubmitter* _getSubmitter)
{
    /* TODO fill this with the necessary business logic */
    GetSubmitterResponse* response = new GetSubmitterResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadAllResults<AviaryCommon::Submitter,SubmitterMapType,GetSubmitterResponse>(co->submitters,response,error);

    return response;
}
