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

//#include "../condor_collector.V6/CollectorPlugin.h"

#include "condor_commands.h"
#include "command_strings.h"
#include "hashkey.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condor_attributes.h"

#include "condor_config.h"

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include "condor_debug.h"
#include "condor_classad.h"

#include "Collector.h"

#include "CollectorObject.h"

#include "PoolUtils.h"

#include "MgmtConversionMacros.h"


using namespace qpid::management;
using namespace com::redhat::grid;


CollectorObject::CollectorObject(ManagementAgent *agent, const char* _name)
{
	mgmtObject = new qmf::com::redhat::grid::Collector(agent, this);

	// By default the collector will be persistent.
	bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
	agent->addObject(mgmtObject, _name, _lifetime);

	advertise();
}


CollectorObject::~CollectorObject() {
	if (mgmtObject) {
		mgmtObject->resourceDestroy();
	}
}


ManagementObject *
CollectorObject::GetManagementObject(void) const
{
	return mgmtObject;
}


void
CollectorObject::advertise()
{
	ClassAd ad;
	MGMT_DECLARATIONS;

		// XXX: This will look very familiar to code in void
		// CollectorDaemon::init_classad(int interval), sigh.

	char *CollectorName = param("COLLECTOR_NAME");

	ad.SetMyTypeName(COLLECTOR_ADTYPE);
	ad.SetTargetTypeName("");

	char *tmp;
	tmp = param( "CONDOR_ADMIN" );
	if( tmp ) {
		ad.Assign( ATTR_CONDOR_ADMIN, tmp );
		free( tmp );
	}

	if( CollectorName ) {
		ad.Assign( ATTR_NAME, CollectorName );
	} else {
		ad.Assign( ATTR_NAME, my_full_hostname() );
	}

	ad.Assign( ATTR_COLLECTOR_IP_ADDR, global_dc_sinful() );

	daemonCore->publish(&ad);

	mgmtObject->set_Pool(GetPoolName());
	mgmtObject->set_System(my_full_hostname());

	STRING(CondorPlatform);
	STRING(CondorVersion);
	STRING(Name);
	STRING(MyAddress);
}


void
CollectorObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

	INTEGER(RunningJobs);
	INTEGER(IdleJobs);
	INTEGER(HostsTotal);
	INTEGER(HostsClaimed);
	INTEGER(HostsUnclaimed);
	INTEGER(HostsOwner);

		// The MonitorSelf* attributes do not have ATTR_*
		// definitions
	INTEGER(MonitorSelfAge);
	DOUBLE(MonitorSelfCPUUsage);
	DOUBLE(MonitorSelfImageSize);
	INTEGER(MonitorSelfRegisteredSocketCount);
	INTEGER(MonitorSelfResidentSetSize);
	TIME_INTEGER(MonitorSelfTime);
}

/* disable for now
Manageable::status_t
CollectorObject::ManagementMethod ( uint32_t methodId,
                                    Args &args,
                                    std::string &text )
{
    switch ( methodId )
    {
        case qmf::com::redhat::grid::Collector::METHOD_ECHO:
            return STATUS_OK;
    }

    return STATUS_NOT_IMPLEMENTED;
}

*/
