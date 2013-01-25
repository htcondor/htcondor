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
#include "condor_adtypes.h"

// axis2 includes
#include "Environment.h"

// local includes
#include "CollectorObject.h"
#include "CollectableCodec.h"
#include "AviaryCollectorServiceSkeleton.h"
#include "Axis2SoapProvider.h"
#include "AviaryUtils.h"
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
using namespace aviary::util;

extern aviary::soap::Axis2SoapProvider* provider;

typedef vector<string*> IDList;
typedef vector<AttributeRequest*> AttrRequestList;
typedef vector<AttributeResponse*> AttrResponseList;

typedef std::map<int,AdTypes> AdMapType;
AdMapType ad_type_map;

AdTypes mapAdTypetoResourceType(int res_type) {
    AdTypes result = NO_AD;
    if (ad_type_map.empty()) {
        ad_type_map[ResourceType_ANY] =  ANY_AD;
        ad_type_map[ResourceType_COLLECTOR] =  COLLECTOR_AD;
        ad_type_map[ResourceType_MASTER] =  MASTER_AD;
        ad_type_map[ResourceType_NEGOTIATOR] =  NEGOTIATOR_AD;
        ad_type_map[ResourceType_SCHEDULER] =  SCHEDD_AD;
        ad_type_map[ResourceType_SLOT] =  STARTD_AD;
    }
    map<int,AdTypes>::iterator it = ad_type_map.find(res_type);
    if (it != ad_type_map.end()) {
        result = (*it).second;
    }
    return result;
}

// generic load of results
template <class AviaryCollectableT, class CollectableMapT, class RequestT, class ResponseT>
void loadResults(CollectableMapT& cmt, RequestT* request, ResponseT* response)
{
    bool summary = request->isIncludeSummariesNil() || request->getIncludeSummaries();
    bool partials = request->isPartialMatchesNil() || request->getPartialMatches();
    if (request->isIdsNil() || request->getIds()->size() == 0) {
        // return all results
        for (typename CollectableMapT::iterator it = cmt.begin(); cmt.end() != it; it++) {
            CollectableCodec codec(provider->getEnv());
            AviaryCollectableT* _collectable = codec.encode(it->second,summary);
            Status* js = new Status;
            js->setCode(new StatusCodeType("OK"));
            _collectable->setStatus(js);
            response->addResults(_collectable);
        }
    }
    else {
        IDList* id_list = request->getIds();
        for (IDList::iterator it = id_list->begin(); id_list->end() != it; it++) {
            // look for exact match on each supplied id
            if (!partials) {
                typename CollectableMapT::iterator mit = cmt.find(*(*it));
                if (mit != cmt.end()) {
                    CollectableCodec codec(provider->getEnv());
                    AviaryCollectableT* _collectable = codec.encode(mit->second,summary);
                    Status* js = new Status;
                    js->setCode(new StatusCodeType("OK"));
                    _collectable->setStatus(js);
                    response->addResults(_collectable);
                }
            }
            else {
                // fuzzy matching...is the supplied id string in the full id name?
                for (typename CollectableMapT::iterator mit = cmt.begin(); cmt.end() != mit; mit++) {
                    if (string::npos != mit->first.find(*(*it))) {
                        CollectableCodec codec(provider->getEnv());
                        AviaryCollectableT* _collectable = codec.encode(mit->second,summary);
                        Status* js = new Status;
                        js->setCode(new StatusCodeType("OK"));
                        _collectable->setStatus(js);
                        response->addResults(_collectable);
                    }
                }
            }
        }
    }
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

// ClassAd attribute queries
GetAttributesResponse* AviaryCollectorServiceSkeleton::getAttributes(MessageContext* /*outCtx*/ ,GetAttributes* _getAttributes)
{
    /* TODO fill this with the necessary business logic */
    GetAttributesResponse* response = new GetAttributesResponse;
    string error;
    CollectorObject* co = CollectorObject::getInstance();

    AttrRequestList* attr_req_list = _getAttributes->getIds();

    for (AttrRequestList::iterator it = attr_req_list->begin(); attr_req_list->end() != it; it++) {
        AttributeRequest* attr_req = (*it);
        ResourceID* res_id = attr_req->getId();
        AttrResponseList attr_resp_list;
        AdTypes res_type = mapAdTypetoResourceType(res_id->getResource()->getResourceTypeEnum());

        AttributeMapType attr_map;
        // if they have supplied attribute names pre-load our map
        if (!attr_req->isNamesNil() && attr_req->getNames()!=0) {
            vector<string*>* name_list = attr_req->getNames();
            for (vector<string*>::iterator nit = name_list->begin();nit!=name_list->end();nit++) {
                attr_map[*(*nit)] = NULL;
            }
        }

        co->findAttribute(res_type, res_id->getName(), res_id->getAddress(),attr_map);
        AviaryCommon::Attributes* attrs = new AviaryCommon::Attributes;
        mapToXsdAttributes(attr_map,attrs);

        AttributeResponse* attr_resp = new AttributeResponse;
        // caa't rely on copy ctor for this
        ResourceID* copy_res_id = new ResourceID;
        copy_res_id->setResource(new ResourceType(res_id->getResource()->getResourceTypeEnum()));
        if (!res_id->isPoolNil() && !res_id->getPool().empty()) {
            copy_res_id->setPool(res_id->getPool());
        }
        copy_res_id->setName(res_id->getName());
        copy_res_id->setAddress(res_id->getAddress());
        if (!res_id->isSub_typeNil() && !res_id->getSub_type().empty()) {
            copy_res_id->setSub_type(res_id->getSub_type());
        }
        copy_res_id->setBirthdate(res_id->getBirthdate());
        attr_resp->setId(copy_res_id);
        attr_resp->setAd(attrs);
        Status* status = new Status;
        status->setCode(new StatusCodeType("OK"));
        attr_resp->setStatus(status);
        response->addResults(attr_resp);
    }

    return response;
}

// id paging
GetMasterIDResponse* AviaryCollectorServiceSkeleton::getMasterID(MessageContext* /*outCtx*/ ,GetMasterID* _getMasterID)
{
    /* TODO fill this with the necessary business logic */
    return (GetMasterIDResponse*)NULL;
}

GetSlotIDResponse* AviaryCollectorServiceSkeleton::getSlotID(MessageContext* /*outCtx*/ ,GetSlotID* _getSlotID)
{
    /* TODO fill this with the necessary business logic */
    return (GetSlotIDResponse*)NULL;
}
