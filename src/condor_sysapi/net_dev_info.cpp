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
#include "condor_config.h"


static bool net_devices_cached = false;
static std::vector<NetworkDeviceInfo> net_devices_cache;
static bool net_devices_cache_want_ipv4;
static bool net_devices_cache_want_ipv6;

#if WIN32

#include <Ws2ipdef.h>
#include <iphlpapi.h>

bool sysapi_get_network_device_info_raw(std::vector<NetworkDeviceInfo> &devices, bool want_ipv4, bool want_ipv6)
{
#if 0
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

		condor_sockaddr addr((struct sockaddr*)&interfaces[i].iiAddress);
		if(!addr.is_valid()) { continue; }

		bool is_up = interfaces[i].iiFlags & IFF_UP;
		devices.emplace_back() = {"", "", addr, is_up};
	}

	delete [] interfaces;
	closesocket(sock);
	return true;
#else
	ULONG family = AF_UNSPEC;
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX; // Set the flags to pass to GetAdaptersAddresses
	ULONG cbBuf = 16*1024; // start with 16Kb
	PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
	auto_free_ptr addrbuf((char*)malloc(cbBuf));
	int iterations = 0;

	char ipbuf[IP_STRING_BUF_SIZE+1];
	dprintf(D_ALWAYS, "Win32 sysapi_get_network_device_info_raw()\n");

	do {
		ASSERT(addrbuf.ptr())
		pAddresses = (PIP_ADAPTER_ADDRESSES)addrbuf.ptr();
		DWORD retval = GetAdaptersAddresses(family, flags, NULL, pAddresses, &cbBuf);
		if (retval == ERROR_BUFFER_OVERFLOW) {
			addrbuf.set((char*)malloc(cbBuf));
			continue;
		} else if (retval == NO_ERROR) {
			break;
		} else {
			dprintf(D_ERROR, "sysapi_get_network_device_info_raw() error %u from GetAdaptersAddresses\n", retval);
			return 0;
		}
		if (++iterations >= 3) {
			dprintf(D_ERROR, "sysapi_get_network_device_info_raw() giving up after 3 tries to GetAdaptersAddresses, size=%ld\n", cbBuf);
			return 0;
		}
	} while (true);

	auto_free_ptr name;
	int name_space = 0;
	bool is_up = true;
	for (auto * pip = pAddresses; pip; pip = pip->Next) {
		// Convert FriendlyName from a PWCHAR to a utf-8 char*
		size_t wname_sz = wcslen(pip->FriendlyName);
		int name_sz = WideCharToMultiByte(CP_UTF8, 0, pip->FriendlyName, (int)wname_sz, nullptr, 0, nullptr, nullptr);
		if (name_sz >= name_space) {
			name_space = name_sz + 1;
			name.set((char*)malloc(name_space));
		}
		WideCharToMultiByte(CP_UTF8, 0, pip->FriendlyName, wname_sz, name.ptr(), name_sz, nullptr, nullptr);
		name.ptr()[name_sz] = '\0';
		is_up = pip->OperStatus == IfOperStatusUp;
		for (auto * fma = pip->FirstUnicastAddress; fma; fma = fma->Next) {
			if (fma->Address.lpSockaddr) {
				condor_sockaddr addr(fma->Address.lpSockaddr);
				if (!addr.is_valid()) { continue; }
				if (addr.is_ipv4() && !want_ipv4) { continue; }
				if (addr.is_ipv6() && !want_ipv6) { continue; }

				devices.emplace_back() = {name.ptr(), pip->AdapterName, addr, is_up};
			}
		}
	}

	return true;
#endif
}

#elif HAVE_GETIFADDRS
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>

