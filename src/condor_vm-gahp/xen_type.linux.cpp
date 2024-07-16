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
#include "basename.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_mkstemp.h"
#include "util_lib_proto.h"
#include "vmgahp_common.h"
#include "vm_type.h"
#include "xen_type.linux.h"
#include "vmgahp_error_codes.h"
#include "condor_vm_universe_types.h"
#include "my_popen.h"
#include <string>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#define XEN_CONFIG_FILE_NAME "xen_vm.config"
#define XEN_CKPT_TIMESTAMP_FILE_SUFFIX ".timestamp"
#define XEN_MEM_SAVED_FILE "xen.mem.ckpt"
#define XEN_CKPT_TIMESTAMP_FILE XEN_MEM_SAVED_FILE XEN_CKPT_TIMESTAMP_FILE_SUFFIX

extern VMGahp *vmgahp;

VirshType::VirshType(const char* workingpath,ClassAd* ad) : VMType("", "none", workingpath, ad)
{
	m_cputime_before_suspend = 0;
	m_xen_hw_vt = false;
	m_allow_hw_vt_suspend = false;
	m_restart_with_ckpt = false;
	m_has_transferred_disk_file = false;
	m_libvirt_connection = NULL;
}

VirshType::~VirshType()
{
	priv_state old_priv = set_user_priv();
	Shutdown();
	set_priv( old_priv );

	if( getVMStatus() != VM_STOPPED ) {
		// To make sure VM exits
		killVM();
	}
	setVMStatus(VM_STOPPED);

	for (auto disk : m_disk_list) {
		delete disk;
	}
	m_disk_list.clear();
}

void
VirshType::Config()
{
	char *config_value = NULL;

	config_value = param("VM_NETWORKING_BRIDGE_INTERFACE");
	if( config_value ) {
		m_vm_bridge_interface = delete_quotation_marks(config_value).c_str();
		free(config_value);
	} else if(contains(vmgahp->m_gahp_config->m_vm_networking_types, "bridge") == true) {
		vmprintf( D_ALWAYS, "ERROR: 'VM_NETWORKING_TYPE' contains "
				"'bridge' but VM_NETWORKING_BRIDGE_INTERFACE "
				"isn't defined, so 'bridge' "
				"networking is disabled\n");
		std::erase(vmgahp->m_gahp_config->m_vm_networking_types, "bridge");
		if( vmgahp->m_gahp_config->m_vm_networking_types.empty() ) {
			vmprintf( D_ALWAYS, "ERROR: 'VM_NETWORKING' is true "
					"but 'VM_NETWORKING_TYPE' contains "
					"no valid entries, so 'VM_NETWORKING' "
					"is disabled\n");
			vmgahp->m_gahp_config->m_vm_networking = false;
		} else {
			vmprintf( D_ALWAYS,
					"Setting default networking type to 'nat'\n");
			vmgahp->m_gahp_config->m_vm_default_networking_type = "nat";
		}
	}
}

bool
VirshType::Start()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::Start\n");

	if( (m_configfile.length() == 0)) {

	        m_result_msg = VMGAHP_ERR_INTERNAL;
		vmprintf(D_FULLDEBUG, "Config file was not set configfile: %s\n", m_configfile.c_str());
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
			m_start_time = time(NULL);
			return true;
		}else {
			// Failed to restart with checkpointed files
			vmprintf(D_ALWAYS, "Failed to restart with checkpointed files\n");
			vmprintf(D_ALWAYS, "So, we will try to create a new configuration file\n");

			deleteNonTransferredFiles();
			m_configfile = "";
			m_suspendfile = "";
			m_restart_with_ckpt = false;

			if( this->CreateConfigFile() == false ) {
				vmprintf(D_ALWAYS, "Failed to create a new configuration files\n");
				return false;
			}

			// Succeeded to create a configuration file
			// Keep going..
		}
	}
	vmprintf(D_FULLDEBUG, "Trying XML: %s\n", m_xml.c_str());
	priv_state priv = set_root_priv();
	virDomainPtr vm = virDomainCreateXML(m_libvirt_connection, m_xml.c_str(), 0);
	set_priv(priv);

	if(vm == NULL)
	  {
	    // Error in creating the vm; let's find out what the error
	    // was
	    virErrorPtr err = virConnGetLastError(m_libvirt_connection);
	    vmprintf(D_ALWAYS, "Failed to create libvirt domain: %s\n", (err ? err->message : "No reason found"));

	    if (err && err->message && (strstr(err->message, "image is not in qcow2 format") != NULL)) {
			m_result_msg = VMGAHP_ERR_BAD_IMAGE;
		}

	    //virFreeError(err);
	    return false;
	  }


	priv = set_root_priv();
	virDomainFree(vm);
	set_priv(priv);

	setVMStatus(VM_RUNNING);
	m_start_time = time(NULL);
	m_cpu_time = 0;

	// Here we manually update timestamp of all writable disk files
	return true;
}

bool
VirshType::Shutdown()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::Shutdown\n");

	if( (m_configfile.length() == 0) ) {
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
				if( !m_suspendfile.empty() ) {
					unlink(m_suspendfile.c_str());
				}
				m_suspendfile = "";

				// delete the created xen vm configuration file
				if( !m_configfile.empty() ) {
					unlink(m_configfile.c_str());
				}

				// delete the checkpoint timestamp file
				std::string tmpfilename;
				formatstr(tmpfilename, "%s%c%s", m_workingpath.c_str(),
						DIR_DELIM_CHAR, XEN_CKPT_TIMESTAMP_FILE);
				unlink(tmpfilename.c_str());

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
		priv_state priv = set_root_priv();
                virDomainPtr dom = virDomainLookupByName(m_libvirt_connection, m_vm_name.c_str());
		set_priv(priv);
		if(dom == NULL)
		  {
		    virErrorPtr err = virConnGetLastError(m_libvirt_connection);
		    if (err && err->code != VIR_ERR_NO_DOMAIN)
		      {
			vmprintf(D_ALWAYS, "Error finding domain %s: %s\n", m_vm_name.c_str(), err->message);
			return false;
		      }
		  }
		else
		  {
		    priv = set_root_priv();
		    int result = virDomainShutdown(dom);
		    virDomainFree(dom);
		    set_priv(priv);
		    if( result != 0 ) {
			    // system error happens
			    // killing VM by force
			    killVM();
		    }
		  }
		if ( m_vm_no_output_vm  ) {
			// Now we don't need working files any more
			m_delete_working_files = true;
			m_is_checkpointed = false;
		}
	}

	setVMStatus(VM_STOPPED);
	m_stop_time = time(NULL);
	return true;
}

