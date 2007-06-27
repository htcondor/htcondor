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
#include "condor_string.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_mkstemp.h"
#include "MyString.h"
#include "util_lib_proto.h"
#include "vmgahp_common.h"
#include "vm_type.h"
#include "xen_type.h"
#include "vmgahp_error_codes.h"
#include "condor_vm_universe_types.h"

#define XEN_STATUS_TMP_FILE "xen_status.condor"
#define XEN_CONFIG_FILE_NAME "xen_vm.config"
#define XEN_CKPT_TIMESTAMP_FILE_SUFFIX ".timestamp"

static int
getVMGahpErrorNumber(const char* fname)
{
	FILE *file;
	char buffer[1024];
	file = safe_fopen_wrapper(fname, "r");

	if( !file ) {
		return VMGAHP_ERR_INTERNAL;
	}
	if( fgets(buffer, sizeof(buffer), file) == NULL) {
		fclose(file);
		return VMGAHP_ERR_INTERNAL;
	}

	fclose(file);

	if( buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer) - 1] = '\0';
	}

	if( !strcasecmp(buffer, "MEMORY ERROR" ) ) {
		return VMGAHP_ERR_VM_NOT_ENOUGH_MEMORY;
	}else if( !strcasecmp(buffer, "VBD CONNECT ERROR" ) ) {
		return VMGAHP_ERR_XEN_VBD_CONNECT_ERROR;
	}else if( !strcasecmp(buffer, "INVALID ARGUMENT" ) ) {
		return VMGAHP_ERR_XEN_INVALID_GUEST_KERNEL;
	}

	return VMGAHP_ERR_XEN_CONFIG_FILE_ERROR;
}

XenType::XenType(const char* scriptname, const char* workingpath, 
		ClassAd* ad) : VMType("", scriptname, workingpath, ad)
{
	m_vmtype = CONDOR_VM_UNIVERSE_XEN;
	m_cputime_before_suspend = 0;
	m_xen_hw_vt = false;
	m_allow_hw_vt_suspend = false;
	m_restart_with_ckpt = false;
	m_has_transferred_disk_file = false;
}

XenType::~XenType()
{
	Shutdown();

	if( getVMStatus() != VM_STOPPED ) {
		// To make sure VM exits
		killVM();
	}
	setVMStatus(VM_STOPPED);

	XenDisk *disk = NULL;
	m_disk_list.Rewind();
	while( m_disk_list.Next(disk) ) {
		m_disk_list.DeleteCurrent();
		delete disk;
	}
}

bool 
XenType::Start()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::Start\n");

	m_result_msg = "";

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() != VM_STOPPED ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	if( m_restart_with_ckpt ) {
		bool res = Resume();
		if( res ) {
			// Success!!
			vmprintf(D_ALWAYS, "Succeeded to restart with checkpointed files\n");

			// Here we manually update timestamp of all writable disk files
			updateAllWriteDiskTimestamp(time(NULL));
			m_start_time.getTime();
			return true;
		}else {
			// Failed to restart with checkpointed files 
			vmprintf(D_ALWAYS, "Failed to restart with checkpointed files\n");
			vmprintf(D_ALWAYS, "So, we will try to create a new configuration file\n");

			deleteNonTransferredFiles();
			m_configfile = "";
			m_suspendfile = "";
			m_restart_with_ckpt = false;

			if( CreateConfigFile() == false ) {
				vmprintf(D_ALWAYS, "Failed to create a new configuration files\n");
				return false;
			}

			// Succeeded to create a configuration file
			// Keep going..
		}
	}

	MyString tmpfilename;
	tmpfilename.sprintf("%s%c%s", m_workingpath.Value(), 
			DIR_DELIM_CHAR, XEN_STATUS_TMP_FILE);
	unlink(tmpfilename.Value());

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("start");
	systemcmd.AppendArg(m_configfile);
	systemcmd.AppendArg(tmpfilename);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		// Read error file
		m_result_msg = "";
		m_result_msg += getVMGahpErrorNumber(tmpfilename.Value());
		unlink(tmpfilename.Value());
		return false;
	}
	unlink(tmpfilename.Value());

	setVMStatus(VM_RUNNING);
	m_start_time.getTime();
	m_cpu_time = 0;

	// Here we manually update timestamp of all writable disk files
	updateAllWriteDiskTimestamp(time(NULL));
	return true;
}

bool
XenType::Shutdown()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::Shutdown\n");

	m_result_msg = "";

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_STOPPED ) {
		if( m_self_shutdown ) {
			if( m_vm_no_output_vm ) {
				// A job user doesn't want to get back VM files.
				// So we will delete all files in working directory.
				m_delete_working_files = true;
				m_is_checkpointed = false;
			}else {
				if( !m_suspendfile.IsEmpty() ) {
					unlink(m_suspendfile.Value());
				}
				m_suspendfile = "";

				// We need to update timestamp of transferred writable disk files
				updateLocalWriteDiskTimestamp(time(NULL));
			}
		}
		// We here set m_self_shutdown to false
		// So, above functions will not be called twice
		m_self_shutdown = false;
		return true;
	}

	// if( getVMStatus() == VM_SUSPENDED )
	// do nothing
	
	// If a VM is soft suspended, resume it first
	ResumeFromSoftSuspend();

	if( getVMStatus() == VM_RUNNING ) {
		ArgList systemcmd;
		systemcmd.AppendArg(m_scriptname);
		systemcmd.AppendArg("stop");
		systemcmd.AppendArg(m_configfile);

		int result = systemCommand(systemcmd, true);
		if( result != 0 ) {
			// system error happens
			// killing VM by force
			killVM();
		}
		// Now we don't need working files any more
		m_delete_working_files = true;
		m_is_checkpointed = false;
	}
	
	setVMStatus(VM_STOPPED);
	m_stop_time.getTime();
	return true;
}

