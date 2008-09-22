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
		return NULL;
	}

	// Loop 'til we've read in all the interfaces, keep increasing
	// the number that we try to read each time
	struct sockaddr_in	 *in_addr = NULL;
	struct ifreq		 *ifr = NULL;
	while( NULL == ifr ) {
		int size	= num_req * sizeof(struct ifreq);
		ifc.ifc_buf	= (char *) calloc( num_req, sizeof(struct ifreq) );
		ifc.ifc_len	= size;

		int status = ioctl( sock, SIOCGIFCONF, &ifc );
		if ( status < 0 ) {
			derror( "ioctl(..,SIOCGIFCONF,..)" );
			break;
		}

		// Did we find it?
		int				 num = ifc.ifc_len / sizeof(struct ifreq);
		struct ifreq	*tmp_ifr = ifc.ifc_req;
		for ( int i = 0;  i < num;  i++ ) {
			in_addr = (struct sockaddr_in *) &(tmp_ifr->ifr_addr.sa_data);
			if ( ntohl(in_addr->sin_addr.s_addr) == m_ip_addr ) {
				ifr = tmp_ifr;
				break;
			}
			tmp_ifr++;
		}

		// If the length returned by ioctl() is the same as the size
		// we started with, it probably overflowed.... try again
		if ( (ifr == NULL) && (ifc.ifc_len == size) ) {
			num_req += 2;
			free( ifc.ifc_buf );
		}
	}

	// Get the hardware address
	if ( ifr ) {
		strcpy( ifr->ifr_name, m_if_name );
		int status = ioctl( sock, SIOCGIFHWADDR, ifr );
		if ( status < 0 ) {
			derror( "ioctl(..,SIOCGIFHWADDR,..)" );
		}
		else {
			setHwAddr( ifr );
		}
	}

	// Get the net mask
	if ( ifr ) {
		strcpy( ifr->ifr_name, m_if_name );
		int status = ioctl( sock, SIOCGIFNETMASK, &ifr );
		if ( status < 0 ) {
			derror( "ioctl(..,SIOCGIFNETMASK,..)" );
		}
		else {
			setNetMask( ifr );
		}
	}

	// Don't forget to close the socket!
	close( sock );

	// Did we succeed?
	if ( ifr ) {
		dprintf( D_FULLDEBUG,
				 "Found interface %s (%s) that matches %s\n",
				 interfaceName( ),
				 hardwareAddress( ),
				 sin_to_string( in_addr )
				 );
		setIFR(ifr);
		m_if_name = strdup( ifr->ifr_name );
	}
	else {
		dprintf( D_FULLDEBUG,
				 "No interface for address %s\n",
				 sin_to_string( in_addr )
				 );
		m_if_name = NULL;
	}

	// Make sure we free up the buffer memory
	free( ifc.ifc_buf );

	// And, we're done
	return m_if_name == NULL;
# endif
}

bool
LinuxNetworkAdapter::detectWOL ( void )
{
#if (HAVE_DECL_SIOCETHTOOL) && (HAVE_STRUCT_IFREQ)
	int						err;
	struct ethtool_wolinfo	wolinfo;
	struct ifreq			ifr;

	// Copy our IFR
	getIFR( &ifr );

	// Open control socket.
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		dprintf( D_ALWAYS, "Cannot get control socket for WOL detection\n" );
		return false;
	}

	wolinfo.cmd = ETHTOOL_GWOL;
	ifr.ifr_data = (caddr_t)&wolinfo;
	err = ioctl(sock, SIOCETHTOOL, &ifr);
	if (errno == EOPNOTSUPP) {
		dprintf( D_ALWAYS, "SIOCETHTOOL not supported by this kernel\n" );
		return false;
	}
	else if ( err ) {
		derror( "ioctl(..,SIOCETHTOOL,..)" );
		return false;
	}

	m_wolopts = wolinfo.supported;

	// For now, all we support is the "magic" packet
	if (m_wolopts & WAKE_MASK)
		m_wol = true;

	dprintf( D_FULLDEBUG, "%s supports Wake-on: %s (raw: 0x%02x)\n",
			 m_if_name, m_wol ? "yes" : "no", m_wolopts );

	close( sock );
	return true;
#  else
	return false;
#  endif
}