bool
VirshType::Checkpoint()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::Checkpoint\n");

	if( (m_configfile.length() == 0)) {
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
		// However, Virsh cannot suspend this type of VM yet.
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

// I really need a good way to determine the type of a classad
// attribute.  Right now I just try all four possibilities, which is a
// horrible mess...
bool VirshType::CreateVirshConfigFile(const char*  /*filename*/)
{
  vmprintf(D_FULLDEBUG, "In VirshType::CreateVirshConfigFile\n");
  //  std::string name;
  char * tmp = param("LIBVIRT_XML_SCRIPT");
  if(tmp == NULL)
    {
      vmprintf(D_ALWAYS, "LIBVIRT_XML_SCRIPT not defined\n");
      return false;
    }
  // This probably needs some work...
  ArgList args;
  args.AppendArg(tmp);
  free(tmp);

  // We might want to have specific debugging output enabled in the
  // helper script; however, it is not clear where that output should
  // go.  This gives us a way to do so even in cases where the script
  // is unable to read from condor_config (why would this ever
  // happen?)
  tmp = param("LIBVIRT_XML_SCRIPT_ARGS");
  if(tmp != NULL)
    {
      std::string errormsg;
      if (!args.AppendArgsV1RawOrV2Quoted(tmp, errormsg)) {
		vmprintf(D_ALWAYS, "Cannot parse LIBVIRT_XML_SCRIPT_ARGS: %s\n", tmp);
	  }
      free(tmp);
    }
  std::vector<std::string> input_strings, output_strings, error_strings;
  std::string classad_string;
  sPrintAd(classad_string, m_classAd);
  classad_string += VMPARAM_XEN_BOOTLOADER;
  classad_string += " = \"";
  classad_string += m_xen_bootloader;
  classad_string += "\"\n";
  if(classad_string.find(VMPARAM_XEN_INITRD) == std::string::npos)
    {
      classad_string += VMPARAM_XEN_INITRD;
      classad_string += " = \"";
      classad_string += m_xen_initrd_file;
      classad_string += "\"\n";
    }
  if(!m_vm_bridge_interface.empty())
    {
      classad_string += VMPARAM_BRIDGE_INTERFACE;
      classad_string += " = \"";
      classad_string += m_vm_bridge_interface;
      classad_string += "\"\n";
    }
  if(classad_string.find(ATTR_JOB_VM_NETWORKING_TYPE) == std::string::npos)
    {
      classad_string += ATTR_JOB_VM_NETWORKING_TYPE;
      classad_string += " = \"";
      classad_string += m_vm_networking_type;
      classad_string += "\"\n";
    }
  input_strings.emplace_back(classad_string);
  
  std::string buff = join(input_strings, ",");
  vmprintf(D_FULLDEBUG, "LIBVIRT_XML_SCRIPT_ARGS input_strings= %s\n", buff.c_str());

  int ret = systemCommand(args, PRIV_ROOT, &output_strings, &input_strings, &error_strings, false);
  if(ret != 0)
    {
      vmprintf(D_ALWAYS, "XML helper script could not be executed\n");
      // If there is any output from the helper, write it to the debug
      // log.  Presumably, this is separate from the script's own
      // debug log.
      for(const auto& line : error_strings)
	{
	  vmprintf(D_FULLDEBUG, "Helper stderr output: %s\n", line.c_str());
	}
      return false;
    }
  for(const auto& line : error_strings)
    {
      vmprintf(D_ALWAYS, "Helper stderr output: %s\n", line.c_str());
    }
  for(const auto& line : output_strings)
    {
      m_xml += line;
    }
  return true;
}

bool
VirshType::ResumeFromSoftSuspend(void)
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::ResumeFromSoftSuspend\n");
	if( (m_configfile.length() == 0)) {
		return false;
	}

	if( m_is_soft_suspended ) {

		priv_state priv = set_root_priv();
		virDomainPtr dom = virDomainLookupByName(m_libvirt_connection, m_vm_name.c_str());
		set_priv(priv);
		if(dom == NULL)
		  {
		    virErrorPtr err = virConnGetLastError(m_libvirt_connection);
		    vmprintf(D_ALWAYS, "Error finding domain %s: %s\n", m_vm_name.c_str(), (err ? err->message : "No reason found"));
		    return false;
		  }

		priv = set_root_priv();
		int result = virDomainResume(dom);
		virDomainFree(dom);
		set_priv(priv);
		if( result != 0 ) {
			// unpause failed.
			vmprintf(D_ALWAYS, "Unpausing VM failed in "
					"VirshType::ResumeFromSoftSuspend\n");
			return false;
		}
		m_is_soft_suspended = false;
	}
	return true;
}

