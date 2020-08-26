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

#ifndef _NETWORK_ADAPTER_UNIX_H_
#define _NETWORK_ADAPTER_UNIX_H_

#include "network_adapter.h"

#if defined(HAVE_NET_IF_H)
# include <net/if.h>
#endif


/***************************************************************
* UnixNetworkAdapter class
* Given the name of a network adapter (the GUID), discovers all
* the required power information.
***************************************************************/

class UnixNetworkAdapter : public NetworkAdapterBase
{

public:

	/** @name Instantiation.
	*/
	//@{

	/// Constructor
	UnixNetworkAdapter ( const condor_sockaddr& ip_addr ) noexcept;

	/// Constructor
	UnixNetworkAdapter ( const char *name ) noexcept;

	/// Destructor
	virtual ~UnixNetworkAdapter ( void ) noexcept;


	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Initialize the adapter
		@return true if successful, false if unsuccessful
	*/
	virtual bool initialize( void );

	/** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware
        address
	*/
	const char *hardwareAddress (void) const { return m_hw_addr_str; };

    /** Returns the adapter's IP address as a string
		@return the addapter's IP address
	*/
	virtual condor_sockaddr ipAddress (void) const { return m_ip_addr; };

    /** Returns the adapter's subnet mask
		@return a string representation of the subnet mask
	*/
	const char *subnetMask (void) const { return m_netmask_str; };

    /** Returns wether the interface was found or not
		@return true if the interface is found
	*/
	bool exists (void) const { return m_found; };

	/** Returns the adapter's logical name
		@return a string with the logical name
	*/
	const char *interfaceName( void ) const { return m_if_name; };


	//@}

protected:
	bool				 m_found;

	condor_sockaddr		 m_ip_addr;
    //struct in_addr		 m_in_addr;
	char				*m_if_name;

	// HW address & string rep of it
    unsigned char		 m_hw_addr[32];
	char				 m_hw_addr_str[32];

	// Network mask
    struct sockaddr		 m_netmask;
    char			 	 m_netmask_str[32];

	// Very UNIX specific definitions

	// Internal methods
	virtual bool findAdapter( const condor_sockaddr& ip_addr );
	virtual bool findAdapter( const char *if_name );
	virtual bool getAdapterInfo( void );
	virtual bool detectWOL( void );

	void setName( const char * );
	void setIpAddr( const condor_sockaddr& ip_addr );

	void resetName( bool init = false );
	void resetIpAddr( bool init = false );
	void resetHwAddr( bool init = false );
	void resetNetMask( bool init = false );

#  if defined(HAVE_STRUCT_IFREQ)
	void getName( struct ifreq &ifr, const char *name = NULL ) const;

	void setName( const struct ifreq &ifr );
	void setIpAddr( const struct ifreq &ifr );
	void setHwAddr( const struct ifreq &ifr );
	void setNetMask( const struct ifreq &ifr );
#  endif

	// Dump out an error
	void derror( const char *label ) const;

	// Helper methods
	void *MemZero( const void *buf, unsigned size );
	char *StrZero( const char *buf, unsigned size );
	void *MemCopy( const void *dest, const void *src, unsigned size );

};

#endif // _NETWORK_ADAPTER_UNIX_H_
