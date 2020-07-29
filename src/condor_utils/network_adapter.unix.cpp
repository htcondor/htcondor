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
#include "internet.h"
#include "network_adapter.unix.h"


/***************************************************************
* UnixNetworkAdapter class
***************************************************************/

/// Constructor
UnixNetworkAdapter::UnixNetworkAdapter ( const condor_sockaddr& ip_addr ) noexcept
{
	m_found = false;
	resetIpAddr( true );
	resetName( true );
	setIpAddr( ip_addr );

	// IFR specific things
#  if HAVE_STRUCT_IFREQ
	resetNetMask( true );
	resetHwAddr( true );
#  endif
}

/// Constructor
UnixNetworkAdapter::UnixNetworkAdapter ( const char *name ) noexcept
{
	m_found = false;
	resetIpAddr( true );
	resetName( true );
	setName( name );

	// IFR specific things
#  if HAVE_STRUCT_IFREQ
	resetNetMask( true );
	resetHwAddr( true );
#  endif
}

/// Destructor
UnixNetworkAdapter::~UnixNetworkAdapter (void) noexcept
{
	resetName( );
}

/// Initializer
bool
UnixNetworkAdapter::initialize ( void )
{
	if ( m_ip_addr != condor_sockaddr::null && !findAdapter(m_ip_addr) ) {
		return false;
	}
	else if ( !findAdapter(m_if_name ) ) {
		return false;
	}
	m_found = true;

	// learn basic info about the adapter
	getAdapterInfo( );

	// Detect if it supports Wake On Lan
	detectWOL( );

	// Done
	return true;
}

bool
UnixNetworkAdapter::findAdapter( const condor_sockaddr& /*ip_addr*/ )
{
	return false;
}
bool
UnixNetworkAdapter::findAdapter( const char * /*if_name*/ )
{
	return false;
}
bool
UnixNetworkAdapter::getAdapterInfo ( void )
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

// Set the IP address
void
UnixNetworkAdapter::resetIpAddr( bool /*init*/ )
{
	m_ip_addr.clear();
	//MemZero( &m_in_addr, sizeof(m_in_addr) );
}
void
UnixNetworkAdapter::setIpAddr( const condor_sockaddr& ip )
{
	m_ip_addr = ip;
//	struct in_addr	*in = (&m_in_addr);
//	in->s_addr = ip;
}

// Reset hardware address
void
UnixNetworkAdapter::resetHwAddr( bool /*init*/ )
{
	MemZero( m_hw_addr, sizeof(m_hw_addr) );
	StrZero( m_hw_addr_str, sizeof(m_hw_addr_str) );
}

// Reset the net mask
void
UnixNetworkAdapter::resetNetMask( bool /*init*/ )
{
	MemZero( &m_netmask, sizeof(m_netmask) );
	StrZero( m_netmask_str, sizeof(m_netmask_str) );
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
		free( m_if_name );
		m_if_name = NULL;
	}
}

//
// This block of methods require 'struct ifreq' ...
//
#if defined(HAVE_STRUCT_IFREQ) && defined(HAVE_STRUCT_IFREQ_IFR_HWADDR)

// Set the interface name from the ifreq
void
UnixNetworkAdapter::setName( const struct ifreq &ifr )
{
	setName( ifr.ifr_name );
}
// Fill in the name field of an ifreq
void
UnixNetworkAdapter::getName( struct ifreq &ifr, const char *name ) const
{
	if ( NULL == name ) {
		name = m_if_name;
	}
	strncpy( ifr.ifr_name, name, IFNAMSIZ );
	ifr.ifr_name[IFNAMSIZ-1] = '\0';
}

// Set the hardware address from the ifreq
void
UnixNetworkAdapter::setHwAddr( const struct ifreq &ifr )
{
	resetHwAddr( );
	MemCopy( m_hw_addr, &(ifr.ifr_hwaddr.sa_data), 8 );

	char			*str = m_hw_addr_str;
	unsigned		 len = 0;
	const unsigned	 maxlen = sizeof(m_hw_addr_str)-1;

	*str = '\0';
	for( int i = 0;  i < 6;  i++ ) {
		char	tmp[4];
		snprintf( tmp, sizeof(tmp), "%02x", m_hw_addr[i] );
		len += strlen(tmp);
		ASSERT( len < maxlen );
		strcat( str, tmp );
		if ( i < 5 ) {
			len += 1;
			ASSERT( len < maxlen );
			strcat( str, ":" );
		}
	}
}
void
UnixNetworkAdapter::setIpAddr( const struct ifreq &ifr )
{
	resetIpAddr( );

    condor_sockaddr addr((const sockaddr*)&ifr.ifr_addr);
    m_ip_addr = addr;

//	const struct sockaddr_in *in = (const struct sockaddr_in*)&(ifr.ifr_addr);
//	struct sockaddr_in	sin_addr;
//	MemCopy( &sin_addr, in, sizeof(struct sockaddr_in) );
//	MemCopy( &m_in_addr, &sin_addr.sin_addr, sizeof(struct in_addr) );
//	m_ip_addr = in->sin_addr.s_addr;
}

// Set the net mask from the ifreq
void
UnixNetworkAdapter::setNetMask( const struct ifreq &ifr )
{
	resetNetMask( );

	struct sockaddr *maskptr = &m_netmask;
	MemCopy( maskptr, &(ifr.ifr_netmask), sizeof(m_netmask) );

	const struct sockaddr_in *in_addr =
		(const struct sockaddr_in *) &(m_netmask);
	strncpy( m_netmask_str,
			 inet_ntoa(in_addr->sin_addr),
			 sizeof(m_netmask_str) - 1);
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