bool
XenType::Checkpoint()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::Checkpoint\n");

	m_result_msg = "";

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_STOPPED ) {
		vmprintf(D_ALWAYS, "Checkpoint is called for a stopped VM\n");
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	if( !m_vm_checkpoint ) {
		vmprintf(D_ALWAYS, "Checkpoint is not supported.\n");
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT;
		return false;
	}

	if( m_xen_hw_vt && !m_allow_hw_vt_suspend ) {
		// This VM uses hardware virtualization.
		// However, Xen cannot suspend this type of VM yet.
		// So we cannot checkpoint this VM.
		vmprintf(D_ALWAYS, "Checkpoint of Hardware VT is not supported.\n");
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT;
		return false;
	}

	// If a VM is soft suspended, resume it first
	ResumeFromSoftSuspend();

	// This function cause a running VM to be suspended.
	if( createCkptFiles() == false ) { 
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_CANNOT_CREATE_CKPT_FILES;
		vmprintf(D_ALWAYS, "failed to create checkpoint files\n");
		return false;
	}

	return true;
}

bool
XenType::ResumeFromSoftSuspend(void)
{
	vmprintf(D_FULLDEBUG, "Inside XenType::ResumeFromSoftSuspend\n");
	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		return false;
	}

	if( m_is_soft_suspended ) {
		ArgList systemcmd;
		systemcmd.AppendArg(m_scriptname);
		systemcmd.AppendArg("unpause");
		systemcmd.AppendArg(m_configfile);

		int result = systemCommand(systemcmd, true);
		if( result != 0 ) {
			// unpause failed.
			vmprintf(D_ALWAYS, "Unpausing VM failed in "
					"XenType::ResumeFromSoftSuspend\n");
			return false;
		}
		m_is_soft_suspended = false;
	}
	return true;
}

bool 
XenType::SoftSuspend()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::SoftSuspend\n");

	m_result_msg = "";

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( m_is_soft_suspended ) {
		return true;
	}

	if( getVMStatus() != VM_RUNNING ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("pause");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, true);
	if( result == 0 ) {
		// pause succeeds.
		m_is_soft_suspended = true;
		return true;
	}

	// Failed to suspend a VM softly.
	// Instead of soft suspend, we use hard suspend.
	vmprintf(D_ALWAYS, "SoftSuspend failed, so try hard Suspend instead!.\n");
	return Suspend();
}

bool 
XenType::Suspend()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::Suspend\n");

	m_result_msg = "";

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_SUSPENDED ) {
		return true;
	}

	if( getVMStatus() != VM_RUNNING ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	if( m_xen_hw_vt && !m_allow_hw_vt_suspend ) {
		// This VM uses hardware virtualization.
		// However, Xen cannot suspend this type of VM yet.
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_NO_SUPPORT_SUSPEND;
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	MyString tmpfilename;
	makeNameofSuspendfile(tmpfilename);
	unlink(tmpfilename.Value());

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("suspend");
	systemcmd.AppendArg(m_configfile);
	systemcmd.AppendArg(tmpfilename);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_CRITICAL;
		unlink(tmpfilename.Value());
		return false;
	}

	m_suspendfile = tmpfilename;

	setVMStatus(VM_SUSPENDED);
	m_cputime_before_suspend += m_cpu_time;
	m_cpu_time = 0;
	return true;
}

bool 
XenType::Resume()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::Resume\n");

	m_result_msg = "";

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	if( getVMStatus() == VM_RUNNING ) {
		return true;
	}

	if( !m_restart_with_ckpt && 
			( getVMStatus() != VM_SUSPENDED) ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}
	m_restart_with_ckpt = false;

	if( m_is_checkpointed ) {
		if( m_is_chowned ) {
			// Because files in the working directory were chowned to a caller.
			// we need to restore ownership of files from caller uid to a job user
			chownWorkingFiles(get_job_user_uid());
		}
		m_is_checkpointed = false;
	}

	if( check_vm_read_access_file(m_suspendfile.Value(), true) == false ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_VM_INVALID_SUSPEND_FILE;
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("resume");
	systemcmd.AppendArg(m_suspendfile);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_CRITICAL;
		return false;
	}

	setVMStatus(VM_RUNNING);
	m_cpu_time = 0;

	// Delete suspend file
	unlink(m_suspendfile.Value());
	m_suspendfile = "";
	return true;
}

