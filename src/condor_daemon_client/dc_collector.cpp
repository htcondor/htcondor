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
#include "condor_config.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "internet.h"
#include "daemon.h"
#include "condor_daemon_core.h"
#include "dc_collector.h"
#include "daemon_core_sock_adapter.h"

// Instantiate things

DCCollector::DCCollector( const char* dcName, UpdateType uType ) 
	: Daemon( DT_COLLECTOR, dcName, NULL )
{
	up_type = uType;
	init( true );
	adSeqMan = new DCCollectorAdSeqMan();
}


void
DCCollector::init( bool needs_reconfig )
{
	static long bootTime = 0;

	pending_update_list = NULL;
	update_rsock = NULL;
	tcp_collector_host = NULL;
	tcp_collector_addr = NULL;
	tcp_collector_port = 0;
	use_tcp = false;
	use_nonblocking_update = true;
	udp_update_destination = NULL;
	tcp_update_destination = NULL;

	if (bootTime == 0) {
		bootTime = time( NULL );
	} 
	startTime = bootTime;

	adSeqMan = NULL;

	if( needs_reconfig ) {
		reconfig();
	}
}


DCCollector::DCCollector( const DCCollector& copy ) : Daemon(copy)
{
	init( false );
	deepCopy( copy );
}


DCCollector&
DCCollector::operator = ( const DCCollector& copy )
{
		// don't copy ourself!
    if (&copy != this) {
		deepCopy( copy );
	}

    return *this;
}


