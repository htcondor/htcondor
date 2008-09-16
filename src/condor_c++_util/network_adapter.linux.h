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

#ifndef _NETWORK_ADAPTER_LINUX_H_
#define _NETWORK_ADAPTER_LINUX_H_

#include "network_adapter.h"

#if HAVE_NET_IF_H
# include <net/if.h>
#endif


/***************************************************************
* LinuxNetworkAdapter class
* Given the name of a network adapter (the GUID), discovers all
* the required power information.
***************************************************************/

class LinuxNetworkAdapter : public NetworkAdapterBase
{

public:

	/** @name Instantiation. 
	*/
	//@{

    /// Constructor
	LinuxNetworkAdapter ( void ) throw ();

	/// Constructor
	LinuxNetworkAdapter ( unsigned int ip_addr ) throw ();

	/// Destructor
	virtual ~LinuxNetworkAdapter ( void ) throw (); 


	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	const char *hardwareAddress (void) const { return m_hw_addr_str; };

    /** Returns the adapter's hardware address
		@return a string representation of the subnet mask
	*/
	const char *subnet (void) const;

    /** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	virtual bool wakeAble (void) const { return m_wol; };

	/** Returns the adapter's logical name
		@return a string with the logical name
	*/
	const char *interfaceName( void ) const { return m_if_name; };

	
	//@}

private:

	unsigned			 m_ip_addr;
	const char			*m_if_name;
	bool				 m_wol;

	// HW address & string rep of it
    const char			 m_hw_addr[32];
	const char			 m_hw_addr_str[32];

	// Network mask
    const struct sockaddr m_netmask;
    const char		 	 m_netmask_str[32];
	
	// Very Linux specific definitions
#  if defined HAVE_NET_IF_H
	const struct ifreq	 m_ifr;
#  endif
	int					 m_wolopts;

	// Internal methods
	bool findAdapter( void );
	bool detectWOL( void );
#  if defined HAVE_NET_IF_H
	void setIFR( const struct ifreq *ifr );
	void getIFR( struct ifreq *ifr );
	void setHwAddr( const struct ifreq *ifr );
#  endif
	void setNetMask( const struct ifreq *ifr );

	// Dump out an error
	void derror( const char *label ) const;
};

#endif // _NETWORK_ADAPTER_LINUX_H_