bool sysapi_get_network_device_info_raw(std::vector<NetworkDeviceInfo> &devices, bool want_ipv4, bool want_ipv6)
{
	struct ifaddrs *ifap_list=NULL;
	if( getifaddrs(&ifap_list) == -1 ) {
		dprintf(D_ALWAYS,"getifaddrs failed: errno=%d: %s\n",errno,strerror(errno));
		return false;
	}
	struct ifaddrs *ifap=ifap_list;
	for(ifap=ifap_list;
		ifap;
		ifap=ifap->ifa_next)
	{
		char const *name = ifap->ifa_name;

		if( ! ifap->ifa_addr ) { continue; }
		// if(ifap->ifa_addr->sa_family == AF_INET && !want_ipv4) { continue; }
		// if(ifap->ifa_addr->sa_family == AF_INET6 && !want_ipv6) { continue; }
		//
		// Is there really any reason to check interfaces which aren't
		// AF_INET or AF_INET6?
		//
		switch( ifap->ifa_addr->sa_family ) {
			case AF_INET:
				if( ! want_ipv4 ) { continue; }
				break;
			case AF_INET6:
				if( ! want_ipv6 ) { continue; }
				break;
			default:
				continue;
		}

		condor_sockaddr addr(ifap->ifa_addr);

		if (!addr.is_valid()) {
			continue;
		}

		bool is_up = ifap->ifa_flags & IFF_UP;
		if (IsDebugCategory(D_NETWORK)) {
			std::string ip_str = addr.to_ip_string();
			dprintf(D_NETWORK, "Enumerating interfaces: %s %s %s\n", name, ip_str.c_str(), is_up?"up":"down");
		}
		devices.emplace_back() = {name, "", addr, is_up};
	}
	freeifaddrs(ifap_list);

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

	struct ifconf ifc,prev;
	memset(&ifc, 0, sizeof(ifc));

	const int max_interfaces = 10000;
	int num_interfaces;
	for(num_interfaces=100;num_interfaces<max_interfaces;num_interfaces+=100) {
		prev.ifc_len = ifc.ifc_len;
		ifc.ifc_len = num_interfaces*sizeof(struct ifreq);

		ifc.ifc_req = (struct ifreq *)malloc(ifc.ifc_len);

		if( ifc.ifc_req == NULL ) {
			dprintf(D_ALWAYS,"sysapi_get_network_device_info_raw: out of memory\n");
			return false;
		}

			// EINVAL (on some platforms) indicates that the buffer
			// is not large enough
		if( ioctl(sock, SIOCGIFCONF, &ifc) < 0 && errno != EINVAL ) {
			dprintf(D_ALWAYS,"sysapi_get_network_device_info_raw: ioctlsocket() failed after allocating buffer: (errno %d) %s\n",
					errno,strerror(errno));
			free( ifc.ifc_req );
			return false;
		}

		if( ifc.ifc_len == prev.ifc_len ) {
				// Getting the same length in successive calls to
				// SIOCGIFCONF is the only reliable way to know that
				// the buffer was big enough, because some
				// implementations silently return truncated results
				// if the buffer isn't big enough.
			break;
		}
		free( ifc.ifc_req );
		ifc.ifc_req = NULL;
	}
	if( num_interfaces > max_interfaces ) {
			// something must be going wrong
		dprintf(D_ALWAYS,"sysapi_get_network_device_info_raw: unexpectedly large SIOCGIFCONF buffer: %d (errno=%d)\n",num_interfaces,errno);
		return false;
	}

	num_interfaces = ifc.ifc_len/sizeof(struct ifreq);

	int i;
	for(i=0; i<num_interfaces; i++) {
		struct ifreq *ifr = &ifc.ifc_req[i];
		char const *name = ifr->ifr_name;

		if(ifr->ifr_addr.sa_family == AF_INET && !want_ipv4) { continue; }
		if(ifr->ifr_addr.sa_family == AF_INET6 && !want_ipv6) { continue; }

		condor_sockaddr addr(&ifr->ifr_addr);
		if(!addr.is_valid()) { continue; }

		bool is_up = true;
		devices.emplace_back() = {name, "", addr, is_up};
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
