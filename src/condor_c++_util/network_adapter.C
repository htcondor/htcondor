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


/***************************************************************
 * Headers
 ***************************************************************/
#include "condor_common.h"
#include "condor_attributes.h"
#include "internet.h"
#include "network_adapter.h"
#if defined ( WIN32 )
# include "network_adapter.WINDOWS.h"
#elif defined ( LINUX )
# include "network_adapter.linux.h"
#endif


/***************************************************************
 * Base NetworkAdapterBase class
 ***************************************************************/
NetworkAdapterBase::NetworkAdapterBase (void) throw ()
{
}

NetworkAdapterBase::~NetworkAdapterBase (void) throw ()
{
}


/***************************************************************
 * NetworkAdapterBase static members 
 ***************************************************************/
NetworkAdapterBase* 
NetworkAdapterBase::createNetworkAdapter ( const char *sinful )
{
    NetworkAdapterBase *adapter = NULL;    

# if defined ( NETWORK_ADAPTER_TYPE_DEFINED )
	adapter = new NetworkAdapter (
		string_to_ipstr(sinful),
		string_to_ip(sinful)  );
	adapter->doInitialize( );
# endif

	return adapter;
}

/***************************************************************
 * NetworkAdapterBase members 
 ***************************************************************/
bool
NetworkAdapterBase::doInitialize ( void )
{
	m_initialization_status = initialize( );
	return m_initialization_status;
}

void 
NetworkAdapterBase::publish ( ClassAd &ad )
{
    ad.Assign ( ATTR_HARDWARE_ADDRESS, hardwareAddress () );
    ad.Assign ( ATTR_SUBNET, subnet () );
	ad.Assign ( ATTR_IS_WAKE_ABLE, wakeAble () );
}
