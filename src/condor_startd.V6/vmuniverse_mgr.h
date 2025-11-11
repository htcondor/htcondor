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


#ifndef _CONDOR_VMUNIVERSE_MGR_H
#define _CONDOR_VMUNIVERSE_MGR_H

#include "condor_common.h"
#include "condor_classad.h"

class VMStarterInfo {
public:
	friend class VMUniverseMgr;

	VMStarterInfo();

	void getUsageOfVM(ProcFamilyUsage &usage);

	void addProcessForVM(pid_t vm_pid);
	pid_t getProcessForVM(void) const;

	void addMACForVM(const char *mac);
	const char *getMACForVM(void);

	void addIPForVM(const char *ip);
	const char *getIPForVM(void);

	void publishVMInfo(ClassAd* ad);

private:
	bool updateUsageOfVM(void);

	pid_t m_pid;
	int m_memory;
	int m_vcpus;
	ClassAd m_job_ad;

	pid_t m_vm_pid;
	struct procInfo m_vm_exited_pinfo;
	struct procInfo m_vm_alive_pinfo;

	std::string m_vm_mac;
	std::string m_vm_ip;
	std::string m_execute_dir;
};

class VMUniverseMgr : public Service {
public:
	VMUniverseMgr();
	~VMUniverseMgr();

	void publish(ClassAd* ad);
	void publishVMInfo(pid_t s_pid, ClassAd* ad);
	void printVMGahpInfo( int debug_level );

	bool allocVM(pid_t pid, const ClassAd &ad, char const *execute_dir); 
	void freeVM(pid_t pid); // pid for exited starter

	// This is only supposed to check if VM universe is available, but
	// is more commonly called as a response to the vm-gahp failing.
	// If warn is false, don't print the scary log message about that.
	void checkVMUniverse( bool warn = true );

	int getFreeVMMemSize(void) const;
	bool canCreateVM(const ClassAd *jobAd = NULL);

	int numOfRunningVM(void);
	bool hasVMLimit() { return m_vm_max_num > 0; }
	void setStarterAbility(bool has_vmcode);
	bool isStarterForVM(pid_t s_pid);
	bool getUsageForVM(pid_t s_pid, ProcFamilyUsage &usage);

	bool addProcessForVM(pid_t s_pid, pid_t vm_pid);
	pid_t getProcessForVM(pid_t s_pid);

	void addMACForVM(pid_t s_pid, const char *mac);
	const char *getMACForVM(pid_t s_pid);

	void addIPForVM(pid_t s_pid, const char *ip);
	const char *getIPForVM(pid_t s_pid);

	void killVM(const char *matchstring);

private:
	bool init();

	bool testVMGahp(const char* vmgahppath, const char* vmtype);
	void docheckVMUniverse( int timerID = -1 );
	void killVM(VMStarterInfo *info);
	VMStarterInfo* findVMStarterInfoWithStarterPid(pid_t s_pid);
	VMStarterInfo* findVMStarterInfoWithVMPid(pid_t vm_pid);
	VMStarterInfo* findVMStarterInfoWithMac(const char* mac);
	VMStarterInfo* findVMStarterInfoWithIP(const char* ip);

	bool m_starter_has_vmcode;
	bool m_needCheck;
	int m_check_tid;
	int m_check_interval;

	std::string m_vmgahp_server;
	ClassAd m_vmgahp_info;
	std::string m_vm_type;

	std::vector<VMStarterInfo*> m_vm_starter_list;
	int m_vm_max_memory;	// maximum amount of memory for VMs
	int m_vm_used_memory;	// memory being used for VMs
	int m_vm_max_num;	// maximum number of running VM
	bool m_vm_networking;

};

#endif /* _CONDOR_VMUNIVERSE_MGR_H */
