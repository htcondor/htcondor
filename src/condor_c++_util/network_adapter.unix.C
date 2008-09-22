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
#include "network_adapter.unix.h"

#if HAVE_NET_IF_H
# include <net/if.h>
#endif


/***************************************************************
* UnixNetworkAdapter class
***************************************************************/
UnixNetworkAdapter::UnixNetworkAdapter ( void ) throw ()
{
	return;
}

UnixNetworkAdapter::UnixNetworkAdapter ( unsigned int ip_addr ) throw ()
{
	m_ip_addr = ip_addr;
	m_if_name = NULL;
	m_wol = false;

	// Linux specific things
	m_wolopts = 0;
#  if HAVE_STRUCT_IFREQ
	setIFR( NULL );
	setHwAddr( NULL );
#  endif

	if ( !findAdapter() ) {
		return;
	}

	detectWOL( );
}

UnixNetworkAdapter::~UnixNetworkAdapter (void) throw ()
{
	return;
}

bool
UnixNetworkAdapter::findAdapter( void )
{
	return false;
}
bool
UnixNetworkAdapter::detectWOL ( void )
{
	return false;
}


//
// This block of methods require 'struct ifreq' ...
//
#if HAVE_STRUCT_IFREQ

// Set the hardware address from the ifreq
void
UnixNetworkAdapter::setHwAddr( const struct ifreq *ifr )
{
	memset( const_cast<char *>(m_hw_addr), 0, sizeof(m_hw_addr) );
	memset( const_cast<char *>(m_hw_addr_str), 0, sizeof(m_hw_addr_str) );

	if ( ifr ) {
		memcpy( const_cast<char *>(m_hw_addr), ifr->ifr_hwaddr.sa_data, 8 );

		char	*str = const_cast<char *>(m_hw_addr_str);
		for( int i = 0;  i < 6;  i++ ) {
			char	tmp[3];
			snprintf( tmp, sizeof(tmp), "%02x", m_hw_addr[i] );
			if ( i ) {
				strcat( tmp, ":" );
			}
			strcat( str, tmp );
		}
	}
}

// Set the net mask from the ifreq
void
UnixNetworkAdapter::setNetMask( const struct ifreq *ifr )
{
	
	struct sockaddr *maskptr = const_cast<struct sockaddr *>(&m_netmask);
	memset( maskptr, 0, sizeof(m_netmask) );

	if ( ifr ) {
		memcpy( maskptr, &(ifr->ifr_netmask), sizeof(m_netmask) );
		char	*str = const_cast<char *>(m_netmask_str);
		const struct sockaddr_in *
			in_addr = (const struct sockaddr_in *) &(m_netmask);
		strncpy( str,
				 sin_to_string(in_addr),
				 sizeof(m_netmask_str) );
	}
}

// Store the ifrequst
void
UnixNetworkAdapter::setIFR( const struct ifreq *ifr )
{
	if ( ifr ) {
		memcpy( const_cast<struct ifreq*>(&m_ifr), ifr, sizeof(struct ifreq) );
	}
	else {
		memset( const_cast<struct ifreq*>(&m_ifr), 0, sizeof(struct ifreq) );
	}
}

// Get the ifrequest
void
UnixNetworkAdapter::getIFR( struct ifreq *ifr )
{
	memcpy( ifr, &m_ifr, sizeof(struct ifreq) );
}
#endif

// dprintf an error message
void
UnixNetworkAdapter::derror( const char *label ) const
{
	MyString	message;
	dprintf( D_ALWAYS, "%s failed: %s (%d)\n",
			 label, strerror(errno), errno );
}
