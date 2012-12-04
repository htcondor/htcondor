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
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_qmgr.h"
#include "../condor_schedd.V6/scheduler.h"

// local includes
#include "AviaryUtils.h"
#include "AviaryConversionMacros.h"
#include "HadoopObject.h"
#include "Codec.h"

// Global Scheduler object, used for needReschedule etc.
extern Scheduler scheduler;
extern char * Name;

using namespace aviary::hadoop;
using namespace aviary::util;
using namespace aviary::codec;

HadoopObject* HadoopObject::m_instance = NULL;

HadoopObject::HadoopObject()
{
    m_pool = getPoolName();
	m_name = getScheddName();
    m_codec = new BaseCodec();
}

HadoopObject::~HadoopObject()
{
	delete m_codec;
}

HadoopObject* HadoopObject::getInstance()
{
    if (!m_instance) {
        m_instance = new HadoopObject();
    }
    return m_instance;
}

void
HadoopObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

	m_stats.Pool = getPoolName();
	STRING(CondorPlatform);
	STRING(CondorVersion);
	TIME_INTEGER(DaemonStartTime);
	TIME_INTEGER(JobQueueBirthdate);
	STRING(Machine);
	INTEGER(MaxJobsRunning);
	INTEGER(MonitorSelfAge);
	DOUBLE(MonitorSelfCPUUsage);
	DOUBLE(MonitorSelfImageSize);
	INTEGER(MonitorSelfRegisteredSocketCount);
	INTEGER(MonitorSelfResidentSetSize);
	TIME_INTEGER(MonitorSelfTime);
	STRING(MyAddress);
    //TIME_INTEGER(MyCurrentTime);
	STRING(Name);
	INTEGER(NumUsers);
	STRING(MyAddress);
	INTEGER(TotalHeldJobs);
	INTEGER(TotalIdleJobs);
	INTEGER(TotalJobAds);
	INTEGER(TotalRemovedJobs);
	INTEGER(TotalRunningJobs);
	m_stats.System = m_stats.Machine;

	// debug
	if (DebugFlags & D_FULLDEBUG) {
		const_cast<ClassAd*>(&ad)->dPrint(D_FULLDEBUG|D_NOHEADER);
	}
}

// TODO: start, stop, get impl...

