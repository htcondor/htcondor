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
#include "condor_debug.h"
#include "condor_uid.h"
#include "sysapi.h"

#ifdef LINUX
#include <linux/capability.h>

/*
	Function to get Linux process capability masks
*/
uint64_t sysapi_get_process_caps_mask(int pid, LinuxCapsMaskType type) {
	uint64_t cap_mask = 0;
	struct __user_cap_header_struct hs;
	struct __user_cap_data_struct ds[2];
	TemporaryPrivSentry sentry(PRIV_ROOT);
	//syscall to get the machines linux_capabilty_version
	if (syscall(SYS_capget,&hs,NULL)) {
		dprintf(D_ERROR,"Error: Linux system call for capget failed to initialize linux_capability_version.\n");
		return UINT64_MAX;
	}
	//Set the process id
	hs.pid = pid;
	
	//Get capability masks (Returns Permitted, Inheritable, and Effective in structure)
	if (syscall(SYS_capget,&hs,ds)) {
		dprintf(D_ERROR,"Error: Linux system call for capget failed to retrieve capability masks.\n");
		return UINT64_MAX;
	}
	
	//Set the return mask to requested mask type
	//ds[1] = upper 32 bits ds[0] = lower 32 bits
	switch (type) {
		case Linux_permittedMask:
			cap_mask = ds[0].permitted;
			cap_mask |= static_cast<uint64_t>(ds[1].permitted) << 32;
			break;
		case Linux_inheritableMask:
			cap_mask = ds[0].inheritable;
			cap_mask |= static_cast<uint64_t>(ds[1].inheritable) << 32;
			break;
		case Linux_effectiveMask:
			cap_mask = ds[0].effective;
			cap_mask |= static_cast<uint64_t>(ds[1].effective) << 32;
			break;
		default:
			dprintf(D_ERROR,"Error: Failed to find Linux capabilty mask type.\n");
			return UINT64_MAX;
	}
			
	return cap_mask;
}

#endif

