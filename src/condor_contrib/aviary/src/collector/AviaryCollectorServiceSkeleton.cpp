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
#include "condor_attributes.h"

// c++ includes
#include <algorithm>

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

// private local impl START

AdTypes mapResourceTypeToAdType(int res_type) {
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

template <class DateMapT>
bool dateCompare(const DateMapT& x, const DateMapT& y) {
  return x.first <= y.first;
}

// need a way to step over qdate dupes when a specific offset comes in
// treat name/address as the secondary "key"
bool advanceDateIndex(DaemonCollectable* index, ResourceID* offset) {
    bool advance = false;

    // quick check
    if (index->DaemonStartTime != offset->getBirthdate()) {
        return advance;
    }

    // now see how specific the offset is
    if (!offset->isNameNil() && !offset->getName().empty()) {
        advance = (index->Name == offset->getName());
    }
    if (!offset->isAddressNil() && !offset->getAddress().empty()) {
        advance = (index->MyAddress == offset->getAddress());
    }

    return advance;
}

template <class CollectableBirthdateMapT, class CollectableMapT, class RequestT, class ResponseT>
void pagedResults(CollectableBirthdateMapT& birthdate_map, CollectableMapT& collectable_map, 
                  RequestT* request, ResponseT* response)
{
    ResourceID* offset = NULL;
    ScanMode* mode = NULL;

    int size = request->getSize();
    int bdate;
    bool scan_back = false;

    // some fast track stuff... should be empty together
    if (birthdate_map.empty() && collectable_map.empty()) {
            response->setRemaining(0);
            return;
    }

    if (!request->isOffsetNil()) {
        offset = request->getOffset();
        bdate = offset->getBirthdate();
    }

    if (!request->isModeNil()) {
        mode = request->getMode();
    }

    // see if we are scanning using a qdate index
    if (mode) {

        typename CollectableBirthdateMapT::iterator it, start, last;
        int i=0;

        scan_back = mode->getScanModeEnum() == ScanMode_BEFORE;

        // BEFORE mode
        if (scan_back) {
            if (offset) {
                start = max_element(
                            birthdate_map.begin(),
                            birthdate_map.upper_bound(
                                offset->getBirthdate()
                            ),
                            dateCompare<typename CollectableBirthdateMapT::value_type>
                        );
            }
            else {
                start = --birthdate_map.end();
            }
            it=last=start;
            if (bdate>=(*it).second->DaemonStartTime && bdate>0)  {
                do {
                    if (!advanceDateIndex((*it).second,offset)) {
                        CollectableCodec codec(provider->getEnv());
                        response->addResults(codec.encode((*it).second,false));
                        i++;
                    }
                    last = it;
                }
                while (birthdate_map.begin()!=it-- && i<size);
            }
            response->setRemaining(distance(birthdate_map.begin(),last));
        }
        // AFTER mode
        else {
            if (offset) {
                start = birthdate_map.upper_bound(offset->getBirthdate());
            }
            else {
                start = birthdate_map.begin();
            }
            it = --birthdate_map.end();
            // TODO: integer rollover, but interop of xsd unsignedInt?
            if (bdate<it->second->DaemonStartTime && bdate<INT_MAX)  {
                for (it=start; it!=birthdate_map.end() && i<size; it++) {
                     if (!advanceDateIndex((*it).second,offset)) {
                        CollectableCodec codec(provider->getEnv());
                        response->addResults(codec.encode((*it).second,false));
                        i++;
                     }
                }
            }
            response->setRemaining(i?distance(it,birthdate_map.end()):0);
        }

        return;
    }

    // otherwise it is a lexical scan of the collectable
    typename CollectableMapT::iterator it,start;
    if (offset) {
        start = collectable_map.find(offset->getName().c_str());
    }
    else {
        start = collectable_map.begin();
    }

    // bi-directional iterator
    int i=0;
    for (it=start; it!=collectable_map.end() && i<size; it++)
    {
        CollectableCodec codec(provider->getEnv());
        response->addResults(codec.encode((*it).second,false));
        i++;
    }

    response->setRemaining(distance(it,collectable_map.end()));

}

// public RPC methods START

GetCollectorResponse* AviaryCollectorServiceSkeleton::getCollector(MessageContext* /*outCtx*/ ,GetCollector* _getCollector)
{
    GetCollectorResponse* response = new GetCollectorResponse;

    CollectorObject* co = CollectorObject::getInstance();

    loadResults<AviaryCommon::Collector,CollectorMapType,GetCollector,GetCollectorResponse>(co->collectors,_getCollector,response);

    return response;
}

GetMasterResponse* AviaryCollectorServiceSkeleton::getMaster(MessageContext* /*outCtx*/ ,GetMaster* _getMaster)
{
    GetMasterResponse* response = new GetMasterResponse;

    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Master,MasterMapType,GetMaster,GetMasterResponse>(co->masters,_getMaster,response);
    
    return response;
}

GetNegotiatorResponse* AviaryCollectorServiceSkeleton::getNegotiator(MessageContext* /*outCtx*/ ,GetNegotiator* _getNegotiator)
{
    GetNegotiatorResponse* response = new GetNegotiatorResponse;

    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Negotiator,NegotiatorMapType,GetNegotiator,GetNegotiatorResponse>(co->negotiators,_getNegotiator,response);
    
    return response;
}

GetSlotResponse* AviaryCollectorServiceSkeleton::getSlot(MessageContext* /*outCtx*/ ,GetSlot* _getSlot)
{
    GetSlotResponse* response = new GetSlotResponse;

    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Slot,SlotMapType,GetSlot,GetSlotResponse>(co->stable_slots,_getSlot,response);

    // now attach dslots to any pslots if requested
    // TODO: 2 pass
    if (!_getSlot->isIncludeDynamicNil() && _getSlot->getIncludeDynamic()) {
        vector<AviaryCommon::Slot*>* resp_pslots = response->getResults();
        if (resp_pslots && !resp_pslots->empty()) {
            for (vector<AviaryCommon::Slot*>::iterator it = resp_pslots->begin(); it != resp_pslots->end(); it++) {
                if ((*it)->getSlot_type()->getSlotTypeEnum() == SlotType_PARTITIONABLE) {
                    SlotDynamicType:: iterator pit = co->pslots.find((*it)->getId()->getName());
                    if (pit != co->pslots.end()) {
                        for (SlotSetType::iterator dit = (*pit).second->begin(); dit != (*pit).second->end(); dit++) {
                                CollectableCodec codec(provider->getEnv());
                                AviaryCommon::Slot* dslot = codec.encode((*dit),_getSlot->getIncludeSummaries());
                                Status* js = new Status;
                                js->setCode(new StatusCodeType("OK"));
                                dslot->setStatus(js);
                                (*it)->addDynamic_slots(dslot);
                        }
                    }
                }
            }
        }
    }

    return response;
}

GetSchedulerResponse* AviaryCollectorServiceSkeleton::getScheduler(MessageContext* /*outCtx*/ ,GetScheduler* _getScheduler)
{
    GetSchedulerResponse* response = new GetSchedulerResponse;

    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Scheduler,SchedulerMapType,GetScheduler,GetSchedulerResponse>(co->schedulers,_getScheduler,response);

    return response;
}

GetSubmitterResponse* AviaryCollectorServiceSkeleton::getSubmitter(MessageContext* /*outCtx*/ ,GetSubmitter* _getSubmitter)
{
    GetSubmitterResponse* response = new GetSubmitterResponse;

    CollectorObject* co = CollectorObject::getInstance();
    
    loadResults<AviaryCommon::Submitter,SubmitterMapType,GetSubmitter,GetSubmitterResponse>(co->submitters,_getSubmitter,response);

    return response;
}

// ClassAd attribute queries
GetAttributesResponse* AviaryCollectorServiceSkeleton::getAttributes(MessageContext* /*outCtx*/ ,GetAttributes* _getAttributes)
{
    GetAttributesResponse* response = new GetAttributesResponse;

    CollectorObject* co = CollectorObject::getInstance();

    AttrRequestList* attr_req_list = _getAttributes->getIds();

    for (AttrRequestList::iterator it = attr_req_list->begin(); attr_req_list->end() != it; it++) {
        AttributeRequest* attr_req = (*it);
        ResourceID* res_id = attr_req->getId();
        AttrResponseList attr_resp_list;
        AdTypes res_type = mapResourceTypeToAdType(res_id->getResource()->getResourceTypeEnum());

        AttributeMapType requested_attr_map,resource_attr_map;
        // if they have supplied attribute names pre-load our map
        if (!attr_req->isNamesNil() && attr_req->getNames()!=0) {
            vector<string*>* name_list = attr_req->getNames();
            for (vector<string*>::iterator nit = name_list->begin();nit!=name_list->end();nit++) {
                requested_attr_map[*(*nit)] = NULL;
            }
        }

        AttributeResponse* attr_resp = new AttributeResponse;
        ResourceID* copy_res_id = new ResourceID;
        copy_res_id->setResource(new ResourceType(res_id->getResource()->getResourceTypeEnum()));
        if (co->findAttribute(res_type, res_id->getName(), res_id->getAddress(),
                requested_attr_map,resource_attr_map)) {
            AviaryCommon::Attributes* attrs = new AviaryCommon::Attributes;
            mapToXsdAttributes(requested_attr_map,attrs);

            // build from the resource id map in hand
            copy_res_id->setPool(co->getPool());
            copy_res_id->setName(resource_attr_map[ATTR_NAME]->getValue());
            copy_res_id->setAddress(resource_attr_map[ATTR_MY_ADDRESS]->getValue());
            if (!res_id->isSub_typeNil() && !res_id->getSub_type().empty()) {
                copy_res_id->setSub_type(res_id->getSub_type());
            }
            copy_res_id->setBirthdate(atoi(resource_attr_map[ATTR_DAEMON_START_TIME]->getValue()));
            attr_resp->setId(copy_res_id);
            attr_resp->setAd(attrs);
            Status* status = new Status;
            status->setCode(new StatusCodeType("OK"));
            attr_resp->setStatus(status);
        }
        else {
            // no match so just reflect back the supplied fields we were given
            if (!res_id->isPoolNil() && !res_id->getPool().empty()) {
                copy_res_id->setPool(res_id->getPool());
            }
            copy_res_id->setName(res_id->getName());
            copy_res_id->setAddress(res_id->getAddress());
            if (!res_id->isSub_typeNil() && !res_id->getSub_type().empty()) {
                copy_res_id->setSub_type(res_id->getSub_type());
            }
            if (!res_id->isBirthdateNil()) {
                copy_res_id->setBirthdate(res_id->getBirthdate());
            }
            attr_resp->setId(copy_res_id);
            Status* status = new Status;
            status->setCode(new StatusCodeType("NO_MATCH"));
            status->setText("no such resource");
            attr_resp->setStatus(status);
        }
        response->addResults(attr_resp);
        
        // cleanup after ourselves
        for (aviary::codec::AttributeMapType::iterator it = requested_attr_map.begin();requested_attr_map.end() != it; it++) {
            delete (*it).second;
        }
        for (aviary::codec::AttributeMapType::iterator it = resource_attr_map.begin();resource_attr_map.end() != it; it++) {
            delete (*it).second;
        }
    }

    return response;
}

// id paging
GetMasterIDResponse* AviaryCollectorServiceSkeleton::getMasterID(MessageContext* /*outCtx*/ ,GetMasterID* _getMasterID)
{
    GetMasterIDResponse* response = new GetMasterIDResponse;

    CollectorObject* co = CollectorObject::getInstance();

    pagedResults<MasterDateMapType,MasterMapType,GetMasterID,GetMasterIDResponse>(co->master_ids,co->masters,_getMasterID,response);

    return response;
}

GetSlotIDResponse* AviaryCollectorServiceSkeleton::getSlotID(MessageContext* /*outCtx*/ ,GetSlotID* _getSlotID)
{
    GetSlotIDResponse* response = new GetSlotIDResponse;

    CollectorObject* co = CollectorObject::getInstance();

    pagedResults<SlotDateMapType,SlotMapType,GetSlotID,GetSlotIDResponse>(co->stable_slot_ids,co->stable_slots,_getSlotID,response);

    return response;
}
