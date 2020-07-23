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
#include "condor_debug.h"
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
NetworkAdapterBase::NetworkAdapterBase (void) noexcept
{
	wolResetSupportBits( );
	wolResetEnableBits( );
	m_initialization_status = false;
	m_is_primary = false;
}

NetworkAdapterBase::~NetworkAdapterBase (void) noexcept
{
}


/***************************************************************
 * NetworkAdapterBase static members
 ***************************************************************/
NetworkAdapterBase*
NetworkAdapterBase::createNetworkAdapter ( const char *sinful_or_name,
										   bool is_primary )
{

	if ( NULL == sinful_or_name ) {

		dprintf (
			D_FULLDEBUG,
			"Warning: Can't create network adapter\n" );

		return NULL;

	}

# if defined ( NETWORK_ADAPTER_TYPE_DEFINED )

	NetworkAdapterBase *adapter = NULL;

	condor_sockaddr addr;

	// if from_sinful() returns true, it surely is valid sinful and
	// has a numeric IP address.
	if ( addr.from_sinful(sinful_or_name) ) {
		adapter = new NetworkAdapter ( addr );
	}
	else {
		adapter = new NetworkAdapter ( sinful_or_name );
	}

	// Try to initialize it; delete it if it fails
	if ( !adapter->doInitialize () ) {

        dprintf (
            D_FULLDEBUG,
            "doInitialize() failed for %s\n",
            sinful_or_name );

		delete adapter;
		adapter = NULL;

	} else {

		adapter->setIsPrimary ( is_primary );

	}

	return adapter;
#else
	if (is_primary) {} // Fight compiler warnings!
	return NULL;
# endif

}


/***************************************************************
 * NetworkAdapterBase members
 ***************************************************************/
bool
NetworkAdapterBase::doInitialize ()
{
	m_initialization_status = initialize ();
	return m_initialization_status;
}

void
NetworkAdapterBase::publish ( ClassAd &ad ) const
{
    ad.Assign ( ATTR_HARDWARE_ADDRESS, hardwareAddress () );
    ad.Assign ( ATTR_SUBNET_MASK, subnetMask () );
	ad.Assign ( ATTR_IS_WAKE_SUPPORTED, isWakeSupported () );
	ad.Assign ( ATTR_IS_WAKE_ENABLED, isWakeEnabled () );
	ad.Assign ( ATTR_IS_WAKEABLE, isWakeable() );

	std::string tmp;
	ad.Assign ( ATTR_WAKE_SUPPORTED_FLAGS, wakeSupportedString(tmp) );
	ad.Assign ( ATTR_WAKE_ENABLED_FLAGS, wakeEnabledString(tmp) );
}

unsigned
NetworkAdapterBase::wolSetBit ( WOL_TYPE type, WOL_BITS bit )
{
	if ( WOL_HW_SUPPORT == type ) {
		return wolEnableSupportBit ( bit );
	}
	else if ( WOL_HW_ENABLED == type ) {
		return wolEnableEnableBit ( bit );
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

std::string&
NetworkAdapterBase::getWolString ( unsigned bits, std::string &s ) const
{
	s.clear();
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

bool
NetworkAdapterBase::setIsPrimary ( bool is_primary )
{
	return m_is_primary = is_primary;
}

bool
NetworkAdapterBase::isPrimary () const
{
	return m_is_primary;
}

bool
NetworkAdapterBase::isWakeSupported () const
{
	return (m_wol_support_bits & WOL_SUPPORTED) ? true : false;
}

unsigned
NetworkAdapterBase::wakeSupportedBits () const
{
	return m_wol_support_bits;
}

std::string&
NetworkAdapterBase::wakeSupportedString ( std::string &s ) const
{
	return getWolString ( m_wol_support_bits, s );
}

bool
NetworkAdapterBase::isWakeEnabled () const
{
	return ( m_wol_enable_bits & WOL_SUPPORTED ) ? true : false;
}

unsigned
NetworkAdapterBase::wakeEnabledBits () const
{
	return m_wol_enable_bits;
}

std::string&
NetworkAdapterBase::wakeEnabledString ( std::string &s ) const
{
	return getWolString ( m_wol_enable_bits, s );
}

bool
NetworkAdapterBase::isWakeable () const
{
	return ( m_wol_support_bits & m_wol_enable_bits ) ? true : false;
}

bool
NetworkAdapterBase::getInitStatus () const
{
	return m_initialization_status;
}

void
NetworkAdapterBase::wolResetSupportBits ()
{
	m_wol_support_bits = 0;
}

unsigned
NetworkAdapterBase::wolEnableSupportBit ( WOL_BITS bit )
{
	m_wol_support_bits|= bit; return m_wol_support_bits;
}

void
NetworkAdapterBase::wolResetEnableBits ()
{
	m_wol_enable_bits = 0;
}

unsigned
NetworkAdapterBase::wolEnableEnableBit ( WOL_BITS bit )
{
	m_wol_enable_bits|= bit; return m_wol_enable_bits;
}



