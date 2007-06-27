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
#ifndef VMGAHP_ERROR_CODES_H_INCLUDE
#define VMGAHP_ERROR_CODES_H_INCLUDE

#include "MyString.h"
#include "condor_attributes.h"

const int VMGAHP_NO_ERR = 0;

// COMMON 
const int VMGAHP_ERR_VM_NOT_FOUND = 1000;
const int VMGAHP_ERR_INTERNAL = 1001;
const int VMGAHP_ERR_VM_INVALID_OPERATION = 1002;
const int VMGAHP_ERR_CRITICAL = 1003;
const int VMGAHP_ERR_CANNOT_CREATE_ISO_FILE = 1004;
const int VMGAHP_ERR_CANNOT_READ_ISO_FILE = 1005;
const int VMGAHP_ERR_CANNOT_CREATE_ARG_FILE = 1006;

// VM START
const int VMGAHP_ERR_NO_JOBCLASSAD_INFO = 1100;
const int VMGAHP_ERR_NO_SUPPORTED_VM_TYPE = 1101;
const int VMGAHP_ERR_VM_NOT_ENOUGH_MEMORY = 1102;
const int VMGAHP_ERR_XEN_INVALID_GUEST_KERNEL = 1103; //xen
const int VMGAHP_ERR_XEN_VBD_CONNECT_ERROR = 1104;  //xen
const int VMGAHP_ERR_XEN_CONFIG_FILE_ERROR = 1105; //xen

// VM CHECKPOINT
const int VMGAHP_ERR_VM_CANNOT_CREATE_CKPT_FILES = 1200;
const int VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT = 1201;

// VM RESUME/SUSPEND
const int VMGAHP_ERR_VM_INVALID_SUSPEND_FILE = 1300;
const int VMGAHP_ERR_VM_NO_SUPPORT_SUSPEND = 1301;

// Job ClassAd error
const int VMGAHP_ERR_JOBCLASSAD_NO_VM_MEMORY_PARAM = 1400;
const int VMGAHP_ERR_JOBCLASSAD_TOO_MUCH_MEMORY_REQUEST = 1401;
const int VMGAHP_ERR_JOBCLASSAD_MISMATCHED_NETWORKING = 1402;
const int VMGAHP_ERR_JOBCLASSAD_MISMATCHED_NETWORKING_TYPE = 1403;
const int VMGAHP_ERR_JOBCLASSAD_MISMATCHED_HARDWARE_VT = 1404;

// Error for Xen
const int VMGAHP_ERR_JOBCLASSAD_XEN_NO_KERNEL_PARAM = 1500;
const int VMGAHP_ERR_JOBCLASSAD_XEN_NO_ROOT_DEVICE_PARAM = 1501;
const int VMGAHP_ERR_JOBCLASSAD_XEN_NO_DISK_PARAM = 1502;
const int VMGAHP_ERR_JOBCLASSAD_XEN_INVALID_DISK_PARAM = 1503;
const int VMGAHP_ERR_JOBCLASSAD_XEN_KERNEL_NOT_FOUND = 1504;
const int VMGAHP_ERR_JOBCLASSAD_XEN_RAMDISK_NOT_FOUND = 1505;
const int VMGAHP_ERR_JOBCLASSAD_XEN_NO_CDROM_DEVICE = 1506;
const int VMGAHP_ERR_JOBCLASSAD_XEN_MISMATCHED_CHECKPOINT = 1507;

// Error for VMware
const int VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_DIR_PARAM = 1600;
const int VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_VMX_PARAM = 1601;
const int VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_VMDK_PARAM = 1602;
const int VMGAHP_ERR_JOBCLASSAD_VMWARE_VMX_NOT_FOUND = 1603;
const int VMGAHP_ERR_JOBCLASSAD_VMWARE_VMX_ERROR = 1604;
const int VMGAHP_ERR_JOBCLASSAD_VMWARE_NO_CDROM_DEVICE = 1605;


