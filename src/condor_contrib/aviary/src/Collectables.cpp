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

// local includes
#include "Collectables.h"
#include "AviaryConversionMacros.h"
#include "AviaryUtils.h"

using namespace std;
using namespace aviary::util;

using namespace aviary::collector;

void DaemonCollectable::update(const ClassAd& ad)
{
    MGMT_DECLARATIONS;
    m_stats.Pool = getPoolName();
    STRING(Name);
    STRING(MyAddress);
    STRING(CondorPlatform);
    STRING(CondorVersion);
    TIME_INTEGER(DaemonStartTime);
}


void Collector::update(const ClassAd& ad)
{
    MGMT_DECLARATIONS;
    DaemonCollectable::update(ad);
    INTEGER(RunningJobs);
    INTEGER(IdleJobs);
    INTEGER(HostsTotal);
    INTEGER(HostsClaimed);
    INTEGER(HostsUnclaimed);
    INTEGER(HostsOwner);
    
}

void Master::update(const ClassAd& ad)
{
    MGMT_DECLARATIONS;
    DaemonCollectable::update(ad);
    STRING(Arch);
    STRING(OpSysLongName);
    INTEGER(RealUid);
}

void Negotiator::update(const ClassAd& ad)
{
    MGMT_DECLARATIONS;
    DaemonCollectable::update(ad);
    DOUBLE(MatchRate);
    INTEGER(Matches);
    INTEGER(Duration);
    INTEGER(NumSchedulers);
    INTEGER(ActiveSubmitterCount);
    INTEGER(NumIdleJobs);
    INTEGER(NumJobsConsidered);
    INTEGER(Rejections);
    INTEGER(TotalSlots);
    INTEGER(CandidateSlots);
    INTEGER(TrimmedSlots);
}

void Scheduler::update(const ClassAd& ad)
{
    MGMT_DECLARATIONS;
    DaemonCollectable::update(ad);
    INTEGER(JobQueueBirthdate);
    INTEGER(MaxJobsRunning);
    INTEGER(NumUsers);
    INTEGER(TotalJobAds);
    INTEGER(TotalRunningJobs);
    INTEGER(TotalHeldJobs);
    INTEGER(TotalIdleJobs);
    INTEGER(TotalRemovedJobs);
}

void Slot::update(const ClassAd& ad)
{
    MGMT_DECLARATIONS;
    DaemonCollectable::update(ad);
    STRING(Arch);
    STRING(OpSys);
    STRING(Activity);
    STRING(State);
    INTEGER(Cpus);
    INTEGER(Disk);
    INTEGER(Memory);
    INTEGER(Swap);
    INTEGER(Mips);
    DOUBLE(LoadAvg);
    STRING(Start);
    STRING(FileSystemDomain);
}


void Submitter::update(const ClassAd& ad)
{
    MGMT_DECLARATIONS;
    STRING(Name);
    STRING(Machine);
    STRING(ScheddName);
    INTEGER(RunningJobs);
    INTEGER(HeldJobs);
    INTEGER(IdleJobs);
}
