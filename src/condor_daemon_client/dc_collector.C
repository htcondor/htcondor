/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "internet.h"
#include "daemon.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dc_collector.h"
#include "condor_parameters.h"

// Instantiate things
template class ExtArray<DCCollectorAdSeq *>;

DCCollector::DCCollector( const char* name, UpdateType type ) 
	: Daemon( DT_COLLECTOR, name, NULL )
{
	up_type = type;
	init();
}


void
DCCollector::init( void ) 
{
	update_rsock = NULL;
	tcp_collector_host = NULL;
	tcp_collector_addr = NULL;
	tcp_collector_port = 0;
	use_tcp = false;
	udp_update_destination = NULL;
	tcp_update_destination = NULL;
	startTime = time( NULL );
	reconfig();
}


DCCollector::~DCCollector( void )
{
	if( update_rsock ) {
		delete( update_rsock );
	}
	if( tcp_collector_addr ) {
		delete [] tcp_collector_addr;
	}
	if( tcp_collector_host ) {
		delete [] tcp_collector_host;
	}
	if( udp_update_destination ) {
		delete [] udp_update_destination;
	}
	if( tcp_update_destination ) {
		delete [] tcp_update_destination;
	}
}


void
DCCollector::reconfig( void )
{
	char* tmp;
	tmp = param( "TCP_COLLECTOR_HOST" );
	if( tmp ) {
		use_tcp = true;
		if( tcp_collector_host ) {
			if( strcmp(tcp_collector_host, tmp) ) { 
					// the TCP_COLLECTOR_HOST has changed...
				if( update_rsock ) {
					delete( update_rsock );
					update_rsock = NULL;
				}
				delete [] tcp_collector_host;
				tcp_collector_host = strnewp( tmp );
			}
		} else {
				// nothing set yet, so store it now
			tcp_collector_host = strnewp( tmp );
		}
		free( tmp );
	}

	if( ! _addr ) {
		locate();
		if( ! _is_configured ) {
			dprintf( D_FULLDEBUG, "COLLECTOR address not defined in "
					 "config file, not doing updates\n" );
			return;
		}
	}

	StringList tcp_collectors;

	switch( up_type ) {
	case TCP:
		use_tcp = true;
		break;
	case UDP:
		use_tcp = false;
		break;
	case CONFIG:
		tmp = param( "TCP_UPDATE_COLLECTORS" );
		if( tmp ) {
			tcp_collectors.initializeFromString( tmp );
			free( tmp );
 			if( _name && 
				tcp_collectors.contains_anycase_withwildcard(_name) )
			{	
				use_tcp = true;
				break;
			}
		}
		tmp = param( "UPDATE_COLLECTOR_WITH_TCP" );
		if( ! tmp ) {
			tmp = param( "UPDATE_COLLECTORS_WITH_TCP" );
		}		
		if( tmp ) {
			if( tmp[0] == 't' || tmp[0] == 'T' || 
				tmp[0] == 'y' || tmp[0] == 'Y' )
			{ 
				use_tcp = true;
			}
			free( tmp );
		}
		break;
	}

	parseTCPInfo();
	initDestinationStrings();
	displayResults();
}


void
DCCollector::parseTCPInfo( void )
{
	if( tcp_collector_addr ) {
		delete [] tcp_collector_addr;
		tcp_collector_addr = NULL;
	}

	if( ! tcp_collector_host ) {
			// there's no specific TCP host to use.  if needed, use
			// the default collector...
		tcp_collector_port = _port;
		tcp_collector_addr = strnewp( _addr );
	} else {
			// they gave us a specific string.  parse it so we know
			// where to send the TCP updates.  this is in case they
			// want to setup a tree of collectors, etc.
		if( is_valid_sinful(tcp_collector_host) ) {
			tcp_collector_addr = strnewp( tcp_collector_host );
			tcp_collector_port = string_to_port( tcp_collector_host );
			return;
		} 

			// if we're still here, they didn't give us a valid
			// "sinful string", so see if they specified a port... 
		char* host = strnewp( tcp_collector_host );
		char* colon = NULL;
		if( !(colon = strchr(host, ':')) ) {
				// no colon, use the default port, and treat the given
				// string as the address.
			tcp_collector_port = COLLECTOR_PORT;
			tcp_collector_addr = strnewp( tcp_collector_host );
		} else { 
				// there's a colon, so grab what's after it for the
				// port, and what's before it for the address.
			*colon = '\0';
			tcp_collector_addr = strnewp( host );
			colon++;
			tcp_collector_port = atoi( colon );
		}
		delete [] host;
	}
}