bool
XenType::Status()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::Status\n");

	m_result_msg = "";

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( m_is_soft_suspended ) {
		// If a VM is softly suspended,
		// we cannot get info about the VM by using script
		m_result_msg = VMGAHP_STATUS_COMMAND_STATUS;
		m_result_msg += "=";
		m_result_msg += "SoftSuspended";
		return true;
	}

	// Check the last time when we executed status.
	// If the time is in 10 seconds before current time, 
	// We will not execute status again.
	// Maybe this case may happen when it took long time 
	// to execute the last status.
	UtcTime cur_time;
	long diff_seconds = 0;

	cur_time.getTime();
	diff_seconds = cur_time.seconds() - m_last_status_time.seconds();

	if( (diff_seconds < 10) && !m_last_status_result.IsEmpty() ) {
		m_result_msg = m_last_status_result;
		return true;
	}

	MyString tmpfilename;
	tmpfilename.sprintf("%s%c%s", m_workingpath.Value(), 
			DIR_DELIM_CHAR, XEN_STATUS_TMP_FILE);
	unlink(tmpfilename.Value());

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	if( m_vm_networking ) {
		systemcmd.AppendArg("getvminfo");
	}else {
		systemcmd.AppendArg("status");
	}
	systemcmd.AppendArg(m_configfile);
	systemcmd.AppendArg(tmpfilename);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_CRITICAL;
		unlink(tmpfilename.Value());
		return false;
	}

	// Got result
	FILE *file;
	char buffer[1024];
	file = safe_fopen_wrapper(tmpfilename.Value(), "r");
	if( !file ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		unlink(tmpfilename.Value());
		return false;
	}

	MyString one_line;
	MyString name;
	MyString value;

	MyString vm_status;
	float cputime = 0;

	while( fgets(buffer, sizeof(buffer), file) != NULL ) {
		one_line = buffer;
		one_line.chomp();
		one_line.trim();

		if( one_line.Length() == 0 ) {
			continue;
		}

		if( one_line[0] == '#' ) {
			/* Skip over comments */
			continue;
		}

		parse_param_string(one_line.Value(), name, value, true);
		if( !name.Length() || !value.Length() ) {
			continue;
		}

		if( !strcasecmp(name.Value(), VMGAHP_STATUS_COMMAND_STATUS) ) {
			vm_status = value;
		}else if( !strcasecmp(name.Value(), VMGAHP_STATUS_COMMAND_CPUTIME) ) {
			cputime = (float)strtod(value.Value(), (char **)NULL);
			if( cputime <= 0 ) {
				cputime = 0;
			}
		}else if( !strcasecmp(name.Value(), VMGAHP_STATUS_COMMAND_MAC) ) {
			m_vm_mac = value;
		}else if( !strcasecmp(name.Value(), VMGAHP_STATUS_COMMAND_IP) ) {
			m_vm_ip = value;
		}
	}
	fclose(file);
	unlink(tmpfilename.Value());

	if( !vm_status.Length() ) {
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}

	m_result_msg = "";

	if( m_vm_mac.IsEmpty() == false ) {
		if( m_result_msg.IsEmpty() == false ) {
			m_result_msg += " ";
		}
		m_result_msg += VMGAHP_STATUS_COMMAND_MAC;
		m_result_msg += "=";
		m_result_msg += m_vm_mac;
	}

	if( m_vm_ip.IsEmpty() == false ) {
		if( m_result_msg.IsEmpty() == false ) {
			m_result_msg += " ";
		}
		m_result_msg += VMGAHP_STATUS_COMMAND_IP;
		m_result_msg += "=";
		m_result_msg += m_vm_ip;
	}

	if( m_result_msg.IsEmpty() == false ) {
		m_result_msg += " ";
	}

	m_result_msg += VMGAHP_STATUS_COMMAND_STATUS;
	m_result_msg += "=";

	if( strcasecmp(vm_status.Value(), "Running") == 0 ) {
		// VM is still running
		setVMStatus(VM_RUNNING);
		m_result_msg += "Running";

		if( cputime > 0 ) {
			// Update vm running time	
			m_cpu_time = cputime;

			m_result_msg += " ";
			m_result_msg += VMGAHP_STATUS_COMMAND_CPUTIME;
			m_result_msg += "=";
			m_result_msg += (double)(m_cpu_time + m_cputime_before_suspend);
		}
		return true;
	} else if( strcasecmp(vm_status.Value(), "Stopped") == 0 ) {
		// VM is stopped
		if( getVMStatus() == VM_SUSPENDED ) {
			if( m_suspendfile.IsEmpty() == false) {
				m_result_msg += "Suspended";
				return true;
			}
		}

		if( getVMStatus() == VM_RUNNING ) {
			m_self_shutdown = true;
		}

		m_result_msg += "Stopped";
		if( getVMStatus() != VM_STOPPED ) {
			setVMStatus(VM_STOPPED);
			m_stop_time.getTime();
		}
		return true;
	}else {
		// Woops, something is wrong
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_INTERNAL;
		return false;
	}
	return false;
}

