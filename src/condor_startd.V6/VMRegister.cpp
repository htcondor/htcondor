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
#include "VMRegister.h"
#include "vm_common.h"

VMRegister *vmregister = NULL;

/* Interfaces for class VMRegister */
VMRegister::VMRegister(char *host_name)
{
	vm_usable = 0;
	host_classad = new ClassAd();
	m_vm_rg_tid = -1;
	m_vm_host_daemon = NULL;
	m_vm_host_name = strdup(host_name);

	startRegisterTimer();

	// First try to send register packet 
	registerVM();
}

VMRegister::~VMRegister()
{
	vm_usable = 0;
	cancelRegisterTimer();

	free(m_vm_host_name);
	m_vm_host_name = NULL;
	delete(m_vm_host_daemon);
	m_vm_host_daemon = NULL;

	if( host_classad ) delete host_classad;
}

static bool 
_requestVMRegister(const char *addr)
{
	Daemon hstartd(DT_STARTD, addr);

	//Using TCP
	ReliSock ssock;

	ssock.timeout( VM_SOCKET_TIMEOUT );
	ssock.encode();

	if( !ssock.connect(addr) ) {
		dprintf( D_FULLDEBUG, "Failed to connect to host startd(%s)\n", addr);
		return FALSE;
	}

	if( !hstartd.startCommand(VM_REGISTER, &ssock) ) { 
		dprintf( D_FULLDEBUG, "Failed to send VM_REGISTER command to host startd(%s)\n", addr);
		return FALSE;
	}

	// Send <IP address:port> of virtual machine
	if ( !ssock.put(daemonCore->InfoCommandSinfulString()) ) {
		dprintf( D_FULLDEBUG,
				 "Failed to send VM_REGISTER command's arguments to "
				 "host startd %s\n",
				 addr);
		return FALSE;
	}

	if( !ssock.end_of_message() ) {
		dprintf( D_FULLDEBUG, "Failed to send EOM to host startd %s\n", addr );
		return FALSE;
	}

	//Now, read permission information
	ssock.timeout( VM_SOCKET_TIMEOUT );
	ssock.decode();

	int permission = 0;
	ssock.code(permission);

	if( !ssock.end_of_message() ) {
		dprintf( D_FULLDEBUG, "Failed to receive EOM from host startd(%s)\n", addr );
		return FALSE;
	}

	if( permission > 0 ) {
		// Since now this virtual machine can be used for Condor
		if( vmregister ) {
			if( vmregister->vm_usable == 0 ) {
				vmregister->vm_usable = 1;
				if( resmgr ) {
					resmgr->eval_and_update_all();
				}
			}
		}
	}else {
		// For now this virtual machine can't be used for Condor
		if( vmregister ) {
			if( vmregister->vm_usable == 1 ) {
				vmregister->vm_usable = 0;
				if( resmgr ) {
					resmgr->eval_and_update_all();
				}
			}
		}
	}

	return TRUE;
}

void 
VMRegister::registerVM(void)
{
	// find host startd daemon
	if( !m_vm_host_daemon )
		m_vm_host_daemon = vmapi_findDaemon( m_vm_host_name, DT_STARTD);

	if( !m_vm_host_daemon ) {
		dprintf( D_FULLDEBUG, "Can't find host(%s) Startd daemon\n", m_vm_host_name );
		return;
	}

	// send register command	
	if( _requestVMRegister(m_vm_host_daemon->addr()) == FALSE ) {
		dprintf( D_FULLDEBUG, "Can't send a VM register command and receive the permission\n");
		delete(m_vm_host_daemon);
		m_vm_host_daemon = NULL;
		return;
	}
}

void 
VMRegister::sendEventToHost(int cmd, void *data) 
{
	if( !m_vm_host_daemon )
		return;

	if( !vmapi_sendCommand(m_vm_host_daemon->addr(), cmd, data) ) {
		dprintf( D_FULLDEBUG, "Can't send a VM event command(%s) to the host machine(%s)\n", getCommandString(cmd), m_vm_host_daemon->addr() );
		return;
	}
}

