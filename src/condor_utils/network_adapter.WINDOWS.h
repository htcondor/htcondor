/***************************************************************
*
* Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef _NETWORK_ADAPTER_WINDOWS_H_
#define _NETWORK_ADAPTER_WINDOWS_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "network_adapter.h"
#include "condor_constants.h"
#include "setup_api_dll.WINDOWS.h"
#include <iptypes.h>

/***************************************************************
 * WindowsNetworkAdapter class
 ***************************************************************/

class WindowsNetworkAdapter : public NetworkAdapterBase
{

public:

	/** @name Instantiation.
	*/
	//@{

	/// Constructor
	WindowsNetworkAdapter () noexcept;

	/// Constructor
	WindowsNetworkAdapter ( const condor_sockaddr & ipaddr) noexcept;

	/// Alternate
	WindowsNetworkAdapter ( LPCSTR description ) noexcept;

	/// Destructor
	virtual ~WindowsNetworkAdapter () noexcept;

	//@}

	/** @name Device properties.
	*/
	//@{

	/** Returns the adapter's hardware address
		@return a string representation of the adapter's's hardware
        address
	*/
	const char* hardwareAddress () const;

    /** Returns the adapter's IP address as a string
		@return the adapter's IP address
	*/
	virtual condor_sockaddr ipAddress () const;

    /** Returns the adapter's hardware address
		@return a string representation of the subnet mask
	*/
	const char* subnetMask () const;

	/** Returns the adapter's logical name
		@return a string with the logical name
	*/
	const char* interfaceName () const;

    /** Checks that the adapter actually exists
        @returns true if the adapter exists on the machine;
        otherwise, false.
	    */
	bool exists () const;

    /** Initialize the internal structures (can be called multiple
        times--such as in the case of a reconfiguration)
		@return true if it was succesful; otherwise, false.
		*/
    bool initialize ();

	//@}

    /** @name Device parameters.
	Basic Plug and Play device properties.
	*/
	//@{

	/** Returns the device's power information
		@return The function retrieves the device's power management
		information. Use LocalFree) to release the memory.
		*/
	PCM_POWER_DATA getPowerData () const;

   //@}

private:

    /** Data members */
    CHAR        _ip_address[IP_STRING_BUF_SIZE],
                _description[MAX_ADAPTER_DESCRIPTION_LENGTH + 4],
                _hardware_address[32],
                _subnet_mask[IP_STRING_BUF_SIZE],
                _adapter_name[MAX_ADAPTER_NAME_LENGTH + 4];
    bool        _exists;
    SetupApiDLL _setup_dll;

    /**	Some registry values require some preprocessing before they can
		be queried, so we allow a user to specify a function to handle
		preprocessing.
	*/
	typedef void (*PRE_PROCESS_REISTRY_VALUE)(PBYTE);

	/** Returns the device's requested property
		@return The function retrieves a buffer to the device's
		requested  information. Use LocalFree) to release the memory.
		@param ID of the property to query.
		@param Preprocessing function.
		@see Registry Keys for Drivers in the MS DDK.
		*/
	PBYTE getRegistryProperty (
		IN DWORD registry_property,
		IN PRE_PROCESS_REISTRY_VALUE preprocess = NULL ) const;


};

#define NETWORK_ADAPTER_TYPE_DEFINED	1
typedef WindowsNetworkAdapter NetworkAdapter;


#endif //  _NETWORK_ADAPTER_WINDOWS_H_
