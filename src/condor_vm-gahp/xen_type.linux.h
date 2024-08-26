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


#ifndef XEN_TYPE_H
#define XEN_TYPE_H

#include "condor_classad.h"
#include "vmgahp.h"
#include "vm_type.h"
#include <libvirt/libvirt.h>

class XenDisk {
	public:
		std::string filename;
		std::string device;
		std::string permission;
		std::string format;
};

class VirshType : public VMType
{
public:
	static bool killVMFast(const char* script, virConnectPtr libvirt_con);

	VirshType(const char* workingpath, ClassAd* ad);

	virtual ~VirshType();

	virtual void Config();

	virtual bool Start();

	virtual bool Shutdown();

	virtual bool SoftSuspend();

	virtual bool Suspend();

	virtual bool Resume();

	virtual bool Checkpoint();

	virtual bool Status();

	virtual bool CreateConfigFile()=0;

	virtual bool killVM();
protected:
	std::string makeVirshDiskString(void);
	bool createISO();

	void Connect();
	bool parseXenDiskParam(const char *format);
	bool writableXenDisk(const char* file);
	void updateLocalWriteDiskTimestamp(time_t timestamp);
	void makeNameofSuspendfile(std::string& name);
	bool createCkptFiles(void);
	bool findCkptConfigAndSuspendFile(std::string &config, std::string &suspendfile);
	bool checkCkptSuspendFile(const char* file);
	bool ResumeFromSoftSuspend(void);
	bool CreateXenVMConfigFile(const char* filename);

	// For the base class, this just prints out the classad
	// attributes and the type for each attribute.
	virtual bool CreateVirshConfigFile(const char* filename);

	std::vector<XenDisk*> m_disk_list;

	std::string m_xen_cdrom_device;
	std::string m_suspendfile;
	float m_cputime_before_suspend;

	std::string m_xen_kernel_submit_param;
	std::string m_xen_kernel_file;
	std::string m_xen_initrd_file;
	std::string m_xen_root;
	std::string m_xen_kernel_params;
	std::string m_xen_bootloader;
	std::string m_vm_bridge_interface;

	bool m_xen_hw_vt;
	bool m_allow_hw_vt_suspend;
	bool m_restart_with_ckpt;
	bool m_has_transferred_disk_file;

	std::string m_xml;
	std::string m_sessionID; ///< required for connect filled on constructor
	virConnectPtr m_libvirt_connection;
};

class XenType : public VirshType
{
 public:
  XenType(const char* workingpath, ClassAd* ad);
  static bool checkXenParams(VMGahpConfig* config);
  virtual bool CreateConfigFile();
  static bool killVMFast(const char* script);

 protected:
  virtual bool CreateVirshConfigFile(const char * filename);
};

class KVMType : public VirshType
{
 public:
  KVMType(const char* workingpath, ClassAd* ad);
  static bool checkXenParams(VMGahpConfig* config);
  virtual bool CreateConfigFile();
  static bool killVMFast(const char* script);
 protected:
  virtual bool CreateVirshConfigFile(const char * filename);
};

#endif