bool
DCCollector::sendUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 ) 
{
	if( ! _is_configured ) {
			// nothing to do, treat it as success...
		return true;
	}

	// Add start time & seq # to the ads before we publish 'em
	char	buf [80];
	sprintf( buf, "%s=%ld", ATTR_DAEMON_START_TIME, (long) startTime );
	if ( ad1 ) {
		ad1->InsertOrUpdate( buf );
	}
	if ( ad2 ) {
		ad2->InsertOrUpdate( buf );
	}
	if ( ad1 ) {
		int	seq = adSeqMan.GetSequence( ad1 );
		sprintf( buf, "%s=%u", ATTR_UPDATE_SEQUENCE_NUMBER, seq );
		ad1->InsertOrUpdate( buf );
	}
	if ( ad2 ) {
		int	seq = adSeqMan.GetSequence( ad2 );
		sprintf( buf, "%s=%u", ATTR_UPDATE_SEQUENCE_NUMBER, seq );
		ad2->InsertOrUpdate( buf );
	}

	// Insert "my address"
	char	*tmp = global_dc_sinful( );
	if ( tmp ) {
		sprintf( buf, "%s=\"%s\"", ATTR_MY_ADDRESS, tmp );
		// No need to free tmp -- it's static

		if ( ad1 ) {
			if ( ad1->LookupString( ATTR_MY_ADDRESS, &tmp ) ) {
				free( tmp );	// tmp from LookupString does need to be freed
			} else {
				ad1->InsertOrUpdate( buf );
			}
		}
		if ( ad2 ) {
			if ( ad2->LookupString( ATTR_MY_ADDRESS, &tmp ) ) {
				free( tmp );
			} else {
				ad2->InsertOrUpdate( buf );
			}
		}
	}

	if( cmd == UPDATE_COLLECTOR_AD || cmd == INVALIDATE_COLLECTOR_ADS ) {
			// we *never* want to use TCP to send pool updates to the
			// developer collector.  so, regardless of what the config
			// files says, always use UDP for these commands...
		return sendUDPUpdate( cmd, ad1, ad2 );
	}

	if( use_tcp ) {
		return sendTCPUpdate( cmd, ad1, ad2 );
	}
	return sendUDPUpdate( cmd, ad1, ad2  );
}



bool
DCCollector::finishUpdate( Sock* sock, ClassAd* ad1, ClassAd* ad2 )
{
	sock->encode();
	if( ad1 && ! ad1->put(*sock) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send ClassAd #1 to collector" );
		return false;
	}
	if( ad2 && ! ad2->put(*sock) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send ClassAd #2 to collector" );
		return false;
	}
	if( ! sock->eom() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send EOM to collector" );
		return false;
	}
	return true;
}


bool
DCCollector::sendUDPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 )
{
		// with UDP it's pretty straight forward.  We always want to
		// use Daemon::startCommand() so we get all the security stuff
		// in every update.  In fact, we want to recreate the SafeSock
		// itself each time, since it doesn't seem to work if we reuse
		// the SafeSock object from one update to the next...

	dprintf( D_FULLDEBUG,
			 "Attempting to send update via UDP to collector %s\n",
			 udp_update_destination );

	SafeSock ssock;
	ssock.timeout( 30 );
	ssock.encode();

		// since we're dealing w/ UDP here, we can use all the info
		// already stored in the generic Daemon object data members
		// for this...
	if( ! ssock.connect(_addr, _port) ) {
		MyString err_msg = "Failed to connect to collector ";
		err_msg += udp_update_destination;
		newError( CA_CONNECT_FAILED, err_msg.Value() );
		return false;
	}

	if( ! startCommand(cmd, &ssock, 20) ) { 
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send UDP update command to collector" );
		return false;
	}

	return finishUpdate( &ssock, ad1, ad2 );
}


