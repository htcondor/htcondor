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
#  include "network_adapter.WINDOWS.h"
#elif defined ( LINUX )
#  include "network_adapter.linux.h"
#endif
#include <ctype.h>


/***************************************************************
 * Base NetworkAdapterBase class
 ***************************************************************/
NetworkAdapterBase::NetworkAdapterBase (void) throw ()
{
	wolResetSupportBits( );
	wolResetEnableBits( );
}

NetworkAdapterBase::~NetworkAdapterBase (void) throw ()
{
}


/***************************************************************
 * NetworkAdapterBase static members 
 ***************************************************************/
NetworkAdapterBase* 
NetworkAdapterBase::createNetworkAdapter ( const char *sinful_or_name,
										   bool is_primary )
{
    NetworkAdapterBase *adapter = NULL;

# if defined ( NETWORK_ADAPTER_TYPE_DEFINED )
	if ( is_valid_sinful( sinful_or_name ) ) {
		adapter = new NetworkAdapter ( string_to_ipstr(sinful_or_name),
									   string_to_ip(sinful_or_name)  );
	}
	else {
		adapter = new NetworkAdapter ( sinful_or_name );
	}

	// Try to initialize it; delete it if it fails
	if ( !adapter->doInitialize( ) ) {

        dprintf ( 
            D_FULLDEBUG, 
            "doInitialize() failed for %s\n",
            sinful_or_name );

		delete adapter;
		return NULL;
	}
	adapter->setIsPrimary( is_primary );
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
	ad.Assign ( ATTR_IS_WAKE_SUPPORTED, isWakeSupported () );
	ad.Assign ( ATTR_IS_WAKE_ENABLED, isWakeEnabled () );
	ad.Assign ( ATTR_IS_WAKEABLE, isWakeable() );

	MyString	tmp;
	ad.Assign ( ATTR_WAKE_SUPPORTED_FLAGS, wakeSupportedString(tmp) );
	ad.Assign ( ATTR_WAKE_ENABLED_FLAGS, wakeEnabledString(tmp) );
}

unsigned
NetworkAdapterBase::wolSetBit( WOL_TYPE type, WOL_BITS bit )
{
	if ( WOL_HW_SUPPORT == type ) {
		return wolEnableSupportBit( bit );
	}
	else if ( WOL_HW_ENABLED == type ) {
		return wolEnableEnableBit( bit );
	}
	return 0;
}

struct WolTable
{
	NetworkAdapterBase::WOL_BITS	 wol_bits;
	const char						*string;
};
static WolTable wol_table [] =
{
	{ NetworkAdapterBase::WOL_PHYSICAL,		"Physical Packet" },
	{ NetworkAdapterBase::WOL_UCAST,		"UniCast Packet", },
	{ NetworkAdapterBase::WOL_MCAST,		"MultiCast Packet" },
	{ NetworkAdapterBase::WOL_BCAST,		"BroadCast Packet" },
	{ NetworkAdapterBase::WOL_ARP,			"ARP Packet" }, 
	{ NetworkAdapterBase::WOL_MAGIC,		"Magic Packet" },
	{ NetworkAdapterBase::WOL_MAGICSECURE,	"Secure Magic Packet" },
	{ NetworkAdapterBase::WOL_NONE,			NULL },

};

char *
NetworkAdapterBase::getWolString(unsigned bits, char *buf, int bufsize) const
{
	MyString	s;
	getWolString( bits, s );
	strncpy( buf, s.GetCStr(), bufsize );
	buf[bufsize-1] = '\0';
	return buf;
}

MyString &
NetworkAdapterBase::getWolString ( unsigned bits, MyString &s ) const
{
	s = "";
	int	count = 0;

	for( unsigned bit = 0;  wol_table[bit].string;  bit++ ) {
		if ( wol_table[bit].wol_bits & bits ) {
			if ( count++ != 0 ) {
				s += ",";
			}
			s += wol_table[bit].string;
		}
	}
	if ( !count ) {
		s = "NONE";
	}
	return s;
}

