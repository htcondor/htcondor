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
#include "udp_waker.h"

#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "my_hostname.h"

/***************************************************************
 * UdpWakeOnLanWaker constants
 ***************************************************************/

/* auto-detect port number */
const int 
UdpWakeOnLanWaker::detect_port = -1;

/* default port number used when auto-detection fails */
const int 
UdpWakeOnLanWaker::default_port = 9;

/***************************************************************
 * UdpWakeOnLanWaker [c|d]tors
 ***************************************************************/

UdpWakeOnLanWaker::UdpWakeOnLanWaker ( 
    char const     *mac, 
	char const     *subnet, 
	unsigned short port )
: _port ( port ) {
    
    strncpy ( _mac, mac, STRING_MAC_ADDRESS_LENGTH );
    strncpy ( _subnet, subnet, MAX_IP_ADDRESS_LENGTH );
    strncpy ( _public_ip, my_ip_string (), MAX_IP_ADDRESS_LENGTH );
    _can_wake = initialize ();	

}

UdpWakeOnLanWaker::UdpWakeOnLanWaker (
    ClassAd *ad ) {

    int     found   = 0;
    char    *start  = NULL,
            *end    = NULL,
            sinful[MAX_IP_ADDRESS_LENGTH+10];
    
    /* make sure we are only capable of sending the WOL 
    magic packet if all of the initialization succeds */
    _can_wake = false;

    /* retrieve the hardware address from the ad */
    found = ad->LookupString (
        ATTR_HARDWARE_ADDRESS,
        _mac,
        STRING_MAC_ADDRESS_LENGTH );
    
    if ( !found ) {

        dprintf ( 
            D_ALWAYS, 
            "UdpWakeOnLanWaker: no hardware address "
            "(MAC) defined\n" );

        return;

    }

    /* retrieve the IP from the ad */
    found = ad->LookupString (
        ATTR_PUBLIC_NETWORK_IP_ADDR,
        sinful,
        MAX_IP_ADDRESS_LENGTH );
    
    if ( !found ) {
        
        dprintf ( 
            D_ALWAYS, 
            "UdpWakeOnLanWaker: no IP address defined\n" );
        
        return;
        
    }

    /* extract the IP from the public 'sinful' string */
    start = sinful + 1;
    end = strchr ( sinful, ':' );
    *end = '\0';
    strcpy ( _public_ip, start );

    /* retrieve the subnet from the ad */
    found = ad->LookupString (
        ATTR_SUBNET,
        _subnet,
        MAX_IP_ADDRESS_LENGTH );

    if ( !found ) {
        
        dprintf ( 
            D_ALWAYS, 
            "UdpWakeOnLanWaker: no subnet defined\n" );
            
        return;
        
    }

    /* auto-detect the port number to use */
    _port = detect_port;

    /* initialize the internal structures */
    if ( !initialize () ) {

        dprintf ( 
            D_ALWAYS, 
            "UdpWakeOnLanWaker: failed to initialize\n" );

        return;

    }

    /* if we made it here, then initialization succeded */
    _can_wake = true;
    
}

UdpWakeOnLanWaker::~UdpWakeOnLanWaker () throw () {
}

/***************************************************************
 * UdpWakeOnLanWaker methods
 ***************************************************************/

