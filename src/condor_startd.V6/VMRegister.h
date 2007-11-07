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

#ifndef _VM_REGISTER_H
#define _VM_REGISTER_H

#include "condor_common.h"

bool vmapi_is_virtual_machine(void);
void vmapi_create_vmregister(char *);
void vmapi_destroy_vmregister(void);

class VMRegister;
extern VMRegister *vmregister;

class VMRegister : public Service {
public:
	VMRegister(char *host_name);
	virtual ~VMRegister();

	void registerVM(void);
	void sendEventToHost(int, void*);
	void requestHostClassAds(void);
	char *getHostSinful(void);

	void startRegisterTimer(void);
	void cancelRegisterTimer(void);

	int vm_usable;
	ClassAd* host_classad;
private:
	int m_vm_rg_tid;
	char *m_vm_host_name;
	Daemon *m_vm_host_daemon; // startd daemon on host machine 
};

#endif /* _VM_REGISTER_H */