bool
VirshType::SoftSuspend()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::SoftSuspend\n");

	if( (m_configfile.length() == 0)) {
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

	priv_state priv = set_root_priv();
	virDomainPtr dom = virDomainLookupByName(m_libvirt_connection, m_vm_name.c_str());
	set_priv(priv);
	if(dom == NULL)
	  {
	    virErrorPtr err = virConnGetLastError(m_libvirt_connection);
	    vmprintf(D_ALWAYS, "Error finding domain %s: %s\n", m_vm_name.c_str(), (err ? err->message : "No reason found"));
	    return false;
	  }

	int result = virDomainSuspend(dom);
	virDomainFree(dom);
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
VirshType::Suspend()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::Suspend\n");

	if( (m_configfile.length() == 0) ) {
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
		// However, Virsh cannot suspend this type of VM yet.
		m_result_msg = VMGAHP_ERR_VM_NO_SUPPORT_SUSPEND;
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	std::string tmpfilename;
	makeNameofSuspendfile(tmpfilename);
	unlink(tmpfilename.c_str());

	priv_state priv = set_root_priv();
	virDomainPtr dom = virDomainLookupByName(m_libvirt_connection, m_vm_name.c_str());
	set_priv(priv);
	if(dom == NULL)
	  {
	    virErrorPtr err = virConnGetLastError(m_libvirt_connection);
	    vmprintf(D_ALWAYS, "Error finding domain %s: %s\n", m_vm_name.c_str(), (err ? err->message : "No reason found"));
	    return false;
	  }

	priv = set_root_priv();
	int result = virDomainSave(dom, tmpfilename.c_str());
	virDomainFree(dom);
	set_priv(priv);

	if( result != 0 ) {
		unlink(tmpfilename.c_str());
		return false;
	}

	priv = set_root_priv();
	result = chown( tmpfilename.c_str(), get_user_uid(), get_user_gid() );
	set_priv( priv );

	if( result != 0 ) {
		dprintf( D_ALWAYS, "Error changing ownership of checkpoint: %d (%s), failing.\n", errno, strerror( errno ) );
		return false;
	}

	m_suspendfile = tmpfilename;

	setVMStatus(VM_SUSPENDED);
	m_cputime_before_suspend += m_cpu_time;
	m_cpu_time = 0;
	return true;
}

bool
VirshType::Resume()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::Resume\n");

	if((m_configfile.length() == 0)) {
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

	if( check_vm_read_access_file(m_suspendfile.c_str(), true) == false ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_SUSPEND_FILE;
		return false;
	}

	priv_state priv = set_root_priv();
	int result = virDomainRestore(m_libvirt_connection, m_suspendfile.c_str());
	set_priv(priv);

	if( result != 0 ) {
		// Read error output
// 		char *temp = cmd_out.print_to_delimed_string("/");
// 		m_result_msg = temp;
// 		free( temp );
		return false;
	}

	setVMStatus(VM_RUNNING);
	m_cpu_time = 0;

	// Delete suspend file
	unlink(m_suspendfile.c_str());
	m_suspendfile = "";
	return true;
}

bool
VirshType::Status()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::Status\n");

 	if((m_configfile.length() == 0)) {
 		m_result_msg = VMGAHP_ERR_INTERNAL;
 		return false;
 	}

 	m_result_msg = "";

 	if( m_vm_networking ) {
 		if( m_vm_mac.empty() == false ) {
 			if( m_result_msg.empty() == false ) {
 				m_result_msg += " ";
 			}
 			m_result_msg += VMGAHP_STATUS_COMMAND_MAC;
 			m_result_msg += "=";
 			m_result_msg += m_vm_mac;
 		}

 		if( m_vm_ip.empty() == false ) {
 			if( m_result_msg.empty() == false ) {
 				m_result_msg += " ";
 			}
 			m_result_msg += VMGAHP_STATUS_COMMAND_IP;
 			m_result_msg += "=";
 			m_result_msg += m_vm_ip;
 		}
 	}

 	if( m_result_msg.empty() == false ) {
 		m_result_msg += " ";
 	}

 	m_result_msg += VMGAHP_STATUS_COMMAND_STATUS;
 	m_result_msg += "=";

	priv_state priv = set_root_priv();
	virDomainPtr dom = virDomainLookupByName(m_libvirt_connection, m_vm_name.c_str());
	set_priv(priv);
	if(dom == NULL)
	{
		virErrorPtr err = virConnGetLastError(m_libvirt_connection);

	    if ( err )
		{
			switch (err->code)
			{
				case (VIR_ERR_NO_DOMAIN):
					// The VM isn't there anymore, so signal shutdown
					vmprintf(D_FULLDEBUG, "Couldn't find domain %s, assuming it was shutdown\n", m_vm_name.c_str());
					if(getVMStatus() == VM_RUNNING) {
						m_self_shutdown = true;
					}
					if(getVMStatus() != VM_STOPPED) {
						setVMStatus(VM_STOPPED);
						m_stop_time = time(NULL);
					}
					m_result_msg += "Stopped";
					return true;
				break;
				case (VIR_ERR_SYSTEM_ERROR): //
					vmprintf(D_ALWAYS, "libvirt communication error detected, attempting to reconnect...\n");
					this->Connect();
					if ( NULL == ( dom = virDomainLookupByName(m_libvirt_connection, m_vm_name.c_str() ) ) )
					{
						vmprintf(D_ALWAYS, "could not reconnect to libvirt... marking vm as stopped (should exit)\n");
						if(getVMStatus() == VM_RUNNING) {
							m_self_shutdown = true;
						}
						if(getVMStatus() != VM_STOPPED) {
							setVMStatus(VM_STOPPED);
							m_stop_time = time(NULL);
						}
						m_result_msg += "Stopped";
						return true;
					}
					else
					{
						vmprintf(D_ALWAYS, "libvirt has successfully reconnected!\n");
					}
				break;
			default:
				vmprintf(D_ALWAYS, "Error finding domain %s(%d): %s\n", m_vm_name.c_str(), err->code, err->message );
				return false;
			}
		}
		else
		{
			vmprintf(D_ALWAYS, "Error finding domain, no error returned from libvirt\n" );
			return false;
		}

	}

	virDomainInfo _info;
	virDomainInfoPtr info = &_info;
	memset( info, 0, sizeof( virDomainInfo ) );
	if(virDomainGetInfo(dom, info) < 0)
	  {
	    virErrorPtr err = virConnGetLastError(m_libvirt_connection);
	    vmprintf(D_ALWAYS, "Error finding domain info %s: %s\n", m_vm_name.c_str(), (err ? err->message : "No reason found"));
	    return false;
	  }
	if(info->state == VIR_DOMAIN_RUNNING || info->state == VIR_DOMAIN_BLOCKED)
	  {
	    static unsigned long long LastCpuTime = 0;
	    static time_t LastStamp = time(0);

	    time_t CurrentStamp = time(0);
	    unsigned long long CurrentCpuTime = info->cpuTime;
	    double percentUtilization = 1.0;

	    setVMStatus(VM_RUNNING);
	    // libvirt reports cputime in nanoseconds
	    m_cpu_time = info->cpuTime / 1'000'000'000.0;
	    m_result_msg += "Running";

	    if ( (CurrentStamp - LastStamp) > 0 )
	    {
	      // Old calc method because of libvirt version mismatches.
	      // courtesy of http://people.redhat.com/~rjones/virt-top/faq.html#calccpu 
	      percentUtilization = (1.0 * (CurrentCpuTime-LastCpuTime) ) / ((CurrentStamp - LastStamp)*info->nrVirtCpu*1'000'000'000.0);
	      vmprintf(D_FULLDEBUG, "Computing utilization %f = (%llu) / (%d * %d * 1'000'000'000.0)\n",percentUtilization, (CurrentCpuTime-LastCpuTime), (int) (CurrentStamp - LastStamp), info->nrVirtCpu );
	    }

	    formatstr_cat( m_result_msg, " %s=%f",
	                   VMGAHP_STATUS_COMMAND_CPUUTILIZATION,
	                   percentUtilization );

	    formatstr_cat( m_result_msg, " %s=%f",
	                   VMGAHP_STATUS_COMMAND_CPUTIME, m_cpu_time );

	    LastCpuTime = CurrentCpuTime;
	    LastStamp = CurrentStamp;

	    // Memory usage is in kbytes.
	    if( info->memory != 0 ) {
	    	formatstr_cat( m_result_msg, " %s=%lu", VMGAHP_STATUS_COMMAND_MEMORY, info->memory );
	    }

	    if( info->maxMem != 0 ) {
	    	formatstr_cat( m_result_msg, " %s=%lu", VMGAHP_STATUS_COMMAND_MAX_MEMORY, info->maxMem );
	    }

		vmprintf( D_FULLDEBUG, "Reporting status: %s\n", m_result_msg.c_str() );
	    virDomainFree(dom);
	    return true;
	  }
	else if(info->state == VIR_DOMAIN_PAUSED)
	  {
	    m_result_msg += "Suspended";
	    virDomainFree(dom);
	    return true;
	  }
	else
	  {
	    if(getVMStatus() == VM_RUNNING)
	      {
		m_self_shutdown = true;
	      }
	    if(getVMStatus() != VM_STOPPED)
	      {
		setVMStatus(VM_STOPPED);
		m_stop_time = time(NULL);
	      }
	    m_result_msg += "Stopped";
	    virDomainFree(dom);
	    return true;
	  }
	/* UNREACHABLE */
	//virDomainFree(dom);
	//return false;
}

/*
 * Just so that we can get out of the habit of using "goto".
 */
void virshIOError(const char * filename, FILE * fp)
{
	vmprintf(D_ALWAYS, "failed to fprintf in CreateVirshConfigFile(%s:%s)\n",
			filename, strerror(errno));
	if( fp ) {
		fclose(fp);
	}
	unlink(filename);

}

bool KVMType::CreateVirshConfigFile(const char * filename)
{
	std::string disk_string;

	if(!filename) return false;

  // The old way of doing things was to write the XML directly to a
  // file; the new way is to store it in m_xml.

	if (!VirshType::CreateVirshConfigFile(filename))
	{
		vmprintf(D_ALWAYS, "KVMType::CreateVirshConfigFile no XML found, generating defaults\n");

		m_xml += "<domain type='kvm'>";
		m_xml += "<name>";
		m_xml += m_vm_name;
		m_xml += "</name>";
		m_xml += "<memory>";
		m_xml += std::to_string( m_vm_mem * 1024 );
		m_xml += "</memory>";
		m_xml += "<vcpu>";
		m_xml += std::to_string( m_vcpus );
		m_xml += "</vcpu>";
		m_xml += "<os><type>hvm</type></os>";
		m_xml += "<devices>";
		m_xml += "<console type='pty'><source path='/dev/ptmx'/></console>";
		if( m_vm_networking )
			{
			vmprintf(D_FULLDEBUG, "mac address is %s\n", m_vm_job_mac.c_str());
			if( m_vm_networking_type.find("nat") != std::string::npos ) {
			m_xml += "<interface type='network'><source network='default'/></interface>";
			}
			else if( m_vm_networking_type.find("bridge") != std::string::npos )
			{
			m_xml += "<interface type='bridge'>";
			if (!m_vm_bridge_interface.empty()) {
				m_xml += "<source bridge='";
				m_xml += m_vm_bridge_interface;
				m_xml += "'/>";
			}
			if(!m_vm_job_mac.empty())
				{
				m_xml += "<mac address='";
				m_xml += m_vm_job_mac;
				m_xml += "'/>";
				}
			m_xml += "</interface>";
			}
			}
		disk_string = makeVirshDiskString();

		m_xml += disk_string;
		m_xml += "</devices></domain>";


	}

	return true;
}


bool
XenType::CreateVirshConfigFile(const char* filename)
{
	std::string disk_string;

	if( !filename ) return false;

	if (!VirshType::CreateVirshConfigFile(filename))
	{
		vmprintf(D_ALWAYS, "XenType::CreateVirshConfigFile no XML found, generating defaults\n");

		m_xml += "<domain type='xen'>";
		m_xml += "<name>";
		m_xml += m_vm_name;
		m_xml += "</name>";
		m_xml += "<memory>";
		m_xml += std::to_string( m_vm_mem * 1024 );
		m_xml += "</memory>";
		m_xml += "<vcpu>";
		m_xml += std::to_string( m_vcpus );
		m_xml += "</vcpu>";
		m_xml += "<os><type>linux</type>";

		if( m_xen_kernel_file.empty() == false ) {
			m_xml += "<kernel>";
			m_xml += m_xen_kernel_file;
			m_xml += "</kernel>";
			if( m_xen_initrd_file.empty() == false ) {
				m_xml += "<initrd>";
				m_xml += m_xen_initrd_file;
				m_xml += "</initrd>";
			}
			if( m_xen_root.empty() == false ) {
				m_xml += "<root>";
				m_xml += m_xen_root;
				m_xml += "</root>";
			}

			if( m_xen_kernel_params.empty() == false ) {
				m_xml += "<cmdline>";
				m_xml += m_xen_kernel_params;
				m_xml += "</cmdline>";
			}
		}

		m_xml += "</os>";
		if( strcasecmp(m_xen_kernel_submit_param.c_str(), XEN_KERNEL_INCLUDED) == 0) {
			m_xml += "<bootloader>";
			m_xml += m_xen_bootloader;
			m_xml += "</bootloader>";
		}
		m_xml += "<devices>";
		m_xml += "<console type='pty'><source path='/dev/ptmx'/></console>";
		if( m_vm_networking ) {
			if( m_vm_networking_type.find("nat") != std::string::npos ) {
				m_xml += "<interface type='network'><source network='default'/></interface>";
			} else if( m_vm_networking_type.find("bridge") != std::string::npos ){
				m_xml += "<interface type='bridge'>";
				if (!m_vm_bridge_interface.empty()) {
					m_xml += "<source bridge='";
					m_xml += m_vm_bridge_interface;
					m_xml += "'/>";
				}
				vmprintf(D_FULLDEBUG, "mac address is %s", m_vm_job_mac.c_str());
				if(!m_vm_job_mac.empty())
				{
					m_xml += "<mac address='";
					m_xml += m_vm_job_mac;
					m_xml += "'/>";
				}
				m_xml += "</interface>";
			}
		}

		// Create disk parameter in Virsh config file
		disk_string = makeVirshDiskString();
		m_xml += disk_string;
		m_xml += "</devices></domain>";
	}

	return true;
}

bool
VirshType::CreateXenVMConfigFile(const char* filename)
{
	if( !filename ) {
		return false;
	}

       	return CreateVirshConfigFile(filename);
}

void VirshType::Connect()
{
	priv_state priv = set_root_priv();

	if ( m_libvirt_connection )
	{
		virConnectClose( m_libvirt_connection );
	}

    m_libvirt_connection = virConnectOpen( m_sessionID.c_str() );
    set_priv(priv);

  	if( m_libvirt_connection == NULL )
    {
		virErrorPtr err = virGetLastError();
		EXCEPT("Failed to create libvirt connection: %s", (err ? err->message : "No reason found"));
    }
}

bool
VirshType::parseXenDiskParam(const char *format)
{
	if( !format || (format[0] == '\0') ) {
		return false;
	}

    vmprintf(D_FULLDEBUG, "format = %s\n", format);

	std::vector<std::string> working_files;
	find_all_files_in_dir(m_workingpath.c_str(), working_files, true);

	std::vector<std::string> disk_files = split(format, ",");
	if( disk_files.empty() ) {
		return false;
	}

	for (const auto& one_disk : disk_files) {
		// found a disk file
		std::vector<std::string> single_disk_file = split(one_disk, ":");
		size_t iNumParams = single_disk_file.size();
		if( iNumParams < 3 || iNumParams > 4 ) 
        {
			return false;
		}

		// name of a disk file
		std::string dfile = single_disk_file[0];
		if( dfile.empty() ) {
			return false;
		}
		trim(dfile);

		const char* tmp_base_name = condor_basename(dfile.c_str());
		if( !tmp_base_name ) {
			return false;
		}

		// Every disk file for Virsh must have full path name
		std::string disk_file;
		if( filelist_contains_file(dfile.c_str(),
					working_files, true) ) {
			// file is transferred
			disk_file = m_workingpath;
			disk_file += DIR_DELIM_CHAR;
			disk_file += tmp_base_name;

			m_has_transferred_disk_file = true;
		}else {
			// file is not transferred.
			if( fullpath(dfile.c_str()) == false) {
				vmprintf(D_ALWAYS, "File(%s) for xen disk "
						"should have full path name\n", dfile.c_str());
				return false;
			}
			disk_file = dfile;
		}

		// device name
		std::string disk_device = single_disk_file[1];
		trim(disk_device);
		lower_case(disk_device);

		// disk permission
		std::string disk_perm = single_disk_file[2];
		trim(disk_perm);

		if( !strcasecmp(disk_perm.c_str(), "w") || !strcasecmp(disk_perm.c_str(), "rw")) 
        {
			// check if this disk file is writable
			if( check_vm_write_access_file(disk_file.c_str(), false) == false ) {
				vmprintf(D_ALWAYS, "xen disk image file('%s') cannot be modified\n",
					   	disk_file.c_str());
				return false;
			}
		}else 
        {
			// check if this disk file is readable
			if( check_vm_read_access_file(disk_file.c_str(), false) == false ) {
				vmprintf(D_ALWAYS, "xen disk image file('%s') cannot be read\n",
						disk_file.c_str());
				return false;
			}
		}

		XenDisk *newdisk = new XenDisk;
		ASSERT(newdisk);
		newdisk->filename = disk_file;
		newdisk->device = disk_device;
		newdisk->permission = disk_perm;

        // only when a format is specified do we check
        if (iNumParams == 4 )
        {
            newdisk->format = single_disk_file[3];
            trim(newdisk->format);
            lower_case(newdisk->format);
        }
        
		m_disk_list.push_back(newdisk);
	}

	if( m_disk_list.size() == 0 ) {
		vmprintf(D_ALWAYS, "No valid Virsh disk\n");
		return false;
	}

	return true;
}


// This function should be called after parseVirshDiskParam
std::string
VirshType::makeVirshDiskString(void)
{
	std::string xendisk;
	bool first_disk = true;

	for (auto vdisk : m_disk_list) {
		if( !first_disk ) {
			xendisk += "</disk>";
		}
		first_disk = false;
        if ( strstr (vdisk->filename.c_str(), ".iso") )
        {
            xendisk += "<disk type='file' device='cdrom'>";
        }
        else
        {
            xendisk += "<disk type='file'>";
        }

        if (vdisk->format.length())
        {
           xendisk += "<driver name='qemu' type='";
           xendisk += vdisk->format;
           xendisk += "'/>";
        }

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

	return xendisk;
}

bool
VirshType::writableXenDisk(const char* file)
{
	if( !file ) {
		return false;
	}

	for (auto vdisk : m_disk_list) {
		if( !strcasecmp(basename(file), basename(vdisk->filename.c_str())) ) {
			if( !strcasecmp(vdisk->permission.c_str(), "w") ||
					!strcasecmp(vdisk->permission.c_str(), "rw")) {
				return true;
			}else {
				return false;
			}
		}
	}
	return false;
}

void
VirshType::updateLocalWriteDiskTimestamp(time_t timestamp)
{
	std::vector<std::string> working_files;
	struct utimbuf timewrap;

	find_all_files_in_dir(m_workingpath.c_str(), working_files, true);

	for (const auto& tmp_file : working_files) {
		// In Virsh, disk file is generally used via loopback-mounted
		// file. However, mtime of those files is not updated
		// even after modification.
		// So we manually update mtimes of writable disk files.
		if( writableXenDisk(tmp_file.c_str()) ) {
			timewrap.actime = timestamp;
			timewrap.modtime = timestamp;
			utime(tmp_file.c_str(), &timewrap);
		}
	}
}


bool
VirshType::createCkptFiles(void)
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::createCkptFiles\n");

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
		std::string timestampfile(m_suspendfile);
		timestampfile += XEN_CKPT_TIMESTAMP_FILE_SUFFIX;

		FILE *fp = safe_fopen_wrapper(timestampfile.c_str(), "w");
		if( !fp ) {
			vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper file(%s) for "
					"checkpoint timestamp : safe_fopen_wrapper returns %s\n",
					timestampfile.c_str(), strerror(errno));
			Resume();
			return false;
		}
		if( fprintf(fp, "%lld\n", (long long)current_time) < 0 ) {
			fclose(fp);
			unlink(timestampfile.c_str());
			vmprintf(D_ALWAYS, "failed to fprintf for checkpoint timestamp "
					"file(%s:%s)\n", timestampfile.c_str(), strerror(errno));
			Resume();
			return false;
		}
		fclose(fp);

		// checkpoint succeeds
		m_is_checkpointed = true;
		return true;
	}

	return false;
}

