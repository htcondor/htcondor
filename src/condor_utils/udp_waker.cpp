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


#include "condor_common.h"
#include "condor_debug.h"
#include "udp_waker.h"

#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "ipv6_hostname.h"
#include "daemon.h"
#include "condor_sinful.h"

/***************************************************************
 * UdpWakeOnLanWaker constants
 ***************************************************************/

/* auto-detect port number */
const int
UdpWakeOnLanWaker::detect_port = 0;

/* default port number used when auto-detection fails */
const int
UdpWakeOnLanWaker::default_port = 9;


/***************************************************************
 * UdpWakeOnLanWaker [c|d]tors
 ***************************************************************/

UdpWakeOnLanWaker::UdpWakeOnLanWaker (
    char const     *mac,
    char const     *subnet,
    unsigned short port ) noexcept
	: WakerBase (), 
	m_port ( port )
{
	// TODO: Picking IPv4 arbitrarily.
	std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();

    strncpy ( m_mac, mac, STRING_MAC_ADDRESS_LENGTH-1 );
	m_mac[STRING_MAC_ADDRESS_LENGTH-1] = '\0';
    strncpy ( m_subnet, subnet, MAX_IP_ADDRESS_LENGTH-1 );
	m_subnet[MAX_IP_ADDRESS_LENGTH-1] = '\0';
    strncpy ( m_public_ip, my_ip.c_str(), MAX_IP_ADDRESS_LENGTH-1 );
	m_public_ip[MAX_IP_ADDRESS_LENGTH-1] = '\0';
    m_can_wake = initialize ();	

}

UdpWakeOnLanWaker::UdpWakeOnLanWaker (
    ClassAd *ad ) noexcept
	: WakerBase ()
{

    int     found   = 0;

	// Basic initialization
	m_port = 0;
    memset ( &m_broadcast, 0, sizeof (m_broadcast));

    /* make sure we are only capable of sending the WOL
    magic packet if all of the initialization succeds */
    m_can_wake = false;

    /* retrieve the hardware address from the ad */
    found = ad->LookupString (
        ATTR_HARDWARE_ADDRESS,
        m_mac,
        STRING_MAC_ADDRESS_LENGTH );

    if ( !found ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker: no hardware address "
            "(MAC) defined\n" );

        return;

    }

    /* retrieve the IP from the ad */
	Daemon d(ad,DT_STARTD,NULL);
	char const *addr = d.addr();
	Sinful sinful( addr );
	if( !addr || !sinful.getHost() ) {
        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker: no IP address defined\n" );

        return;
	}
	strncpy( m_public_ip, sinful.getHost(), MAX_IP_ADDRESS_LENGTH-1);
	m_public_ip[MAX_IP_ADDRESS_LENGTH-1] = '\0';

    /* retrieve the subnet from the ad */
    found = ad->LookupString (
        ATTR_SUBNET_MASK,
        m_subnet,
        MAX_IP_ADDRESS_LENGTH );

    if ( !found ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker: no subnet defined\n" );

        return;

    }

	/* retrieve the port number from the ad */
	found = ad->LookupInteger (
		ATTR_WOL_PORT,
		m_port );

	if ( !found ) {

		/* no error, just auto-detect the port number */
		m_port = detect_port;

	}

    /* initialize the internal structures */
    if ( !initialize () ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker: failed to initialize\n" );

        return;

    }

    /* if we made it here, then initialization succeeded */
    m_can_wake = true;

}

UdpWakeOnLanWaker::~UdpWakeOnLanWaker () noexcept
{
}

/***************************************************************
 * UdpWakeOnLanWaker methods
 ***************************************************************/

bool
UdpWakeOnLanWaker::initialize ()
{

    if ( !initializePacket () ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker::initialize: "
            "Failed to initialize magic WOL packet\n" );

        return false;

    }

    if ( !initializePort () ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker::initialize: "
            "Failed to initialize port number\n" );

        return false;

    }

    if ( !initializeBroadcastAddress () ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker::initialize: "
            "Failed to initialize broadcast address\n" );

        return false;

    }

    /* if we get here then we are fine */
    return true;

}

