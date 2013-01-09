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

typedef vector<string*> IDList;

// generic load of results
template <class AviaryCollectableT, class CollectableMapT, class RequestT, class ResponseT>
void loadResults(CollectableMapT& cmt, RequestT* request, ResponseT* response)
{
    bool summary = request->isIncludeSummariesNil() || request->getIncludeSummaries();
    bool partials = request->isPartialMatchesNil() || request->getPartialMatches();
    if (request->isIdsNil() || request->getIds()->size() == 0) {
        for (typename CollectableMapT::iterator it = cmt.begin(); cmt.end() != it; it++) {
            CollectableCodec codec(provider->getEnv());
            AviaryCollectableT* _collectable = codec.encode(it->second);
            Status* js = new Status;
            js->setCode(new StatusCodeType("OK"));
            _collectable->setStatus(js);
            response->addResults(_collectable);
        }
    }
    else {
        IDList* id_list = request->getIds();
        for (IDList::iterator it = id_list->begin(); id_list->end() != it; it++) {
            
        }
    }
}

GetAttributesResponse* AviaryCollectorServiceSkeleton::getAttributes(MessageContext* /*outCtx*/ ,GetAttributes* _getAttributes)
{
    /* TODO fill this with the necessary business logic */
    return (GetAttributesResponse*)NULL;
}

GetCollectorResponse* AviaryCollectorServiceSkeleton::getCollector(MessageContext* /*outCtx*/ ,GetCollector* _getCollector)
{
    /* TODO fill this with the necessary business logic */
    GetCollectorResponse* response = new GetCollectorResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();

    loadResults<AviaryCommon::Collector,CollectorMapType,GetCollector,GetCollectorResponse>(co->collectors,_getCollector,response);

    return response;
}

GetMasterResponse* AviaryCollectorServiceSkeleton::getMaster(MessageContext* /*outCtx*/ ,GetMaster* _getMaster)
{
    /* TODO fill this with the necessary business logic */
    GetMasterResponse* response = new GetMasterResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Master,MasterMapType,GetMaster,GetMasterResponse>(co->masters,_getMaster,response);
    
    return response;
}

GetMasterIDResponse* AviaryCollectorServiceSkeleton::getMasterID(MessageContext* /*outCtx*/ ,GetMasterID* _getMasterID)
{
    /* TODO fill this with the necessary business logic */
    return (GetMasterIDResponse*)NULL;
}

GetNegotiatorResponse* AviaryCollectorServiceSkeleton::getNegotiator(MessageContext* /*outCtx*/ ,GetNegotiator* _getNegotiator)
{
    /* TODO fill this with the necessary business logic */
    GetNegotiatorResponse* response = new GetNegotiatorResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Negotiator,NegotiatorMapType,GetNegotiator,GetNegotiatorResponse>(co->negotiators,_getNegotiator,response);
    
    return response;
}

GetSlotResponse* AviaryCollectorServiceSkeleton::getSlot(MessageContext* /*outCtx*/ ,GetSlot* _getSlot)
{
    /* TODO fill this with the necessary business logic */
    GetSlotResponse* response = new GetSlotResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Slot,SlotMapType,GetSlot,GetSlotResponse>(co->slots,_getSlot,response);

    return response;
}

GetSlotIDResponse* AviaryCollectorServiceSkeleton::getSlotID(MessageContext* /*outCtx*/ ,GetSlotID* _getSlotID)
{
    /* TODO fill this with the necessary business logic */
    return (GetSlotIDResponse*)NULL;
}

GetSchedulerResponse* AviaryCollectorServiceSkeleton::getScheduler(MessageContext* /*outCtx*/ ,GetScheduler* _getScheduler)
{
    /* TODO fill this with the necessary business logic */
    GetSchedulerResponse* response = new GetSchedulerResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Scheduler,SchedulerMapType,GetScheduler,GetSchedulerResponse>(co->schedulers,_getScheduler,response);

    return response;
}


GetSubmitterResponse* AviaryCollectorServiceSkeleton::getSubmitter(MessageContext* /*outCtx*/ ,GetSubmitter* _getSubmitter)
{
    /* TODO fill this with the necessary business logic */
    GetSubmitterResponse* response = new GetSubmitterResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Submitter,SubmitterMapType,GetSubmitter,GetSubmitterResponse>(co->submitters,_getSubmitter,response);

    return response;
}
