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


#include "condor_common.h"
#include "startd.h"
#include "VMMachine.h"
#include "VMManager.h"
#include "vm_common.h"
#include "ipv6_addrinfo.h"

#define VM_UNREGISTER_TIMEOUT 3*vm_register_interval

extern ResMgr* resmgr;
VMManager *vmmanager = NULL;

/* Interfaces for class VMManager */
VMManager::VMManager()
{
	m_vm_unrg_tid = -1;
	host_usable = 1;
	m_vm_registered_num = 0;
	allowed_vm_list = NULL;
}

VMManager::~VMManager()
{
	cancelUnRegisterTimer();

	/* remove all registered VMs */
	m_virtualmachines.Rewind();

	VMMachine *i;
	while( m_virtualmachines.Next(i) ) {
		// Destructor will detach each virtual machine from vmmanger 
		delete(i);
		if( m_vm_registered_num == 0 ) {
			if( host_usable == 0 ) {
				host_usable = 1;
				if( resmgr ) {
					resmgr->eval_and_update_all();
				}
			}
		}
	}

	if( allowed_vm_list )
		delete(allowed_vm_list);
}

int
VMManager::numOfVM(void) const
{
	return m_vm_registered_num;
}

bool 
VMManager::isRegistered(const char *addr, int update_time)
{
	m_virtualmachines.Rewind();

	VMMachine *i;
	bool result = FALSE;
	while( m_virtualmachines.Next(i) ) {
		result = i->match(addr);

		if( result ) {
			if( update_time )
				i->updateTimeStamp();

			return TRUE;
		}
	}

	return FALSE;
}

void 
VMManager::attach(VMMachine *o) 
{
	m_virtualmachines.Append(o);
	m_vm_registered_num++;
	startUnRegisterTimer();
	dprintf( D_ALWAYS,"Virtual machine(%s) is attached\n", o->getVMSinful());
}

void 
VMManager::detach(VMMachine *o) 
{
	m_virtualmachines.Delete(o);
	m_vm_registered_num--;

	if( m_vm_registered_num == 0 )
		cancelUnRegisterTimer();

	dprintf(D_ALWAYS,"Virtual machine(%s) is detached\n", o->getVMSinful());
}

void 
VMManager::allNotify( const char *except_ip, int cmd, void *data )
{
	m_virtualmachines.Rewind();

	VMMachine *i;
	while( m_virtualmachines.Next(i) ) {
		if( except_ip && i->match(except_ip) ) {
			i->updateTimeStamp();
			continue;
		}

		i->sendEventToVM(cmd, data);
	}
}

void 
VMManager::checkRegisterTimeout(void)
{
	int timeout = VM_UNREGISTER_TIMEOUT;
	m_virtualmachines.Rewind();

	VMMachine *i;
	time_t now;
	now = time(NULL);

	while( m_virtualmachines.Next(i) ) {
		if( ( now - i->getTimeStamp() ) > timeout ) {
		// Destructor will detach timeout virtual machine from vmmanger 
			delete(i);
			if( m_vm_registered_num == 0 ) {
				if( host_usable == 0 ) {
					host_usable = 1;
					if( resmgr ) {
						resmgr->eval_and_update_all();
					}
				}
			}
		}
	}
}

void 
VMManager::printAllElements(void) 
{
	m_virtualmachines.Rewind();

	VMMachine *i;
	while( m_virtualmachines.Next(i) ) {
		i->print();
	}
}

void 
VMManager::startUnRegisterTimer(void)
{
	if( m_vm_unrg_tid >= 0 ) {
		//Unregister Timer already started
		return;
	}

	m_vm_unrg_tid = daemonCore->Register_Timer(VM_UNREGISTER_TIMEOUT,
			VM_UNREGISTER_TIMEOUT,
			(TimerHandlercpp)&VMManager::checkRegisterTimeout,
			"poll_registered_vm", this);

	if( m_vm_unrg_tid < 0 ) {
		EXCEPT("Can't register DaemonCore Timer");
	}

	dprintf( D_FULLDEBUG, "Starting vm unregister timer.\n");
}

void 
VMManager::cancelUnRegisterTimer(void)
{
	int rval;
	if( m_vm_unrg_tid != -1 ) {
		rval = daemonCore->Cancel_Timer(m_vm_unrg_tid);

		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel vm unregister timer (%d): daemonCore error\n", m_vm_unrg_tid);
		}else
			dprintf( D_FULLDEBUG, "Canceled vm unregister timer (%d)\n", m_vm_unrg_tid);
	}

	m_vm_unrg_tid = -1;
}

void
vmapi_create_vmmanager(const char *list)
{
	StringList tmplist;

	if( !list )
		return;

	if( vmmanager ) delete(vmmanager);

	tmplist.initializeFromString(list);
	if( tmplist.number() == 0 )
		return;

	char *vm_name;

	StringList *vm_list;
	vm_list = new StringList();
	tmplist.rewind();
	while( (vm_name = tmplist.next()) ) {
		// checking valid IP
		addrinfo_iterator iter;
		int ret;
		ret = ipv6_getaddrinfo(vm_name, NULL, iter);
		if (ret != 0) continue;

		addrinfo* ai = iter.next();
		if (ai) {
			condor_sockaddr addr(ai->ai_addr);
			vm_list->append(addr.to_ip_string().c_str());
		}
	}

	if( vm_list->number() > 0 ) {
		vmmanager = new VMManager();
		vmmanager->allowed_vm_list = vm_list;
	}else {
		dprintf( D_ALWAYS, "There is no valid name of virtual machine\n");
		delete(vm_list);
	}
}

void
vmapi_destroy_vmmanager(void)
{
	if( vmmanager ) {
		delete(vmmanager);
		vmmanager = NULL;
	}

}


bool
vmapi_is_host_machine(void)
{
	if( vmmanager )
		return TRUE;
	else
		return FALSE;
}
