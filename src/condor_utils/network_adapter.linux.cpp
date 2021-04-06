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
#include "condor_debug.h"
#include "condor_uid.h"
#include "internet.h"
#include "network_adapter.linux.h"

#if HAVE_NET_IF_H
#  include <net/if.h>
#endif
#if HAVE_LINUX_SOCKIOS_H
#  include <linux/sockios.h>
#endif
#if HAVE_LINUX_TYPES_H
#  include <linux/types.h>
#endif
#if HAVE_OS_TYPES_H
#  include <os_types.h>
#endif
#if HAVE_LINUX_ETHTOOL_H
#  include <linux/ethtool.h>
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
LinuxNetworkAdapter::LinuxNetworkAdapter ( const condor_sockaddr& ip_addr ) noexcept
		: UnixNetworkAdapter( ip_addr )
{
	m_wol_support_mask = 0;
	m_wol_enable_mask = 0;
}

LinuxNetworkAdapter::LinuxNetworkAdapter ( const char *name ) noexcept
		: UnixNetworkAdapter( name )
{
	m_wol_support_mask = 0;
	m_wol_enable_mask = 0;
}

/// Destructor
LinuxNetworkAdapter::~LinuxNetworkAdapter ( void ) noexcept
{
}

bool
LinuxNetworkAdapter::findAdapter( const condor_sockaddr& ip_addr )
{
	bool			found = false;
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
	//struct sockaddr_in	in_addr;
	condor_sockaddr addr;
	ifc.ifc_buf = NULL;

	// [TODO:IPV6]
	// ifreq never returns IPv6 address
	// should change to getifaddrs()
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
			//struct sockaddr_in *in = (struct sockaddr_in*)&(ifr->ifr_addr);
			condor_sockaddr in(&ifr->ifr_addr);
			//MemCopy( &in_addr, in, sizeof(struct sockaddr_in) );
			addr = in;

			//if ( in->sin_addr.s_addr == ip_addr ) {
			if ( in.compare_address(ip_addr) ) {
				setIpAddr( *ifr );
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
			ifc.ifc_buf = NULL;
		}
		else {
			break;
		}
	}

	// Make sure we free up the buffer memory
	if ( ifc.ifc_buf ) {
		free( ifc.ifc_buf );
	}

	if ( found ) {
		dprintf( D_FULLDEBUG,
				 "Found interface %s that matches %s\n",
				 interfaceName( ),
				 addr.to_sinful().c_str()
				 );
	}
	else
	{
		m_if_name = NULL;
		dprintf( D_FULLDEBUG,
				 "No interface for address %s\n",
				 addr.to_sinful().c_str()
				 );
	}

	// Don't forget to close the socket!
	close( sock );

#endif
	return found;

}

bool
LinuxNetworkAdapter::findAdapter( const char *name )
{
	bool			found = false;
# if (HAVE_STRUCT_IFCONF) && (HAVE_STRUCT_IFREQ) && (HAVE_DECL_SIOCGIFCONF)
	struct ifreq	ifr;
	int				sock;

	// Get a 'control socket' for the operations
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		derror( "Cannot get control socket for WOL detection" );
		return false;
	}

	// Loop 'til we've read in all the interfaces, keep increasing
	// the number that we try to read each time
	getName( ifr, name );
	int status = ioctl( sock, SIOCGIFADDR, &ifr );
	if ( status < 0 ) {
		derror( "ioctl(SIOCGIFADDR)" );
	}
	else {
		found = true;
		setIpAddr( ifr );
	}

	if ( found ) {
		dprintf( D_FULLDEBUG,
				 "Found interface %s with ip %s\n",
				 name,
				 m_ip_addr.to_ip_string().c_str()
				 );
	}
	else
	{
		m_if_name = NULL;
		dprintf( D_FULLDEBUG, "No interface for name %s\n", name );
	}

	// Don't forget to close the socket!
	close( sock );

#endif
	return found;

}

