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
#include "network_adapter.unix.h"


/***************************************************************
* LinuxNetworkAdapter class
* Given the name of a network adapter (the GUID), discovers all
* the required power information.
***************************************************************/

class LinuxNetworkAdapter : public UnixNetworkAdapter
{

public:

	/** @name Instantiation.
	*/
	//@{

	/// Constructor
	LinuxNetworkAdapter ( const condor_sockaddr& ip_addr )
		noexcept;

	// Alternate
	LinuxNetworkAdapter ( const char *name )
		noexcept;

	/// Destructor
	virtual ~LinuxNetworkAdapter ( void ) noexcept;


	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{


	//@}

private:
	unsigned	m_wol_support_mask;
	unsigned	m_wol_enable_mask;

	// Internal methods
	virtual bool findAdapter( const condor_sockaddr& /*ip_addr*/ );
	virtual bool findAdapter( const char * /*if_name*/ );
	bool getAdapterInfo( void );
	virtual bool detectWOL( void );

	void setWolBits ( WOL_TYPE type, unsigned bits );

};

#define NETWORK_ADAPTER_TYPE_DEFINED	1
typedef LinuxNetworkAdapter	NetworkAdapter;

#endif // _NETWORK_ADAPTER_LINUX_H_
