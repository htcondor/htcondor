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

//condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_commands.h"
#include "hashkey.h"
#include "stl_string_utils.h"

// C++ includes
// enable for debugging classad to ostream
// watch out for unistd clash
//#include <sstream>

//local includes
#include "CollectorObject.h"
#include "Collectables.h"
#include "AviaryConversionMacros.h"
#include "AviaryUtils.h"

using namespace std;
using namespace aviary::collector;
using namespace aviary::util;

namespace aviary {
namespace collector {

CollectorObject collector;

}}

CollectorObject::CollectorObject ()
{
    //
}

CollectorObject::~CollectorObject()
{
    //
}

CollectorObject* CollectorObject::getInstance()
{
    if (!m_instance) {
        m_instance = new CollectorObject();
    }
    return m_instance;
}

string 
CollectorObject::getPool() {
    return getPoolName();
}

// private to impl
template<class CollectablesT, class CollectableT>
bool updateCollectable(const ClassAd& ad, CollectablesT& collectables)
{
    string name;
    if (!ad.LookupString(ATTR_NAME,name)) {
        return false;
    }
    typename CollectablesT::iterator it = collectables.find(name);
    if (it == collectables.end()) {
        CollectableT* collectable = new CollectableT;
        collectable->update(ad);
        collectables.insert(make_pair(name,collectable));
    }
    else {
        (*it).second->update(ad);
    }
    return true;
}

template<class CollectablesT>
bool invalidateCollectable(const ClassAd& ad, CollectablesT& collectables)
{
    string name;
    if (!ad.LookupString(ATTR_NAME,name)) {
        return false;
    }
    typename CollectablesT::iterator it = collectables.find(name);
    if (it == collectables.end()) {
       return false;
    }
    else {
        collectables.erase(it);
    }
    return true;
}

// slots are special due to their dynamic/partitionable links
bool updateSlot(const ClassAd& ad, SlotMapType& slots)
{
    
    bool status = false;
    status = updateCollectable<SlotMapType,Slot>(ad, slots);
    //TODO: fix association with other slots
    return status;
}

bool invalidateSlot(const ClassAd& ad, SlotMapType& slots)
{
    bool status = false;
    status = invalidateCollectable<SlotMapType>(ad, slots);
    // TODO: disassociate links to dynamic slots
    return status;
}

// daemonCore methods
bool
CollectorObject::update(int command, const ClassAd& ad)
{
    bool status = false;
    switch (command) {
        case UPDATE_COLLECTOR_AD:
            status = updateCollectable<CollectorMapType,Collector>(ad, m_collectors);
            break;
        case UPDATE_MASTER_AD:
            status = updateCollectable<MasterMapType,Master>(ad, m_masters);
            break;
        case UPDATE_NEGOTIATOR_AD:
            status = updateCollectable<NegotiatorMapType,Negotiator>(ad, m_negotiators);
            break;
        case UPDATE_SCHEDD_AD:
            status = updateCollectable<SchedulerMapType,Scheduler>(ad, m_schedulers);
            break;
        case UPDATE_STARTD_AD:
            status = updateSlot(ad, m_slots);
            break;
        case UPDATE_SUBMITTOR_AD:
            status = updateCollectable<SubmitterMapType,Submitter>(ad, m_submitters);
            break;
        default:
            // fall through on unknown command
            break;
    }
    return status;
}

bool
CollectorObject::invalidate(int command, const ClassAd& ad)
{
    bool status = false;
    switch (command) {
        case INVALIDATE_COLLECTOR_ADS:
            status = invalidateCollectable<CollectorMapType>(ad, m_collectors);
            break;
        case INVALIDATE_MASTER_ADS:
            status = invalidateCollectable<MasterMapType>(ad, m_masters);
            break;
        case INVALIDATE_NEGOTIATOR_ADS:
            status = invalidateCollectable<NegotiatorMapType>(ad, m_negotiators);
            break;
        case INVALIDATE_SCHEDD_ADS:
            status = invalidateCollectable<SchedulerMapType>(ad, m_schedulers);
            break;
        case INVALIDATE_STARTD_ADS:
            status = invalidateSlot(ad, m_slots);
            break;
        case INVALIDATE_SUBMITTOR_ADS:
            status = invalidateCollectable<SubmitterMapType>(ad, m_submitters);
            break;
        default:
            // fall through on unknown command
            break;
    }
    return status;
}

// private to impl
template<class CollectableMapT, class CollectableSetT>
void findCollectable(const string& name, bool grep, CollectableMapT& coll_map, CollectableSetT& coll_set)
{
    typename CollectableMapT::iterator it;
    if (!grep && !name.empty()) { // exact match
        it = coll_map.find(name);
        if (it != coll_map.end()) {
            coll_set.insert((*it).second);
        }
    }
    else // we are scanning for a partial name match
    {
        bool no_name = name.empty();
        for (it = coll_map.begin(); it != coll_map.end(); it++) {
            // we are scanning for a partial name match or 
            // grabbing all of them by virtue of no name supplied
            if (no_name || (string::npos != (*it).second->Name.find(name))) {
                coll_set.insert((*it).second);
            }
        }
    }
}

// RPC API methods
void
CollectorObject::findCollector(const string& name, bool grep, CollectorSetType& coll_set) 
{
    findCollectable<CollectorMapType,CollectorSetType>(name, grep, m_collectors, coll_set);
}

void
CollectorObject::findMaster(const string& name, bool grep, MasterSetType& master_set) 
{
    findCollectable<MasterMapType,MasterSetType>(name, grep, m_masters, master_set);
}

void
CollectorObject::findNegotiator(const string& name, bool grep, NegotiatorSetType& neg_set) 
{
    findCollectable<NegotiatorMapType,NegotiatorSetType>(name, grep, m_negotiators, neg_set);
}

void
CollectorObject::findScheduler(const string& name, bool grep, SchedulerSetType& schedd_set) 
{
    findCollectable<SchedulerMapType,SchedulerSetType>(name, grep, m_schedulers, schedd_set);
}

void
CollectorObject::findSlot(const string& name, bool grep, SlotSetType& slot_set) 
{
    findCollectable<SlotMapType,SlotSetType>(name, grep, m_slots, slot_set);
    // TODO: associations?
}

void
CollectorObject::findSubmitter(const string& name, bool grep, SubmitterSetType& subm_set) 
{
    findCollectable<SubmitterMapType,SubmitterSetType>(name, grep, m_submitters, subm_set);
}