bool
LinuxNetworkAdapter::getAdapterInfo( void )
{
	bool			ok = true;
# if (HAVE_STRUCT_IFCONF) && (HAVE_STRUCT_IFREQ) && (HAVE_DECL_SIOCGIFCONF)
	struct ifreq	ifr;
	int				sock;
	int				status;

	// Get a 'control socket' for the operations
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		derror( "Cannot get control socket for WOL detection" );
		return false;
	}

	// Get the hardware address
	getName( ifr );
	status = ioctl( sock, SIOCGIFHWADDR, &ifr );
	if ( status < 0 ) {
		derror( "ioctl(SIOCGIFHWADDR)" );
	}
	else {
		setHwAddr( ifr );
	}

	// Get the net mask
	getName( ifr );
	ifr.ifr_addr.sa_family = AF_INET;
	status = ioctl( sock, SIOCGIFNETMASK, &ifr );
	if ( status < 0 ) {
		derror( "ioctl(SIOCGIFNETMASK)" );
	}
	else {
		setNetMask( ifr );
	}

	// And, we're done
	close(sock);
# endif
	return ok;
}

bool
LinuxNetworkAdapter::detectWOL ( void )
{
	bool					ok = false;
#if (HAVE_DECL_SIOCETHTOOL) && (HAVE_STRUCT_IFREQ) && (HAVE_LINUX_ETHTOOL_H)
	int						err;
	struct ethtool_wolinfo	wolinfo;
	struct ifreq			ifr;

	memset(&ifr, '\0', sizeof(struct ifreq));

	// Open control socket.
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		dprintf( D_ALWAYS, "Cannot get control socket for WOL detection\n" );
		return false;
	}

	// Fill in the WOL request and the ioctl request
	wolinfo.cmd = ETHTOOL_GWOL;
	getName( ifr );
	ifr.ifr_data = (char *)(& wolinfo);

	priv_state saved_priv = set_priv( PRIV_ROOT );
	err = ioctl(sock, SIOCETHTOOL, &ifr);
	set_priv( saved_priv );

	if ( err < 0 ) {
		if ( (EPERM != errno) || (geteuid() == 0) ) {
			derror( "ioctl(SIOCETHTOOL/GWOL)" );
			dprintf( D_ALWAYS,
					 "You can safely ignore the above error if you're not"
					 " using hibernation\n" );
		}
		m_wol_support_mask = 0;
		m_wol_enable_mask = 0;
	}
	else {
		m_wol_support_mask = wolinfo.supported;
		m_wol_enable_mask = wolinfo.wolopts;
		ok = true;
	}

	// For now, all we support is the "magic" packet
	setWolBits( NetworkAdapterBase::WOL_HW_SUPPORT, m_wol_support_mask );
	setWolBits( NetworkAdapterBase::WOL_HW_ENABLED, m_wol_enable_mask );
	dprintf( D_FULLDEBUG, "%s supports Wake-on: %s (raw: 0x%02x)\n",
			 m_if_name, isWakeSupported() ? "yes" : "no", m_wol_support_mask );
	dprintf( D_FULLDEBUG, "%s enabled Wake-on: %s (raw: 0x%02x)\n",
			 m_if_name, isWakeEnabled() ? "yes" : "no", m_wol_enable_mask );

	close( sock );
#  endif
	return ok;
}

struct WolTable
{
	unsigned						bit_mask;
	NetworkAdapterBase::WOL_BITS	wol_bits;
};
static WolTable wol_table [] =
{
# if (HAVE_LINUX_ETHTOOL_H)
	{ WAKE_PHY,			NetworkAdapterBase::WOL_PHYSICAL },
	{ WAKE_UCAST,		NetworkAdapterBase::WOL_UCAST },
	{ WAKE_MCAST,		NetworkAdapterBase::WOL_MCAST },
	{ WAKE_BCAST,		NetworkAdapterBase::WOL_BCAST },
	{ WAKE_ARP,			NetworkAdapterBase::WOL_ARP },
	{ WAKE_MAGIC,		NetworkAdapterBase::WOL_MAGIC },
	{ WAKE_MAGICSECURE,	NetworkAdapterBase::WOL_MAGICSECURE },
# endif
	{ 0,				NetworkAdapterBase::WOL_NONE }
};

void
LinuxNetworkAdapter::setWolBits ( WOL_TYPE type, unsigned bits )
{
	if ( type == WOL_HW_SUPPORT ) {
		wolResetSupportBits( );
	}
	else {
		wolResetEnableBits( );
	}
	for( unsigned bit = 0;  wol_table[bit].bit_mask;  bit++ ) {
		if ( wol_table[bit].bit_mask & bits ) {
			wolSetBit( type, wol_table[bit].wol_bits );
		}
	}
}
