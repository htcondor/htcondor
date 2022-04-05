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


#ifndef VMWARE_TYPE_H
#define VMWARE_TYPE_H

#include "condor_classad.h"
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

	virtual void Config();

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

	bool findCkptConfig(std::string &vmconfig);
	bool readVMXfile(const char *filename, const char *dirpath);

	StringList m_configVars;

	bool m_need_snapshot;
	bool m_restart_with_ckpt;
	bool m_vmware_transfer;
	bool m_vmware_snapshot_disk;

	std::string m_vmware_dir;
	std::string m_vmware_vmx;
	std::string m_vmware_vmdk;
};
#endif
