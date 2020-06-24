/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "proc_family_interface.h"
#include "proc_family_proxy.h"
#include "proc_family_direct.h"

ProcFamilyInterface* ProcFamilyInterface::create(const char* subsys)
{
	ProcFamilyInterface* ptr;

	bool is_master = ((subsys != NULL) && !strcmp(subsys, "MASTER"));

	if (param_boolean("USE_PROCD", true)) {

		// if we're not the Master, create the ProcFamilyProxy
		// object with our subsystem as the address suffix; this
		// avoids address collisions in case we have multiple
		// daemons that all want to spawn ProcDs
		//
		const char* address_suffix = is_master ? NULL : subsys;
		ptr = new ProcFamilyProxy(address_suffix);
	}
	else if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {

		dprintf(D_ALWAYS,
		        "GID-based process tracking requires use of ProcD; "
		            "ignoring USE_PROCD setting\n");
		ptr = new ProcFamilyProxy;
	}
	else if (param_boolean("GLEXEC_JOB", false)) {

		dprintf(D_ALWAYS,
		        "GLEXEC_JOB requires use of ProcD; "
		            "ignoring USE_PROCD setting\n");
		ptr = new ProcFamilyProxy;
	// Note: if CGROUPS is turned on and the startd has USE_PROCD=false,
	// then we will respect the procd setting and not use cgroups.
	} else {

		ptr = new ProcFamilyDirect;
	}
	ASSERT(ptr != NULL);

	return ptr;
}
