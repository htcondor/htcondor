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
#include "condor_io.h"
#include "time_offset.h"
#include <math.h>

// --------------------------------------------
// CEDAR STUBS
// --------------------------------------------

/**
 * Given a Stream, this stub will send & receive the TimeOffsetPackets
 * over the Stream and populate the min/max ranges passed in by reference
 * If we are unable to establish the offset, either by invalid information
 * or network failure, we will return false
 * 
 * @param s - the Stream connection to the other daemon
 * @param offset - the theoritical offset between ourselves and the other daemon
 * @return true if the offset was succesfully calculated between 2 daemons
 **/
bool
time_offset_cedar_stub( Stream* s, long &offset ) {
	bool ret = false;
		//
		// The init method should put in our local timestamp
		// for when the packet is leaving us on the local side
		//
	TimeOffsetPacket packet = time_offset_initPacket();
	TimeOffsetPacket rPacket;
	
		//
		// Use the base Cedar sending function to populate our
		// packet information
		//
	if ( time_offset_send_cedar_stub( s, packet, rPacket ) ) {
		ret = time_offset_calculate( packet, rPacket, offset );
	}
	return ( ret );
}

/**
 * Given a Stream, this stub will send & receive the TimeOffsetPackets
 * over the Stream and populate the min/max ranges passed in by reference
 * If we are unable to establish the range, either by invalid information
 * or network failure, we will return false
 * 
 * @param s - the Stream connection to the other daemon
 * @param min_range - the minimum range value for the time offset
 * @param max_range - the maximum range value for the time offset
 * @return true if the range was succesfully calculated between 2 daemons
 **/
bool
time_offset_range_cedar_stub( Stream* s, long &min_range, long &max_range ) {
	bool ret = false;
		//
		// The init method should put in our local timestamp
		// for when the packet is leaving us on the local side
		//
	TimeOffsetPacket packet = time_offset_initPacket();
	TimeOffsetPacket rPacket;
	
		//
		// Use the base Cedar sending function to populate our
		// packet information
		//
	if ( time_offset_send_cedar_stub( s, packet, rPacket ) ) {
		ret = time_offset_range_calculate( packet, rPacket, min_range, max_range );
	}
	return ( ret );
}

/**
 * Given a Stream, this stub will send & receive the TimeOffsetPackets
 * over the Stream. If we fail to communiucate with the remote daemon
 * we will return false
 * 
 * @param s - the Stream connection to the other daemon
 * @param packet - the packet we are sending to the remote side
 * @param rPacket - the packet we will get back from the remote daemon
 * @return true we were able to communicate with the other daemon
 **/
bool
time_offset_send_cedar_stub( Stream* s,
							 TimeOffsetPacket &packet, TimeOffsetPacket &rPacket ) {
		//
		// Construct a time offset packet and shove it over
		// the wire. We add in our local time for when the packet
		// was sent
		//
	s->encode();
	if ( ! time_offset_codePacket_cedar( packet, s ) ) {
		dprintf( D_FULLDEBUG, "time_offset_send_cedar() failed to send inital packet "
							  "to remote daemon\n" );
		return ( false );
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
		dprintf( D_FULLDEBUG, "time_offset_send_cedar() failed to receive response "
							  "packet from remote daemon\n" );
		return ( false );
	}
	s->end_of_message();
	
		//
		// Stuff in the time that we received the packet back
		//
	rPacket.localArrive = time( NULL );
		//
		// Our packets have been updated, so we can return true
		//
	return ( true );
}

/**
 * This is the connection code for the other end of a time offset range
 * routine. The other connection will have sent us a DC_TIME_OFFSET
 * command which will have take us into this function
 * 
 * @param Service - not used
 * @param cmd - the daemon command that we are being asked to execute
 * @param s -the Stream connection to the other daemon
 * @return true if
 **/
int
time_offset_receive_cedar_stub( int /* cmd */, Stream* s ) {
	 	//
	 	// Get the TimeOffsetPacket information off the wire
	 	//
 	s->decode();
 	TimeOffsetPacket packet;
	if ( ! time_offset_codePacket_cedar( packet, s ) ) {
 		dprintf( D_FULLDEBUG, "time_offset_receive_cedar_stub() failed to "
 							  "receive intial packet from remote daemon\n" );
		return ( FALSE );
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
	  		return ( FALSE );
  		}
	  	s->end_of_message();
	 	dprintf( D_FULLDEBUG, "time_offset_receive_cedar_stub() sent back response "
	 					      "packet!\n");
	}
  	return ( TRUE );
}