bool
UdpWakeOnLanWaker::initializePacket ()
{
	
	int i, c, offset;

	/* parse the hardware address */
	unsigned mac[6];
	c = sscanf ( m_mac,
		"%2x:%2x:%2x:%2x:%2x:%2x",
		&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] );
	
	if ( c != 6 || strlen ( m_mac ) < STRING_MAC_ADDRESS_LENGTH - 1 ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker::initializePacket: "
            "Malformed hardware address: %s\n",
            m_mac );

		return false;

	}
	for ( i = 0;  i < 6;  i++ ) {
		m_raw_mac[i] = (unsigned char) mac[i];
	}
	
	/* pad the start of the packet */
	memset ( m_packet, 0xFF, 6 );
	offset = 6;

	/* create the body of packet: it contains the machine address
	   repeated 16 times */
	for ( i = 0; i < 16; i++ ) {
		memcpy ( m_packet + offset, m_raw_mac, 6 );
		offset += 6;
	}

    return true;

}

bool
UdpWakeOnLanWaker::initializePort ()
{

    /* if we've been given a zero value, then look-up the
       port number to use */
    if ( detect_port == m_port ) {
        servent *sp = getservbyname ( "discard", "udp" );
        if ( sp ) {
            m_port = ntohs ( sp->s_port );
        } else {
            m_port = default_port;
        }
	}

    return true;

}

bool
UdpWakeOnLanWaker::initializeBroadcastAddress ()
{
    bool        ok = false;
    sockaddr_in public_ip_address;
    int ret;

    memset ( &m_broadcast, 0, sizeof ( sockaddr_in ) );
    m_broadcast.sin_family = AF_INET;
    m_broadcast.sin_port   = htons ( m_port );

    /* subnet address will always be provided in dotted notation */
    if ( MATCH == strcmp ( m_subnet, "255.255.255.255" ) ) {

        m_broadcast.sin_addr.s_addr = htonl ( INADDR_BROADCAST );

    } else if ( (ret = inet_pton( AF_INET, m_subnet, &m_broadcast.sin_addr.s_addr)) <= 0 ) {
        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker::doWake: Malformed subnet "
            "'%s'\n", m_subnet );

        goto Cleanup;

    }

    /* log display subnet */
# if defined ( WIN32 )
    dprintf (
        D_FULLDEBUG,
        "UdpWakeOnLanWaker::doWake: "
        "Broadcasting on subnet: %d.%d.%d.%d\n",
        m_broadcast.sin_addr.S_un.S_un_b.s_b1,
        m_broadcast.sin_addr.S_un.S_un_b.s_b2,
        m_broadcast.sin_addr.S_un.S_un_b.s_b3,
		m_broadcast.sin_addr.S_un.S_un_b.s_b4 );
# else
    dprintf ( D_FULLDEBUG,
			  "UdpWakeOnLanWaker::doWake: "
			  "Broadcasting on subnet: %s\n",
			  inet_ntoa( m_broadcast.sin_addr ) );
# endif

    /* invert the subnet mask (xor) */
    m_broadcast.sin_addr.s_addr ^= 0xffffffff;

    /* logically or the IP address with the inverted subnet mast */
    if (inet_pton(AF_INET, m_public_ip, &public_ip_address.sin_addr.s_addr) < 1)
	{
		dprintf(D_ALWAYS, "UDP waker, public ip is not a valid address, %s\n", m_public_ip);
		goto Cleanup;
	}
    m_broadcast.sin_addr.s_addr |= public_ip_address.sin_addr.s_addr;

    /* log display broadcast address */
# if defined ( WIN32 )
    dprintf (
        D_FULLDEBUG,
        "UdpWakeOnLanWaker::doWake: "
        "Broadcast address: %d.%d.%d.%d\n",
		m_broadcast.sin_addr.S_un.S_un_b.s_b1,
        m_broadcast.sin_addr.S_un.S_un_b.s_b2,
        m_broadcast.sin_addr.S_un.S_un_b.s_b3,
		m_broadcast.sin_addr.S_un.S_un_b.s_b4 );
