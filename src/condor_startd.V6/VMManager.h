/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef _VM_MANAGER_H
#define _VM_MANAGER_H

#include "condor_common.h"
#include "simplelist.h"
#include "VMMachine.h"

bool vmapi_is_host_machine(void);
void vmapi_create_vmmanager(char *);
void vmapi_destroy_vmmanager(void);

class VMManager;
extern VMManager *vmmanager;

class VMMachine;

class VMManager : public Service {
public:
	VMManager();
	virtual ~VMManager();

	bool isRegistered(char *,int);
	int numOfVM(void);
	void attach(VMMachine *);
	void detach(VMMachine *);
	void allNotify(char *, int cmd, void *data);
	void printAllElements(void);

	void checkRegisterTimeout(void);
	void startUnRegisterTimer(void);
	void cancelUnRegisterTimer(void);

	int host_usable;
	StringList *allowed_vm_list;
private:
	int m_vm_registered_num;
	int m_vm_unrg_tid;
	SimpleList<VMMachine *> m_virtualmachines;
};
#endif /* _VM_MANAGER_H */