void
DCCollector::deepCopy( const DCCollector& copy )
{
	if( update_rsock ) {
		delete update_rsock;
		update_rsock = NULL;
	}
		/*
		  for now, we're not going to attempt to copy the update_rsock
		  from the copy, since i'm not sure i trust ReliSock's copy
		  constructor to do the right thing once the original goes
		  away... DCCollector will be able to re-create this socket
		  for TCP updates.  it's a little expensive, since we need a
		  whole new connect(), etc, but in most cases, we're not going
		  to be doing this very often, and correctness is more
		  important than speed at the moment.  once we have more time
		  for testing, we can figure out if just copying the
		  update_rsock works and TCP updates are still happy...
		*/

	if( tcp_collector_host ) {
		delete [] tcp_collector_host;
	}
	tcp_collector_host = strnewp( copy.tcp_collector_host );

	if( tcp_collector_addr ) {
		delete [] tcp_collector_addr;
	}
	tcp_collector_addr = strnewp( copy.tcp_collector_addr );

	tcp_collector_port = copy.tcp_collector_port;

	use_tcp = copy.use_tcp;
	use_nonblocking_update = copy.use_nonblocking_update;

	up_type = copy.up_type;

	if( udp_update_destination ) {
        delete [] udp_update_destination;
    }
	udp_update_destination = strnewp( copy.udp_update_destination );

    if( tcp_update_destination ) {
        delete [] tcp_update_destination;
	}

    tcp_update_destination = strnewp( copy.tcp_update_destination );
    

	startTime = copy.startTime;

	if( adSeqMan ) {
		delete adSeqMan;
		adSeqMan = NULL;
	}
	if( copy.adSeqMan ) {
		adSeqMan = new DCCollectorAdSeqMan( *copy.adSeqMan );
	} else {
		adSeqMan = new DCCollectorAdSeqMan();
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

	use_nonblocking_update = param_boolean("NONBLOCKING_COLLECTOR_UPDATE",true);

	if( ! _addr ) {
		locate();
		if( ! _is_configured ) {
			dprintf( D_FULLDEBUG, "COLLECTOR address not defined in "
					 "config file, not doing updates\n" );
			return;
		}
	}

		// Blacklist this collector if last failed contact took more
		// than 1% of the time that has passed since that operation
		// started.  (i.e. if contact fails quickly, don't worry, but
		// if it takes a long time to fail, be cautious.
	blacklisted.setTimeslice(0.01);
		// Set an upper bound of one hour for the collector to be blacklisted.
	int avoid_time = param_integer("DEAD_COLLECTOR_MAX_AVOIDANCE_TIME",3600);
	blacklisted.setMaxInterval(avoid_time);
	blacklisted.setInitialInterval(0);

	parseTCPInfo();
	initDestinationStrings();
	displayResults();
}


void
DCCollector::parseTCPInfo( void )
{
	switch( up_type ) {
	case TCP:
		use_tcp = true;
		break;
	case UDP:
		use_tcp = false;
		break;
	case CONFIG:
		use_tcp = false;
		char *tmp = param( "TCP_UPDATE_COLLECTORS" );
		if( tmp ) {
			StringList tcp_collectors;

			tcp_collectors.initializeFromString( tmp );
			free( tmp );
 			if( _name && 
				tcp_collectors.contains_anycase_withwildcard(_name) )
			{	
				use_tcp = true;
				break;
			}
		}
		use_tcp = param_boolean( "UPDATE_COLLECTOR_WITH_TCP", use_tcp );
		if( !hasUDPCommandPort() ) {
			use_tcp = true;
		}
		break;
	}

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
DCCollector::sendUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking ) 
{
	if( ! _is_configured ) {
			// nothing to do, treat it as success...
		return true;
	}

	if(!use_nonblocking_update || !daemonCoreSockAdapter.isEnabled()) {
			// Either caller OR config may turn off nonblocking updates.
			// In other words, both must be true to enable nonblocking.
			// Also, must have daemonCoreSockAdapter enabled.
		nonblocking = false;
	}

	// Add start time & seq # to the ads before we publish 'em
	if ( ad1 ) {
		ad1->Assign(ATTR_DAEMON_START_TIME,(long)startTime);
	}
	if ( ad2 ) {
		ad2->Assign(ATTR_DAEMON_START_TIME,(long)startTime);
	}
	if ( ad1 ) {
		unsigned seq = adSeqMan->getSequence( ad1 );
		ad1->Assign(ATTR_UPDATE_SEQUENCE_NUMBER,seq);
	}
	if ( ad2 ) {
		unsigned seq = adSeqMan->getSequence( ad2 );
		ad2->Assign(ATTR_UPDATE_SEQUENCE_NUMBER,seq);
	}

		// Prior to 7.2.0, the negotiator depended on the startd
		// supplying matching MyAddress in public and private ads.
	if ( ad1 && ad2 ) {
		ad2->CopyAttribute(ATTR_MY_ADDRESS,ad1);
	}

    // My initial plan was to publish these for schedd, however they will provide
    // potentially useful context for performance/health assessment of any daemon 
    if (ad1) {
        ad1->Assign(ATTR_DETECTED_CPUS, param_integer("DETECTED_CORES", 0));
        ad1->Assign(ATTR_DETECTED_MEMORY, param_integer("DETECTED_MEMORY", 0));
    }
    if (ad2) {
        ad2->Assign(ATTR_DETECTED_CPUS, param_integer("DETECTED_CORES", 0));
        ad2->Assign(ATTR_DETECTED_MEMORY, param_integer("DETECTED_MEMORY", 0));
    }

		// We never want to try sending an update to port 0.  If we're
		// about to try that, and we're trying to talk to a local
		// collector, we should try re-reading the address file and
		// re-setting our port.
	if( _port == 0 ) {
		dprintf( D_HOSTNAME, "About to update collector with port 0, "
				 "attempting to re-read address file\n" );
		if( readAddressFile(_subsys) ) {
			_port = string_to_port( _addr );
			tcp_collector_port = _port;
			if( tcp_collector_addr ) {
				delete [] tcp_collector_addr;
			}
			tcp_collector_addr = strnewp( _addr );
			parseTCPInfo(); // update use_tcp
			dprintf( D_HOSTNAME, "Using port %d based on address \"%s\"\n",
					 _port, _addr );
		}
	}

	if( _port <= 0 ) {
			// If it's still 0, we've got to give up and fail.
		std::string err_msg;
		formatstr(err_msg, "Can't send update: invalid collector port (%d)",
						 _port );
		newError( CA_COMMUNICATION_ERROR, err_msg.c_str() );
		return false;
	}

	if( cmd == UPDATE_COLLECTOR_AD || cmd == INVALIDATE_COLLECTOR_ADS ) {
			// we *never* want to use TCP to send pool updates to the
			// developer collector.  so, regardless of what the config
			// files says, always use UDP for these commands...
		return sendUDPUpdate( cmd, ad1, ad2, nonblocking );
	}

	if( use_tcp ) {
		return sendTCPUpdate( cmd, ad1, ad2, nonblocking );
	}
	return sendUDPUpdate( cmd, ad1, ad2, nonblocking );
}



bool
DCCollector::finishUpdate( DCCollector *self, Sock* sock, ClassAd* ad1, ClassAd* ad2 )
{
	// This is a static function so that we can call it from a
	// nonblocking startCommand() callback without worrying about
	// longevity of the DCCollector instance.

	sock->encode();
	if( ad1 && ! ad1->put(*sock) ) {
		if(self) {
			self->newError( CA_COMMUNICATION_ERROR,
			                "Failed to send ClassAd #1 to collector" );
		}
		return false;
	}
	if( ad2 && ! ad2->put(*sock) ) {
		if(self) {
			self->newError( CA_COMMUNICATION_ERROR,
			          "Failed to send ClassAd #2 to collector" );
			return false;
		}
	}
	if( ! sock->end_of_message() ) {
		if(self) {
			self->newError( CA_COMMUNICATION_ERROR,
			          "Failed to send EOM to collector" );
		}
		return false;
	}
	return true;
}

class UpdateData {
public:
	ClassAd *ad1;
	ClassAd *ad2;
	DCCollector *dc_collector;
	UpdateData *next_in_list;

	UpdateData(ClassAd *cad1, ClassAd *cad2, DCCollector *dc_collect) {
		this->ad1 = NULL;
		this->ad2 = NULL;
		this->dc_collector = dc_collect;
			// In case the collector object gets destructed before this
			// update is finished, we need to register ourselves with
			// the dc_collector object so that it can null out our
			// pointer to it.  This is done using a linked-list of
			// UpdateData objects.

		next_in_list = dc_collect->pending_update_list;
		dc_collect->pending_update_list = this;

		if(cad1) {
			this->ad1 = new ClassAd(*cad1);
		}
		if(cad2) {
			this->ad2 = new ClassAd(*cad2);
		}
	}
	~UpdateData() {
		if(ad1) {
			delete ad1;
		}
		if(ad2) {
			delete ad2;
		}
			// Remove ourselves from the dc_collector's list.
		if(dc_collector) {
			UpdateData **ud = &dc_collector->pending_update_list;
			while(*ud) {
				if(*ud == this) {
					*ud = next_in_list;
					break;
				}
				ud = &(*ud)->next_in_list;
			}
		}
	}
	void DCCollectorGoingAway() {
			// The DCCollector object is being deleted.  We don't
			// need it in order to finish the update.  We only keep
			// a reference to it in order to do non-essential things.

		dc_collector = NULL;
		if(next_in_list) {
			next_in_list->DCCollectorGoingAway();
		}
	}
	static void startUpdateCallback(bool success,Sock *sock,CondorError * /* errstack */,void *misc_data) {
		UpdateData *ud = (UpdateData *)misc_data;

			// We got here because a nonblocking call to startCommand()
			// has called us back.  Now we will finish sending the update.

			// NOTE: it is possible that by the time we get here,
			// dc_collector has been deleted.  If that is the case,
			// dc_collector will be NULL.  We will go ahead and finish
			// the update anyway, but we will not do anything that
			// modifies dc_collector (such as saving the TCP sock for
			// future use).

		if(!success) {
			char const *who = "unknown";
			if(sock) who = sock->get_sinful_peer();
			dprintf(D_ALWAYS,"Failed to start non-blocking update to %s.\n",who);
		}
		else if(sock && !DCCollector::finishUpdate(ud->dc_collector,sock,ud->ad1,ud->ad2)) {
			char const *who = "unknown";
			if(sock) who = sock->get_sinful_peer();
			dprintf(D_ALWAYS,"Failed to send non-blocking update to %s.\n",who);
		}
		else if(sock && sock->type() == Sock::reli_sock) {
			// We keep the TCP socket around for sending more updates.
			if(ud->dc_collector && ud->dc_collector->update_rsock == NULL) {
				ud->dc_collector->update_rsock = (ReliSock *)sock;
				sock = NULL;
			}
		}
		if(sock) {
			delete sock;
		}
		delete ud;
	}
};

bool
DCCollector::sendUDPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking )
{
		// with UDP it's pretty straight forward.  We always want to
		// use Daemon::startCommand() so we get all the security stuff
		// in every update.  In fact, we want to recreate the SafeSock
		// itself each time, since it doesn't seem to work if we reuse
		// the SafeSock object from one update to the next...

	dprintf( D_FULLDEBUG,
			 "Attempting to send update via UDP to collector %s\n",
			 udp_update_destination );

	bool raw_protocol = false;
	if( cmd == UPDATE_COLLECTOR_AD || cmd == INVALIDATE_COLLECTOR_ADS ) {
			// we *never* want to do security negotiation with the
			// developer collector.
		raw_protocol = true;
	}

	if(nonblocking) {
		UpdateData *ud = new UpdateData(ad1,ad2,this);
		startCommand_nonblocking(cmd, Sock::safe_sock, 20, NULL, UpdateData::startUpdateCallback, ud, NULL, raw_protocol );
		return true;
	}

	Sock *ssock = startCommand(cmd, Sock::safe_sock, 20, NULL, NULL, raw_protocol);
	if(!ssock) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send UDP update command to collector" );
		return false;
	}

	bool success = finishUpdate( this, ssock, ad1, ad2 );
	delete ssock;

	return success;
}


