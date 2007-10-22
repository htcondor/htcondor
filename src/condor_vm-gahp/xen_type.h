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

#ifndef XEN_TYPE_H
#define XEN_TYPE_H

#include "condor_classad.h"
#include "MyString.h"
#include "simplelist.h"
#include "gahp_common.h"
#include "vmgahp.h"
#include "vm_type.h"

class XenDisk {
	public:
		MyString filename;
		MyString device;
		MyString permission;
};

class XenType : public VMType
{
public:
	static bool checkXenParams(VMGahpConfig* config);
	static bool testXen(VMGahpConfig* config);
	static bool killVMFast(const char* script, const char* vmname, const char* controller);

	XenType(const char* scriptname, const char* workingpath, ClassAd* ad);

	virtual ~XenType();

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
	MyString makeXMDiskString(void);
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
	bool CreateXenVMCofigFile(const char* controller, const char* filename);
	bool CreateVirshConfigFile(const char* filename);
	bool CreateXMConfigFile(const char* filename);

	SimpleList<XenDisk*> m_disk_list;

	MyString m_xen_controller;
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
};
#endif