bool
UdpWakeOnLanWaker::initialize () {
    
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
UdpWakeOnLanWaker::initializePacket () {
	
	int i, c, offset;

	/* parse the hardware address */
	c = sscanf ( _mac, 
		"%2x:%2x:%2x:%2x:%2x:%2x",
		&_raw_mac[0], &_raw_mac[1], 
		&_raw_mac[2], &_raw_mac[3], 
		&_raw_mac[4], &_raw_mac[5] );
	
	if ( c != 6 || strlen ( _mac ) < STRING_MAC_ADDRESS_LENGTH - 1) {

        dprintf ( 
            D_ALWAYS, 
            "UdpWakeOnLanWaker::initializePacket: "
            "Malformed hardware address: %s\n",
            _mac );

		return false;

	}
	
	/* pad the start of the packet */
	memset ( _packet, 0xFF, 6 );
	offset = 6;

	/* create the body of packet: it contains the machine address 
	   repeated 16 times */
	for ( i = 0; i < 16; i++ ) {
		memcpy ( _packet + offset, _raw_mac, 6 );
		offset += 6;
	}

    return true;

}

bool
UdpWakeOnLanWaker::initializePort () {

    /* if we've been given a negative value, then look-up the 
       port number to use */
    if ( _port < 0 ) {
        servent *sp = getservbyname ( "discard", "udp" );
        if ( sp ) {
            _port = ntohs ( sp->s_port );
        } else {
            _port = default_port;
        }
	}

    return true;

}

bool
UdpWakeOnLanWaker::initializeBroadcastAddress () {

    bool        ok = false;
    sockaddr_in public_ip_address;

    memset ( &_boardcast, 0, sizeof ( sockaddr_in ) );
    _boardcast.sin_family = AF_INET;
    _boardcast.sin_port   = htons ( _port );
    
    /* subnet address will always be provided in dotted notation */
    if ( MATCH == strcmp ( _subnet, "255.255.255.255" ) ) {
        
        _boardcast.sin_addr.s_addr = htonl ( INADDR_BROADCAST );
        
    } else if ( INADDR_NONE == 
        ( _boardcast.sin_addr.s_addr = inet_addr ( _subnet ) ) ) {
        
        dprintf ( 
            D_ALWAYS, 
            "UdpWakeOnLanWaker::doWake: Malformed subnet "
            "'%s'\n", _subnet );
        
        goto Cleanup;
        
    }
    
    /* log display subnet */
    dprintf ( 
        D_FULLDEBUG, 
        "UdpWakeOnLanWaker::doWake: "
        "Broadcasting on subnet: %d.%d.%d.%d\n", 
        _boardcast.sin_addr.S_un.S_un_b.s_b1, 
        _boardcast.sin_addr.S_un.S_un_b.s_b2, 
        _boardcast.sin_addr.S_un.S_un_b.s_b3, 
		_boardcast.sin_addr.S_un.S_un_b.s_b4 );

    /* invert the subnet mask (xor) */
    _boardcast.sin_addr.s_addr ^= 0xffffffff;

    /* logically or the IP address with the inverted subnet mast */
    public_ip_address.sin_addr.s_addr = inet_addr ( _public_ip );
    _boardcast.sin_addr.s_addr |= public_ip_address.sin_addr.s_addr;

    /* log display broadcast address */
    dprintf ( 
        D_FULLDEBUG, 
        "UdpWakeOnLanWaker::doWake: "
        "Broadcast address: %d.%d.%d.%d\n", 
        _boardcast.sin_addr.S_un.S_un_b.s_b1, 
        _boardcast.sin_addr.S_un.S_un_b.s_b2, 
        _boardcast.sin_addr.S_un.S_un_b.s_b3, 
		_boardcast.sin_addr.S_un.S_un_b.s_b4 );

    /* if we made it here, then it all went perfectly */
    ok = true;

Cleanup:
    return ok;

}

void 
UdpWakeOnLanWaker::printLastSocketError () const {

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
	
    dprintf ( 
        D_ALWAYS,
        "Reason: %s (errno = %d)\n", 
        strerror ( errno ),
        errno );

#endif

}

bool
UdpWakeOnLanWaker::doWake () const {

    /* bail-out early if we were not fully initialized */
    if ( !_can_wake ) {
        return false;
    }

	int  error	= SOCKET_ERROR, 
		 sock	= INVALID_SOCKET,
		 on		= 1;
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
        (char const*) _packet, 
        WOL_PACKET_LENGTH, 
		0, 
        (sockaddr*) &_boardcast, 
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

