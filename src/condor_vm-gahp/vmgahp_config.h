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

		bool init(const char* vmtype, const char* configfile);
		bool isAllowUser(const char* user);

		StringList m_allow_user_list;

		MyString m_vm_type;
		MyString m_vm_version;

		int m_vm_max_memory;

		bool m_vm_networking;
		StringList m_vm_networking_types;
		MyString m_vm_default_networking_type;

		bool m_vm_hardware_vt;

		MyString m_prog_for_script; // program to execute a below script(perl etc.)
		MyString m_vm_script; // Script program for virtual machines
		MyString m_configfile; // gahp configuration file
		MyString m_controller; // This field is used for Xen
};

#endif /* VM_GAHP_CONFIG_H */
