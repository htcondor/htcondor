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

#define XEN_CONFIG_FILE_NAME "xen_vm.config"
#define XEN_CKPT_TIMESTAMP_FILE_SUFFIX ".timestamp"
#define XEN_MEM_SAVED_FILE "xen.mem.ckpt"
#define XEN_CKPT_TIMESTAMP_FILE XEN_MEM_SAVED_FILE XEN_CKPT_TIMESTAMP_FILE_SUFFIX

#define XEN_LOCAL_SETTINGS_PARAM "XEN_LOCAL_SETTINGS_FILE"
#define XEN_LOCAL_VT_SETTINGS_PARAM "XEN_LOCAL_VT_SETTINGS_FILE"

static MyString
getScriptErrorString(const char* fname)
{
	MyString err_msg;
	FILE *file = NULL;
	char buffer[1024];
	file = safe_fopen_wrapper(fname, "r");

	if( !file ) {
		err_msg = VMGAHP_ERR_INTERNAL;
		return err_msg;
	}

	memset(buffer, 0, sizeof(buffer));
	while( fgets(buffer, sizeof(buffer), file) != NULL ) {
		err_msg += buffer;
		memset(buffer, 0, sizeof(buffer));
	}
	fclose(file);
	return err_msg;
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
	priv_state old_priv = set_user_priv();
	Shutdown();
	set_priv( old_priv );

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

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() != VM_STOPPED ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
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

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("start");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, true, &cmd_out);
	if( result != 0 ) {
		// Read error output
		// TODO Should we grab more than just the first line?
		cmd_out.rewind();
		m_result_msg = cmd_out.next();
		return false;
	}

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

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
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

				// delete the created xen vm configuration file
				if( !m_configfile.IsEmpty() ) {
					unlink(m_configfile.Value());
				}

				// delete the checkpoint timestamp file
				MyString tmpfilename;
				tmpfilename.sprintf("%s%c%s", m_workingpath.Value(),
						DIR_DELIM_CHAR, XEN_CKPT_TIMESTAMP_FILE);
				unlink(tmpfilename.Value());

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

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_STOPPED ) {
		vmprintf(D_ALWAYS, "Checkpoint is called for a stopped VM\n");
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	if( !m_vm_checkpoint ) {
		vmprintf(D_ALWAYS, "Checkpoint is not supported.\n");
		m_result_msg = VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT;
		return false;
	}

	if( m_xen_hw_vt && !m_allow_hw_vt_suspend ) {
		// This VM uses hardware virtualization.
		// However, Xen cannot suspend this type of VM yet.
		// So we cannot checkpoint this VM.
		vmprintf(D_ALWAYS, "Checkpoint of Hardware VT is not supported.\n");
		m_result_msg = VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT;
		return false;
	}

	// If a VM is soft suspended, resume it first
	ResumeFromSoftSuspend();

	// This function cause a running VM to be suspended.
	if( createCkptFiles() == false ) { 
		m_result_msg = VMGAHP_ERR_VM_CANNOT_CREATE_CKPT_FILES;
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

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( m_is_soft_suspended ) {
		return true;
	}

	if( getVMStatus() != VM_RUNNING ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
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

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_SUSPENDED ) {
		return true;
	}

	if( getVMStatus() != VM_RUNNING ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	if( m_xen_hw_vt && !m_allow_hw_vt_suspend ) {
		// This VM uses hardware virtualization.
		// However, Xen cannot suspend this type of VM yet.
		m_result_msg = VMGAHP_ERR_VM_NO_SUPPORT_SUSPEND;
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
		m_result_msg = VMGAHP_ERR_CRITICAL;
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

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	if( getVMStatus() == VM_RUNNING ) {
		return true;
	}

	if( !m_restart_with_ckpt && 
			( getVMStatus() != VM_SUSPENDED) ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}
	m_restart_with_ckpt = false;

	m_is_checkpointed = false;

	if( check_vm_read_access_file(m_suspendfile.Value(), true) == false ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_SUSPEND_FILE;
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("resume");
	systemcmd.AppendArg(m_suspendfile);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		m_result_msg = VMGAHP_ERR_CRITICAL;
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

	if( (m_scriptname.Length() == 0) ||
		(m_configfile.Length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
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

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_scriptname);
	if( m_vm_networking ) {
		systemcmd.AppendArg("getvminfo");
	}else {
		systemcmd.AppendArg("status");
	}
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		m_result_msg = VMGAHP_ERR_CRITICAL;
		return false;
	}

	// Got result
	cmd_out.rewind();

	const char *next_line;
	MyString one_line;
	MyString name;
	MyString value;

	MyString vm_status;
	float cputime = 0;

	while( (next_line = cmd_out.next()) != NULL ) {
		one_line = next_line;
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
			continue;
		}
		if( !strcasecmp(name.Value(), VMGAHP_STATUS_COMMAND_CPUTIME) ) {
			cputime = (float)strtod(value.Value(), (char **)NULL);
			if( cputime <= 0 ) {
				cputime = 0;
			}
			continue;
		}
		if( m_vm_networking ) {
			if( !strcasecmp(name.Value(), VMGAHP_STATUS_COMMAND_MAC) ) {
				m_vm_mac = value;
				continue;
			}
			if( !strcasecmp(name.Value(), VMGAHP_STATUS_COMMAND_IP) ) {
				m_vm_ip = value;
				continue;
			}
		}
	}

	if( !vm_status.Length() ) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	m_result_msg = "";

	if( m_vm_networking ) {
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
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}
	return false;
}

bool
XenType::CreateVirshConfigFile(const char* filename)
{
	MyString disk_string;
	char* config_value = NULL;
	MyString bridge_script;

	if( !filename ) {
		return false;
	}

	FILE *fp = NULL;

	fp = safe_fopen_wrapper(filename, "w");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper vm config file "
				"in write mode: safe_fopen_wrapper(%s) returns %s\n", 
				filename, strerror(errno));
		return false;
	}

	config_value = param( "XEN_BRIDGE_SCRIPT" );
	if( !config_value ) {
		vmprintf(D_ALWAYS, "XEN_BRIDGE_SCRIPT is not defined in the "
				 "vmgahp config file\n");

			// I'm not so sure this should be required. The problem
			// with it being missing/wrong is that a job expecting to
			// have un-nat'd network access might not get it. There's
			// no way for us to tell if the script given is correct
			// though, so this error might just be better as a warning
			// in the log. If this is turned into a warning an EXCEPT
			// must be removed when 'bridge_script' is used below.
			//  - matt 17 oct 2007
			//return false;
	} else {
		bridge_script = config_value;
		free(config_value); 
		config_value = NULL;
	}

	if( fprintf(fp, "<domain type='xen'>") < 0 ) {
		goto virshwriteerror;
	}

	if( fprintf(fp, "<name>%s</name>", m_vm_name.Value()) < 0 ) {
		goto virshwriteerror;
	}

	if( fprintf(fp, "<memory>%d</memory>", m_vm_mem) < 0 ) {
		goto virshwriteerror;
	}
	if( fprintf(fp, "<vcpu>%d</vcpu>", m_vcpus) < 0 ) {
	  goto virshwriteerror;
	}

	if( fprintf(fp, "<os>") < 0 ) {
		goto virshwriteerror;
	}

	if( fprintf(fp, "<type>linux</type>") < 0 ) {
		goto virshwriteerror;
	}

	if( m_xen_kernel_file.IsEmpty() == false ) {
		MyString tmp_fullname;

		if( fprintf(fp, "<kernel>") < 0 ) {
			goto virshwriteerror;
		}
		if( isTransferedFile(m_xen_kernel_file.Value(), 
					tmp_fullname) ) {
			// this file is transferred
			// we need a full path
			m_xen_kernel_file = tmp_fullname;
		}
		if( fprintf(fp, "%s", m_xen_kernel_file.Value()) < 0 ) {
			goto virshwriteerror;
		}
		if( fprintf(fp, "</kernel>") < 0 ) {
			goto virshwriteerror;
		}

		if( m_xen_initrd_file.IsEmpty() == false ) {
			if( fprintf(fp, "<initrd>") < 0 ) {
				goto virshwriteerror;
			}
			if( isTransferedFile(m_xen_initrd_file.Value(), 
						tmp_fullname) ) {
				// this file is transferred
				// we need a full path
				m_xen_initrd_file = tmp_fullname;
			}
			if( fprintf(fp, "%s", m_xen_initrd_file.Value()) < 0 ) {
				goto virshwriteerror;
			}
			if( fprintf(fp, "</initrd>") < 0 ) {
				goto virshwriteerror;
			}
		}
		if( m_xen_root.IsEmpty() == false ) {
			if( fprintf(fp,"<root>%s</root>", m_xen_root.Value()) < 0 ) {
				goto virshwriteerror;
			}
		}

		if( m_xen_kernel_params.IsEmpty() == false ) {
			if( fprintf(fp,"<cmdline>%s</cmdline>", 
						m_xen_kernel_params.Value()) < 0 ) {
				goto virshwriteerror;
			}
		}
	}


	if( fprintf(fp, "</os>") < 0 ) {
		goto virshwriteerror;
	}

	if( strcasecmp(m_xen_kernel_submit_param.Value(), XEN_KERNEL_INCLUDED) == 0) {
		if( fprintf(fp, "<bootloader>%s</bootloader>", m_xen_bootloader.Value()) < 0 ) {
			goto virshwriteerror;
		}
	}

	if( fprintf(fp, "<devices>") < 0 ) {
		goto virshwriteerror;
	}

	if( m_vm_networking ) {
		if( m_vm_networking_type.find("nat") >= 0 ) {
			if( fprintf(fp, "<interface type='network'>"
						"<source network='default'/>"
						"</interface>") < 0 ) {
				goto virshwriteerror;
			}
		} else {
			if( fprintf(fp, "<interface type='bridge'>") < 0 ) {
				goto virshwriteerror;
			}
			if (!bridge_script.IsEmpty()) {
				if( fprintf(fp, "<script path='%s'/>",
							bridge_script.Value()) < 0 ) {
					goto virshwriteerror;
				}
			}
			if(!m_vm_job_mac.IsEmpty())
			  {
			    if(fprintf(fp, "<mac address=\'%s\'/>", m_vm_job_mac.Value()) < 0) {
			      goto virshwriteerror;
			    }
			  }
			if( fprintf(fp, "</interface>") < 0 ) {
				goto virshwriteerror;
			}
		}
	}

	// Create disk parameter in Xen config file
	disk_string = makeVirshDiskString();
	if( fprintf(fp,"%s", disk_string.Value()) < 0 ) {
		goto virshwriteerror;
	}

	if( fprintf(fp, "</devices>") < 0 ) {
		goto virshwriteerror;
	}

	if( fprintf(fp, "</domain>") < 0 ) {
		goto virshwriteerror;
	}

	if (!write_local_settings_from_file(fp, XEN_LOCAL_SETTINGS_PARAM)) {
		goto virshwriteerror;
	}

	fclose(fp);
	fp = NULL;

	if( m_use_script_to_create_config ) {
		// We will call the script program 
		// to create a configuration file for VM
		
		if( createConfigUsingScript(filename) == false ) {
			unlink(filename);
			return false;
		}
	}

	return true;

virshwriteerror:
	vmprintf(D_ALWAYS, "failed to fprintf in CreateVirshConfigFile(%s:%s)\n",
			filename, strerror(errno));
	if( fp ) {
		fclose(fp);
	}
	unlink(filename);
	return false;
}

bool
XenType::CreateXenVMCofigFile(const char* filename)
{
	if( !filename ) {
		return false;
	}

	return CreateVirshConfigFile(filename);
}

bool
XenType::CreateConfigFile()
{
	char *config_value = NULL;

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
	if( m_classAd.LookupString( VMPARAM_XEN_KERNEL, m_xen_kernel_submit_param) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
							VMPARAM_XEN_KERNEL);
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_NO_KERNEL_PARAM;
		return false;
	}
	m_xen_kernel_submit_param.trim();

	if(strcasecmp(m_xen_kernel_submit_param.Value(), XEN_KERNEL_ANY) == 0) {
		vmprintf(D_ALWAYS, "VMGahp will use default xen kernel\n");
		config_value = param( "XEN_DEFAULT_KERNEL" );
		if( !config_value ) {
			vmprintf(D_ALWAYS, "Default xen kernel is not defined "
					"in vmgahp config file\n");
			m_result_msg = VMGAHP_ERR_CRITICAL;
			return false;
		}else {
			m_xen_kernel_file = delete_quotation_marks(config_value);
			free(config_value);

			config_value = param( "XEN_DEFAULT_INITRD" );
			if( config_value ) {
				m_xen_initrd_file = delete_quotation_marks(config_value);
				free(config_value);
			}
		}
	}else if(strcasecmp(m_xen_kernel_submit_param.Value(), XEN_KERNEL_INCLUDED) == 0) {
		vmprintf(D_ALWAYS, "VMGahp will use xen bootloader\n");
		config_value = param( "XEN_BOOTLOADER" );
		if( !config_value ) {
			vmprintf(D_ALWAYS, "xen bootloader is not defined "
					"in vmgahp config file\n");
			m_result_msg = VMGAHP_ERR_CRITICAL;
			return false;
		}else {
			m_xen_bootloader = delete_quotation_marks(config_value);
			free(config_value);
		}
	}else if(strcasecmp(m_xen_kernel_submit_param.Value(), XEN_KERNEL_HW_VT) == 0) {
		vmprintf(D_ALWAYS, "This VM requires hardware virtualization\n");
		if( !m_vm_hardware_vt ) {
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_MISMATCHED_HARDWARE_VT;
			return false;
		}
		m_xen_hw_vt = true;
		m_allow_hw_vt_suspend = 
			param_boolean("XEN_ALLOW_HARDWARE_VT_SUSPEND", false);
	}else {
		// A job user defined a customized kernel
		// make sure that the file for xen kernel is readable
		if( check_vm_read_access_file(m_xen_kernel_submit_param.Value(), false) == false) {
			vmprintf(D_ALWAYS, "xen kernel file '%s' cannot be read\n", 
					m_xen_kernel_submit_param.Value());
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_KERNEL_NOT_FOUND;
			return false;
		}
		m_xen_kernel_file = m_xen_kernel_submit_param;

		if( m_classAd.LookupString(VMPARAM_XEN_INITRD, m_xen_initrd_file) 
					== 1 ) {
			// A job user defined a customized ramdisk
			m_xen_initrd_file.trim();
			if( check_vm_read_access_file(m_xen_initrd_file.Value(), false) == false) {
				// make sure that the file for xen ramdisk is readable
				vmprintf(D_ALWAYS, "xen ramdisk file '%s' cannot be read\n", 
						m_xen_initrd_file.Value());
				m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_INITRD_NOT_FOUND;
				return false;
			}
		}
	}

	if( m_xen_kernel_file.IsEmpty() == false ) {
		// Read the parameter of Xen Root
		if( m_classAd.LookupString(VMPARAM_XEN_ROOT, m_xen_root) != 1 ) {
			vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
					VMPARAM_XEN_ROOT);
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_NO_ROOT_DEVICE_PARAM;
			return false;
		}
		m_xen_root.trim();
	}

	MyString xen_disk;
	// Read the parameter of Xen Disk
	if( m_classAd.LookupString(VMPARAM_XEN_DISK, xen_disk) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
				VMPARAM_XEN_DISK);
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_NO_DISK_PARAM;
		return false;
	}
	xen_disk.trim();
	if( parseXenDiskParam(xen_disk.Value()) == false ) {
		vmprintf(D_ALWAYS, "xen disk format(%s) is incorrect\n", 
				xen_disk.Value());
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_INVALID_DISK_PARAM;
		return false;
	}

	// Read the parameter of Xen Kernel Param
	if( m_classAd.LookupString(VMPARAM_XEN_KERNEL_PARAMS, m_xen_kernel_params) == 1 ) {
		m_xen_kernel_params.trim();
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
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_NO_CDROM_DEVICE;
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
			m_result_msg = VMGAHP_ERR_CANNOT_CREATE_ISO_FILE;
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
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_MISMATCHED_CHECKPOINT;
			return false;
		}
	}


	// create a vm config file
	MyString tmp_config_name;
	tmp_config_name.sprintf("%s%c%s",m_workingpath.Value(), 
			DIR_DELIM_CHAR, XEN_CONFIG_FILE_NAME);

	if( CreateXenVMCofigFile(tmp_config_name.Value()) 
			== false ) {
		m_result_msg = VMGAHP_ERR_CRITICAL;
		return false;
	}

	// set vm config file
	m_configfile = tmp_config_name.Value();
	return true;
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
			if( check_vm_write_access_file(disk_file.Value(), false) == false ) {
				vmprintf(D_ALWAYS, "xen disk image file('%s') cannot be modified\n",
					   	disk_file.Value());
				return false;
			}
		}else {
			// check if this disk file is readable 
			if( check_vm_read_access_file(disk_file.Value(), false) == false ) {
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
XenType::makeVirshDiskString(void)
{
	MyString xendisk;
	xendisk = "";
	bool first_disk = true;

	XenDisk *vdisk = NULL;
	m_disk_list.Rewind();
	while( m_disk_list.Next(vdisk) ) {
		if( !first_disk ) {
			xendisk += "</disk>";
		}
		first_disk = false;
		xendisk += "<disk type='file'>";
		xendisk += "<source file='";
		xendisk += vdisk->filename;
		xendisk += "'/>";
		xendisk += "<target dev='";
		xendisk += vdisk->device;
		xendisk += "'/>";

		if (vdisk->permission == "r") {
			xendisk += "<readonly/>";
		}
	}
	xendisk += "</disk>";

	if( m_iso_file.IsEmpty() == false ) {
		xendisk += "<disk type='file' device='cdrom'>";
		xendisk += "<source file='";
		xendisk += m_iso_file;
		xendisk += "'/>";
		xendisk += "<target dev='";
		xendisk += m_xen_cdrom_device;
		xendisk += "'/>";
		xendisk += "<readonly/>";
		xendisk += "</disk>";
	}

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
	config_value = param("XEN_SCRIPT");
	if( !config_value ) {
		vmprintf(D_ALWAYS,
		         "\nERROR: 'XEN_SCRIPT' not defined in configuration\n");
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
	config_value = param("XEN_DEFAULT_KERNEL");
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
	
	// Read XEN_DEFAULT_INITRD (optional parameter)
	config_value = param("XEN_DEFAULT_INITRD");
	if( config_value ) {
		fixedvalue = delete_quotation_marks(config_value);
		free(config_value);

		// Can read default xen ramdisk ?
		if( check_vm_read_access_file(fixedvalue.Value(), true) == false ) {
			return false;
		}
	}

	// Read XEN_BOOTLOADER (required parameter)
	config_value = param("XEN_BOOTLOADER");
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
	name.sprintf("%s%c%s", m_workingpath.Value(), DIR_DELIM_CHAR, 
			XEN_MEM_SAVED_FILE);
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

bool
XenType::createISO()
{
	vmprintf(D_FULLDEBUG, "Inside XenType::createISO\n");

	m_iso_file = "";

	if( m_scriptname.IsEmpty() || m_vm_cdrom_files.isEmpty() ) {
		return false;
	}

	MyString tmp_config;
	MyString tmp_file;
	if( createISOConfigAndName(&m_vm_cdrom_files, tmp_config, 
				tmp_file) == false ) {
		return false;
	}

	ArgList systemcmd;
	if( m_prog_for_script.IsEmpty() == false ) {
		systemcmd.AppendArg(m_prog_for_script);
	}
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("createiso");
	systemcmd.AppendArg(tmp_config);
	systemcmd.AppendArg(tmp_file);

	int result = systemCommand(systemcmd, true);
	if( result != 0 ) {
		return false;
	}

#if defined(LINUX)	
	// To avoid lazy-write behavior to disk
	sync();
#endif

	unlink(tmp_config.Value());
	m_iso_file = tmp_file;
	m_local_iso = true;

	// Insert the name of created iso file to classAd for future use
	m_classAd.Assign("VMPARAM_ISO_NAME", condor_basename(m_iso_file.Value()));
	return true;
}
