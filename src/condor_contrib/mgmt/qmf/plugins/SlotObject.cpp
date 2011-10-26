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

#include "condor_common.h"
#include "condor_config.h"

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include "condor_debug.h"
#include "condor_classad.h"

#include "Slot.h"

#include "SlotObject.h"

#include "PoolUtils.h"

#include "MgmtConversionMacros.h"


using namespace qpid::management;
using namespace com::redhat::grid;


SlotObject::SlotObject(ManagementAgent *agent, const char* _name)
{
	mgmtObject = new qmf::com::redhat::grid::Slot(agent, this);

	// By default the slot will be persistent.
	bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
	agent->addObject(mgmtObject, _name, _lifetime);
}


SlotObject::~SlotObject()
{
	if (mgmtObject) {
		mgmtObject->resourceDestroy();
	}
}


ManagementObject *
SlotObject::GetManagementObject(void) const
{
	return mgmtObject;
}


void
SlotObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;
//    string name;

		// Logic to set values on mgmtObject

//    FieldTable fieldTable;
//
//    ((ClassAd) ad).ResetExpr();
//    while (NULL != (expr = ((ClassAd) ad).NextExpr())) {
//       name = ((Variable *) expr->LArg())->Name();
// 
//       switch (expr->RArg()->MyType()) {
//       case LX_INTEGER:
//          num = (uint32_t) ((Integer *) expr->RArg())->Value();
//          fieldTable.setInt(name, num);
//          break;
//       case LX_FLOAT:
//          dbl = (double) ((Float *) expr->RArg())->Value();
//          fieldTable.setInt(name, (int) dbl);
//          break;
//       case LX_STRING:
//          str = ((String *) expr->RArg())->Value();
//          fieldTable.setString(name, str);
//          break;
//       default:
//          expr->RArg()->PrintToNewStr(&str);
//          fieldTable.setString(name, str);
//          free(str);
//       }
//    }

	mgmtObject->set_Pool(GetPoolName());

		// There is no CondorPlatform or CondorVersion ATTR_*
		// definition
	STRING(CondorPlatform);
	STRING(CondorVersion);

	TIME_INTEGER(DaemonStartTime);
	STRING(Arch);
	STRING(CheckpointPlatform);
	INTEGER(Cpus);
	INTEGER(Disk);
	STRING(FileSystemDomain);
	EXPR(IsValidCheckpointPlatform);
	INTEGER(KFlops);
	STRING(Machine);
	EXPR(MaxJobRetirementTime);
	INTEGER(Memory);
	INTEGER(Mips);
	STRING(MyAddress);
	STRING(Name);
	STRING(OpSys);
	EXPR(Requirements);
	STRING(MyAddress);
	EXPR(Rank);
	INTEGER(SlotID);
	EXPR(Start);
	STRING(StarterAbilityList);
	INTEGER(TotalCpus);
	INTEGER(TotalDisk);
	INTEGER(TotalMemory);
	INTEGER(TotalSlots);
	INTEGER(TotalVirtualMemory);
	STRING(UidDomain);
	INTEGER(VirtualMemory);
	INTEGER(WindowsBuildNumber);
	INTEGER(WindowsMajorVersion);
	INTEGER(WindowsMinorVersion);

		// The MonitorSelf* attributes do not have ATTR_*
		// definitions
	INTEGER(MonitorSelfAge);
	DOUBLE(MonitorSelfCPUUsage);
	DOUBLE(MonitorSelfImageSize);
	INTEGER(MonitorSelfRegisteredSocketCount);
	INTEGER(MonitorSelfResidentSetSize);

	OPT_STRING(AccountingGroup);
	STRING(Activity);
	OPT_STRING(ClientMachine);
	INTEGER(ClockDay);
	INTEGER(ClockMin);
	OPT_STRING(ConcurrencyLimits);
	DOUBLE(CondorLoadAvg);
	INTEGER(ConsoleIdle);
	OPT_DOUBLE(CurrentRank);
	TIME_INTEGER(EnteredCurrentActivity);
	TIME_INTEGER(EnteredCurrentState);
	OPT_STRING(GlobalJobId);

		// ImageSize is treated specially because it is an optional
		// statistic. When not present it defaults to a value of 0.
	//INTEGER(ImageSize);
	if (ad.LookupInteger("ImageSize", num)) {
		mgmtObject->set_ImageSize((uint32_t) num);
	} else {
		mgmtObject->set_ImageSize(0);
	}

	OPT_STRING(JobId);
	OPT_TIME_INTEGER(JobStart);
	INTEGER(KeyboardIdle);
	TIME_INTEGER(LastBenchmark);
	TIME_INTEGER(LastFetchWorkCompleted);
	TIME_INTEGER(LastFetchWorkSpawned);
	TIME_INTEGER(LastPeriodicCheckpoint);
	DOUBLE(LoadAvg);
	TIME_INTEGER(MyCurrentTime);
	INTEGER(NextFetchWorkDelay);
	OPT_STRING(PreemptingConcurrencyLimits);
	OPT_STRING(PreemptingOwner);
	OPT_STRING(PreemptingUser);
	OPT_DOUBLE(PreemptingRank);
	OPT_STRING(RemoteOwner);
	OPT_STRING(RemoteUser);
	STRING(State);
	INTEGER(TimeToLive);
	OPT_INTEGER(TotalClaimRunTime);
	OPT_INTEGER(TotalClaimSuspendTime);
	DOUBLE(TotalCondorLoadAvg);
	OPT_INTEGER(TotalJobRunTime);
	OPT_INTEGER(TotalJobSuspendTime);
	DOUBLE(TotalLoadAvg);
	INTEGER(TotalTimeBackfillBusy);
	INTEGER(TotalTimeBackfillIdle);
	INTEGER(TotalTimeBackfillKilling);
	INTEGER(TotalTimeClaimedBusy);
	INTEGER(TotalTimeClaimedIdle);
	INTEGER(TotalTimeClaimedRetiring);
	INTEGER(TotalTimeClaimedSuspended);
	INTEGER(TotalTimeMatchedIdle);
	INTEGER(TotalTimeOwnerIdle);
	INTEGER(TotalTimePreemptingKilling);
	INTEGER(TotalTimePreemptingVacating);
	INTEGER(TotalTimeUnclaimedBenchmarking);
	INTEGER(TotalTimeUnclaimedIdle);

		// XXX: While we do not have the System's UUID we will use
		// its name. Right now this is a placeholder so when we do
		// get a UUID users of the System attribute don't have
		// to change.
	mgmtObject->set_System(mgmtObject->get_Machine());
}

Manageable::status_t
SlotObject::ManagementMethod ( uint32_t methodId,
                                    Args &args,
                                    std::string &text )
{
    switch ( methodId )
    {
        case qmf::com::redhat::grid::Slot::METHOD_ECHO:
			if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;
            return STATUS_OK;
    }

    return STATUS_NOT_IMPLEMENTED;
}