void
VMRegister::requestHostClassAds(void)
{
	// find host startd daemon
	if( !m_vm_host_daemon )
		m_vm_host_daemon = vmapi_findDaemon( m_vm_host_name, DT_STARTD);

	if( !m_vm_host_daemon ) {
		dprintf( D_FULLDEBUG, "Can't find host(%s) Startd daemon\n", m_vm_host_name );
		return;
	}

	ClassAd query_ad;
	SetMyTypeName(query_ad, QUERY_ADTYPE);
	SetTargetTypeName(query_ad, STARTD_ADTYPE);
	query_ad.Assign(ATTR_REQUIREMENTS, true);

	const char *addr = m_vm_host_daemon->addr();
	Daemon hstartd(DT_STARTD, addr);
	ReliSock ssock;

	ssock.timeout( VM_SOCKET_TIMEOUT );
	ssock.encode();

	if( !ssock.connect(addr) ) {
		dprintf( D_FULLDEBUG, "Failed to connect to host startd(%s)\n to get host classAd", addr);
		return;
	}

	if(!hstartd.startCommand( QUERY_STARTD_ADS, &ssock )) {
		dprintf( D_FULLDEBUG, "Failed to send QUERY_STARTD_ADS command to host startd(%s)\n", addr);
		return;
	}

	if( !putClassAd(&ssock, query_ad) ) {
		dprintf(D_FULLDEBUG, "Failed to send query Ad to host startd(%s)\n", addr);
	}

	if( !ssock.end_of_message() ) {
		dprintf(D_FULLDEBUG, "Failed to send query EOM to host startd(%s)\n", addr);
	}

	// Read host classAds
	ssock.timeout( VM_SOCKET_TIMEOUT );
	ssock.decode();
	int more = 1, num_ads = 0;
	ClassAdList adList;
	ClassAd *ad;

	while (more) {
		if( !ssock.code(more) ) {
			ssock.end_of_message();
			return;
		}

		if(more) {
			ad = new ClassAd;
			if( !getClassAd(&ssock, *ad) ) {
				ssock.end_of_message();
				delete ad;
				return;
			}

			adList.Insert(ad);
			num_ads++;
		}
	}

	ssock.end_of_message();

	dprintf(D_FULLDEBUG, "Got %d classAds from host\n", num_ads);

	// Although we can get more than one classAd from host machine, 
	// we use only the first one classAd
	adList.Rewind();
	ad = adList.Next();

	// Get each Attribute from the classAd
	// added "HOST_" in front of each Attribute name

	for ( auto itr = ad->begin(); itr != ad->end(); itr++ ) {
		std::string attr;
		attr += "HOST_";
		attr += itr->first;

		// Insert or Update an attribute to host_classAd in a VMRegister object
		ExprTree * pTree = itr->second->Copy();
		host_classad->Insert(attr, pTree);
	}
}

const char *
VMRegister::getHostSinful(void)
{
	if( !m_vm_host_daemon )
		return NULL;

	return m_vm_host_daemon->addr();
}

void 
VMRegister::startRegisterTimer(void)
{
	if( m_vm_rg_tid >= 0 ) {
		//Register Timer already started
		return;
	}

	m_vm_rg_tid = daemonCore->Register_Timer( vm_register_interval,
			vm_register_interval,
			(TimerHandlercpp)&VMRegister::registerVM,
			"register_vm_to_host", this);

	if( m_vm_rg_tid < 0 ) {
		EXCEPT("Can't register DaemonCore Timer");
	}

	dprintf( D_FULLDEBUG, "Started vm register timer.\n");
}

void 
VMRegister::cancelRegisterTimer(void)
{
	int rval;
	if( m_vm_rg_tid != -1 ) {
		rval = daemonCore->Cancel_Timer(m_vm_rg_tid);

		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel vm register timer (%d): daemonCore error\n", m_vm_rg_tid);
		}else
			dprintf( D_FULLDEBUG, "Canceled vm register timer (%d)\n", m_vm_rg_tid);
	}

	m_vm_rg_tid = -1;
}

void 
vmapi_create_vmregister(char *host_name)
{
	if( !host_name )
		return;

	if(vmregister) delete(vmregister);

	vmregister = new VMRegister(host_name);
}

void 
vmapi_destroy_vmregister(void)
{
	delete(vmregister);
	vmregister = NULL;
}

bool
vmapi_is_virtual_machine(void)
{
	if( vmregister )
		return TRUE;
	else
		return FALSE;
}
