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

#ifndef _VM_MANAGER_H
#define _VM_MANAGER_H

#include "condor_common.h"
#include "simplelist.h"

bool vmapi_is_host_machine(void);
void vmapi_create_vmmanager(const char *);
void vmapi_destroy_vmmanager(void);

class VMMachine;
class VMManager : public Service {
public:
	VMManager();
	virtual ~VMManager();

	bool isRegistered(const char *,int);
	int numOfVM(void) const;
	void attach(VMMachine *);
	void detach(VMMachine *);
	void allNotify(const char *, int cmd, void *data);
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
