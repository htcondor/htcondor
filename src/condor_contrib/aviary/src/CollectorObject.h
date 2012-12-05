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

#include "Collectables.h"

using namespace std;
using namespace compat_classad;

namespace aviary {
namespace collector {

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
    
class CollectorObject
{
public:

    // RPC-facing method
    void findCollector(const string& name, bool grep, CollectorSetType& coll_set);
    void findMaster(const string& name, bool grep, MasterSetType& master_set);
    void findNegotiator(const string& name, bool grep, NegotiatorSetType& neg_set);
    void findScheduler(const string& name, bool grep, SchedulerSetType& schedd_set);
    void findSlot(const string& name, bool grep, SlotSetType& slot_set);
    void findSubmitter(const string& name, bool grep, SubmitterSetType& subm_set);

    // daemonCore-facing methods
    bool update(int command, const ClassAd& ad);
    bool invalidate(int command, const ClassAd& ad);
    void invalidateAll();
    
    static CollectorObject* getInstance();

    CollectorObject();
    ~CollectorObject();
    string getPool();
    string getMyAddress() { return m_address; }
    void setMyAddress(const char* myaddr) { m_address = myaddr; }

private:
    CollectorMapType m_collectors;
    MasterMapType m_masters;
    NegotiatorMapType m_negotiators;
    SchedulerMapType m_schedulers;
    SlotMapType m_slots;
    SubmitterMapType m_submitters;
    string m_address;
    
    static CollectorObject* m_instance;

};

}} /* aviary::collector */

#endif /* _COLLECTOROBJECT_H */
