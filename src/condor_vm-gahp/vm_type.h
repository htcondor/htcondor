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


#ifndef VM_TYPE_H
#define VM_TYPE_H

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_daemon_core.h"
#include "utc_time.h"
#include "directory.h"
#include "condor_vm_universe_types.h"
#include "enum_utils.h"
#include "vm_univ_utils.h"

enum vm_status {
	VM_RUNNING, VM_STOPPED, VM_SUSPENDED
};

class VMType
{
public:
	static bool createVMName(ClassAd *ad, std::string& vmname);

	VMType(const char* prog_for_script, const char* scriptname, 
			const char* workingpath, ClassAd *ad);

	virtual ~VMType();

	virtual void Config() = 0;

	virtual bool Start() = 0;

	virtual bool Shutdown() = 0;

	virtual bool SoftSuspend() = 0;

	virtual bool Suspend() = 0;

	virtual bool Resume() = 0;

	virtual bool Checkpoint() = 0;

	virtual bool Status() = 0;

	virtual bool killVM() = 0;

	virtual bool CreateConfigFile() = 0;

	int getVMId(void) const { return m_vm_id; }

	int PidOfVM(void) const { return m_vm_pid; }

	vm_status getVMStatus(void);

	void setLastStatus(const char *result_msg);

	std::string m_result_msg;

protected:

	void setVMStatus(vm_status status);
	void deleteNonTransferredFiles();
	bool parseCommonParamFromClassAd(bool is_root = false);
	bool createConfigUsingScript(const char* configfile);
	bool createTempFile(const char *template_string, const char *suffix, std::string &outname);
	bool isTransferedFile(const char* file_name, std::string& fullname);

	std::string m_vmtype;
	std::string m_vm_name;
	int m_vm_id;

	int	m_vm_pid;	// PID of acutal vmware process for this VM
	std::string m_vm_mac;	// MAC address of VM
	std::string m_vm_ip;	// IP address of VM

	std::string m_workingpath;
	std::string m_prog_for_script;	 // perl
	std::string m_scriptname;

	std::string m_configfile;
	
	// File list for TransferInput from submit machine.(full path)
	std::vector<std::string> m_transfer_input_files;
	// File list for TransferIntermediate from spool directory.(full path)
	std::vector<std::string> m_transfer_intermediate_files;
	// Files list in working directory
	std::vector<std::string> m_initial_working_files;

	ClassAd m_classAd;
	int m_vm_mem;  // VM memory requested in Job classAd
	bool m_vm_networking;
	std::string m_vm_networking_type;
	bool m_vm_checkpoint;
	bool m_vm_no_output_vm;
	bool m_vm_hardware_vt;

	// Usually, when we suspend a VM, the memory being used by the VM 
	// will be freed and the memory will be saved into a file.
	// However, when we use soft suspend, the memory being used by the VM 
	// will not be freed and no file for the memory will be created.
	//
	// Here is how we implement soft suspension.
	// In VMware, we send SIGSTOP to a process for VM in order to 
	// stop the VM temporarily and send SIGCONT to resume the VM.
	// In Xen, we pause CPU. 
	// Pausing CPU doesn't save the memory of VM into a file.
	// It just stops the execution of a VM temporarily.
	
		// indicate whether VM is soft suspended.
	bool m_is_soft_suspended; 
		// indicate whether VM is poweroff by itself
	bool m_self_shutdown; 
		// indicate whether VM is checkpointed
	bool m_is_checkpointed;

		// indicate delete files in the working directory
	bool m_delete_working_files;
		// use the script program to create a configuration file for VM.
	bool m_use_script_to_create_config;

	vm_status m_status;
	time_t m_start_time;
	time_t m_stop_time;
	float m_cpu_time;

	time_t m_last_status_time;
	std::string m_last_status_result;
	int m_vcpus;
	std::string m_vm_job_mac;

	priv_state m_file_owner;
private:
	void createInitialFileList();
};

#endif