/**
 * Given a TimeOffsetPacket & Stream, get all the data over the wire
 * I realize that this could probably be in the Stream code, but 
 * I didn't want ot muddy it up and I didn't want to mess around
 * with the circular dependencies
 * 
 * @param p - the container to update
 * @param s - the Stream connection to the other daemon
 * @return true if we ere able to update the packet successfully
 **/
bool 
time_offset_codePacket_cedar( TimeOffsetPacket &p, Stream *s )
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

/**
 * This method receives a packet, stuffs its own arrival
 * time and departure time into it, then returns the packet.
 * We may want to use a more precise timestamp, but this works
 * for now
 * 
 * @param packet - the packet to add our timestamps to
 * @return true if we were able to update the packet for our reply
 **/
bool
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

/**
 * Validates that the packets have matching information
 * If this function returns false, we should not use these packets
 * to calculate offset information
 * 
 * @param packet - the packet we originally sent to the remote side
 * @param rPacket - the packet sent back to use from the remote daemon
 * @return true if the packets have the expected information
 **/
bool
time_offset_validate( TimeOffsetPacket &packet, TimeOffsetPacket &rPacket )
{
		//
		// Make sure that we have the remoteArrive & remoteDepart times
		//
	if ( ! rPacket.remoteArrive ) {
		dprintf( D_FULLDEBUG, "The time offset response does not have the "
							  "remote arrival time. Offset will default to %d\n",
							  TIME_OFFSET_DEFAULT );
		return ( false );
	}
	if ( ! rPacket.remoteDepart ) {
		dprintf( D_FULLDEBUG, "The time offset response does not have the "
							  "remote departure time. Offset will default to %d\n",
							  TIME_OFFSET_DEFAULT );
		return ( false );
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
		return ( false );
	}
	return ( true );
}
 
 
/**
 * Given the packet we sent to the remote daemon, and the packet
 * we got in the reply, we will calculate the time offset range
 * between ourselves and them. We do some simple validation checks,
 * and if the information isn't what we expect, we will return false
 * 
 * @param packet - the packet we originally sent to the remote side
 * @param rPacket - the packet sent back to use from the remote daemon
 * @param min_range - the minimum range value for the time offset
 * @param max_range - the maximum range value for the time offset
 * @return true if the range was succesfully calculated between 2 daemons
 **/
bool
time_offset_range_calculate( TimeOffsetPacket &packet, TimeOffsetPacket &rPacket,
							 long &min_range, long &max_range )
{
		//
		// Validate the packets first
		//
	if ( ! time_offset_validate( packet, rPacket ) ) {
		return ( false );
	}
 		//
 		// Now use the basic formula from NTP to formulate the offset range
 		//
 	long a = rPacket.remoteArrive - rPacket.localDepart;
 	long b = rPacket.remoteDepart - rPacket.localArrive;
 	long theta = (long)rint( ( a + b ) / 2 );
 	long sigma = (long)rint( ( a - b ) / 2 );
 	min_range = theta - sigma;
 	max_range = theta + sigma;
 	return ( true );
}

/**
 * Given the packet we sent to the remote daemon, and the packet
 * we got in the reply, we will calculate the time offset
 * between ourselves and them. We do some simple validation checks,
 * and if the information isn't what we expect, we will return false
 * Note that this is assuming that the network connection is symmetrical
 * 
 * @param packet - the packet we originally sent to the remote side
 * @param rPacket - the packet sent back to use from the remote daemon
 * @param offset - the offset that will calculated between the daemons
 * @return true if we were able to calculate the offset 
 **/
bool
time_offset_calculate( TimeOffsetPacket &packet, TimeOffsetPacket &rPacket,
					   long &offset )
{
		//
		// Validate the packets first
		//
	if ( ! time_offset_validate( packet, rPacket ) ) {
		return ( false );
	}
		//
		// This is the theoritical offset if you assume that the network
		// connection is symmetrical. I'm keeping it here for posterity
		//
	offset = (long)rint( ( (rPacket.remoteArrive - rPacket.localDepart) +
						   (rPacket.remoteDepart - rPacket.localArrive) ) / 2);
	return ( true );
}

/**
 * Initializes a TimeOffsetPacket
 * We simply stuff our current time into the localDepart field
 * 
 * @return a new initialized TimeOffsetPacket struct
 **/
TimeOffsetPacket
time_offset_initPacket() {
	TimeOffsetPacket packet = { 0, 0, 0, 0 };
	packet.localDepart = time( NULL );	
	return ( packet );
}
