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

#if HAVE_LINUX_IF_H
# include <linux/if.h>
#endif
#if HAVE_LINUX_SOCKIOS_H
# include <linux/sockios.h>
#endif
#if HAVE_LINUX_ETHTOOL_H
# include <linux/ethtool.h>
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
	LinuxNetworkAdapter () throw ();

	/// Constructor
	LinuxNetworkAdapter ( unsigned int ip_addr ) throw ();

	/// Destructor
	virtual ~LinuxNetworkAdapter () throw (); 

	
	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	virtual char* hardwareAddress () const;

    /** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	virtual char* subnet () const;

    /** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	virtual bool wakeAble () const { return m_wol; };
	
	//@}

private:

	unsigned			 m_ip_addr;
	const char			*m_device_name;
	bool				 m_wol;

	// Very Linux specific definitions
#  if defined HAVE_LINUX_IF_H
	const struct ifreq	 m_ifr;
#  endif
	int					 m_wolopts;

	// Internal methods
	bool findAdapter( void );
	bool detectWOL( void );
#  if defined HAVE_LINUX_IF_H
	void setIFR( const struct ifreq *ifr );
	void getIFR( struct ifreq *ifr );
#  endif
	
};

#endif // _NETWORK_ADAPTER_LINUX_H_
