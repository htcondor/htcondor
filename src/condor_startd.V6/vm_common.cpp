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
#include "VMManager.h"
#include "VMRegister.h"
#include "VMMachine.h"
#include "vm_common.h"
#include "ipv6_hostname.h"

extern VMManager *vmmanager;

int vm_register_interval = 60; //seconds

bool 
vmapi_is_allowed_host_addr(const char *addr)
{
	// The format of addr is <ip_addr:port>. Ex.) <10.3.4.5:2345>
	if( !addr || !is_valid_sinful(addr))
		return FALSE;

	if( vmapi_is_virtual_machine() == TRUE ) {
		// Packets are only allowed from the host machine
		const char* hostip;
		hostip = vmregister->getHostSinful();

		if( !hostip )
			return FALSE;

		if( !strcmp(addr, hostip))
			return TRUE;

	}

	return FALSE;
}

bool 
vmapi_is_allowed_vm_addr(const char *addr)
{
	// The format of addr is <ip_addr:port>. Ex.) <10.3.4.5:2345>
	if( !addr || !is_valid_sinful(addr))
		return FALSE;

	if ( vmapi_is_host_machine() == TRUE ) {
		if( !vmmanager->allowed_vm_list || vmmanager->allowed_vm_list->number() == 0 )
			return FALSE;

		MyString ip;
		if( ! sinful_to_ipstr(addr, ip) ) { return false; }

		char *vm_name;
		vmmanager->allowed_vm_list->rewind();
		while( (vm_name = vmmanager->allowed_vm_list->next()) ) {
			if( !strcmp(ip.c_str(), vm_name) ) {
				return TRUE;
			}
		}

		dprintf( D_FULLDEBUG, "IP(%s) is not an allowed virtual machine\n", ip.c_str());
	}

	return FALSE;
}

int 
vmapi_num_of_registered_vm(void)
{
	if( vmapi_is_host_machine() == FALSE )
		return 0;

	return vmmanager->numOfVM();
}

int 
vmapi_register_cmd_handler(const char *addr, int *permission)
{
	if( vmapi_is_host_machine() == FALSE )
		return FALSE;

	if( !vmapi_is_allowed_vm_addr(addr) )
		return FALSE;

	// Check if register packet comes from host machine itself
	if( !strcmp(addr, daemonCore->InfoCommandSinfulString()) ) {
		return FALSE;
	}

	// Check whether this virtual machine is already registered
	if( vmmanager->isRegistered(addr, 1) == FALSE ) {
		// Constructor will attach new virtual machine to vmmanger 
		// During attaching this vm to vmmanager, Unregister_Timer will also start.
		VMMachine* vmmachine = new VMMachine(vmmanager, addr);
		vmmachine->print();
	}

	// We decide the permission of virtual machine depending on the status of host machine.
	// If the status of host machine is either "Owner" or "Unclaimed", the permission is set to 
	// the number of registered virtual machines. 
	// Otherwise, the permission is set to 0.
	//
	// According to current policy, while virtual machines are used for condor, host machine will not be used any more.
	// So when the permission is larger than 0, "Start" for host machine is set to "False".
	
	*permission = 0;

	if( resmgr ) {
		State s = resmgr->state();
		if( ( s == owner_state ) || ( s == unclaimed_state )) {
			*permission = vmmanager->numOfVM();

			if( vmmanager->host_usable == 1 ) {
				//change the "START" attribute of host machine to "False".
				vmmanager->host_usable = 0;
				resmgr->eval_and_update_all();
			}
		}
	}

	return TRUE;
}

bool 
vmapi_is_usable_for_condor(void)
{
	if( ( ( vmapi_is_host_machine() == TRUE ) && 
			( vmmanager->host_usable == 0 ) ) || 
		( ( vmapi_is_virtual_machine() == TRUE ) && 
		  ( vmregister->vm_usable == 0 ) ) ) {
		return FALSE;
	}

	return TRUE;
}

bool 
vmapi_is_my_machine(char *h1)
{
	if( !h1 )
		return FALSE;

	if( !strcmp(h1, get_local_fqdn().c_str()) )
		return TRUE;

	// TODO: Picking IPv4 arbitrarily.
	std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();
	if( !strcmp(h1, my_ip.c_str()) )
		return TRUE;

	return FALSE;
}


void
vmapi_request_host_classAd(void)
{
	if( vmapi_is_virtual_machine() == FALSE )
		return;

	vmregister->requestHostClassAds();
}

ClassAd* 
vmapi_get_host_classAd(void)
{
	if( vmapi_is_virtual_machine() == FALSE )
		return NULL;

	return vmregister->host_classad;
}

