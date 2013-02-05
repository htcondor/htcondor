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

// condor includes
#include "condor_common.h"

// c++ includes
#include <locale>

// local includes
#include "AviaryUtils.h"
#include "CollectableCodec.h"
#include "AviaryCommon_SubmissionID.h"

// code to perform wso2<->internal summary translations

using namespace aviary::util;
using namespace AviaryCommon;
using namespace aviary::collector;

AviaryCommon::ResourceID* CollectableCodec::createResourceID(DaemonCollectable* dc, const char* daemon_type)
{
    ResourceID* resid = new ResourceID;
    resid->setResource(new ResourceType(daemon_type));
    resid->setName(dc->Name);
    resid->setPool(getPoolName());
    resid->setAddress(dc->MyAddress);
    resid->setBirthdate(dc->DaemonStartTime);
    return resid;
}

AviaryCommon::Collector* CollectableCodec::encode(aviary::collector::Collector* in_, bool add_summary)
{
    AviaryCommon::Collector* out_ = new AviaryCommon::Collector;
    out_->setId(this->createResourceID(in_,"COLLECTOR"));
    if (add_summary) {
        CollectorSummary* summary= new CollectorSummary;
        summary->setClaimed_hosts(in_->HostsClaimed);
        summary->setIdle_jobs(in_->IdleJobs);
        summary->setOwner_hosts(in_->HostsOwner);
        summary->setRunning_jobs(in_->RunningJobs);
        summary->setTotal_hosts(in_->HostsTotal);
        summary->setUnclaimed_hosts(in_->HostsUnclaimed);
        out_->setSummary(summary);
    }
    return out_;
}

AviaryCommon::Master* CollectableCodec::encode(aviary::collector::Master* in_, bool add_summary)
{
    AviaryCommon::Master* out_ = new AviaryCommon::Master;
    out_->setId(this->createResourceID(in_,"MASTER"));
    if (add_summary) {
        MasterSummary* summary= new MasterSummary;
        summary->setArch(new ArchType(in_->Arch));
        summary->setOs(new OSType(in_->OpSysLongName));
        summary->setReal_uid(in_->RealUid);
        out_->setSummary(summary);
    }
    return out_;
}

AviaryCommon::Negotiator* CollectableCodec::encode(aviary::collector::Negotiator* in_, bool add_summary)
{
    AviaryCommon::Negotiator* out_ = new AviaryCommon::Negotiator;
    out_->setId(this->createResourceID(in_,"NEGOTIATOR"));
    if (add_summary) {
        NegotiatorSummary* summary= new NegotiatorSummary;
        summary->setActive_submitters(in_->ActiveSubmitterCount);
        summary->setCandidate_slots(in_->CandidateSlots);
        summary->setDuration(in_->Duration);
        summary->setIdle_jobs(in_->NumIdleJobs);
        summary->setJobs_considered(in_->NumJobsConsidered);
        summary->setLatest_cycle(encodeDateTime(in_->LastNegotiationCycleEnd,m_env));
        summary->setMatch_rate(in_->MatchRate);
        summary->setMatches(in_->Matches);
        summary->setRejections(in_->Rejections);
        summary->setSchedulers(in_->NumSchedulers);
        summary->setTotal_slots(in_->TotalSlots);
        summary->setTrimmed_slots(in_->TrimmedSlots);
        out_->setSummary(summary);
    }
    return out_;
}

AviaryCommon::Scheduler* CollectableCodec::encode(aviary::collector::Scheduler* in_, bool add_summary)
{
    AviaryCommon::Scheduler* out_ = new AviaryCommon::Scheduler;
    out_->setId(this->createResourceID(in_,"SCHEDULER"));
    if (add_summary) {
        SchedulerSummary* summary= new SchedulerSummary;
        summary->setAds(in_->TotalJobAds);
        summary->setHeld(in_->TotalHeldJobs);
        summary->setIdle(in_->TotalIdleJobs);
        summary->setMax_jobs_running(in_->MaxJobsRunning);
        summary->setQueue_created(encodeDateTime(in_->JobQueueBirthdate,m_env));
        summary->setRemoved(in_->TotalRemovedJobs);
        summary->setRunning(in_->TotalRunningJobs);
        summary->setUsers(in_->NumUsers);
        out_->setSummary(summary);
    }
    return out_;
}

AviaryCommon::Slot* CollectableCodec::encode(aviary::collector::Slot* in_, bool add_summary)
{
    AviaryCommon::Slot* out_ = new AviaryCommon::Slot;
    out_->setId(this->createResourceID(in_,"SLOT"));
    out_->setSlot_type(new SlotType(in_->SlotType));
    if (add_summary) {
        SlotSummary* summary = new SlotSummary;
        summary->setActivity(in_->Activity);
        summary->setArch(new ArchType(in_->Arch));
        summary->setCpus(in_->Cpus);
        summary->setDisk(in_->Disk);
        summary->setDomain(in_->FileSystemDomain);
        summary->setLoad_avg(in_->LoadAvg);
        summary->setMemory(in_->Memory);
        summary->setMips(in_->Mips);
        // TODO: ostype conversion?
        summary->setOs(new OSType(in_->OpSys));
        summary->setStart(in_->Start);
        // TODO: state conversion?
        summary->setState(in_->State);
        summary->setSwap(in_->Swap);
        // TODO: dynamic associations?
        out_->setSummary(summary);
    }
    return out_;
}

AviaryCommon::Submitter* CollectableCodec::encode(aviary::collector::Submitter* in_, bool add_summary)
{
    AviaryCommon::Submitter* out_ = new AviaryCommon::Submitter;
    SubmitterID* sid = new SubmitterID;
    sid->setName(in_->Name);
    sid->setMachine(in_->Machine);
    sid->setScheduler(in_->ScheddName);
    out_->setId(sid);
    if (add_summary) {
        SubmitterSummary* summary= new SubmitterSummary;
        summary->setHeld(in_->HeldJobs);
        summary->setIdle(in_->IdleJobs);
        summary->setRunning(in_->RunningJobs);
        summary->setCreated(encodeDateTime(in_->JobQueueBirthdate,m_env));
        summary->setOwner(in_->Owner);
        out_->setSummary(summary);
    }
    return out_;
}
