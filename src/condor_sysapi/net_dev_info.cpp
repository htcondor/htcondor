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
#include "condor_debug.h"

#include "sysapi.h"
#include "sysapi_externs.h"

#include "condor_sockaddr.h"


static bool net_devices_cached = false;
static std::vector<NetworkDeviceInfo> net_devices_cache;
static bool net_devices_cache_want_ipv4;
static bool net_devices_cache_want_ipv6;

#if WIN32

#include <Ws2ipdef.h>

bool sysapi_get_network_device_info_raw(std::vector<NetworkDeviceInfo> &devices, bool want_ipv4, bool want_ipv6)
{
	int i,num_interfaces=0,max_interfaces=20;
	LPINTERFACE_INFO interfaces=NULL;
	DWORD bytes_out=0;
	SOCKET sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if( sock == INVALID_SOCKET ) {
		dprintf(D_ALWAYS,"sysapi_get_network_device_info_raw: socket() failed: (errno %d)\n",
				WSAGetLastError());
		return false;
	}

	while( true ) {
		interfaces = new INTERFACE_INFO[max_interfaces];

		int rc = WSAIoctl(
			sock,
			SIO_GET_INTERFACE_LIST,
			NULL,
			0,
			interfaces,
			sizeof(INTERFACE_INFO)*max_interfaces,
			&bytes_out,
			NULL,
			NULL);

		if( rc == 0 ) { // success
			num_interfaces = bytes_out/sizeof(INTERFACE_INFO);
			break;
		}

		delete [] interfaces;

		int error = WSAGetLastError();
		if( error == WSAEFAULT ) { // not enough space in buffer
			max_interfaces *= 2;
			continue;
		}
		dprintf(D_ALWAYS,"SIO_GET_INTERFACE_LIST failed: %d\n",error);
		closesocket(sock);
		return false;
	}

	for(i=0;i<num_interfaces;i++) {
		char const *ip = NULL;
		if( interfaces[i].iiAddress.Address.sa_family == AF_INET && !want_ipv4) { continue; }
		if( interfaces[i].iiAddress.Address.sa_family == AF_INET6 && !want_ipv6) { continue; }
		if( interfaces[i].iiAddress.Address.sa_family == AF_INET ||
			interfaces[i].iiAddress.Address.sa_family == AF_INET6) {
			ip = inet_ntoa(((struct sockaddr_in *)&interfaces[i].iiAddress)->sin_addr);
		}
		if( ip ) {
			bool is_up = interfaces[i].iiFlags & IFF_UP;
			NetworkDeviceInfo inf("",ip, is_up);
			devices.push_back(inf);
		}
	}

	delete [] interfaces;
	closesocket(sock);
	return true;
}

#elif defined(SIOCGIFCONF)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

bool sysapi_get_network_device_info_raw(std::vector<NetworkDeviceInfo> &devices, bool want_ipv4, bool want_ipv6)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if( sock == -1 ) {
		dprintf(D_ALWAYS,"sysapi_get_network_device_info_raw: socket() failed: (errno %d) %s\n",
				errno,strerror(errno));
		return false;
	}


	const int maxSize = 10000;
	int previousSize = 0;
	struct ifconf ifc;
	for( int size = 100; size <= maxSize; size += 100 ) {
		// If we don't zero the list, we'll get random garbage and
		// not be able to tell.
		ifc.ifc_len = size * sizeof(struct ifreq);
		ifc.ifc_req = (struct ifreq *)calloc( size, sizeof(struct ifreq) );

		if( ifc.ifc_req == NULL ) {
			dprintf( D_ALWAYS, "sysapi_get_network_device_info_raw: failed to allocate memory for network interface list, trying to read %u of them.\n", size );
			return false;
		}

		// On some platforms, EINVAL means buffer-too-small.
		if( ioctl( sock, SIOCGIFCONF, & ifc ) < 0 && errno != EINVAL ) {
			dprintf( D_ALWAYS, "sysapi_get_network_device_info_raw: ioctlsocket() failed after allocating buffer: (errno %d) %s\n", errno, strerror(errno));
			free( ifc.ifc_req );
			return false;
		}

		// Only start looking at the interfaces once we're certain
		// we have all of them (because we increased the size of
		// returnable array, but didn't get anything more returned).
		if( previousSize != ifc.ifc_len ) {
			previousSize = ifc.ifc_len;
			free( ifc.ifc_req );
			ifc.ifc_req = NULL;
			continue;
		}
	}

	if( ifc.ifc_req == NULL ) {
		dprintf( D_ALWAYS, "Found too many network interfaces (more than %u), failing.\n", maxSize );
		return false;
	}


	int num_interfaces = ifc.ifc_len/sizeof(struct ifreq);

	char ip_buf[INET6_ADDRSTRLEN];
	for(int i=0; i<num_interfaces; i++) {
		struct ifreq *ifr = &ifc.ifc_req[i];
		char const *name = ifr->ifr_name;
		const char* ip = NULL;

		if(ifr->ifr_addr.sa_family == AF_INET && !want_ipv4) { continue; }
		if(ifr->ifr_addr.sa_family == AF_INET6 && !want_ipv6) { continue; }

		// condor_sockaddr() gets really cranky otherwise.
		if( ifr->ifr_addr.sa_family == AF_UNSPEC ) { continue; }
		condor_sockaddr addr(&ifr->ifr_addr);
		ip = addr.to_ip_string(ip_buf, INET6_ADDRSTRLEN);
		if(!ip) { continue; }

		bool is_up = true;
		NetworkDeviceInfo inf(name,ip,is_up);
		devices.push_back(inf);
	}
	free( ifc.ifc_req );

	return true;
}

#else

#error sysapi_get_network_device_info_raw() must be implemented for this platform

#endif

bool sysapi_get_network_device_info(std::vector<NetworkDeviceInfo> &devices, bool want_ipv4, bool want_ipv6)
{
	if( net_devices_cached && want_ipv4 == net_devices_cache_want_ipv4 && want_ipv6 == net_devices_cache_want_ipv6) {
		devices = net_devices_cache;
		return true;
	}

	bool rc = sysapi_get_network_device_info_raw(devices, want_ipv4, want_ipv6);

	if( rc ) {
		net_devices_cached = true;
		net_devices_cache = devices;
		net_devices_cache_want_ipv4 = want_ipv4;
		net_devices_cache_want_ipv6 = want_ipv6;
	}
	return rc;
}

void sysapi_clear_network_device_info_cache() {
	net_devices_cached = false;
}
