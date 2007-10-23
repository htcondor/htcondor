/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "proc_family_interface.h"
#include "proc_family_proxy.h"
#include "proc_family_direct.h"
#include "../condor_privsep/condor_privsep.h"

ProcFamilyInterface* ProcFamilyInterface::create(char* subsys)
{
	ProcFamilyInterface* ptr;

	bool is_master = ((subsys != NULL) && !strcmp(subsys, "MASTER"));

	bool use_procd_default = !is_master;
	if (param_boolean("USE_PROCD", use_procd_default)) {

		// if we're not the Master, create the ProcFamilyProxy
		// object with our subsystem as the address suffix; this
		// avoids address collisions in case we have multiple
		// daemons that all want to spawn ProcDs
		//
		char* address_suffix = is_master ? NULL : subsys;
		ptr = new ProcFamilyProxy(address_suffix);
	}
	else if (privsep_enabled()) {

		dprintf(D_ALWAYS,
		        "PrivSep requires use of ProcD; "
		            "ignoring USE_PROCD setting\n");
		ptr = new ProcFamilyProxy;
	}
	else if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {

		dprintf(D_ALWAYS,
		        "GID-based process tracking requires use of ProcD; "
		            "ignoring USE_PROCD setting\n");
		ptr = new ProcFamilyProxy;
	}
	else {

		ptr = new ProcFamilyDirect;
	}
	ASSERT(ptr != NULL);

	return ptr;
}
