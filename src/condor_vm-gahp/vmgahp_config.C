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
#include "MyString.h"
#include "vmgahp_common.h"
#include "vmgahp_config.h"

VMGahpConfig::VMGahpConfig()
{
	m_vm_max_memory = 0;
	m_vm_networking = false;
	m_vm_hardware_vt = false;
}

VMGahpConfig& VMGahpConfig::
operator=(const VMGahpConfig& old)
{
	VMGahpConfig *oldp = NULL;
	oldp = const_cast<VMGahpConfig *>(&old);

	m_allow_user_list.clearAll();
	if( old.m_allow_user_list.isEmpty() == false ) {
		m_allow_user_list.create_union(oldp->m_allow_user_list, false);
	}

	m_vm_type = old.m_vm_type;
	m_vm_version = old.m_vm_version;
	m_vm_max_memory = old.m_vm_max_memory;
	m_vm_networking = old.m_vm_networking;
	m_vm_networking_types.clearAll();
	if( old.m_vm_networking_types.isEmpty() == false ) { 
		m_vm_networking_types.create_union(oldp->m_vm_networking_types, false);
	}

	m_vm_hardware_vt = old.m_vm_hardware_vt;
	m_vm_script = old.m_vm_script;
	m_prog_for_script = old.m_prog_for_script;
	m_configfile = old.m_configfile;
	return *this;
}

VMGahpConfig::VMGahpConfig(const VMGahpConfig& old)
{
	*this = old; // = operator is overloaded 
}

bool
VMGahpConfig::init(const char* vmtype, const char* configfile)
{
	char *config_value = NULL;
	MyString fixedvalue;

	if( !vmtype || !configfile ) {
		return false;
	}

	m_configfile = configfile;

	if( isSetUidRoot() ) {
		// vmgahp has setuid-root, 'ALLOW_USERS' is required.
		// Read the list of local users eligible to execute vmgahp
		config_value = vmgahp_param("ALLOW_USERS");
		if( !config_value ) {
			vmprintf( D_ALWAYS, "\nERROR: Failed to find 'ALLOW_USERS' "
					"in \"%s\"\n", configfile);
			return false;
		}

		fixedvalue = delete_quotation_marks(config_value);
		free(config_value);

		m_allow_user_list.initializeFromString(fixedvalue.Value());
		if( m_allow_user_list.isEmpty() ) {
			vmprintf( D_ALWAYS, "\nERROR: 'ALLOW_USERS' must have at least "
					"one local user in \"%s\"\n", configfile);
			return false;
		}
	}

	// Read VM_TYPE
	config_value = vmgahp_param("VM_TYPE");
	if( !config_value ) {
		vmprintf( D_ALWAYS, "\nERROR: 'VM_TYPE' is not defined "
				"in \"%s\"\n", configfile);
		return false;
	}
	fixedvalue = delete_quotation_marks(config_value);
	free(config_value);

	if( strcasecmp(vmtype, fixedvalue.Value()) != 0 ) {
		vmprintf(D_ALWAYS, "\nERROR: vmgahp config file \"%s\" is not "
				"for (%s) but (%s)\n", configfile, vmtype, fixedvalue.Value());
		return false;
	}

	fixedvalue.strlwr();
	m_vm_type = fixedvalue;

	// Read VM_VERSION
	config_value = vmgahp_param("VM_VERSION");
	if( !config_value ) {
		vmprintf( D_ALWAYS, "\nERROR: 'VM_VERSION' is not defined "
				"in \"%s\"\n", configfile);
		return false;
	}
	m_vm_version = delete_quotation_marks(config_value);
	free(config_value);

	// Read VM_MAX_MEMORY
	int tmp_config_value = vmgahp_param_integer("VM_MAX_MEMORY", 0);
	if( tmp_config_value <= 0 ) {
		vmprintf( D_ALWAYS, "\nERROR: 'VM_MAX_MEMORY' is not defined "
				"in \"%s\"\n", configfile);
		return false;
	}
	m_vm_max_memory = tmp_config_value;

	// Read VM_NETWORKING
	m_vm_networking = vmgahp_param_boolean("VM_NETWORKING", false);

	// Read VM_NETWORKING_TYPE
	if( m_vm_networking ) {
		config_value = vmgahp_param("VM_NETWORKING_TYPE");
		if( !config_value ) {
			vmprintf( D_ALWAYS, "\nERROR: 'VM_NETWORKING' is true "
					"but 'VM_NETWORKING_TYPE' is not defined in \"%s\" "
					"So 'VM_NETWORKING' is disabled\n", configfile);
			m_vm_networking = false;
		}else {
			MyString networking_type = delete_quotation_marks(config_value);
			networking_type.trim();
			// change string to lowercase
			networking_type.strlwr();
			free(config_value);

			StringList networking_types(networking_type.Value(), ", ");
			m_vm_networking_types.create_union(networking_types, false);

			if( m_vm_networking_types.isEmpty() ) {
				vmprintf( D_ALWAYS, "\nERROR: 'VM_NETWORKING' is true but "
						"'VM_NETWORKING_TYPE' is empty in \"%s\" "
						"So 'VM_NETWORKING' is disabled\n", configfile);
				m_vm_networking = false;
			}else {
				config_value = vmgahp_param("VM_NETWORKING_DEFAULT_TYPE");
				if( config_value ) {
					m_vm_default_networking_type = delete_quotation_marks(config_value);
					free(config_value);
				}
			}
		}

		/*
		 * Future work
		// Read VM_NETWORKING_MAC_PREFIX
		config_value = vmgahp_param("VM_NETWORKING_MAC_PREFIX");
		if( config_value ) {
			m_vm_networking_mac_prefix = config_value;
			free(config_value);
		}
		*/
	}

	// Read VM_HARDWARE_VT
	m_vm_hardware_vt = vmgahp_param_boolean("VM_HARDWARE_VT", false);

	return true;
}

bool 
VMGahpConfig::isAllowUser(const char* user)
{
	if( !user || user[0] == '\0' ) {
		return false;
	}

	if( m_allow_user_list.isEmpty() ) {
		return false;
	}

	// Check if the given user is in 'ALLOW_USERS'
	if( m_allow_user_list.contains(user) == false ) {
		return false;
	}
	return true;
}