static inline void getVMGahpErrString(int gahp_error, MyString& outmsg) 
{
	switch( gahp_error ) {
		case VMGAHP_NO_ERR:
			outmsg = "";
			break;
		case VMGAHP_ERR_VM_NOT_FOUND:
			outmsg = "VM cannot be found";
			break;
		case VMGAHP_ERR_INTERNAL:
			outmsg = "VMGahp internal error";
			break;
		case VMGAHP_ERR_VM_INVALID_OPERATION:
			outmsg = "Invalid operation on VM";
			break;
		case VMGAHP_ERR_CRITICAL:
			outmsg = "VMGahp critical error";
			break;
		case VMGAHP_ERR_CANNOT_CREATE_ISO_FILE:
			outmsg = "Cannot create ISO file";
			break;
		case VMGAHP_ERR_CANNOT_READ_ISO_FILE:
			outmsg = "Cannot read ISO file";
			break;
		case VMGAHP_ERR_CANNOT_CREATE_ARG_FILE:
			outmsg = "Cannot create a file for arguments";
			break;
		case VMGAHP_ERR_NO_JOBCLASSAD_INFO:
			outmsg = "VMGahp didn't receive ClassAd from starter";
			break;
		case VMGAHP_ERR_NO_SUPPORTED_VM_TYPE:
			outmsg = "Not supported VM type";
			break;
		case VMGAHP_ERR_VM_NOT_ENOUGH_MEMORY:
			outmsg = "Not enough memory for VM";
			break;
		case VMGAHP_ERR_XEN_INVALID_GUEST_KERNEL:
			outmsg = "Invalid kernel for VM";
			break;
		case VMGAHP_ERR_XEN_VBD_CONNECT_ERROR:
			outmsg = "VBD could not be connected";
			break;
		case VMGAHP_ERR_XEN_CONFIG_FILE_ERROR:
			outmsg = "Unknown VM config file error";
			break;
		case VMGAHP_ERR_VM_CANNOT_CREATE_CKPT_FILES:
			outmsg = "Ckpt files cannot be created";
			break;
		case VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT:
			outmsg = "This VM doesn't support checkpoint";
			break;
		case VMGAHP_ERR_VM_INVALID_SUSPEND_FILE:
			outmsg = "Invalid suspend file";
			break;
		case VMGAHP_ERR_VM_NO_SUPPORT_SUSPEND:
			outmsg = "This VM doesn't support suspend";
			break;
		case VMGAHP_ERR_JOBCLASSAD_NO_VM_MEMORY_PARAM:
			outmsg = "VM_MEMORY is not defined in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_TOO_MUCH_MEMORY_REQUEST:
			outmsg = "VM_MEMORY is larger than max memory for VM";
			break;
		case VMGAHP_ERR_JOBCLASSAD_MISMATCHED_NETWORKING:
			outmsg = "VM_Networking is not supported by this machine";
			break;
		case VMGAHP_ERR_JOBCLASSAD_MISMATCHED_NETWORKING_TYPE:
			outmsg = "Not supported networking type";
			break;
		case VMGAHP_ERR_JOBCLASSAD_MISMATCHED_HARDWARE_VT:
			outmsg = "No support of hardware virtualization";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_NO_KERNEL_PARAM:
			outmsg = "No Xen kernel in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_NO_ROOT_DEVICE_PARAM:
			outmsg = "No Xen root device in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_NO_DISK_PARAM:
			outmsg = "No Xen disk in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_INVALID_DISK_PARAM:
			outmsg = "Incorrect Xen disk parameter";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_KERNEL_NOT_FOUND:
			outmsg = "Cannot read Xen kernel";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_RAMDISK_NOT_FOUND:
			outmsg = "Cannot read Xen ramdisk";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_NO_CDROM_DEVICE:
			outmsg = "No Xen CDROM device in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_XEN_MISMATCHED_CHECKPOINT:
			outmsg = "Cannot use vm checkpoint in Xen due to file path";
			break;
		case VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_DIR_PARAM:
			outmsg = "VMware dir parameter is not defined in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_VMX_PARAM:
			outmsg = "VMware vmx parameter is not defined in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_VMDK_PARAM:
			outmsg = "VMware vmdk parameter is not defined in Job ClassAd";
			break;
		case VMGAHP_ERR_JOBCLASSAD_VMWARE_VMX_NOT_FOUND:
			outmsg = "VMX file cannot be read";
			break;
		case VMGAHP_ERR_JOBCLASSAD_VMWARE_VMX_ERROR:
			outmsg = "VMX file has invalid formats such as file-path or permission";
			break;
		case VMGAHP_ERR_JOBCLASSAD_VMWARE_NO_CDROM_DEVICE:
			outmsg = "VMX file has no CDROM device";
			break;
		default:
			outmsg = "Unknown error";
	}
}

#endif