bool
XenType::CreateConfigFile()
{
	MyString tmp_config_name;
	MyString disk_string;
	MyString xen_kernel_param;
	MyString xen_kernel_file;
	MyString xen_ramdisk_file;
	MyString xen_bootloader;
	MyString xen_root;
	MyString xen_disk;
	MyString xen_extra;

	FILE *fp = NULL;

	m_result_msg = "";

	// Read common parameters for VM
	// and create the name of this VM
	if( parseCommonParamFromClassAd(true) == false ) {
		return false;
	}

	if( m_vm_mem < 32 ) {
		// Allocating less than 32MBs is not recommended in Xen.
		m_vm_mem = 32;
	}

	// Read the parameter of Xen kernel
	if( m_classAd.LookupString( VMPARAM_XEN_KERNEL, xen_kernel_param) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
							VMPARAM_XEN_KERNEL);
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_NO_KERNEL_PARAM;
		return false;
	}
	xen_kernel_param.trim();

	if(strcasecmp(xen_kernel_param.Value(), XEN_KERNEL_ANY) == 0) {
		vmprintf(D_ALWAYS, "VMGahp will use default xen kernel\n");
		char* config_value = vmgahp_param( "XEN_DEFAULT_KERNEL" );
		if( !config_value ) {
			vmprintf(D_ALWAYS, "Default xen kernel is not defined "
					"in vmgahp config file\n");
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_CRITICAL;
			return false;
		}else {
			xen_kernel_file = delete_quotation_marks(config_value);
			free(config_value);

			config_value = vmgahp_param( "XEN_DEFAULT_RAMDISK" );
			if( config_value ) {
				xen_ramdisk_file = delete_quotation_marks(config_value);
				free(config_value);
			}
		}
	}else if(strcasecmp(xen_kernel_param.Value(), XEN_KERNEL_INCLUDED) == 0) {
		vmprintf(D_ALWAYS, "VMGahp will use xen bootloader\n");
		char* config_value = vmgahp_param( "XEN_BOOTLOADER" );
		if( !config_value ) {
			vmprintf(D_ALWAYS, "xen bootloader is not defined "
					"in vmgahp config file\n");
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_CRITICAL;
			return false;
		}else {
			xen_bootloader = delete_quotation_marks(config_value);
			free(config_value);
		}
	}else if(strcasecmp(xen_kernel_param.Value(), XEN_KERNEL_HW_VT) == 0) {
		vmprintf(D_ALWAYS, "This VM requires hardware virtualization\n");
		if( !m_vm_hardware_vt ) {
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_JOBCLASSAD_MISMATCHED_HARDWARE_VT;
			return false;
		}
		m_xen_hw_vt = true;
		m_allow_hw_vt_suspend = 
			vmgahp_param_boolean("XEN_ALLOW_HARDWARE_VT_SUSPEND", false);
	}else {
		// A job user defined a customized kernel
		// make sure that the file for xen kernel is readable
		if( check_vm_read_access_file(xen_kernel_param.Value(), true) == false) {
			vmprintf(D_ALWAYS, "xen kernel file '%s' cannot be read\n", 
					xen_kernel_param.Value());
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_KERNEL_NOT_FOUND;
			return false;
		}
		xen_kernel_file = xen_kernel_param;

		if( m_classAd.LookupString( VMPARAM_XEN_RAMDISK, xen_ramdisk_file) 
					== 1 ) {
			// A job user defined a customized ramdisk
			xen_ramdisk_file.trim();
			if( check_vm_read_access_file(xen_ramdisk_file.Value(), true) == false) {
				// make sure that the file for xen ramdisk is readable
				vmprintf(D_ALWAYS, "xen ramdisk file '%s' cannot be read\n", 
						xen_ramdisk_file.Value());
				m_result_msg = "";
				m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_RAMDISK_NOT_FOUND;
				return false;
			}
		}
	}

	if( xen_kernel_file.IsEmpty() == false ) {
		// Read the parameter of Xen Root
		if( m_classAd.LookupString( VMPARAM_XEN_ROOT, xen_root) != 1 ) {
			vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
					VMPARAM_XEN_ROOT);
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_NO_ROOT_DEVICE_PARAM;
			return false;
		}
		xen_root.trim();
	}

	// Read the parameter of Xen Disk
	if( m_classAd.LookupString(VMPARAM_XEN_DISK, xen_disk) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
				VMPARAM_XEN_DISK);
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_NO_DISK_PARAM;
		return false;
	}
	xen_disk.trim();
	if( parseXenDiskParam(xen_disk.Value()) == false ) {
		vmprintf(D_ALWAYS, "xen disk format(%s) is incorrect\n", 
				xen_disk.Value());
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_INVALID_DISK_PARAM;
	}

	// Read the parameter of Xen Extra
	if( m_classAd.LookupString(VMPARAM_XEN_EXTRA, xen_extra) == 1 ) {
		xen_extra.trim();
	}

	// Read the parameter of Xen cdrom device
	if( m_classAd.LookupString(VMPARAM_XEN_CDROM_DEVICE, m_xen_cdrom_device) == 1 ) {
		m_xen_cdrom_device.trim();
		m_xen_cdrom_device.strlwr();
	}

	if( (m_vm_cdrom_files.isEmpty() == false) && 
			m_xen_cdrom_device.IsEmpty() ) {
		vmprintf(D_ALWAYS, "A job user defined files for a CDROM, "
				"but the job user didn't define CDROM device\n");
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_NO_CDROM_DEVICE;
		return false;
	}

	// Check whether this is re-starting after vacating checkpointing,
	if( m_transfer_intermediate_files.isEmpty() == false ) {
		// we have chckecpointed files
		// Find vm config file and suspend file
		MyString ckpt_config_file;
		MyString ckpt_suspend_file;
		if( findCkptConfigAndSuspendFile(ckpt_config_file, ckpt_suspend_file) 
				== false ) {
			vmprintf(D_ALWAYS, "Checkpoint files exist but "
					"cannot find the correct config file and suspend file\n");
			//Delete all non-transferred files from submit machine
			deleteNonTransferredFiles();
			m_restart_with_ckpt = false;
		}else {
			m_configfile = ckpt_config_file;
			m_suspendfile = ckpt_suspend_file;
			vmprintf(D_ALWAYS, "Found checkpointed files, "
					"so we start using them\n");
			m_restart_with_ckpt = true;
			return true;
		}
	}

	if( (m_vm_cdrom_files.isEmpty() == false) && !m_has_iso) {
		// Create ISO file
		if( createISO() == false ) {
			vmprintf(D_ALWAYS, "Cannot create a ISO file for CDROM\n");
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_CANNOT_CREATE_ISO_FILE;
			return false;
		}
	}

	// Here we check if this job actually can use checkpoint 
	if( m_vm_checkpoint ) {
		// For vm checkpoint in Xen
		// 1. all disk files should be in a shared file system
		// 2. If a job uses CDROM files, it should be 
		// 	  single ISO file and be in a shared file system
		if( m_has_transferred_disk_file || m_local_iso ) {
			// In this case, we cannot use vm checkpoint for Xen
			// To use vm checkpoint in Xen, 
			// all disk and iso files should be in a shared file system
			vmprintf(D_ALWAYS, "To use vm checkpint in Xen, "
					"all disk and iso files should be "
					"in a shared file system\n");
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_JOBCLASSAD_XEN_MISMATCHED_CHECKPOINT;
			return false;
		}
	}

	// create a vm config file
	tmp_config_name.sprintf("%s%c%s",m_workingpath.Value(), 
			DIR_DELIM_CHAR, XEN_CONFIG_FILE_NAME);
	fp = safe_fopen_wrapper(tmp_config_name.Value(), "w");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper vm config file "
				"in write mode: safe_fopen_wrapper(%s) returns %s\n", 
				tmp_config_name.Value(), strerror(errno));
		m_result_msg = "";
		m_result_msg += VMGAHP_ERR_CRITICAL;
		return false;
	}

	if( fprintf(fp, "name=\"%s\"\n", m_vm_name.Value()) < 0 ) {
		goto writeerror;
	}

	if( fprintf(fp, "memory=%d\n", m_vm_mem) < 0 ) {
		goto writeerror;
	}

	if( xen_kernel_file.IsEmpty() == false ) {
		MyString tmp_fullname;
		if( isTransferedFile(xen_kernel_file.Value(), 
					tmp_fullname) ) {
			// this file is transferred
			// So we use basename
			if( fprintf(fp, "kernel=\"%s\"\n", basename(tmp_fullname.Value())) < 0 ) {
				goto writeerror;
			}
			xen_kernel_file = tmp_fullname;
		}else {
			// this file is not transferred
			if( fprintf(fp, "kernel=\"%s\"\n", xen_kernel_file.Value()) < 0 ) {
				goto writeerror;
			}
		}
		if( xen_ramdisk_file.IsEmpty() == false ) {
			if( isTransferedFile(xen_ramdisk_file.Value(), 
						tmp_fullname) ) {
				// this file is transferred
				// So we use basename
				if(fprintf(fp, "ramdisk=\"%s\"\n", basename(tmp_fullname.Value())) < 0) {
					goto writeerror;
				}
				xen_ramdisk_file = tmp_fullname;
			}else {
				// this file is not transferred
				if( fprintf(fp, "ramdisk=\"%s\"\n", xen_ramdisk_file.Value()) < 0 ) {
					goto writeerror;
				}
			}
		}
		if( xen_root.IsEmpty() == false ) {
			if( fprintf(fp,"root=\"%s\"\n", xen_root.Value()) < 0 ) {
				goto writeerror;
			}
		}
	}

	if( m_vm_networking ) {
		MyString networking_type;
		char* xen_vif_param = NULL;

		if( m_vm_networking_interfaces.find("dhcp") >= 0 ) {
			if( fprintf(fp, "dhcp=\"dhcp\"\n") < 0 ) {
				goto writeerror;
			}
		}

		if( m_xen_hw_vt ) {
			xen_vif_param = vmgahp_param("XEN_VT_VIF");
			if( xen_vif_param ) {
				networking_type = delete_quotation_marks(xen_vif_param);
				free(xen_vif_param);
			}
		}

		if( networking_type.IsEmpty() ) {
			xen_vif_param = vmgahp_param("XEN_VIF_PARAMETER");
			if( xen_vif_param) {
				networking_type = delete_quotation_marks(xen_vif_param);
				free(xen_vif_param);
			}else {
				networking_type = "['']";
			}
		}

		if( fprintf(fp, "vif=%s\n", networking_type.Value()) < 0 ) {
			goto writeerror;
		}
	}

	// Create disk parameter in Xen config file
	disk_string = makeXenDiskString();
	if( fprintf(fp,"disk=%s\n", disk_string.Value()) < 0 ) {
		goto writeerror;
	}

	if( strcasecmp(xen_kernel_param.Value(), XEN_KERNEL_INCLUDED) == 0) {
		if( fprintf(fp, "bootloader=\"%s\"\n", xen_bootloader.Value()) < 0 ) {
			goto writeerror;
		}
	}

	if( m_xen_hw_vt ) {
		if( write_specific_vm_params_to_file("XEN_VT_", fp) == false ) {
			goto writeerror;
		}
	}

	if( xen_extra.IsEmpty() == false ) {
		if( fprintf(fp,"extra=\"%s\"\n", xen_extra.Value()) < 0 ) {
			goto writeerror;
		}
	}

	if( write_forced_vm_params_to_file(fp) == false ) {
		goto writeerror;
	}
	fclose(fp);
	fp = NULL;

	if( m_use_script_to_create_config ) {
		// We will call the script program 
		// to create a configuration file for VM
		
		if( createConfigUsingScript(tmp_config_name.Value()) == false ) {
			unlink(tmp_config_name.Value());
			m_result_msg = "";
			m_result_msg += VMGAHP_ERR_CRITICAL;
			return false;
		}
	}

	// set vm config file
	m_configfile = tmp_config_name;
	return true;

