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

#ifndef _UDP_WAKER_H_
#define _UDP_WAKER_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "waker.h"

/***************************************************************
 * Forward declarations
 ***************************************************************/

//class ClassAd;

/***************************************************************
 * UdpWakeOnLanWaker class
 ***************************************************************/

class UdpWakeOnLanWaker : public WakerBase
{

public:

    /** @name Constants.		
	*/
	//@{
	
    /** @name detect_port Used to make the initialization find
        the WOL port automatically.
        */
    static const int detect_port  /* = -1 */,
                     default_port /* =  9 */;

	//@}

    /** @name Instantiation and Initialization.
     */
	//@{

    /** Constructor
        The mac is of the form: xx:xx:xx:xx:xx:xx
	    The subnet is of the form: x.x.x.x
	    The port can be one of 0, 7, or 9
        */
    UdpWakeOnLanWaker (
	char const *mac,
	char const *subnet,
        unsigned short port = default_port ) noexcept;

    /** Constructor
        */
    UdpWakeOnLanWaker (
        ClassAd *ad ) noexcept;
	
    /** Destructor
        */
    ~UdpWakeOnLanWaker () noexcept;
    /** Initialize the internal structures (can be called multiple
        times--such as in the case of a reconfiguration)
		@return true if it was succesful; otherwise, false.
		@see initializePacket
		@see initializePort
		*/
    bool initialize ();

    //@}

	/** @name Wake-up Mechanism.		
		*/
	//@{

    /** Send the magic WOL packet.
        @return true if it was succesful; otherwise, false.
    */
    bool doWake () const;
	
    //@}

protected:

    /* Constructs the WOL magic packet from the hardware address */
    bool initializePacket ();

    /* Determines which port to use */
    bool initializePort ();

    /* Determines which broadcast address to use */
    bool initializeBroadcastAddress ();

    /* Print the system-specific socket error */
    void printLastSocketError () const;

    /* The WOL packet consists of 6 times 0xFF followed by
       the hardware address repeated 16 times. The largest
       subnet is '255.255.255.255' plus the NULL character. */
    enum {
        RAW_MAC_ADDRESS_LENGTH    = 6,
        STRING_MAC_ADDRESS_LENGTH = 3*RAW_MAC_ADDRESS_LENGTH,
	    WOL_PACKET_LENGTH         = 17*RAW_MAC_ADDRESS_LENGTH,
        MAX_IP_ADDRESS_LENGTH     = 16
    };

private:

    char			m_mac[STRING_MAC_ADDRESS_LENGTH];
    char			m_subnet[MAX_IP_ADDRESS_LENGTH];
    char			m_public_ip[MAX_IP_ADDRESS_LENGTH];
    unsigned char	m_raw_mac[RAW_MAC_ADDRESS_LENGTH];
    sockaddr_in		m_broadcast;
    int				m_port;
    unsigned char	m_packet[WOL_PACKET_LENGTH];
    bool			m_can_wake;

};

#endif // _UDP_WAKER_H_
