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