bool KVMType::checkXenParams(VMGahpConfig * config)
{
  if( !config ) {
    return false;
  }

  // Do we need to check for both read and write access?
  if(check_vm_read_access_file("/dev/kvm", true) == false) {
    vmprintf(D_ALWAYS, "\nFile Permission Error: Cannot read /dev/kvm as root\n");
    return false;
  }
  if(check_vm_write_access_file("/dev/kvm", true) == false) {
    vmprintf(D_ALWAYS, "\nFile Permission Error: Cannot write /dev/kvm as root\n");
    return false;
  }

	virConnectPtr libvirt_connection = virConnectOpen("qemu:///session");
	if (libvirt_connection == nullptr) {
		virErrorPtr err = virGetLastError();
		vmprintf(D_ALWAYS, "Failed to create libvirt connection: %s\n",
		         (err ? err->message : "No reason found"));
		return false;
	}
	virConnectClose(libvirt_connection);

	return true;
}

bool
KVMType::killVMFast(const char* vmname)
{
	vmprintf(D_FULLDEBUG, "Inside KVMType::killVMFast\n");
	priv_state priv = set_root_priv();
	virConnectPtr libvirt_connection = virConnectOpen("qemu:///session");
	set_priv(priv);
	return VirshType::killVMFast(vmname, libvirt_connection);
}

