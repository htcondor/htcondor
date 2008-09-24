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

#ifndef _NETWORK_ADAPTER_BASE_BASE_H_
#define _NETWORK_ADAPTER_BASE_BASE_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_classad.h"

/***************************************************************
 * NetworkAdapterBase class
 ***************************************************************/

class NetworkAdapterBase
{

public:

	/** @name Instantiation. 
	*/
	//@{

    /// Constructor
	NetworkAdapterBase (void) throw ();

    /// Destructor
	virtual ~NetworkAdapterBase (void) throw (); 

	//@}
	
	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	virtual const char* hardwareAddress (void) const = 0;

    /** Returns the adapter's subnet
		@return a string representation of the addapter's subnet mask
	*/
	virtual const char* subnet (void) const = 0;

    /** Ensures that the adapter can wake the machine.
    @return true if the adapter can wake the machine; otherwise, false.
	*/
	virtual bool wakeAble (void) const = 0;

    /** Checks that the adapter actually exists
        @returns true if the adapter exists on the machine; 
        otherwise, false.
	*/
	virtual bool exists (void) const = 0;
	
	//@}

    /** Published the network addapter's information into the given ad
        */
    void publish ( ClassAd &ad );

    /** We use this to create adapter objects so we don't need to deal 
        with the differences between OSs at the invocation level.
        @return if successful a valid NetworkAdapterBase*; otherwise 
        NULL.
	*/
	static NetworkAdapterBase* createNetworkAdapter ( const char *sinful );

private:
    
    /** @name Private Instantiation. 
    That is, do not allow for a default constructor.
	*/
	//@{

	//@}
	
};

#endif // _NETWORK_ADAPTER_BASE_BASE_H_
