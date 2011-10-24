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

#include "SubmitterObject.h"

#ifndef WIN32
	#include "stdint.h"
#endif

#include "SubmitterUtils.h"

#include "condor_debug.h"

#include "MgmtConversionMacros.h"


using namespace com::redhat::grid;


SubmitterObject::SubmitterObject(qpid::management::ManagementAgent *agent,
								 SchedulerObject *_scheduler,
								 const char *_name)
{
	mgmtObject = new qmf::com::redhat::grid::Submitter(agent, this, _scheduler);

	// By default the submitter will be persistent.
	bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
	agent->addObject(mgmtObject, _name, _lifetime);
}


SubmitterObject::~SubmitterObject()
{
	if ( mgmtObject )
	{
		mgmtObject->resourceDestroy();
	}
}


void
SubmitterObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

	INTEGER(HeldJobs);
	INTEGER(IdleJobs);
	TIME_INTEGER(JobQueueBirthdate);
	STRING(Machine);
//	STRING(Name);
	INTEGER(RunningJobs);
	STRING(ScheddName);

	if (ad.LookupString("Name", &str)) {
		mgmtObject->set_Name(str);
	} else {
		dprintf(D_FULLDEBUG, "Warning: Could not find Name from ad\n");
	}

	// infer the Owner from the local-part of Name
	if (str) {
		string _owner(str);
		mgmtObject->set_Owner(_owner.substr(0,_owner.find('@')));
		free(str);
	}

	// debug
	if (DebugFlags & D_FULLDEBUG) {
		const_cast<ClassAd*>(&ad)->dPrint(D_FULLDEBUG|D_NOHEADER);
	}
}


qpid::management::ManagementObject *
SubmitterObject::GetManagementObject(void) const
{
	return mgmtObject;
}

Manageable::status_t
SubmitterObject::ManagementMethod ( uint32_t methodId,
                                    Args& /*args*/,
                                    std::string& /*text*/ )
{
    switch ( methodId )
    {
        case qmf::com::redhat::grid::Submitter::METHOD_ECHO:
			if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;
            return STATUS_OK;
    }

    return STATUS_NOT_IMPLEMENTED;
}
