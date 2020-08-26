/***************************************************************
*
* Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#ifndef _NETWORK_ADAPTER_BASE_H_
#define _NETWORK_ADAPTER_BASE_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_classad.h"
#include "condor_sockaddr.h"

/***************************************************************
 * NetworkAdapterBase class
 ***************************************************************/

class NetworkAdapterBase
{

public:

	/**	The following are the wake-on-lan capability bits
		*/
	enum WOL_BITS {
		WOL_NONE		= 0,		// No WOL capabilities
		WOL_PHYSICAL	= (1 << 0),	// 0x01 Supports physical packet
		WOL_UCAST		= (1 << 1),	// 0x02 Supports uni-cast packet
		WOL_MCAST		= (1 << 2),	// 0x04 Supports multi-cast packet
		WOL_BCAST		= (1 << 3),	// 0x08 Supports broadcast packet
		WOL_ARP			= (1 << 4),	// 0x10 Supports ARP packet
		WOL_MAGIC		= (1 << 5),	// 0x20 Supports UDD "magic" packet
		WOL_MAGICSECURE	= (1 << 6),	// 0x40 only meaningful if WOL_MAGIC

		WOL_SUPPORTED	= ( WOL_MAGIC )	// Our currently supported mask
	};

	enum WOL_TYPE {
		WOL_HW_SUPPORT,
		WOL_HW_ENABLED
	};

	/** @name Instantiation.
	*/
	//@{

    /// Constructor
	NetworkAdapterBase () noexcept;

    /// Destructor
	virtual ~NetworkAdapterBase () noexcept;

	//@}

	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Initialize the adapter
		@return true if successful, false if unsuccessful
	*/
	virtual bool initialize () = 0;

	/** Returns the adapter's hardware address
		@return a string representation of the adapter's hardware
        address
	*/
	virtual const char* hardwareAddress () const = 0;

    /** Returns the adapter's IP address as a string
		@return the adapter's IP address
	*/
	virtual condor_sockaddr ipAddress () const = 0;

    /** Returns the adapter's subnet mask
		@return a string representation of the adapter's's subnet mask
	*/
	virtual const char* subnetMask () const = 0;

    /** Is this the primary interface
		@return a string representation of the adapter's's subnet mask
	*/
	bool isPrimary () const;

    /** Ensures that the adapter can wake the machine.
		@return true if the adapter can wake the machine; otherwise,
		false.
	*/
	bool isWakeSupported () const;

	/** Returns the mask of WOL bits that the hardware supports
		@return the bit mask (0==none)
	 */
	unsigned wakeSupportedBits () const;

	/** Returns a string list of the supported types WOL types
		@return comma separated list
	 */
	std::string& wakeSupportedString ( std::string &s ) const;

    /** Ensures that the adapter "waking" is enabled.
		@return true if the adapter waking is enabled; otherwise, false.
	*/
	bool isWakeEnabled () const;

	/** Returns the mask of WOL bits that are enabled
		@return the bit mask (0==none)
	 */
	unsigned wakeEnabledBits () const;;

	/** Returns a string list of the supported types WOL types
		@return comma separated list
	 */
	std::string& wakeEnabledString ( std::string &s ) const;;

	/** Returns the adapter's logical name
		@return a string with the logical name
	*/
	virtual const char* interfaceName () const = 0;

	/** Returns whether this whole combination is wakable or not
		@return true if this adapter is wakable as configured
	*/
	bool isWakeable () const;

    /** Checks that the adapter actually exists
        @returns true if the adapter exists on the machine;
        otherwise, false.
	*/
	virtual bool exists () const = 0;

	//@}

    /** Published the network adapter's's information into the given ad
        */
    void publish ( ClassAd &ad ) const;

	/** Get the status of the initialization.
		@return true:success, false:failed
	 */
	bool getInitStatus () const;


    /** We use this to create adapter objects so we don't need to deal
        with the differences between OSs at the invocation level.
        @return if successful a valid NetworkAdapterBase*; otherwise
        NULL.
	*/
	static NetworkAdapterBase* createNetworkAdapter (
		const char *sinful, bool is_primary = false );

protected:

	void wolResetSupportBits ();
	unsigned wolEnableSupportBit ( WOL_BITS bit );
	void wolResetEnableBits ();
	unsigned wolEnableEnableBit ( WOL_BITS bit );
	unsigned wolSetBit ( WOL_TYPE type, WOL_BITS bit );

	// Get string representations of the supported / enabled bits
	std::string& getWolString ( unsigned bits, std::string &s ) const;


  private:

	unsigned	m_wol_support_bits;
	unsigned	m_wol_enable_bits;
	bool		m_initialization_status;
	bool		m_is_primary;

	bool doInitialize ();
	bool setIsPrimary ( bool is_primary );

    /** @name Private Instantiation.
    That is, do not allow for a default constructor.
	*/
	//@{

	//@}

};

#endif // _NETWORK_ADAPTER_BASE_H_