writeerror:
	vmprintf(D_ALWAYS, "failed to fprintf in CreateConfigFile(%s:%s)\n",
			tmp_config_name.Value(), strerror(errno));
	if( fp ) {
		fclose(fp);
	}
	unlink(tmp_config_name.Value());
	m_result_msg = "";
	m_result_msg += VMGAHP_ERR_CRITICAL;
	return false;
}

bool 
XenType::parseXenDiskParam(const char *format)
{
	if( !format || (format[0] == '\0') ) {
		return false;
	}

	StringList working_files;
	find_all_files_in_dir(m_workingpath.Value(), working_files, true);

	StringList disk_files(format, ",");
	if( disk_files.isEmpty() ) {
		return false;
	}

	disk_files.rewind();
	const char *one_disk = NULL;
	while( (one_disk = disk_files.next() ) != NULL ) {
		// found a disk file 
		StringList single_disk_file(one_disk, ":");
		if( single_disk_file.number() != 3 ) {
			return false;
		}

		single_disk_file.rewind();

		// name of a disk file
		MyString dfile = single_disk_file.next();
		if( dfile.IsEmpty() ) {
			return false;
		}
		dfile.trim();

		const char* tmp_base_name = condor_basename(dfile.Value());
		if( !tmp_base_name ) {
			return false;
		}

		// Every disk file for Xen must have full path name
		MyString disk_file;
		if( filelist_contains_file(dfile.Value(), 
					&working_files, true) ) {
			// file is transferred
			disk_file = m_workingpath;
			disk_file += DIR_DELIM_CHAR;
			disk_file += tmp_base_name; 

			m_has_transferred_disk_file = true;
		}else {
			// file is not transferred.
			if( fullpath(dfile.Value()) == false) {
				vmprintf(D_ALWAYS, "File(%s) for xen disk "
						"should have full path name\n", dfile.Value());
				return false;
			}
			disk_file = dfile;
		}

		// device name
		MyString disk_device = single_disk_file.next();
		disk_device.trim();
		disk_device.strlwr();

		// disk permission
		MyString disk_perm = single_disk_file.next();
		disk_perm.trim();

		if( !strcasecmp(disk_perm.Value(), "w") || 
				!strcasecmp(disk_perm.Value(), "rw")) {
			// check if this disk file is writable
			if( check_vm_write_access_file(disk_file.Value(), true) == false ) {
				vmprintf(D_ALWAYS, "xen disk image file('%s') cannot be modified\n",
					   	disk_file.Value());
				return false;
			}
		}else {
			// check if this disk file is readable 
			if( check_vm_read_access_file(disk_file.Value(), true) == false ) {
				vmprintf(D_ALWAYS, "xen disk image file('%s') cannot be read\n", 
						disk_file.Value());
				return false;
			}
		}

		XenDisk *newdisk = new XenDisk;
		ASSERT(newdisk);
		newdisk->filename = disk_file;
		newdisk->device = disk_device;
		newdisk->permission = disk_perm;
		m_disk_list.Append(newdisk);
	}

	if( m_disk_list.Number() == 0 ) {
		vmprintf(D_ALWAYS, "No valid Xen disk\n");
		return false;
	}

	return true;
}