# else
    dprintf ( D_FULLDEBUG,
			  "UdpWakeOnLanWaker::doWake: "
			  "Broadcast address: %s\n",
			  inet_ntoa( m_broadcast.sin_addr ) );
# endif

    /* if we made it here, then it all went perfectly */
    ok = true;

Cleanup:
    return ok;

}

void
UdpWakeOnLanWaker::printLastSocketError () const
{

#if defined ( WIN32 )

    DWORD   last_error  = WSAGetLastError (),
            length      = 0;
    LPVOID  buffer      = NULL;
    PSTR    message     = NULL;
    BOOL    ok          = FALSE;

    __try {

        length = FormatMessage (
            FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            last_error,
            MAKELANGID ( LANG_NEUTRAL, SUBLANG_DEFAULT ),
            (PSTR) &buffer,
            0,
            NULL );

        if ( 0 == length ) {

            last_error = GetLastError ();

            dprintf (
                D_ALWAYS,
                "PrintFormatedErrorMessage: failed to retrieve error "
                "message. (last-error = %u)\n",
                last_error );

            __leave;

        }

        message = (PSTR) buffer;
        message[length - 2] = '\0'; /* remove new-line */

        dprintf (
            D_ALWAYS,
            "Reason: %s\n",
            message );

        /* all went well */
        ok = TRUE;

    }
    __finally {

        if ( buffer ) {
            LocalFree ( buffer );
            buffer = NULL;
        }

    }

#else /* !WIN32 */
	
    dprintf ( D_ALWAYS,
			  "Reason: %s (errno = %d)\n",
			  strerror ( errno ),
			  errno );

#endif

}

#if !defined( WIN32 )
#  define SOCKET_ERROR		-1
#  define INVALID_SOCKET	-1
#  define closesocket(_s_)	close(_s_)
#endif

bool
UdpWakeOnLanWaker::doWake () const
{

    /* bail-out early if we were not fully initialized */
    if ( !m_can_wake ) {
        return false;
    }

	int  error	= SOCKET_ERROR;
#ifdef WIN32
	INT_PTR sock = INVALID_SOCKET;
#else
	int	 sock	= INVALID_SOCKET;
#endif
	int	 on		= 1;
	bool ok		= false;
	
	/* create a datagram for our UDP broadcast WOL packet */
	sock = socket ( AF_INET, SOCK_DGRAM, 0 );
	
    if ( INVALID_SOCKET == sock ) {
	
        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker::::doWake: Failed to create socket" );
		
        goto Cleanup;

	}

	/* make this a broadcast socket */
	error = setsockopt (
        sock,
        SOL_SOCKET,
        SO_BROADCAST,
		(char const*) &on,
        sizeof ( int ) );
	
    if ( SOCKET_ERROR == error ) {

        dprintf (
            D_ALWAYS,
            "UdpWakeOnLanWaker::doWake: "
			"Failed to set broadcast option\n" );

		goto Cleanup;

	}

	/* broadcast the WOL packet to on the given subnet */
	error = sendto (
        sock,
        (char const*) m_packet,
        WOL_PACKET_LENGTH,
		0,
        (const sockaddr*) &m_broadcast,
        sizeof ( sockaddr_in ) );
	
    if ( SOCKET_ERROR == error ) {
		
        dprintf (
            D_ALWAYS,
            "Failed to send packet\n" );

		goto Cleanup;

	}

    /* if we made it here, then it all went perfectly */
    ok = true;

Cleanup:
	if ( !ok ) { /* print the error code */
		printLastSocketError ();
	}

	/* finally, clean-up the connection */
	if ( INVALID_SOCKET != sock ) {

		if ( 0 != closesocket ( sock ) ) {

            dprintf (
                D_ALWAYS,
                "UdpWakeOnLanWaker::doWake: "
			    "Failed to close socket\n" );

			printLastSocketError ();

		}

	}

	return ok;

}

