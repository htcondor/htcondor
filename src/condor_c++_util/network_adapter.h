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

#ifndef _NETWORK_ADAPTER_BASE_BASE_H_
#define _NETWORK_ADAPTER_BASE_BASE_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_classad.h"

/***************************************************************
 * NetworkAdapterBase class
 ***************************************************************/

class NetworkAdapterBase {

public:

	/** @name Instantiation. 
	*/
	//@{

    /// Destructor
	virtual ~NetworkAdapterBase () throw (); 

	//@}
	
	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	char* hardwareAddress () const = 0;

    /** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	char* subnet () const = 0;

    /** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	bool wakeAble () const = 0;
	
	//@}


    /** Published the network addapter's information into the given ad
        */
    void publish ( ClassAd &ad );


    /** We use this to create adapter objects so we don't need to deal 
        with the differences between OSs at the invocation level.
        @return if successful a valid NetworkAdapterBase*; otherwise 
        NULL.
	*/
	static NetworkAdapterBase* createNetworkAdapter ( char *sinful );

private:
    
    /** @name Private Instantiation. 
    That is, do not allow for a default constructor.
	*/
	//@{

    /// Constructor
	NetworkAdapterBase () throw ();

	//@}
	
};

#endif // _NETWORK_ADAPTER_BASE_BASE_H_