// XXX: Refactor for use with calls like _requestVMRegister
MSC_DISABLE_WARNING(6262) // function uses 60820 bytes of stack
bool 
vmapi_sendCommand(const char *addr, int cmd, void *data)
{
	Daemon hstartd(DT_STARTD, addr);

	SafeSock ssock;

	ssock.timeout( VM_SOCKET_TIMEOUT );
	ssock.encode();
	
	if( !ssock.connect(addr) ) {
		dprintf( D_FULLDEBUG, "Failed to connect to VM startd(%s)\n", addr);
		return FALSE;
	}

	if( !hstartd.startCommand(cmd, &ssock) ) { 
		dprintf( D_FULLDEBUG, "Failed to send UDP command(%s) to VM startd %s\n", getCommandString(cmd), addr);
		return FALSE;
	}

	if ( !ssock.put(daemonCore->InfoCommandSinfulString()) ) {
		dprintf( D_FULLDEBUG,
				 "Failed to send command(%s)'s arguments to "
				 "VM startd %s: %s\n",
				 getCommandString(cmd), addr,
				 daemonCore->InfoCommandSinfulString() );
		return FALSE;
	}

	if( data ) {
		// need to implement in future
		// According to command type, void data pointer should be type casted.
	}

	if( !ssock.end_of_message() ) {
		dprintf( D_FULLDEBUG, "Failed to send EOM to VM startd %s\n", addr );
		return FALSE;
	}

	return TRUE;
}
MSC_RESTORE_WARNING(6262) // function uses 60820 bytes of stack

// Heavily cut and paste from resolveNames in condor_tools/tool.C
static Daemon*
_FindDaemon( const char *host_name, daemon_t real_dt, Daemon *pool)
{
	Daemon* d = NULL;
	char* tmp = NULL;
	const char* host = NULL;
	bool had_error = FALSE;
	const char *pool_addr = NULL;

	if( !pool || !host_name )
		return NULL;

	AdTypes	adtype;
	switch( real_dt ) {
	case DT_MASTER:
		adtype = MASTER_AD;
		break;
	case DT_STARTD:
		adtype = STARTD_AD;
		break;
	case DT_SCHEDD:
		adtype = SCHEDD_AD;
		break;
	default:
		dprintf( D_FULLDEBUG, "Unrecognized daemon type while resolving names\n" );
		return NULL;
	}

	CondorQuery query(adtype);
	ClassAd* ad;
	ClassAdList ads;

	CondorError errstack;
	QueryResult q_result;

	pool_addr = pool->addr();
	q_result = query.fetchAds(ads, pool_addr, &errstack);

	if( q_result != Q_OK ) {
		dprintf( D_FULLDEBUG, "%s\n", errstack.getFullText(true).c_str() );
		dprintf( D_FULLDEBUG, "ERROR: can't connect to %s\n", pool->idStr());
		had_error = TRUE;
	} else if( ads.Length() <= 0 ) { 
		dprintf( D_FULLDEBUG, "Found no ClassAds when querying pool (%s)\n", pool->name() );
		had_error = TRUE;
	}

	if( had_error ) {
		dprintf( D_FULLDEBUG, "Can't find address for %s %s\n", 
					daemonString(real_dt), host_name );
		return NULL;
	}

	ads.Rewind();
	while( !d && (ad = ads.Next()) ) {
		if( real_dt == DT_STARTD && ! strchr(host_name, '@') ) {
			host = get_host_part( host_name );
			ad->LookupString( ATTR_MACHINE, &tmp );
			if( ! tmp ) {
				// weird, malformed ad.
				// should we print a warning?
				continue;
			}
			if( strcasecmp(tmp, host) ) {		// no match
				free( tmp );
				tmp = NULL;
				continue;
			}

			ad->Assign( ATTR_NAME, host_name);
			d = new Daemon( ad, real_dt, pool_addr );
			free( tmp );
			tmp = NULL;

		} else {  // daemon type != DT_STARTD or there's an '@'
			// everything else always uses ATTR_NAME
			ad->LookupString( ATTR_NAME, &tmp );
			if( ! tmp ) {
				// weird, malformed ad.
				// should we print a warning?
				continue;
			}
			if( ! strcasecmp(tmp, host_name) ) {
				d = new Daemon( ad, real_dt, pool_addr );
			}
			free( tmp );
			tmp = NULL;
		} // daemon type
	} // while( each ad from the collector )

	if( d ) {
		dprintf( D_FULLDEBUG, "Found %s of %s at %s\n", daemonString(real_dt), host_name, d->addr());
		return d;
	} else {
		dprintf( D_FULLDEBUG, "Can't find address for %s %s\n",
				daemonString(real_dt), host_name );
	}

	return NULL;
}

Daemon*
vmapi_findDaemon( const char *host_name, daemon_t real_dt)
{
	Daemon *collector;
	Daemon *found;
	CollectorList* collectors = daemonCore->getCollectorList();
	collectors->rewind();
	while (collectors->next(collector)) {
		found = _FindDaemon( host_name, real_dt, collector);
		if( !found )
			continue;
		else
			return found;
	}

	return NULL;
}