bool
DCCollector::sendTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 )
{
	dprintf( D_FULLDEBUG,
			 "Attempting to send update via TCP to collector %s\n",
			 tcp_update_destination );

	if( ! update_rsock ) {
			// we don't have a TCP sock for sending an update.  we've
			// got to create one.  this is a somewhat complicated
			// process, since we've got to create a ReliSock, connect
			// to the collector, and start a security session.
			// unfortunately, the only way to start a security session
			// is to send an initial command, so we'll handle that
			// update at the same time.  if the security API changes
			// in the future, we'll be able to make this code a little
			// more straight-forward...
		return initiateTCPUpdate( cmd, ad1, ad2 );
	}

		// otherwise, we've already got our socket, it's connected,
		// the security session is going, etc.  so, all we have to do
		// is send the actual update to the existing socket.  the only
		// thing we have to watch out for is if the collector
		// invalidated our cached socket, and if so, we'll have to
		// start another connection.
		// since finishUpdate() assumes we've already sent the command
		// int, and since we do *NOT* want to use startCommand() again
		// on a cached TCP socket, just code the int ourselves...
	update_rsock->encode();
	update_rsock->put( cmd );
	if( finishUpdate(update_rsock, ad1, ad2) ) {
		return true;
	}
	dprintf( D_FULLDEBUG, 
			 "Couldn't reuse TCP socket to update collector, "
			 "starting new connection\n" );
	delete update_rsock;
	update_rsock = NULL;
	return initiateTCPUpdate( cmd, ad1, ad2 );
}



bool
DCCollector::initiateTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 )
{
	if( update_rsock ) {
		delete update_rsock;
		update_rsock = NULL;
	}
	update_rsock = new ReliSock;
	update_rsock->timeout( 30 );
	update_rsock->encode();
	if( ! update_rsock->connect(tcp_collector_addr, tcp_collector_port) ) {
		MyString err_msg = "Failed to connect to collector ";
		err_msg += updateDestination();
		newError( CA_CONNECT_FAILED, err_msg.Value() );
		delete update_rsock;
		update_rsock = NULL;
		return false;
	}
	if( ! startCommand(cmd, update_rsock, 20) ) { 
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send TCP update command to collector" );
		return false;
	}
	return finishUpdate( update_rsock, ad1, ad2 );
}


void
DCCollector::displayResults( void )
{
	dprintf( D_FULLDEBUG, "Will use %s to update collector %s\n", 
			 use_tcp ? "TCP" : "UDP", updateDestination() );
}


// Figure out how we're going to identify the destination for our
// updates when printing to the logs, etc. 
const char*
DCCollector::updateDestination( void )
{
	if( use_tcp ) { 
		return tcp_update_destination;
	}
	return udp_update_destination;
}


void
DCCollector::initDestinationStrings( void )
{
	if( udp_update_destination ) {
		delete [] udp_update_destination;
		udp_update_destination = NULL;
	}
	if( tcp_update_destination ) {
		delete [] tcp_update_destination;
		tcp_update_destination = NULL;
	}

	MyString dest;

		// UDP updates will always be sent to whatever info we've got
		// in the Daemon object.  So, there's nothing hard to do for
		// this... just see what useful info we have and use it. 
	if( _full_hostname ) {
		dest = _full_hostname;
		dest += ' ';
		dest += _addr;
	} else {
		dest = _addr;
	}
	udp_update_destination = strnewp( dest.Value() );

		// TCP updates, if they happen at all, might go to a different
		// place.  So, we've got to do a little more work to figure
		// out what we should use...

	if( ! tcp_collector_host ) { 
			// they didn't supply anything, so we should use the info
			// in the Daemon part of ourself, which we've already got
			// in the udp_update_destination.  so we just use that.
		tcp_update_destination = strnewp( udp_update_destination );

	} else if( is_valid_sinful(tcp_collector_host) ) { 
			// they gave us a specific host, but it's already in
			// sinful-string form, so that's all we can do...
		tcp_update_destination = strnewp( tcp_collector_host );

	} else {
			// they gave us either an IP or a hostname, either
			// way... use what they gave us, and tack on the port
			// we're using (which either came from them, or is the
			// default COLLECTOR_PORT if unspecified).

		dest = tcp_collector_addr;
		char buf[64];
		sprintf( buf, "%d", tcp_collector_port );
		dest += " (port: ";
		dest += buf;
		dest += ')';
		tcp_update_destination = strnewp( dest.Value() );
	}
}
/*
DaemonList *
DCCollector::getCollectors() {

	DaemonList * result = new DaemonList();
	DCCollector * collector = NULL;


		// Read the new names from config file
	StringList collector_name_list;
	char * collector_name_param = NULL;
	getCmHostFromConfig ( "COLLECTOR", collector_name_param );
	
	if (collector_name_param  && *collector_name_param) {
		collector_name_list.initializeFromString(collector_name_param);
	
			// Create collector objects
		collector_name_list.rewind();
		char * collector_name = NULL;
		while ((collector_name = collector_name_list.next()) != NULL) {
			dprintf (D_FULLDEBUG, "Adding collector %s\n", collector_name);
			collector = new DCCollector (collector_name);
			result->append (collector);
		}
	} else {
			// Otherwise, just return an empty list
		dprintf (D_ALWAYS, "ERROR: Unable to find collector info in configuration file!!!\n");
	}

	if (collector_name_param)
		free ( collector_name_param );

	return result;
}
*/

