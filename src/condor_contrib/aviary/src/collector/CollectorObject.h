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

#ifndef _COLLECTOROBJECT_H
#define _COLLECTOROBJECT_H

// condor includes
#include "condor_common.h"
#include "condor_classad.h"
#include "condor_adtypes.h"

#include "Collectables.h"
#include "ClassadCodec.h"

using namespace std;
using namespace compat_classad;
using namespace aviary::codec;

namespace aviary {
namespace collector {

typedef multimap<int,Slot*> SlotDateMapType;
typedef multimap<int,Master*> MasterDateMapType;

typedef map<string,Collector*> CollectorMapType;
typedef map<string,Master*> MasterMapType;
typedef map<string,Negotiator*> NegotiatorMapType;
typedef map<string,Scheduler*> SchedulerMapType;
typedef map<string,Submitter*> SubmitterMapType;
// slots... STATIC, PARTITIONABLE, DYNAMIC
typedef map<string,Slot*> SlotMapType;

typedef set<Collector*> CollectorSetType;
typedef set<Master*> MasterSetType;
typedef set<Negotiator*> NegotiatorSetType;
typedef set<Scheduler*> SchedulerSetType;
typedef set<Slot*> SlotSetType;
typedef set<Submitter*> SubmitterSetType;

typedef map<string,SlotSetType*> SlotDynamicType;
    
class CollectorObject
{
public:

    // RPC-facing methods
    void findCollector(const string& name, bool grep, CollectorSetType& coll_set);
    void findMaster(const string& name, bool grep, MasterSetType& master_set);
    void findNegotiator(const string& name, bool grep, NegotiatorSetType& neg_set);
    void findScheduler(const string& name, bool grep, SchedulerSetType& schedd_set);
    void findSlot(const string& name, bool grep, SlotSetType& slot_set);
    void findSubmitter(const string& name, bool grep, SubmitterSetType& subm_set);
    bool findAttribute(AdTypes daemon_type, const string& name, const string& ip_addr,
                       AttributeMapType& requested_attr_map, AttributeMapType& resource_attr_map);

    // daemonCore-facing methods
    bool update(int command, const ClassAd& ad);
    bool invalidate(int command, const ClassAd& ad);
    void invalidateAll();

    static CollectorObject* getInstance();
    ~CollectorObject();

    string getPool();
    string getMyAddress() { return m_address; }
    void setMyAddress(const char* myaddr) { m_address = myaddr; }

    // public for easy return of entire collection
    CollectorMapType collectors;
    MasterMapType masters;
    NegotiatorMapType negotiators;
    SchedulerMapType schedulers;
    SlotMapType stable_slots;
    SlotMapType dynamic_slots;
    SubmitterMapType submitters;

    // birthdate indices for the prolific daemons
    SlotDateMapType stable_slot_ids;
    MasterDateMapType master_ids;

    // cross-reference for pslot to dslot
    SlotDynamicType pslots;

private:
    string m_address;
    static CollectorObject* m_instance;
    CollectorObject();
    CollectorObject(CollectorObject const&);
    CollectorObject& operator=(CollectorObject const&);
    BaseCodec* m_codec;

    Slot* updateSlot(const ClassAd& ad);
    Slot* invalidateSlot(const ClassAd& ad);
    Slot* findPartitionable(Slot* slot);

};

}} /* aviary::collector */

#endif /* _COLLECTOROBJECT_H */
