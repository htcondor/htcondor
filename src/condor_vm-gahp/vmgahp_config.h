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


#ifndef VM_GAHP_CONFIG_H
#define VM_GAHP_CONFIG_H

#include "condor_common.h"
#include "condor_classad.h"
#include "gahp_common.h"
#include "condor_vm_universe_types.h"

class VMGahpConfig {
	public:
		VMGahpConfig();
		VMGahpConfig(const VMGahpConfig& old); // copy constructor
		VMGahpConfig& operator=(const VMGahpConfig& old);

		bool init(const char* vmtype);

		std::string m_vm_type;

		int m_vm_max_memory;

		bool m_vm_networking;
		StringList m_vm_networking_types;
		std::string m_vm_default_networking_type;

		bool m_vm_hardware_vt;

		std::string m_prog_for_script; // program to execute a below script(perl etc.)
		std::string m_vm_script; // Script program for virtual machines
};

#endif /* VM_GAHP_CONFIG_H */