// Constructor for the Ad Sequence Number
DCCollectorAdSeq::DCCollectorAdSeq( const char *inName,
									const char *inMyType,
									const char *inMachine )
{
	// Copy the fields
	if ( inName ) {
		Name = strdup( inName );
	} else {
		Name = NULL;
	}
	if ( inMyType ) {
		MyType = strdup( inMyType );
	} else {
		MyType = NULL;
	}
	if ( inMachine ) {
		Machine = strdup( inMachine );
	} else {
		Machine = NULL;
	}
	sequence = 0;
}

// Destructor for the Ad Sequence Number
DCCollectorAdSeq::~DCCollectorAdSeq( void )
{
	// Free the allocated strings
	if ( Name ) {
		free( Name );
	}
	if ( MyType ) {
		free( MyType );
	}
	if ( Machine ) {
		free( Machine );
	}
}

// Match operator
bool
DCCollectorAdSeq::Match( const char *inName,
						 const char *inMyType,
						 const char *inMachine )
{	
	// Check for complete match.. Return false if there are ANY mismatches
	if ( inName ) {
		if ( ! Name ) {
			return false;
		}
		if ( strcmp( Name, inName ) ) {
			return false;
		}
	} else if ( Name )
	{
		return false;
	}

	if ( inMyType ) {
		if ( ! MyType ) {
			return false;
		}
		if ( strcmp( MyType, inMyType ) ) {
			return false;
		}
	} else if ( MyType )
	{
		return false;
	}

	if ( inMachine ) {
		if ( ! Machine ) {
			return false;
		}
		if ( strcmp( Machine, inMachine ) ) {
			return false;
		}
	} else if ( Machine )
	{
		return false;
	}

	// If we passed all the tests, must be good.
	return true;
}

// Get the sequence number
unsigned
DCCollectorAdSeq::GetSequence( void )
{
	return sequence++;
}


// Constructor for the Ad Sequence Number Manager
DCCollectorAdSeqMan::DCCollectorAdSeqMan( void ) 
{
	// adSeqInfo = new ExtArray<DCCollectorAdSeq*>;
	numAds = 0;
}

// Destructor for the Ad Sequence Number Manager
DCCollectorAdSeqMan::~DCCollectorAdSeqMan( void )
{
	int				adNum;

	for ( adNum = 0;  adNum < numAds;  adNum++ ) {
		delete adSeqInfo[adNum];
	}
}

// Get the sequence number
unsigned
DCCollectorAdSeqMan::GetSequence( ClassAd *ad )
{
	int				adNum;
	char				*name = NULL;
	char				*myType = NULL;
	char				*machine = NULL;
	DCCollectorAdSeq	*adSeq = NULL;

	// Extract the 'key' attributes of the ClassAd
	ad->LookupString( ATTR_NAME, &name );
	ad->LookupString( ATTR_MY_TYPE, &myType );
	ad->LookupString( ATTR_MACHINE, &machine );

	// Walk through the ads that we know of, find a match...
	for ( adNum = 0;  adNum < numAds;  adNum++ ) {
		if ( adSeqInfo[adNum]->Match( name, myType, machine ) ) {
			adSeq = adSeqInfo[adNum];
			break;
		}
	}

	// No matches; create a new one, add to the list
	if ( ! adSeq ) {
		adSeq = new DCCollectorAdSeq ( name, myType, machine );
		adSeqInfo[numAds++] = adSeq;
	}

	// Free up memory...
	if ( name ) {
		free( name );
		name = NULL;
	}
	if ( myType ) {
		free( myType );
		myType = NULL;
	}
	if ( machine ) {
		free( machine );
		machine = NULL;
	}

	// Finally, return the sequence
	return adSeq->GetSequence( );
}