bool
XenType::checkXenParams(VMGahpConfig* config)
{
	char *config_value = NULL;
	std::string fixedvalue;
	vmprintf(D_FULLDEBUG, "In XenType::checkXenParams()\n");
	if( !config ) {
		return false;
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
	if( check_vm_read_access_file(fixedvalue.c_str(), true) == false ) {
		vmprintf(D_ALWAYS, "Xen bootloader file '%s' cannot be read\n",
				fixedvalue.c_str());
		return false;
	}

	virConnectPtr libvirt_connection = virConnectOpen("xen:///");
	if (libvirt_connection == nullptr) {
		virErrorPtr err = virGetLastError();
		vmprintf(D_ALWAYS, "Failed to create libvirt connection: %s\n",
		         (err ? err->message : "No reason found"));
		return false;
	}
	virConnectClose(libvirt_connection);

	return true;
}

bool
XenType::killVMFast(const char* vmname)
{
	vmprintf(D_FULLDEBUG, "Inside XenType::killVMFast\n");
	priv_state priv = set_root_priv();
	virConnectPtr libvirt_connection = virConnectOpen("xen:///");
	set_priv(priv);
	return VirshType::killVMFast(vmname, libvirt_connection);
}

void
VirshType::makeNameofSuspendfile(std::string& name)
{
	formatstr(name, "%s%c%s", m_workingpath.c_str(), DIR_DELIM_CHAR,
			XEN_MEM_SAVED_FILE);
}

// This function compares the timestamp of given file with
// that of writable disk files.
// This function should be called after parseVirshDiskParam()
bool
VirshType::checkCkptSuspendFile(const char* file)
{
	if( !file || file[0] == '\0' ) {
		return false;
	}

	// Read timestamp file
	std::string timestampfile(file);
	timestampfile += XEN_CKPT_TIMESTAMP_FILE_SUFFIX;

	FILE *fp = safe_fopen_wrapper(timestampfile.c_str(), "r");
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

	std::string tmp_str;
	tmp_str = buffer;
	if( tmp_str.empty() ) {
		vmprintf(D_ALWAYS, "Invalid timestamp file\n");
		return false;
	}
	trim(tmp_str);

	time_t timestamp;
	timestamp = (time_t)atoi(tmp_str.c_str());
	if( timestamp <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid timestamp file\n");
		return false;
	}

	for (auto vdisk : m_disk_list) {
		if( !strcasecmp(vdisk->permission.c_str(), "w") ||
				!strcasecmp(vdisk->permission.c_str(), "rw")) {
			// this is a writable disk file
			// get modification time of this disk file
			time_t disk_mtime;
			struct stat disk_stat;
			if( stat(vdisk->filename.c_str(), &disk_stat ) < 0 ) {
				vmprintf(D_ALWAYS, "\nERROR: Failed to access \"%s\":%s\n",
						vdisk->filename.c_str(), strerror(errno));
				return false;
			}
			disk_mtime = disk_stat.st_mtime;

			// compare
			if( disk_mtime != timestamp ) {
				vmprintf(D_ALWAYS, "Checkpoint timestamp mismatch: "
						"timestamp of suspend file=%lld, mtime of disk file=%lld\n",
						(long long)timestamp, (long long)disk_mtime);
				return false;
			}
		}
	}

	return true;
}

bool
VirshType::findCkptConfigAndSuspendFile(std::string &vmconfig, std::string &suspendfile)
{
	if( m_transfer_intermediate_files.empty() ) {
		return false;
	}

	vmconfig = "";
	suspendfile = "";

	std::string tmpconfig;
	std::string tmpsuspendfile;

	if( filelist_contains_file( XEN_CONFIG_FILE_NAME,
				m_transfer_intermediate_files, true) ) {
		// There is a vm config file for checkpointed files
		formatstr(tmpconfig, "%s%c%s", m_workingpath.c_str(),
				DIR_DELIM_CHAR, XEN_CONFIG_FILE_NAME);
	}

	std::string tmp_name;
	makeNameofSuspendfile(tmp_name);

	if( filelist_contains_file(tmp_name.c_str(),
				m_transfer_intermediate_files, true) ) {
		// There is a suspend file that was created during vacate
		tmpsuspendfile = tmp_name;
		if( check_vm_read_access_file(tmpsuspendfile.c_str(), true) == false) {
			return false;
		}
	}

	if( (tmpconfig.length() > 0) &&
			(tmpsuspendfile.length() > 0 )) {
		// check if the timestamp of suspend file is same to
		// that of writable disk files
		// If timestamp differs between suspend file and writable disk file,
		// it means that there was a crash.
		// So we need to restart this VM job from the beginning.
		if( checkCkptSuspendFile(tmpsuspendfile.c_str()) ) {
			vmconfig = tmpconfig;
			suspendfile = tmpsuspendfile;
			return true;
		}
	}
	return false;
}

bool
VirshType::killVM()
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::killVM\n");

	if( ( m_vm_name.length() == 0 ) ) {
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	return killVMFast(m_vm_name.c_str(), m_libvirt_connection);
}

bool
VirshType::killVMFast(const char* vmname, virConnectPtr libvirt_con)
{
	vmprintf(D_FULLDEBUG, "Inside VirshType::killVMFast\n");

	if( !vmname || (vmname[0] == '\0') ) {
		return false;
	}

	priv_state priv = set_root_priv();
	virDomainPtr dom = virDomainLookupByName(libvirt_con, vmname);
	set_priv(priv);
	if(dom == NULL)
	  {
	    virErrorPtr err = virConnGetLastError(libvirt_con);
	    if (err && err->code != VIR_ERR_NO_DOMAIN)
	      {
		vmprintf(D_ALWAYS, "Error finding domain %s: %s\n", vmname, (err->message ? err->message : "No reason found"));
		return false;
	      }
	    else
	      {
		return true;
	      }
	  }

	priv = set_root_priv();
	bool ret = (virDomainDestroy(dom) == 0);
	virDomainFree(dom);
	set_priv(priv);
	return ret;
}

XenType::XenType(const char * workingpath, ClassAd * ad)
  : VirshType(workingpath, ad)
{

  m_sessionID="xen:///";
  this->Connect();
  m_vmtype = CONDOR_VM_UNIVERSE_XEN;

}

bool XenType::CreateConfigFile()
{
	char *config_value = NULL;
	priv_state priv;

	vmprintf(D_FULLDEBUG, "In XenType::CreateConfigFile()\n");
	// Read common parameters for VM
	// and create the name of this VM
	if( parseCommonParamFromClassAd(true) == false ) {
		return false;
	}

	if( m_vm_mem < 32 ) {
		// Allocating less than 32MBs is not recommended in Virsh.
		m_vm_mem = 32;
	}

	// Read the parameter of Virsh kernel
	if( m_classAd.LookupString( VMPARAM_XEN_KERNEL, m_xen_kernel_submit_param) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n",
							VMPARAM_XEN_KERNEL);
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_NO_KERNEL_PARAM;
		return false;
	}
	trim(m_xen_kernel_submit_param);

	if(strcasecmp(m_xen_kernel_submit_param.c_str(), XEN_KERNEL_INCLUDED) == 0 )
	{
		//if (strcasecmp(vm_type.Value(), CONDOR_VM_UNIVERSE_XEN) == 0){
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
		//}
	}else if(strcasecmp(m_xen_kernel_submit_param.c_str(), XEN_KERNEL_HW_VT) == 0) {
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
		if( check_vm_read_access_file(m_xen_kernel_submit_param.c_str(), false) == false) {
			vmprintf(D_ALWAYS, "xen kernel file '%s' cannot be read\n",
					m_xen_kernel_submit_param.c_str());
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_KERNEL_NOT_FOUND;
			return false;
		}
		std::string tmp_fullname;
		if( isTransferedFile(m_xen_kernel_submit_param.c_str(),
					tmp_fullname) ) {
			// this file is transferred
			// we need a full path
			m_xen_kernel_submit_param = tmp_fullname.c_str();
		}

		m_xen_kernel_file = m_xen_kernel_submit_param;

		if( m_classAd.LookupString(VMPARAM_XEN_INITRD, m_xen_initrd_file)
					== 1 ) {
			// A job user defined a customized ramdisk
			trim(m_xen_initrd_file);
			if( check_vm_read_access_file(m_xen_initrd_file.c_str(), false) == false) {
				// make sure that the file for xen ramdisk is readable
				vmprintf(D_ALWAYS, "xen ramdisk file '%s' cannot be read\n",
						m_xen_initrd_file.c_str());
				m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_INITRD_NOT_FOUND;
				return false;
			}
			if( isTransferedFile(m_xen_initrd_file.c_str(),
						tmp_fullname) ) {
				// this file is transferred
				// we need a full path
				m_xen_initrd_file = tmp_fullname.c_str();
			}
		}
	}

	if( m_xen_kernel_file.empty() == false ) {
		// Read the parameter of Virsh Root
		if( m_classAd.LookupString(VMPARAM_XEN_ROOT, m_xen_root) != 1 ) {
			vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n",
					VMPARAM_XEN_ROOT);
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_NO_ROOT_DEVICE_PARAM;
			return false;
		}
		trim(m_xen_root);
	}

	std::string xen_disk;
	// Read the parameter of Virsh Disk
	if( m_classAd.LookupString(VMPARAM_VM_DISK, xen_disk) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n",
				VMPARAM_VM_DISK);
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_NO_DISK_PARAM;
		return false;
	}
	trim(xen_disk);

	// from this point below we start to check the params
	// and access data.
	priv = set_root_priv();

	if( parseXenDiskParam(xen_disk.c_str()) == false ) {
		vmprintf(D_ALWAYS, "xen disk format(%s) is incorrect\n",
				xen_disk.c_str());
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_INVALID_DISK_PARAM;
		set_priv(priv);
		return false;
	}
	set_priv(priv);

	// Read the parameter of Virsh Kernel Param
	if( m_classAd.LookupString(VMPARAM_XEN_KERNEL_PARAMS, m_xen_kernel_params) == 1 ) {
		trim(m_xen_kernel_params);
	}

	// Check whether this is re-starting after vacating checkpointing,
	if( m_transfer_intermediate_files.empty() == false ) {
		// we have chckecpointed files
		// Find vm config file and suspend file
		std::string ckpt_config_file;
		std::string ckpt_suspend_file;
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

	// Here we check if this job actually can use checkpoint
	if( m_vm_checkpoint ) {
		// For vm checkpoint in Virsh
		// 1. all disk files should be in a shared file system
		// 2. If a job uses CDROM files, it should be
		// 	  single ISO file and be in a shared file system
		if( m_has_transferred_disk_file ) {
			// In this case, we cannot use vm checkpoint for Virsh
			// To use vm checkpoint in Virsh,
			// all disk and iso files should be in a shared file system
			vmprintf(D_ALWAYS, "To use vm checkpint in Virsh, "
					"all disk and iso files should be "
					"in a shared file system\n");
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_XEN_MISMATCHED_CHECKPOINT;
			return false;
		}
	}


	// create a vm config file
	std::string tmp_config_name;
	formatstr(tmp_config_name, "%s%c%s", m_workingpath.c_str(),
			DIR_DELIM_CHAR, XEN_CONFIG_FILE_NAME);

	vmprintf(D_ALWAYS, "CreateXenVMConfigFile\n");

	if( CreateXenVMConfigFile(tmp_config_name.c_str())
			== false ) {
		m_result_msg = VMGAHP_ERR_CRITICAL;
		return false;
	}

	// set vm config file
	m_configfile = tmp_config_name.c_str();
	return true;
}


