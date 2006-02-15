/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
  
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "time_offset.h"
#include <math.h>

// --------------------------------------------
// CEDAR STUBS
// --------------------------------------------

//
// Given a Stream, this stub will send & receive the TimeOffsetPackets
// over the Stream and return the offset
// 
long time_offset_cedar_stub( Stream* s ) {
		//
		// The init method should put in our local timestamp
		// for when the packet is leaving us on the local side
		//
	TimeOffsetPacket packet = time_offset_initPacket();
	TimeOffsetPacket rPacket;
		
		//
		// Construct a time offset packet and shove it over
		// the wire. We add in our local time for when the packet
		// was sent
		//
	s->encode();
	if (! time_offset_codePacket_cedar( packet, s ) ) {
		dprintf( D_FULLDEBUG, "time_offset_cedar() failed to send inital packet "
							  "to remote daemon\n" );
		return ( TIME_OFFSET_DEFAULT );
	}
	s->end_of_message();
	
		//
		// We will now get the response from the remote entity
		// This will have the time that the packet arrived at their
		// end and the time that it was sent back out.
		// We will need to set the local arrival time on the packet right away
		//
	s->decode();
	if (! time_offset_codePacket_cedar( rPacket, s ) ) {
		dprintf( D_FULLDEBUG, "time_offset_cedar() failed to receive response "
							  "packet from remote daemon\n" );
		return ( TIME_OFFSET_DEFAULT );
	}
	s->end_of_message();
	
		//
		// Calculate the offset
		//
	long offset = time_offset_calculate( packet, rPacket ) ;

 	return (offset);
}

//
// This is the connection code for the other end of a time offset
// routine. The other connection will have sent us a DC_TIME_OFFSET
// command which will have take us into this function
//
int
time_offset_receive_cedar_stub( Service*, int cmd, Stream* s ) {
 	TimeOffsetPacket packet;
 	
	 	//
	 	// Get the TimeOffsetPacket from the wire
	 	//
 	s->decode();
	if (! time_offset_codePacket_cedar( packet, s ) ) {
 		dprintf( D_FULLDEBUG, "time_offset_receive_cedar_stub() failed to "
 							  "receive intial packet from remote daemon\n" );
		return (false);
 	}
 		//
 		// Close our connection
 		// The other end will be waiting for our response but we need
 		// to check a few conditions before we can send it out to them
 		//
 	s->end_of_message(); 	
 	dprintf( D_FULLDEBUG, "time_offset_receive_cedar_stub() got the intial "
 					   "packet!\n");
 	
 		//
 		// Call to the command handler that does all the logic
 		// for the remote side
 		//
	if ( time_offset_receive( packet ) ){
	  	s->encode();
  		if (! time_offset_codePacket_cedar( packet, s ) ) {
  			dprintf( D_FULLDEBUG, "time_offset_receive_cedar_stub() failed to "
  								  "send response packet to remote daemon\n" );
	  		return (false);
  		}
	  	s->end_of_message();
	 	dprintf( D_FULLDEBUG, "time_offset_receive_cedar_stub() sent back response "
	 					   "packet!\n");
	}
  	return (true);
}

//
// Given a TimeOffsetPacket & Stream, get all the data over the wire
// I realize that this could probably be in the Stream code, but 
// I didn't want ot muddy it up and I didn't want to mess around
// with the circular dependencies
//
int 
time_offset_codePacket_cedar(TimeOffsetPacket &p, Stream *s)
{
	if (!s->code(p.localDepart))  return (false);
	if (!s->code(p.remoteArrive)) return (false);
	if (!s->code(p.remoteDepart)) return (false);
	if (!s->code(p.localArrive))  return (false);
	return (true);
}

// --------------------------------------------
// LOGIC METHODS
// --------------------------------------------

//
// This method receives a packet, stuffs its own arrival
// time and departure time into it, then returns the packet
//
int
time_offset_receive( TimeOffsetPacket &packet ) {
	 	//
 		// Set the remote arrival time on the packet right away
 		// We will check for other conditions but it's important
 		// that we set this as soon as possible
 		//
	packet.remoteArrive = time( NULL );
 		//
 		// Make sure that it has the client's departure timestamp
 		//
 	if ( packet.localArrive == 0 ) {
 		dprintf( D_FULLDEBUG, "Received a time offset request but the "
 		    				  "local departure time was empty." );
 		return (false);
 	}
 		//
 		// We'll add our timestamp of when the packet is being sent out
 		//
  	packet.remoteDepart = time( NULL );
  	return (true);
}
 
//
// Given a packet with all the appropriate
// The first packet is just to make sure that the return packet has the 
//
long
time_offset_calculate( TimeOffsetPacket &packet, TimeOffsetPacket &rPacket ) {
		//
		// First stuff in the time that we received the packet back
		//
	rPacket.localArrive = time( NULL );

		//
		// Makre sure that we have the remoteArrive & remoteDepart times
		//
	if ( ! rPacket.remoteArrive ) {
		dprintf( D_FULLDEBUG, "The time offset response does not have "
						      "the remote arrival time. Offset will default to %d\n",
						   	  TIME_OFFSET_DEFAULT );
		return ( TIME_OFFSET_DEFAULT );
	}
	if ( ! rPacket.remoteDepart ) {
		dprintf( D_FULLDEBUG, "The time offset response does not have "
						      "the remote departure time. Offset will default to %d\n",
						      TIME_OFFSET_DEFAULT );
		return ( TIME_OFFSET_DEFAULT );
	}
		//
		// Make sure that the remote packet and the original packet
		// have the same local departure time
		//
	if ( packet.localDepart != rPacket.localArrive ) {
			//
			// It didn't, which is odd
			// Because we don't know what's going on, we'll just return
			// that the offset is zero
			//
		dprintf( D_FULLDEBUG, "The time offset response has a different local "
					  	      "departure timestamp. Offset will default to %d\n",
					  	      TIME_OFFSET_DEFAULT );
 		return ( TIME_OFFSET_DEFAULT );
 	}
 		//
 		// Now use the basic formula from NTP to determine the offset
 		//
 	long offset = (long)rint( ( (rPacket.remoteArrive - rPacket.localDepart) +
 				  		  (rPacket.remoteDepart - rPacket.localArrive) ) / 2);
 	return (offset);
}

//
// Initializes a TimeOffsetPacket
// We simply stuff our current time into the localDepart field
//
TimeOffsetPacket
time_offset_initPacket() {
	TimeOffsetPacket packet;
	packet.localDepart = time( NULL );	
	return ( packet );
}