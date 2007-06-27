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

#ifndef VM_TYPE_H
#define VM_TYPE_H

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "MyString.h"
#include "gahp_common.h"
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
	static bool createVMName(ClassAd *ad, MyString& vmname);

	VMType(const char* prog_for_script, const char* scriptname, 
			const char* workingpath, ClassAd *ad);

	virtual ~VMType();

	virtual bool Start() = 0;

	virtual bool Shutdown() = 0;

	virtual bool SoftSuspend() = 0;

	virtual bool Suspend() = 0;

	virtual bool Resume() = 0;

	virtual bool Checkpoint() = 0;

	virtual bool Status() = 0;

	virtual bool killVM() = 0;

	virtual bool CreateConfigFile() = 0;

	int getVMId(void) { return m_vm_id; }

	int PidOfVM(void) { return m_vm_pid; }

	vm_status getVMStatus(void);

	void setLastStatus(const char *result_msg);

	MyString m_result_msg;

protected:
	void setVMStatus(vm_status status);
	void deleteNonTransferredFiles();
	bool parseCommonParamFromClassAd(bool is_root = false);
	bool createConfigUsingScript(const char* configfile);
	bool createTempFile(const char *template_string, const char *suffix, MyString &outname);
	bool createISOConfigAndName(StringList *files, MyString &isoconf, MyString &isofile);
	bool createISO();

	bool isTransferedFile(const char* file_name, MyString& fullname);
	void chownWorkingFiles(uid_t dst_uid);

	MyString m_vmtype;
	MyString m_vm_name;
	int m_vm_id;

	int	m_vm_pid;	// PID of acutal vmware process for this VM
	MyString m_vm_mac;	// MAC address of VM
	MyString m_vm_ip;	// IP address of VM

	MyString m_workingpath;
	MyString m_prog_for_script;	 // perl
	MyString m_scriptname;

	MyString m_configfile;
	MyString m_iso_file;
	bool m_local_iso;
	bool m_has_iso;

	// File list for TransferInput from submit machine.(full path)
	StringList m_transfer_input_files;
	// File list for TransferIntermediate from spool directory.(full path)
	StringList m_transfer_intermediate_files;
	// Files list in working directory
	StringList m_initial_working_files;

	ClassAd m_classAd;
	int m_vm_mem;  // VM memory requested in Job classAd
	bool m_vm_networking;
	MyString m_vm_networking_interfaces;
	bool m_vm_checkpoint;
	bool m_vm_no_output_vm;
	bool m_vm_hardware_vt;
	StringList m_vm_cdrom_files;
	bool m_vm_transfer_cdrom_files;
	MyString m_classad_arg;
	MyString m_arg_file;

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
		// indicate whether files in the workingdir were chowned
	bool m_is_chowned;
		// use the script program to create a configuration file for VM.
	bool m_use_script_to_create_config;

	vm_status m_status;
	UtcTime m_start_time;
	UtcTime m_stop_time;
	float m_cpu_time;

	UtcTime m_last_status_time;
	MyString m_last_status_result;

private:
	void createInitialFileList();
};

#endif
