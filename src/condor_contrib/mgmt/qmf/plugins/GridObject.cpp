/*
 * Copyright 2000-2011 Red Hat, Inc.
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

#include "Grid.h"

#include "GridObject.h"

#include "PoolUtils.h"

#include "MgmtConversionMacros.h"


using namespace qpid::management;
using namespace com::redhat::grid;


GridObject::GridObject(ManagementAgent *agent, const char* _name)
{
	mgmtObject = new qmf::com::redhat::grid::Grid(agent, this);

	// By default the grid will be persistent.
	bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
	agent->addObject(mgmtObject, _name, _lifetime);
}


GridObject::~GridObject() {
	if (mgmtObject) {
		mgmtObject->resourceDestroy();
	}
};


ManagementObject *
GridObject::GetManagementObject(void) const
{
	return mgmtObject;
}


void
GridObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

	mgmtObject->set_Pool(GetPoolName());

	STRING(Name);
	STRING(ScheddName);
	STRING(Owner);
	INTEGER(NumJobs);
	INTEGER(JobLimit);
	INTEGER(SubmitLimit);
	INTEGER(SubmitsInProgress);
	INTEGER(SubmitsQueued);
	INTEGER(SubmitsAllowed);
	INTEGER(SubmitsWanted);
	OPT_TIME_INTEGER(GridResourceUnavailableTime);
	INTEGER(RunningJobs);
	INTEGER(IdleJobs);
}

Manageable::status_t
GridObject::ManagementMethod ( uint32_t methodId,
                                    Args &args,
                                    std::string &text )
{
    switch ( methodId )
    {
        case qmf::com::redhat::grid::Grid::METHOD_ECHO:
			if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;
            return STATUS_OK;
    }

    return STATUS_NOT_IMPLEMENTED;
}