// This function should be called after parseXenDiskParam and createISO
MyString
XenType::makeXenDiskString(void)
{
	char* tmp_ptr = NULL;

	MyString iostring;
	tmp_ptr = vmgahp_param("XEN_IMAGE_IO_TYPE" );
	if( !tmp_ptr ) {
		//Default io type is file:
		iostring = "file:";
	}else {
		iostring = delete_quotation_marks(tmp_ptr);
		free(tmp_ptr);
		if( iostring.FindChar(':') == -1 ) {
			iostring += ":";
		}
	}

	MyString devicestring;
	tmp_ptr = vmgahp_param("XEN_DEVICE_TYPE_FOR_VT");
	if( tmp_ptr ) {
		devicestring = delete_quotation_marks(tmp_ptr);
		free(tmp_ptr);
		if( devicestring.FindChar(':') == -1 ) {
			devicestring += ":";
		}
	}

	MyString xendisk;
	xendisk = "[ ";
	bool first_disk = true;

	XenDisk *vdisk = NULL;
	m_disk_list.Rewind();
	while( m_disk_list.Next(vdisk) ) {
		if( !first_disk ) {
			xendisk += ",";
		}
		first_disk = false;
		xendisk += "'";
		xendisk += iostring;
		xendisk += vdisk->filename;
		xendisk += ",";
	
		if( m_xen_hw_vt && !devicestring.IsEmpty() ) {
			xendisk += devicestring;
		}
		xendisk += vdisk->device;
		xendisk += ",";

		xendisk += vdisk->permission;
		xendisk += "'";
	}

	if( m_iso_file.IsEmpty() == false ) {
		xendisk += ",";
		xendisk += "'";
		xendisk += iostring;
		xendisk += m_iso_file;
		xendisk += ",";
		xendisk += m_xen_cdrom_device;
		xendisk += ":cdrom";
		xendisk += ",";
		xendisk += "r";
		xendisk += "'";
	}

	xendisk += " ]";
	return xendisk;
}

bool 
XenType::writableXenDisk(const char* file)
{
	if( !file ) {
		return false;
	}

	XenDisk *vdisk = NULL;
	m_disk_list.Rewind();
	while( m_disk_list.Next(vdisk) ) {
		if( !strcasecmp(basename(file), basename(vdisk->filename.Value())) ) {
			if( !strcasecmp(vdisk->permission.Value(), "w") || 
					!strcasecmp(vdisk->permission.Value(), "rw")) {
				return true;
			}else {
				return false;
			}
		}
	}
	return false;
}

void
XenType::updateLocalWriteDiskTimestamp(time_t timestamp)
{
	char *tmp_file = NULL;
	StringList working_files;
	struct utimbuf timewrap;

	find_all_files_in_dir(m_workingpath.Value(), working_files, true);

	working_files.rewind();
	while( (tmp_file = working_files.next()) != NULL ) {
		// In Xen, disk file is generally used via loopback-mounted 
		// file. However, mtime of those files is not updated 
		// even after modification.
		// So we manually update mtimes of writable disk files.
		if( writableXenDisk(tmp_file) ) {
			timewrap.actime = timestamp;
			timewrap.modtime = timestamp;
			utime(tmp_file, &timewrap);
		}
	}
}

