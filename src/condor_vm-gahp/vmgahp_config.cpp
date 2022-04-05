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
#include "vmgahp_common.h"
#include "vmgahp_config.h"

VMGahpConfig::VMGahpConfig()
{
	m_vm_max_memory = 0;
	m_vm_networking = false;
	m_vm_hardware_vt = false;
    m_vm_script = "none";
}

VMGahpConfig& VMGahpConfig::
operator=(const VMGahpConfig& old)
{
	VMGahpConfig *oldp = NULL;
	oldp = const_cast<VMGahpConfig *>(&old);

	m_vm_type = old.m_vm_type;
	m_vm_max_memory = old.m_vm_max_memory;
	m_vm_networking = old.m_vm_networking;
	m_vm_networking_types.clearAll();
	if( old.m_vm_networking_types.isEmpty() == false ) { 
		m_vm_networking_types.create_union(oldp->m_vm_networking_types, false);
	}

	m_vm_hardware_vt = old.m_vm_hardware_vt;
	m_vm_script = old.m_vm_script;
	m_prog_for_script = old.m_prog_for_script;
	return *this;
}

VMGahpConfig::VMGahpConfig(const VMGahpConfig& old)
{
	*this = old; // = operator is overloaded 
}

bool
VMGahpConfig::init(const char* vmtype)
{
	char *config_value = NULL;

	if( !vmtype ) {
		return false;
	}

	// Handle VM_TYPE
	m_vm_type = vmtype;
	lower_case(m_vm_type);

	// Read VM_MEMORY
	int tmp_config_value = param_integer("VM_MEMORY", 0);
	if( tmp_config_value <= 0 ) {
		vmprintf( D_ALWAYS,
		          "\nERROR: 'VM_MEMORY' is not defined "
		              "in configuration\n");
		return false;
	}
	m_vm_max_memory = tmp_config_value;

	// Read VM_NETWORKING
	m_vm_networking = param_boolean("VM_NETWORKING", false);

	// Read VM_NETWORKING_TYPE
	if( m_vm_networking ) {
		config_value = param("VM_NETWORKING_TYPE");
		if( !config_value ) {
			vmprintf( D_ALWAYS,
			          "\nERROR: 'VM_NETWORKING' is true "
			              "but 'VM_NETWORKING_TYPE' is not defined "
			              "So 'VM_NETWORKING' is disabled\n");
			m_vm_networking = false;
		}else {
			std::string networking_type = delete_quotation_marks(config_value);
			trim(networking_type);
			// change string to lowercase
			lower_case(networking_type);
			free(config_value);

			StringList networking_types(networking_type.c_str(), ", ");
			m_vm_networking_types.create_union(networking_types, false);

			if( m_vm_networking_types.isEmpty() ) {
				vmprintf( D_ALWAYS,
				          "\nERROR: 'VM_NETWORKING' is true but "
				              "'VM_NETWORKING_TYPE' is empty "
				              "So 'VM_NETWORKING' is disabled\n");
				m_vm_networking = false;
			}else {
				config_value = param("VM_NETWORKING_DEFAULT_TYPE");
				if( config_value ) {
					m_vm_default_networking_type = delete_quotation_marks(config_value);
					free(config_value);
				}
			}
		}

		/*		
		// Why only a prefix? 
		// Read VM_NETWORKING_MAC_PREFIX
		config_value = param("VM_NETWORKING_MAC_PREFIX");
		if( config_value ) {
			m_vm_networking_mac_prefix = config_value;
			free(config_value);
		}
		*/
	}

	// Read VM_HARDWARE_VT
	m_vm_hardware_vt = param_boolean("VM_HARDWARE_VT", false);

	return true;
}
