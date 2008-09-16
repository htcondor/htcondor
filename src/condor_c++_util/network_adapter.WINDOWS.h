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

#include "setup_api_dll.h"

/***************************************************************
* WindowsNetworkAdapter class
* Given the name of a network adapter (the GUID), discovers all
* the required power information. Requires that setupapi.dll be 
* loaded (i.e. we are running on post Windows 2K system).  
***************************************************************/

class WindowsNetworkAdapter {

public:

	/** @name Instantiation. 
	*/
	//@{

    /// Constructor
	WindowsNetworkAdapter () throw ();

	/// Constructor
	WindowsNetworkAdapter ( LPSTR device_name ) throw ();

	/// Destructor
	virtual ~WindowsNetworkAdapter () throw (); 

	//@}
	
	/** @name Device parameters.
	Basic Plug and Play device properties.
	*/
	//@{

	/** Returns the device's driver key
		@return The function retrieves a string identifying the 
		device's software key (sometimes called the driver key).
		This can be used to look up power management information
		about the device in the registry. Use LocalFree() to 
		release the memory
		@param Name of the network device to query.
		@see Registry Keys for Drivers in the MS DDK.		
	*/
	PCHAR getDriverKey () const;
	
	/** Returns the device's power information
		@return The function retrieves the device's power management 
		information. Use LocalFree) to release the memory.
		@param Name of the network device to query.
		*/
	PCM_POWER_DATA getPowerData () const;
	
	//@}

private:
	
	static const int _device_name_length = MAX_ADAPTER_NAME_LENGTH + 4;
	CHAR _device_name[_device_name_length];

	/*	Some registry values require some preprocessing before they can 
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

#endif //  _NETWORK_ADAPTER_WINDOWS_H_
