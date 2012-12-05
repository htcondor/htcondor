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

#ifndef _COLLECTABLES_H
#define _COLLECTABLES_H

// condor includes
#include "condor_config.h"
#include "condor_classad.h"

#include <string>
#include <set>

using namespace std;

namespace aviary {
namespace collector {

    struct Collectable{
        string Name;
        virtual void update(const ClassAd& ad) = 0;
    };
    
    struct DaemonCollectable: public Collectable {
        string Pool;
        string MyAddress;
        string CondorVersion;
        string CondorPlatform;
        int DaemonStartTime;
        
        void update(const ClassAd& ad);
    };
    
    struct Collector: public DaemonCollectable {
        int RunningJobs;
        int IdleJobs;
        int HostsTotal;
        int HostsClaimed;
        int HostsUnclaimed;
        int HostsOwner;
        
        void update(const ClassAd& ad);
    };

    struct Master: public DaemonCollectable {
        string Arch;
        string OpSysLongName;
        int RealUid;
        
        void update(const ClassAd& ad);
    };
    
    struct Negotiator: public DaemonCollectable {
        double MatchRate;
        int Matches;
        int Duration;
        int NumSchedulers;
        int ActiveSubmitterCount;
        int NumIdleJobs;
        int NumJobsConsidered;
        int Rejections;
        int TotalSlots;
        int CandidateSlots;
        int TrimmedSlots;
        
        void update(const ClassAd& ad);
    };
    
    struct Scheduler: public DaemonCollectable {
        int JobQueueBirthdate;
        int MaxJobsRunning;
        int NumUsers;
        int TotalJobAds;
        int TotalRunningJobs;
        int TotalHeldJobs;
        int TotalIdleJobs;
        int TotalRemovedJobs;
        
        void update(const ClassAd& ad);
    };
    
    struct Submitter: public Collectable {
        string Machine;
        string ScheddName;
        int RunningJobs;
        int HeldJobs;
        int IdleJobs;
        
        void update(const ClassAd& ad);
    };
    
    struct Slot: public DaemonCollectable {
        string Arch;
        string OpSys;
        string Activity;
        string State;
        int Cpus;
        int Disk;
        int Memory;
        int Swap;
        int Mips;
        double LoadAvg;
        string Start;
        string FileSystemDomain;
        
        void update(const ClassAd& ad);
    };
    
    typedef set<Slot*> DynamicSlotList;
    
    struct PartitionableSlot: public Slot {
        DynamicSlotList m_dynamic_slots;
        
        void update(const ClassAd& ad);
    };

}} 

#endif /* _COLLECTABLES_H */