bool
DCCollector::sendTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking )
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
		return initiateTCPUpdate( cmd, ad1, ad2, nonblocking );
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
	if( finishUpdate(this, update_rsock, ad1, ad2) ) {
		return true;
	}
	dprintf( D_FULLDEBUG, 
			 "Couldn't reuse TCP socket to update collector, "
			 "starting new connection\n" );
	delete update_rsock;
	update_rsock = NULL;
	return initiateTCPUpdate( cmd, ad1, ad2, nonblocking );
}



bool
DCCollector::initiateTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking )
{
	if( update_rsock ) {
		delete update_rsock;
		update_rsock = NULL;
	}
	if(nonblocking) {
		UpdateData *ud = new UpdateData(ad1,ad2,this);
		startCommand_nonblocking(cmd, Sock::reli_sock, 20, NULL, UpdateData::startUpdateCallback, ud );
		return true;
	}
	Sock *sock = startCommand(cmd, Sock::reli_sock, 20);
	if(!sock) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send TCP update command to collector" );
		dprintf(D_ALWAYS,"Failed to send update to %s.\n",idStr());
		return false;
	}
	update_rsock = (ReliSock *)sock;
	return finishUpdate( this, update_rsock, ad1, ad2 );
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

	std::string dest;

		// UDP updates will always be sent to whatever info we've got
		// in the Daemon object.  So, there's nothing hard to do for
		// this... just see what useful info we have and use it. 
	if( _full_hostname ) {
		dest = _full_hostname;
		if ( _addr) {
			dest += ' ';
			dest += _addr;
		}
	} else {
		if (_addr) dest = _addr;
	}
	udp_update_destination = strnewp( dest.c_str() );

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

		formatstr(dest, "%s (port: %d)", tcp_collector_addr ? tcp_collector_addr : "", tcp_collector_port);
		tcp_update_destination = strnewp( dest.c_str() );
	}
}


