/***************************************************************
*
* Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#include "internet.h"
#include "network_adapter.linux.h"

#if HAVE_NET_IF_H
# include <net/if.h>
#endif
#if HAVE_LINUX_SOCKIOS_H
# include <linux/sockios.h>
#endif
#if HAVE_LINUX_ETHTOOL_H
# include <linux/ethtool.h>
#endif

// For now, the only wake-on-lan type we use is UDP magic
#if defined(HAVE_LINUX_ETHTOOL_H)
# define WAKE_MASK	( 0 | (WAKE_MAGIC) )
#endif

// Possible values for the above (OR them together) :
//	WAKE_PHY
//	WAKE_UCAST
//	WAKE_MCAST
//	WAKE_BCAST
//	WAKE_ARP
//	WAKE_MAGIC
//	WAKE_MAGICSECURE


/***************************************************************
* LinuxNetworkAdapter class
***************************************************************/

/// Constructor
LinuxNetworkAdapter::LinuxNetworkAdapter ( unsigned int ip_addr ) throw ()
		: UnixNetworkAdapter( ip_addr )
{
}

/// Destructor
LinuxNetworkAdapter::~LinuxNetworkAdapter ( void ) throw()
{
}

bool
LinuxNetworkAdapter::findAdapter( void )
{
# if (HAVE_STRUCT_IFCONF) && (HAVE_STRUCT_IFREQ) && (HAVE_DECL_SIOCGIFCONF)
	struct ifconf	ifc;
	int				sock;
	int				num_req = 3;	// Should be enough for a machine
									// with lo, eth0, eth1

	// Get a 'control socket' for the operations
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		derror( "Cannot get control socket for WOL detection" );
		return false;
	}

	// Loop 'til we've read in all the interfaces, keep increasing
	// the number that we try to read each time
	struct sockaddr_in	in_addr;
	bool				found = false;
	while( !found ) {
		int size	= num_req * sizeof(struct ifreq);
		ifc.ifc_buf	= (char *) calloc( num_req, sizeof(struct ifreq) );
		ifc.ifc_len	= size;

		int status = ioctl( sock, SIOCGIFCONF, &ifc );
		if ( status < 0 ) {
			derror( "ioctl(SIOCGIFCONF)" );
			break;
		}

		// Did we find it in the ifc?
		int				 num = ifc.ifc_len / sizeof(struct ifreq);
		struct ifreq	*ifr = ifc.ifc_req;
		for ( int i = 0;  i < num;  i++, ifr++ ) {
			struct sockaddr_in *in = (struct sockaddr_in*)&(ifr->ifr_addr);
			MemCopy( &in_addr, in, sizeof(struct sockaddr_in) );

			if ( in->sin_addr.s_addr == m_ip_addr ) {
				setName( *ifr );
				found = true;
				break;
			}
		}

		// If the length returned by ioctl() is the same as the size
		// we started with, it probably overflowed.... try again
		if ( (!found) && (ifc.ifc_len == size) ) {
			num_req += 2;
			free( ifc.ifc_buf );
		}
		else {
			break;
		}
	}

	// Get the hardware address
	if ( found ) {
		printf( "LNA::fA getting HW address\n" );
		struct ifreq	ifr;
		getName( ifr );
		int status = ioctl( sock, SIOCGIFHWADDR, &ifr );
		if ( status < 0 ) {
			derror( "ioctl(SIOCGIFHWADDR)" );
		}
		else {
			setHwAddr( ifr );
		}
	}

	// Get the net mask
	if ( found ) {
		printf( "LNA::fA getting net mask\n" );
		struct ifreq	ifr;
		getName( ifr );
		ifr.ifr_addr.sa_family = AF_INET;
		int status = ioctl( sock, SIOCGIFNETMASK, &ifr );
		if ( status < 0 ) {
			derror( "ioctl(SIOCGIFNETMASK)" );
		}
		else {
			setNetMask( ifr );
		}
	}

	// Don't forget to close the socket!
	printf( "LNA::fA closing socket\n" );
	close( sock );

	// Did we succeed?
	if ( found ) {
		dprintf( D_FULLDEBUG,
				 "Found interface %s (%s) that matches %s\n",
				 interfaceName( ),
				 hardwareAddress( ),
				 sin_to_string( &in_addr )
				 );
	}
	else {
		dprintf( D_FULLDEBUG,
				 "No interface for address %s\n",
				 sin_to_string( &in_addr )
				 );
		m_if_name = NULL;
	}

	// Make sure we free up the buffer memory
	free( ifc.ifc_buf );

	// And, we're done
	return found;
# endif
}

bool
LinuxNetworkAdapter::detectWOL ( void )
{
#if (HAVE_DECL_SIOCETHTOOL) && (HAVE_STRUCT_IFREQ)
	int						err;
	struct ethtool_wolinfo	wolinfo;
	struct ifreq			ifr;
	bool					ok = false;

	// Open control socket.
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		dprintf( D_ALWAYS, "Cannot get control socket for WOL detection\n" );
	}

	// Fill in the WOL request and the ioctl request
	wolinfo.cmd = ETHTOOL_GWOL;
	getName( ifr );
	ifr.ifr_data = (caddr_t)(& wolinfo);
	err = ioctl(sock, SIOCETHTOOL, &ifr);
	if ( err < 0 ) {
		derror( "ioctl(SIOCETHTOOL/GWOL)" );
		m_wolopts = 0;
	}
	else {
		m_wolopts = wolinfo.supported;
		ok = true;
	}

	// For now, all we support is the "magic" packet
	if (m_wolopts & WAKE_MASK) {
		m_wol = true;
	}
	dprintf( D_FULLDEBUG, "%s supports Wake-on: %s (raw: 0x%02x)\n",
			 m_if_name, m_wol ? "yes" : "no", m_wolopts );

	close( sock );
	return ok;
#  else
	return false;
#  endif
}
