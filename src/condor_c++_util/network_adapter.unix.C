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

/// Constructor
UnixNetworkAdapter::UnixNetworkAdapter ( unsigned int ip_addr ) throw ()
{
	m_ip_addr = ip_addr;
	m_wol = false;
	m_found = false;
	resetName( true );

	// IFR specific things
#  if HAVE_STRUCT_IFREQ
	resetNetMask( true );
	resetHwAddr( true );
#  endif
}

/// Destructor
UnixNetworkAdapter::~UnixNetworkAdapter (void) throw ()
{
	resetName( );
}

/// Initializer
bool
UnixNetworkAdapter::initialize ( void )
{
	if ( !findAdapter() ) {
		return false;
	}
	m_found = true;

	// Detect if it supports Wake On Lan
	detectWOL( );

	// Done
	return true;
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
// Memset like things which take const pointers
//
void *
UnixNetworkAdapter::MemZero( const void *buf, unsigned size )
{
	void *p = const_cast<void *>( buf );
	memset( p, 0, size );
	return p;
}
char *
UnixNetworkAdapter::StrZero( const char *buf, unsigned size )
{
	return (char *) MemZero( buf, size );
}
void *
UnixNetworkAdapter::MemCopy( const void *dest, const void *src, unsigned size )
{
	void *p = const_cast<void *>( dest );
	memcpy( p, src, size );
	return p;
}

// Set interface name methods
void
UnixNetworkAdapter::setName( const char *name )
{
	resetName( );
	m_if_name = strdup( name );
}
void
UnixNetworkAdapter::resetName( bool init )
{
	if ( init ) {
		m_if_name = NULL;
	}
	else if ( m_if_name ) {
		free( const_cast<char*>(m_if_name) );
		m_if_name = NULL;
	}
}

//
// This block of methods require 'struct ifreq' ...
//
#if HAVE_STRUCT_IFREQ

// Set the interface name from the ifreq
void
UnixNetworkAdapter::setName( const struct ifreq &ifr )
{
	setName( ifr.ifr_name );
}
// Fill in the name field of an ifreq
void
UnixNetworkAdapter::getName( struct ifreq &ifr )
{
	strncpy( ifr.ifr_name, m_if_name, IFNAMSIZ );
	ifr.ifr_name[IFNAMSIZ-1] = '\0';
}

// Set the hardware address from the ifreq
void
UnixNetworkAdapter::resetHwAddr( bool /*init*/ )
{
	MemZero( m_hw_addr, sizeof(m_hw_addr) );
	StrZero( m_hw_addr_str, sizeof(m_hw_addr_str) );
}
void
UnixNetworkAdapter::setHwAddr( const struct ifreq &ifr )
{
	resetHwAddr( );
	MemCopy( m_hw_addr, &(ifr.ifr_hwaddr.sa_data), 8 );

	char *str = const_cast<char*>(m_hw_addr_str);
	for( int i = 0;  i < 6;  i++ ) {
		char	tmp[3];
		snprintf( tmp, sizeof(tmp), "%02x", m_hw_addr[i] );
		if ( i < 5 ) {
			strcat( tmp, ":" );
		}
		strcat( str, tmp );
	}
}

// Set the net mask from the ifreq
void
UnixNetworkAdapter::resetNetMask( bool /*init*/ )
{
	MemZero( &m_netmask, sizeof(m_netmask) );
	StrZero( m_netmask_str, sizeof(m_netmask_str) );
}
void
UnixNetworkAdapter::setNetMask( const struct ifreq &ifr )
{
	resetNetMask( );

	struct sockaddr *maskptr = const_cast<struct sockaddr *>(&m_netmask);
	MemCopy( maskptr, &(ifr.ifr_netmask), sizeof(m_netmask) );

	const struct sockaddr_in *in_addr =
		(const struct sockaddr_in *) &(m_netmask);
	char *str = const_cast<char *>(m_netmask_str);
	strncpy( str, sin_to_string(in_addr), sizeof(m_netmask_str) );
}

#endif

// dprintf an error message
void
UnixNetworkAdapter::derror( const char *label ) const
{
	dprintf( D_ALWAYS,
			 "%s failed: %s (%d)\n",
			 label, strerror(errno), errno );
}
