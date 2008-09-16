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

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#if defined ( WIN32 )
#include "network_adapter.WINDOWS.h"
#endif
#include "network_adapter.h"

/***************************************************************
 * Base NetworkAdapterBase class
 ***************************************************************/

NetworkAdapterBase::NetworkAdapterBase () throw () {
}

NetworkAdapterBase::~NetworkAdapterBase () throw () {
}

/***************************************************************
 * NetworkAdapterBase static members 
 ***************************************************************/

NetworkAdapterBase* 
NetworkAdapterBase::createNetworkAdapter ( char *sinful ) {
    NetworkAdapterBase *adapter = NULL;    
#if defined ( WIN32 )
    adapter = new WindowsNetworkAdapter ( 
        string_to_ipstr ( sinful ) );
#endif
	return adapter;
}

/***************************************************************
 * NetworkAdapterBase members 
 ***************************************************************/

void 
NetworkAdapterBase::publish ( ClassAd &ad ) {
    
    char line[200];

    sprintf( line, "%s = \"%s\"", ATTR_HARDWARE_ADDRESS,
        hardwareAddress () );
    ad->Assign ( line );

    sprintf( line, "%s = \"%s\"", ATTR_SUBNET,
        subnet () );
    ad->Assign ( line );

    sprintf( line, "%s = \"%s\"", ATTR_IS_WAKE_ABLE,
        wakeAble () ? "TRUE" : "FALSE" );
    ad->Assign ( line );


}