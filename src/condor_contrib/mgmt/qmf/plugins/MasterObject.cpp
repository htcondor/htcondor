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

#include "MasterObject.h"

#include "../condor_master.V6/master.h"

#include "condor_classad.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include "Master.h"

#include "ArgsMasterStop.h"
#include "ArgsMasterStart.h"

#include "PoolUtils.h"

#include "MgmtConversionMacros.h"


extern DaemonCore *daemonCore;

extern class Daemons daemons;

using namespace com::redhat::grid;

using namespace std;
using namespace qpid::management;
using namespace qmf::com::redhat::grid;



MasterObject::MasterObject(ManagementAgent *agent, const char* _name)
{
	mgmtObject = new Master(agent, this);

	// By default the master will be persistent.
	bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
	agent->addObject(mgmtObject, _name, _lifetime);
}


MasterObject::~MasterObject()
{
	if (mgmtObject) {
		mgmtObject->resourceDestroy();
	}
}


ManagementObject *
MasterObject::GetManagementObject(void) const
{
	return mgmtObject;
}


Manageable::status_t
MasterObject::ManagementMethod(uint32_t methodId, Args &args, string &text)
{
	if (!param_boolean("QMF_MANAGEMENT_METHODS", false)) return STATUS_NOT_IMPLEMENTED;

	switch (methodId) {
	case qmf::com::redhat::grid::Master::METHOD_ECHO:
		if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;
            return STATUS_OK;
	case Master::METHOD_STOP:
		return Stop(((ArgsMasterStop &) args).i_Subsystem, text);
	case Master::METHOD_START:
		return Start(((ArgsMasterStart &) args).i_Subsystem, text);
	}

	return STATUS_NOT_IMPLEMENTED;
}


Manageable::status_t
MasterObject::Stop(string subsys, string &text)
{
	class daemon *daemon;

	dprintf(D_ALWAYS, "Received Stop(%s)\n", subsys.c_str());

	if (!(daemon = daemons.FindDaemon(subsys.c_str()))) {
		text = "Unknown subsystem: " + subsys;

		dprintf(D_ALWAYS, "ERROR: %s\n", text.c_str());

		return STATUS_USER + 1;
	}

	daemon->Hold(true);
	daemon->Stop();

	return STATUS_OK;
}


Manageable::status_t
MasterObject::Start(string subsys, string &text)
{
	class daemon *daemon;

	dprintf(D_ALWAYS, "Received Start(%s)\n", subsys.c_str());

	if (!(daemon = daemons.FindDaemon(subsys.c_str()))) {
		text = "Unknown subsystem: " + subsys;

		dprintf(D_ALWAYS, "ERROR: %s\n", text.c_str());

		return STATUS_USER + 1;
	}

		// XXX: daemon::on_hold may be useful to avoid changing
		// state on daemon::Start() failure

	daemon->Hold(false);

	if (!daemon->Start()) {
		text = "Unable to start subsystem: " + subsys;

		dprintf(D_ALWAYS, "ERROR: %s\n", text.c_str());

		return STATUS_USER + 2;
	}

	return STATUS_OK;
}


void
MasterObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

	mgmtObject->set_Pool(GetPoolName());

	STRING(CondorPlatform);
	STRING(CondorVersion);
	TIME_INTEGER(DaemonStartTime);
	STRING(Machine);
	STRING(MyAddress);
	STRING(Name);
	STRING(MyAddress);
	INTEGER(RealUid);

		//INTEGER(WindowsBuildNumber);
		//INTEGER(WindowsMajorVersion);
		//INTEGER(WindowsMinorVersion);

	INTEGER(MonitorSelfAge);
	DOUBLE(MonitorSelfCPUUsage);
	DOUBLE(MonitorSelfImageSize);
	INTEGER(MonitorSelfRegisteredSocketCount);
	INTEGER(MonitorSelfResidentSetSize);

		// XXX: While we do not have the System's UUID we will use
		// its name. Right now this is a placeholder so when we do
		// get a UUID users of the System attribute don't have
		// to change.
	mgmtObject->set_System(mgmtObject->get_Machine());
}
