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

#ifndef _NETWORK_ADAPTER_BASE_H_
#define _NETWORK_ADAPTER_BASE_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "MyString.h"
#include "condor_classad.h"

/***************************************************************
 * NetworkAdapterBase class
 ***************************************************************/

class NetworkAdapterBase
{

public:

	/* The following are the wake-on-lan capability bits
	*/
	enum WOL_BITS {
		WOL_NONE		= 0,		// No WOL cababilities
		WOL_PHYSICAL	= (1 << 0),	// 0x01 Supports physical packet
		WOL_UCAST		= (1 << 1),	// 0x02 Supports unicast packet
		WOL_MCAST		= (1 << 2),	// 0x04 Supports multicast packet
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
	NetworkAdapterBase (void) throw ();

    /// Destructor
	virtual ~NetworkAdapterBase (void) throw (); 

	//@}

	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Initialize the adapter
		@return true if successful, false if unsuccessful
	*/
	virtual bool initialize( void ) { return true; };

	/** Returns the adapter's hardware address
		@return a string representation of the addapter's hardware 
        address
	*/
	virtual const char* hardwareAddress (void) const = 0;

    /** Returns the adapter's IP address as a string
		@return the adapter's IP address
	*/
	virtual unsigned ipAddress (void) const = 0;

    /** Returns the adapter's subnet
		@return a string representation of the addapter's subnet mask
	*/
	virtual const char* subnet (void) const = 0;

    /** Is this the primary interface
		@return a string representation of the addapter's subnet mask
	*/
	bool isPrimary (void) const { return m_is_primary; };

	
    /** Ensures that the adapter can wake the machine.
		@return true if the adapter can wake the machine; otherwise, 
		false.
	*/
	bool isWakeSupported (void) const
		{ return (m_wol_support_bits & WOL_SUPPORTED) ? true : false; };

	/** Returns the mask of WOL bits that the hardware supports
		@return the bit mask (0==none)
	 */
	unsigned wakeSupportedBits (void) const
		{ return m_wol_support_bits; };

	/** Returns a string list of the supported types WOL types
		@return comma separated list
	 */
	MyString & wakeSupportedString ( MyString &s) const
		{ return getWolString( m_wol_support_bits, s ); };

	/** Returns a string list of the supported types WOL types
		@return comma separated list
	 */
	const char *wakeSupportedString ( void ) const {
		static char buf[1024];
		return getWolString( m_wol_support_bits, buf, sizeof(buf) );
	};

    /** Ensures that the adapter "waking" is enabled.
		@return true if the adapter waking is enabled; otherwise, false.
	*/
	bool isWakeEnabled (void) const
		{ return (m_wol_enable_bits & WOL_SUPPORTED) ? true : false; };

	/** Returns the mask of WOL bits that are enabled
		@return the bit mask (0==none)
	 */
	unsigned wakeEnabledBits (void) const
		{ return m_wol_enable_bits; };

	/** Returns a string list of the supported types WOL types
		@return comma separated list
	 */
	MyString & wakeEnabledString ( MyString &s) const
		{ return getWolString( m_wol_enable_bits, s ); };

	/** Returns a string list of the supported types WOL types
		@return comma separated list
	 */
	const char *wakeEnabledString ( void ) const {
		static char buf[1024];
		return getWolString( m_wol_enable_bits, buf, sizeof(buf) );
	};

	/** Returns whether this whole combination is wakable or not
		@return true if this adaptor is wakable as configured
	*/
	bool isWakeable( void ) const
		{ return (m_wol_support_bits & m_wol_enable_bits) ? true : false; };
	
    /** Checks that the adapter actually exists
        @returns true if the adapter exists on the machine; 
        otherwise, false.
	*/
	virtual bool exists (void) const = 0;
	
	//@}

    /** Published the network addapter's information into the given ad
        */
    void publish ( ClassAd &ad );

	/** Get the status of the initialization.
		@return true:success, false:failed
	 */
	bool getInitStatus( void ) { return m_initialization_status; };


    /** We use this to create adapter objects so we don't need to deal 
        with the differences between OSs at the invocation level.
        @return if successful a valid NetworkAdapterBase*; otherwise 
        NULL.
	*/
	static NetworkAdapterBase* createNetworkAdapter( const char *sinful,
													 bool is_primary = false );


  protected:
	void		wolResetSupportBits( void )
		{ m_wol_support_bits = 0; };
	unsigned	wolEnableSupportBit( WOL_BITS bit )
		{ m_wol_support_bits|= bit; return m_wol_support_bits; };

	void		wolResetEnableBits( void )
		{ m_wol_enable_bits = 0; };
	unsigned	wolEnableEnableBit( WOL_BITS bit )
		{ m_wol_enable_bits|= bit; return m_wol_enable_bits; };

	unsigned	wolSetBit( WOL_TYPE type, WOL_BITS bit );

	// Get string representaions of the supported / enabled bits
	MyString & getWolString( unsigned bits, MyString &s ) const;
	char *getWolString( unsigned bits, char *buf, int bufsize ) const;

	
  private:
	unsigned	m_wol_support_bits;
	unsigned	m_wol_enable_bits;
	bool		m_initialization_status;
	bool		m_is_primary;

	bool doInitialize( void );
	bool setIsPrimary( bool is_primary )
		{ return m_is_primary = is_primary; };
    
    /** @name Private Instantiation. 
    That is, do not allow for a default constructor.
	*/
	//@{

	//@}
	
};

#endif // _NETWORK_ADAPTER_BASE_H_