//
// Ad Sequence Number class methods
//

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

// Copy constructor for the Ad Sequence Number
DCCollectorAdSeq::DCCollectorAdSeq( const DCCollectorAdSeq &ref )
{
	const char *tmp;

	tmp = ref.getName( );
	if ( tmp ) {
		this->Name = strdup( tmp );
	} else {
		this->Name = NULL;
	}

	tmp = ref.getMyType( );
	if ( tmp ) {
		this->MyType = strdup( tmp );
	} else {
		this->MyType = NULL;
	}

	tmp = ref.getMachine( );
	if ( tmp ) {
		this->Machine = strdup( tmp );
	} else {
		this->Machine = NULL;
	}

	this->sequence = ref.getSequence( );
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
						 const char *inMachine ) const
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
DCCollectorAdSeq::getSequenceAndIncrement( void )
{
	return sequence++;
}


//
// Ad Sequence Number Mananager class methods
//

// Constructor for the Ad Sequence Number Manager
DCCollectorAdSeqMan::DCCollectorAdSeqMan( void ) 
{
	numAds = 0;
}

// Constructor for the Ad Sequence Number Manager
DCCollectorAdSeqMan::DCCollectorAdSeqMan( const DCCollectorAdSeqMan &ref,
										  bool copy_list )
{
	numAds = 0;
	if ( ! copy_list ) {
		return;
	}

	// Get list info from
	int count = ref.getNumAds( );
	const ExtArray<DCCollectorAdSeq *> &copy_array =
		ref.getSeqInfo( );

	// Now, copy the whole thing
	int		adNum;
	for( adNum = 0;  adNum < count;  adNum++ ) {
		DCCollectorAdSeq *newAdSeq =
			new DCCollectorAdSeq ( *(copy_array[adNum]) );
		this->adSeqInfo[this->numAds++] = newAdSeq;
	}
	
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
DCCollectorAdSeqMan::getSequence( const ClassAd *ad )
{
	int					adNum;
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
	return adSeq->getSequenceAndIncrement( );
}

DCCollector::~DCCollector( void )
{
	if( update_rsock ) {
		delete( update_rsock );
	}
	if( adSeqMan ) {
		delete( adSeqMan );
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

		// In case there are any nonblocking updates in progress,
		// let them know this DCCollector object is going away.
	if(pending_update_list) {
		pending_update_list->DCCollectorGoingAway();
	}
}

bool
DCCollector::isBlacklisted() {
	return !blacklisted.isTimeToRun();
}

void
DCCollector::blacklistMonitorQueryStarted() {
	blacklisted.setStartTimeNow();
}

void
DCCollector::blacklistMonitorQueryFinished( bool success ) {
	if( success ) {
		blacklisted.reset();
	}
	else {
		blacklisted.setFinishTimeNow();

		unsigned int delta = blacklisted.getTimeToNextRun();
		if( delta > 0 ) {
			dprintf( D_ALWAYS, "Will avoid querying collector %s %s for %us "
			         "if an alternative succeeds.\n",
			         name(),
					 addr(),
			         delta );
		}
	}
}