void
XenType::updateAllWriteDiskTimestamp(time_t timestamp)
{
	struct utimbuf timewrap;

	XenDisk *vdisk = NULL;
	m_disk_list.Rewind();
	while( m_disk_list.Next(vdisk) ) {
		if( !strcasecmp(vdisk->permission.Value(), "w") || 
				!strcasecmp(vdisk->permission.Value(), "rw")) {
			// This is a writable disk
			timewrap.actime = timestamp;
			timewrap.modtime = timestamp;
			utime(vdisk->filename.Value(), &timewrap);
		}
	}
}

bool
XenType::createCkptFiles(void)
{
	vmprintf(D_FULLDEBUG, "Inside XenType::createCkptFiles\n");

	// This function will suspend a running VM.
	if( getVMStatus() == VM_STOPPED ) {
		vmprintf(D_ALWAYS, "createCkptFiles is called for a stopped VM\n");
		return false;
	}

	if( getVMStatus() == VM_RUNNING ) {
		if( Suspend() == false ) {
			return false;
		}
	}

	if( getVMStatus() == VM_SUSPENDED ) {
		// we create timestampfile
		time_t current_time = time(NULL);
		MyString timestampfile(m_suspendfile);
		timestampfile += XEN_CKPT_TIMESTAMP_FILE_SUFFIX;

		FILE *fp = safe_fopen_wrapper(timestampfile.Value(), "w");
		if( !fp ) {
			vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper file(%s) for "
					"checkpoint timestamp : safe_fopen_wrapper returns %s\n", 
					timestampfile.Value(), strerror(errno));
			Resume();
			return false;
		}
		if( fprintf(fp, "%d\n", (int)current_time) < 0 ) {
			fclose(fp);
			unlink(timestampfile.Value());
			vmprintf(D_ALWAYS, "failed to fprintf for checkpoint timestamp "
					"file(%s:%s)\n", timestampfile.Value(), strerror(errno));
			Resume();
			return false;
		}
		fclose(fp);
		updateAllWriteDiskTimestamp(current_time);

		if( m_is_chowned ) {
			// Because files in the working directory were chowned to the user,
			// we need to change ownership of files from a job user to caller uid
			// So caller (aka Condor) can access these files.
			chownWorkingFiles(get_caller_uid());
		}

		// checkpoint succeeds
		m_is_checkpointed = true;
		return true;
	}

	return false;
}

bool 
XenType::checkXenParams(VMGahpConfig* config)
{
	char *config_value = NULL;
	MyString fixedvalue;

	if( !config ) {
		return false;
	}

	// find script program for Xen
	config_value = vmgahp_param("XEN_SCRIPT");
	if( !config_value ) {
		vmprintf(D_ALWAYS, "\nERROR: You should define 'XEN_SCRIPT' for "
				"the script program for Xen in \"%s\"\n", 
				config->m_configfile.Value());
		return false;
	}
	fixedvalue = delete_quotation_marks(config_value);
	free(config_value);

	struct stat sbuf;
	if( stat(fixedvalue.Value(), &sbuf ) < 0 ) {
		vmprintf(D_ALWAYS, "\nERROR: Failed to access the script "
				"program for Xen:(%s:%s)\n", fixedvalue.Value(),
			   	strerror(errno));
		return false;
	}

	// owner must be root
	if( sbuf.st_uid != ROOT_UID ) {
		vmprintf(D_ALWAYS, "\nFile Permission Error: "
				"owner of \"%s\" must be root\n", fixedvalue.Value());
		return false;
	}

	// Other writable bit
	if( sbuf.st_mode & S_IWOTH ) {
		vmprintf(D_ALWAYS, "\nFile Permission Error: "
				"other writable bit is not allowed for \"%s\" "
				"due to security issues\n", fixedvalue.Value());
		return false;
	}

	// is executable?
	if( !(sbuf.st_mode & S_IXUSR) ) {
		vmprintf(D_ALWAYS, "\nFile Permission Error: "
			"User executable bit is not set for \"%s\"\n", fixedvalue.Value());
		return false;
	}

	// Can read script program?
	if( check_vm_read_access_file(fixedvalue.Value(), true) == false ) {
		return false;
	}
	config->m_vm_script = fixedvalue;

	// Read XEN_DEFAULT_KERNEL (required parameter)
	config_value = vmgahp_param("XEN_DEFAULT_KERNEL");
	if( !config_value ) {
		vmprintf(D_ALWAYS, "\nERROR: You should define the default "
				"kernel for Xen\n");
		return false;
	}
	fixedvalue = delete_quotation_marks(config_value);
	free(config_value);

	// Can read default xen kernel ?
	if( check_vm_read_access_file(fixedvalue.Value(), true) == false ) {
		return false;
	}
	
	// Read XEN_DEFAULT_RAMDISK (optional parameter)
	config_value = vmgahp_param("XEN_DEFAULT_RAMDISK");
	if( config_value ) {
		fixedvalue = delete_quotation_marks(config_value);
		free(config_value);

		// Can read default xen ramdisk ?
		if( check_vm_read_access_file(fixedvalue.Value(), true) == false ) {
			return false;
		}
	}

	// Read XEN_BOOTLOADER (required parameter)
	config_value = vmgahp_param("XEN_BOOTLOADER");
	if( !config_value ) {
		vmprintf(D_ALWAYS, "\nERROR: You should define Xen bootloader\n");
		return false;
	}
	fixedvalue = delete_quotation_marks(config_value);
	free(config_value);

	// Can read xen boot loader ?
	if( check_vm_read_access_file(fixedvalue.Value(), true) == false ) {
		vmprintf(D_ALWAYS, "Xen bootloader file '%s' cannot be read\n", 
				fixedvalue.Value());
		return false;
	}
	return true;
}

