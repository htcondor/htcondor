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

#ifndef VMWARE_TYPE_H
#define VMWARE_TYPE_H

#include "condor_classad.h"
#include "MyString.h"
#include "simplelist.h"
#include "gahp_common.h"
#include "vmgahp.h"
#include "vm_type.h"

class VMwareType : public VMType
{
public:
	static bool checkVMwareParams(VMGahpConfig* config);
	static bool testVMware(VMGahpConfig* config);
	static bool killVMFast(const char* prog_for_script, const char* script, 
			const char* matchstring, bool is_root = false);

	VMwareType(const char* prog_for_script, const char* scriptname, 
			const char* workingpath, ClassAd* ad);

	virtual ~VMwareType();

	virtual bool Start();

	virtual bool Shutdown();

	virtual bool SoftSuspend();

	virtual bool Suspend();

	virtual bool Resume();

	virtual bool Checkpoint();

	virtual bool Status();

	virtual bool CreateConfigFile();
	
	virtual bool killVM();

private:
	void deleteLockFiles();
	bool createCkptFiles();
	bool CombineDisks();
	void adjustConfigDiskPath();
	bool adjustCkptConfig(const char* vmconfig);
	bool Snapshot();
	bool Unregister();
	bool ShutdownFast();
	bool ShutdownGraceful();
	bool ResumeFromSoftSuspend();
	bool getPIDofVM(int &vm_pid);

	bool findCkptConfig(MyString &vmconfig);
	bool readVMXfile(const char *filename, const char *dirpath);

	StringList m_configVars;

	bool m_need_snapshot;
	bool m_restart_with_ckpt;
	bool m_vmware_transfer;
	bool m_vmware_snapshot_disk;

	MyString m_vmware_dir;
	MyString m_vmware_vmx;
	MyString m_vmware_vmdk;
};
#endif
