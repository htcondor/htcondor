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


#ifndef CONDOR_VM_UNIVERSE_TYPES_H_INCLUDE
#define CONDOR_VM_UNIVERSE_TYPES_H_INCLUDE

#define CONDOR_VM_UNIVERSE_XEN "xen"
#define CONDOR_VM_UNIVERSE_VMWARE "vmware"
#define CONDOR_VM_UNIVERSE_KVM "kvm"


#define VM_CKPT_FILE_EXTENSION	".cckpt"
#define VM_AVAIL_UNLIMITED_NUM	10000

/* Running modes for VM GAHP Server */
#define VMGAHP_TEST_MODE 0
#define VMGAHP_IO_MODE 1		/* Deprecated */
#define VMGAHP_WORKER_MODE 2	/* Deprecated */
#define VMGAHP_STANDALONE_MODE 3
#define VMGAHP_KILL_MODE 4
#define VMGAHP_MODE_MAX 5

/* Parameters in a result of VM GAHP STATUS command  */
#define VMGAHP_STATUS_COMMAND_STATUS	"STATUS"
#define VMGAHP_STATUS_COMMAND_PID		"PID"
#define VMGAHP_STATUS_COMMAND_MAC		"MAC"
#define VMGAHP_STATUS_COMMAND_IP		"IP"
#define VMGAHP_STATUS_COMMAND_CPUTIME	"CPUTIME"
#define VMGAHP_STATUS_COMMAND_CPUUTILIZATION	"CPUUTILIZATION"
#define VMGAHP_STATUS_COMMAND_MEMORY	"MEMORY"
#define VMGAHP_STATUS_COMMAND_MAX_MEMORY	"MAXMEMORY"

/* Parameters for Xen kernel */
#define XEN_KERNEL_INCLUDED		"included"
#define XEN_KERNEL_HW_VT		"vmx"

/* ClassAd Attributes for KVM && Xen */
#define VMPARAM_VM_DISK         "VMPARAM_vm_Disk"

/* ClassAd Attributes for Xen */
#define VMPARAM_XEN_KERNEL			"VMPARAM_Xen_Kernel"
#define VMPARAM_XEN_INITRD			"VMPARAM_Xen_Initrd"
#define VMPARAM_XEN_ROOT			"VMPARAM_Xen_Root"
#define VMPARAM_XEN_KERNEL_PARAMS	"VMPARAM_Xen_Kernel_Params"
#define VMPARAM_XEN_BOOTLOADER		"VMPARAM_Xen_Bootloader"

/* Extra ClassAd Attributes for VM */
#define VMPARAM_NO_OUTPUT_VM			"VMPARAM_No_Output_VM"
#define VMPARAM_BRIDGE_INTERFACE        "VMPARAM_Bridge_Interface"

#endif

