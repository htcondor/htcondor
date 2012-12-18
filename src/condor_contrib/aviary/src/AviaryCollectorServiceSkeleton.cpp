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
#include "AviaryCollectorServiceSkeleton.h"
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

GetAttributesResponse* AviaryCollectorServiceSkeleton::getAttributes(MessageContext *outCtx ,GetAttributes* _getAttributes)
{
    /* TODO fill this with the necessary business logic */
    return (GetAttributesResponse*)NULL;
}

GetCollectorResponse* AviaryCollectorServiceSkeleton::getCollector(MessageContext *outCtx ,GetCollector* _getCollector)
{
    /* TODO fill this with the necessary business logic */
    return (GetCollectorResponse*)NULL;
}

GetMasterResponse* AviaryCollectorServiceSkeleton::getMaster(MessageContext *outCtx ,GetMaster* _getMaster)
{
    /* TODO fill this with the necessary business logic */
    GetMasterResponse* response = new GetMasterResponse;
    MasterSetType slot_set;
    CollectorObject* co = CollectorObject::getInstance();
    for (MasterMapType::iterator it = co->masters.begin(); co->masters.end() != it; it++) {
    }
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
    return (GetNegotiatorResponse*)NULL;
}

GetSlotResponse* AviaryCollectorServiceSkeleton::getSlot(MessageContext *outCtx ,GetSlot* _getSlot)
{
    /* TODO fill this with the necessary business logic */
    GetSlotResponse* response = new GetSlotResponse;
    SlotSetType slot_set;
    CollectorObject* co = CollectorObject::getInstance();
    SlotMapType& slot_map = co->slots;
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
    return (GetSchedulerResponse*)NULL;
}


GetSubmitterResponse* AviaryCollectorServiceSkeleton::getSubmitter(MessageContext *outCtx ,GetSubmitter* _getSubmitter)
{
    /* TODO fill this with the necessary business logic */
    return (GetSubmitterResponse*)NULL;
}
