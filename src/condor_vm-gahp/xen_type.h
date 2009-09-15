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
#include "MyString.h"
#include "simplelist.h"
#include "gahp_common.h"
#include "vmgahp.h"
#include "vm_type.h"
#include <libvirt/libvirt.h>

class XenDisk {
	public:
		MyString filename;
		MyString device;
		MyString permission;
};

class VirshType : public VMType
{
public:
	static bool testXen(VMGahpConfig* config);
	static bool killVMFast(const char* script, const char* vmname);

	VirshType(const char* scriptname, const char* workingpath, ClassAd* ad);

	virtual ~VirshType();

	virtual bool Start();

	virtual bool Shutdown();

	virtual bool SoftSuspend();

	virtual bool Suspend();

	virtual bool Resume();

	virtual bool Checkpoint();

	virtual bool Status();

	virtual bool CreateConfigFile();

	virtual bool killVM();
protected:
	MyString makeVirshDiskString(void);
	bool createISO();

	bool parseXenDiskParam(const char *format);
	bool writableXenDisk(const char* file);
	void updateLocalWriteDiskTimestamp(time_t timestamp);
	void updateAllWriteDiskTimestamp(time_t timestamp);
	void makeNameofSuspendfile(MyString& name);
	bool createCkptFiles(void);
	bool findCkptConfigAndSuspendFile(MyString &config, MyString &suspendfile);
	bool checkCkptSuspendFile(const char* file);
	bool ResumeFromSoftSuspend(void);
	bool CreateXenVMConfigFile(const char* filename);

	// For the base class, this just prints out the classad
	// attributes and the type for each attribute.
	virtual bool CreateVirshConfigFile(const char* filename);

	SimpleList<XenDisk*> m_disk_list;

	MyString m_xen_cdrom_device;
	MyString m_suspendfile;
	float m_cputime_before_suspend;

	MyString m_xen_kernel_submit_param;
	MyString m_xen_kernel_file;
	MyString m_xen_initrd_file;
	MyString m_xen_root;
	MyString m_xen_kernel_params;
	MyString m_xen_bootloader;

	bool m_xen_hw_vt;
	bool m_allow_hw_vt_suspend;
	bool m_restart_with_ckpt;
	bool m_has_transferred_disk_file;

	MyString m_xml;
	virConnectPtr m_libvirt_connection;
};

class XenType : public VirshType
{
 public:
  XenType(const char* scriptname, const char* workingpath, ClassAd* ad);
  static bool checkXenParams(VMGahpConfig* config);
 protected:
  virtual bool CreateVirshConfigFile(const char * filename);
};

class KVMType : public VirshType
{
 public:
  KVMType(const char* scriptname, const char* workingpath, ClassAd* ad);
  static bool checkXenParams(VMGahpConfig* config);
 protected:
  virtual bool CreateVirshConfigFile(const char * filename);
};

#endif