KVMType::KVMType(const char * workingpath, ClassAd * ad)
  : VirshType(workingpath, ad)
{

	m_sessionID="qemu:///session";
	this->Connect();
	m_vmtype = CONDOR_VM_UNIVERSE_KVM;

}

bool
KVMType::CreateConfigFile()
{
	priv_state priv;

	vmprintf(D_FULLDEBUG, "In KVMType::CreateConfigFile()\n");
	// Read common parameters for VM
	// and create the name of this VM
	if( parseCommonParamFromClassAd(true) == false ) {
		return false;
	}

	if( m_vm_mem < 32 ) {
		// Allocating less than 32MBs is not recommended in Virsh.
		m_vm_mem = 32;
	}

	std::string kvm_disk;
	// Read the parameter of Virsh Disk
	if( m_classAd.LookupString(VMPARAM_VM_DISK, kvm_disk) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n",
				VMPARAM_VM_DISK);
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_KVM_NO_DISK_PARAM;
		return false;
	}
	trim(kvm_disk);

	// from this point below we start to check the params
	// and access data.
	priv = set_root_priv();

	if( parseXenDiskParam(kvm_disk.c_str()) == false ) {
		vmprintf(D_ALWAYS, "kvm disk format(%s) is incorrect\n",
				kvm_disk.c_str());
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_KVM_INVALID_DISK_PARAM;
		set_priv(priv);
		return false;
	}
	set_priv(priv);

	// Check whether this is re-starting after vacating checkpointing,
	if( m_transfer_intermediate_files.empty() == false ) {
		// we have chckecpointed files
		// Find vm config file and suspend file
		std::string ckpt_config_file;
		std::string ckpt_suspend_file;
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

	// Here we check if this job actually can use checkpoint
	if( m_vm_checkpoint ) {
		// For vm checkpoint in Virsh
		// 1. all disk files should be in a shared file system
		if( m_has_transferred_disk_file ) {
			// In this case, we cannot use vm checkpoint for Virsh
			// To use vm checkpoint in Virsh,
			// all disk and iso files should be in a shared file system
			vmprintf(D_ALWAYS, "To use vm checkpint in Virsh, "
					"all disk and iso files should be "
					"in a shared file system\n");
			m_result_msg = VMGAHP_ERR_JOBCLASSAD_KVM_MISMATCHED_CHECKPOINT;
			return false;
		}
	}


	// create a vm config file
	std::string tmp_config_name;
	formatstr(tmp_config_name, "%s%c%s", m_workingpath.c_str(),
			DIR_DELIM_CHAR, XEN_CONFIG_FILE_NAME);

	vmprintf(D_ALWAYS, "CreateKvmVMConfigFile\n");

	if( CreateXenVMConfigFile(tmp_config_name.c_str())
			== false ) {
		m_result_msg = VMGAHP_ERR_CRITICAL;
		return false;
	}

	// set vm config file
	m_configfile = tmp_config_name.c_str();
	return true;
}