bool 
XenType::testXen(VMGahpConfig* config)
{
	if( !config ) {
		return false;
	}

	if( XenType::checkXenParams(config) == false ) {
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(config->m_vm_script);
	systemcmd.AppendArg("check");

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		return false;
	}

	return true;
}

void
XenType::makeNameofSuspendfile(MyString& name)
{
	name.sprintf("%s%c%s.mem.ckpt", m_workingpath.Value(), 
			DIR_DELIM_CHAR, m_vmtype.Value());
}

// This function compares the timestamp of given file with 
// that of writable disk files.
// This function should be called after parseXenDiskParam()
bool
XenType::checkCkptSuspendFile(const char* file)
{
	if( !file || file[0] == '\0' ) {
		return false;
	}

	// Read timestamp file 
	MyString timestampfile(file);
	timestampfile += XEN_CKPT_TIMESTAMP_FILE_SUFFIX;

	FILE *fp = safe_fopen_wrapper(timestampfile.Value(), "r");
	if( !fp ) {
		vmprintf(D_ALWAYS, "Cannot find the timestamp file\n");
		return false;
	}

	char buffer[128];
	if( fgets(buffer, sizeof(buffer), fp) == NULL ) {
		fclose(fp);
		vmprintf(D_ALWAYS, "Invalid timestamp file\n");
		return false;
	}
	fclose(fp);

	MyString tmp_str;
	tmp_str = buffer;
	if( tmp_str.IsEmpty() ) {
		vmprintf(D_ALWAYS, "Invalid timestamp file\n");
		return false;
	}
	tmp_str.chomp();
	tmp_str.trim();

	time_t timestamp; 
	timestamp = (time_t)atoi(tmp_str.Value());
	if( timestamp <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid timestamp file\n");
		return false;
	}

	XenDisk *vdisk = NULL;
	m_disk_list.Rewind();
	while( m_disk_list.Next(vdisk) ) {
		if( !strcasecmp(vdisk->permission.Value(), "w") || 
				!strcasecmp(vdisk->permission.Value(), "rw")) {
			// this is a writable disk file
			// get modification time of this disk file
			time_t disk_mtime;
			struct stat disk_stat;
			if( stat(vdisk->filename.Value(), &disk_stat ) < 0 ) {
				vmprintf(D_ALWAYS, "\nERROR: Failed to access \"%s\":%s\n", 
						vdisk->filename.Value(), strerror(errno));
				return false;
			}
			disk_mtime = disk_stat.st_mtime;

			// compare  
			if( disk_mtime != timestamp ) {
				vmprintf(D_ALWAYS, "Checkpoint timestamp mismatch: "
						"timestamp of suspend file=%d, mtime of disk file=%d\n", 
						(int)timestamp, (int)disk_mtime);
				return false;
			}
		}
	}

	return true;
}

bool
XenType::findCkptConfigAndSuspendFile(MyString &vmconfig, MyString &suspendfile)
{
	if( m_transfer_intermediate_files.isEmpty() ) {
		return false;
	}

	vmconfig = "";
	suspendfile = "";

	MyString tmpconfig;
	MyString tmpsuspendfile;

	if( filelist_contains_file( XEN_CONFIG_FILE_NAME, 
				&m_transfer_intermediate_files, true) ) {
		// There is a vm config file for checkpointed files
		tmpconfig.sprintf("%s%c%s",m_workingpath.Value(), 
				DIR_DELIM_CHAR, XEN_CONFIG_FILE_NAME);
	}

	MyString tmp_name;
	makeNameofSuspendfile(tmp_name);

	if( filelist_contains_file(tmp_name.Value(), 
				&m_transfer_intermediate_files, true) ) {
		// There is a suspend file that was created during vacate
		tmpsuspendfile = tmp_name;
		if( check_vm_read_access_file(tmpsuspendfile.Value(), true) == false) {
			return false;
		}
	}

	if( (tmpconfig.Length() > 0) && 
			(tmpsuspendfile.Length() > 0 )) {
		// check if the timestamp of suspend file is same to 
		// that of writable disk files
		// If timestamp differs between suspend file and writable disk file,
		// it means that there was a crash.
		// So we need to restart this VM job from the beginning.
		if( checkCkptSuspendFile(tmpsuspendfile.Value()) ) {
			vmconfig = tmpconfig;
			suspendfile = tmpsuspendfile;
			return true;
		}
	}
	return false;
}

bool
XenType::killVM()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::killVM\n");

	if( (m_scriptname.Length() == 0 ) || 
			( m_vm_name.Length() == 0 ) ) {
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	return killVMFast(m_scriptname.Value(), m_vm_name.Value());
}

bool
XenType::killVMFast(const char* script, const char* vmname)
{
	vmprintf(D_FULLDEBUG, "Inside XenType::killVMFast\n");
	
	if( !script || (script[0] == '\0') || 
			!vmname || (vmname[0] == '\0') ) {
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(script);
	systemcmd.AppendArg("killvm");
	systemcmd.AppendArg(vmname);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		return false;
	}
	return true;
}
