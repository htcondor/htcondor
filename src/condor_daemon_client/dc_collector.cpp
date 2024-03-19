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
#include "condor_attributes.h"
#include "internet.h"
#include "daemon.h"
#include "condor_daemon_core.h"
#include "dc_collector.h"

#include <algorithm>

std::map< std::string, Timeslice > DCCollector::blacklist;


// Instantiate things

DCCollector::DCCollector( const char* dcName, UpdateType uType ) 
	: Daemon( DT_COLLECTOR, dcName, NULL )
{
	up_type = uType;
	init( true );
}

void
DCCollector::init( bool needs_reconfig )
{
	static long bootTime = 0;
	reconfigTime = 0;

	update_rsock = NULL;
	use_tcp = true;
	use_nonblocking_update = true;
	update_destination = NULL;
	timerclear( &m_blacklist_monitor_query_started );

	if (bootTime == 0) {
		bootTime = time( NULL );
	} 
	reconfigTime = startTime = bootTime;

	if( needs_reconfig ) {
		reconfigTime = time( NULL );
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

	use_tcp = copy.use_tcp;
	use_nonblocking_update = copy.use_nonblocking_update;

	up_type = copy.up_type;

	if( update_destination ) {
        free(update_destination);
    }
	update_destination = copy.update_destination ? strdup( copy.update_destination ) : NULL;

	startTime = copy.startTime;
}


bool
DCCollector::requestScheddToken(const std::string &schedd_name,
	const std::vector<std::string> &authz_bounding_set,
	int lifetime, std::string &token, CondorError &err)
{
	classad::ClassAd request_ad;

	if (!authz_bounding_set.empty())
	{
		if (!request_ad.InsertAttr(ATTR_SEC_LIMIT_AUTHORIZATION, join(authz_bounding_set, ","))) {
			err.push("DCCollector", 1, "Failed to insert authorization bound.");
			return false;
		}
	}

	if ((lifetime >= 0) &&
		!request_ad.InsertAttr(ATTR_SEC_TOKEN_LIFETIME, lifetime))
	{
		err.push("DCCollector", 1, "Failed to insert lifetime.");
		return false;
	}

	if (!request_ad.InsertAttr(ATTR_NAME, schedd_name))
	{
		err.push("DCCollector", 1, "Failed to insert schedd name.");
		return false;
	}

	ReliSock rSock;
	rSock.timeout(5);
	if (!connectSock(&rSock)) {
		err.pushf("DCCollector", 2, "Failed to connect "
			"to remote daemon at '%s'", _addr.c_str());
		dprintf(D_FULLDEBUG, "DCCollector::requestScheddToken() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!startCommand(IMPERSONATION_TOKEN_REQUEST, &rSock, 20, &err))
	{
		err.pushf("DAEMON", 1, "failed to start "
			"command for token request with remote collector at '%s'.",
			_addr.c_str());
		dprintf(D_FULLDEBUG, "DCCollector::requestScheddToken() failed to start "
			"command for token request with remote collector at '%s'.",
			_addr.c_str());
		return false;
	}

	rSock.encode();
	if (!putClassAd(&rSock, request_ad) || !rSock.end_of_message()) {
		err.pushf("DAEMON", 1, "Failed to send request to "
			"remote collector at '%s'",
			_addr.c_str());
		dprintf(D_FULLDEBUG, "DCCollector::requestScheddToken() failed to send "
			"request to remote collector at '%s'\n",
			_addr.c_str());
		return false;
	}

	rSock.decode();
	classad::ClassAd result_ad;
	if (!getClassAd(&rSock, result_ad) || !rSock.end_of_message()) {
		err.pushf("DAEMON", 1, "Failed to recieve "
			"response from remote collector at '%s'",
			_addr.c_str());
		dprintf(D_FULLDEBUG, "DCCollector::requestScheddToken() failed to recieve "
			"response from remote daemon at '%s'\n",
			_addr.c_str());
		return false;
	}

	std::string err_msg;
	if (result_ad.EvaluateAttrString(ATTR_ERROR_STRING, err_msg)) {
		int error_code = 0;
		result_ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code);
		if (!error_code) error_code = -1;

		err.push("DAEMON", error_code, err_msg.c_str());
		return false;
	}

	if (!result_ad.EvaluateAttrString(ATTR_SEC_TOKEN, token) || token.empty()) {
		err.pushf("DAEMON", 1, "BUG! DCCollector::requestScheddToken() "
			"received a malformed ad, containing no resulting token and no "
			"error message, from remote collector at '%s'",
			_addr.c_str());
		dprintf(D_FULLDEBUG, "BUG!  DCCollector::requestScheddToken() "
			"received a malformed ad, containing no resulting token and no "
			"error message, from remote daemon at '%s'\n",
			_addr.c_str());
		return false;
	}

	return true;
}

void
DCCollector::reconfig( void )
{
	use_nonblocking_update = param_boolean("NONBLOCKING_COLLECTOR_UPDATE",true);

	if( _addr.empty() ) {
		locate();
		if( ! _is_configured ) {
			dprintf( D_FULLDEBUG, "COLLECTOR address not defined in "
					 "config file, not doing updates\n" );
			return;
		}
	}

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
	case CONFIG_VIEW:
		use_tcp = false;
		char *tmp = param( "TCP_UPDATE_COLLECTORS" );
		if( tmp ) {
			std::vector<std::string> tcp_collectors = split(tmp);
			free( tmp );
			if( ! _name.empty() &&
				contains_anycase_withwildcard(tcp_collectors, _name) )
			{	
				use_tcp = true;
				break;
			}
		}
		if ( up_type == CONFIG_VIEW ) {
			use_tcp = param_boolean( "UPDATE_VIEW_COLLECTOR_WITH_TCP", false );
		} else {
			use_tcp = param_boolean( "UPDATE_COLLECTOR_WITH_TCP", true );
		}
		if( !hasUDPCommandPort() ) {
			use_tcp = true;
		}
		break;
	}
}


bool
DCCollector::sendUpdate( int cmd, ClassAd* ad1, DCCollectorAdSequences& adSeq, ClassAd* ad2, bool nonblocking, StartCommandCallbackType callback_fn, void *miscdata)
{
	if( ! _is_configured ) {
			// nothing to do, treat it as success...
		return true;
	}

	if(!use_nonblocking_update || !daemonCore) {
			// Either caller OR config may turn off nonblocking updates.
			// In other words, both must be true to enable nonblocking.
			// Also, must have DaemonCore intialized.
		nonblocking = false;
	}

	// Add start time & seq # to the ads before we publish 'em
	if ( ad1 ) {
		ad1->Assign(ATTR_DAEMON_START_TIME, startTime);
		ad1->Assign(ATTR_DAEMON_LAST_RECONFIG_TIME, reconfigTime);
	}
	if ( ad2 ) {
		ad2->Assign(ATTR_DAEMON_START_TIME, startTime);
		ad2->Assign(ATTR_DAEMON_LAST_RECONFIG_TIME, reconfigTime);
	}

	if ( ad1 ) {
		DCCollectorAdSeq* seqgen = adSeq.getAdSeq(*ad1);
		if (seqgen) {
			long long seq = seqgen->getSequence();
			ad1->Assign(ATTR_UPDATE_SEQUENCE_NUMBER, seq);
			if (ad2) { ad2->Assign(ATTR_UPDATE_SEQUENCE_NUMBER, seq); }
		}
	}

		// Prior to 7.2.0, the negotiator depended on the startd
		// supplying matching MyAddress in public and private ads.
	if ( ad1 && ad2 ) {
		CopyAttribute(ATTR_MY_ADDRESS,*ad2,*ad1);
	}

		// We never want to try sending an update to port 0.  If we're
		// about to try that, and we're trying to talk to a local
		// collector, we should try re-reading the address file and
		// re-setting our port.
	if( _port == 0 ) {
		dprintf( D_HOSTNAME, "About to update collector with port 0, "
				 "attempting to re-read address file\n" );
		if( readAddressFile(_subsys.c_str()) ) {
			_port = string_to_port( _addr.c_str() );
			parseTCPInfo(); // update use_tcp
			dprintf( D_HOSTNAME, "Using port %d based on address \"%s\"\n",
					 _port, _addr.c_str() );
		}
	}

	if( _port <= 0 ) {
			// If it's still 0, we've got to give up and fail.
		std::string err_msg;
		formatstr(err_msg, "Can't send update: invalid collector port (%d)",
						 _port );
		newError( CA_COMMUNICATION_ERROR, err_msg.c_str() );
		if (callback_fn) {
			(*callback_fn)(false, nullptr, nullptr, "", false, miscdata);
		}
		return false;
	}

	//
	// We don't want the collector to send TCP updates to itself, since
	// this could cause it to deadlock.  Since the only ad a collector
	// will ever advertise is its own, only check for *_COLLECTOR_ADS.
	//
	if( cmd == UPDATE_COLLECTOR_AD || cmd == INVALIDATE_COLLECTOR_ADS ) {
		if( daemonCore ) {
			const char * myOwnSinful = daemonCore->InfoCommandSinfulString();
			if( myOwnSinful == NULL ) {
				dprintf( D_ALWAYS, "Unable to determine my own address, will not update or invalidate collector ad to avoid potential deadlock.\n" );
				if (callback_fn) {
					(*callback_fn)(false, nullptr, nullptr, "", false, miscdata);
				}
				return false;
			}
			if( _addr.empty() ) {
				dprintf( D_ALWAYS, "Failing attempt to update or invalidate collector ad because of missing daemon address (probably an unresolved hostname; daemon name is '%s').\n", _name.c_str() );
				if (callback_fn) {
					(*callback_fn)(false, nullptr, nullptr, "", false, miscdata);
				}
				return false;
			}
			if( strcmp( myOwnSinful, _addr.c_str() ) == 0 ) {
				EXCEPT( "Collector attempted to send itself an update." );
			}
		}
	}

	if( use_tcp ) {
		return sendTCPUpdate( cmd, ad1, ad2, nonblocking, callback_fn, miscdata );
	}
	return sendUDPUpdate( cmd, ad1, ad2, nonblocking, callback_fn, miscdata );
}



bool
DCCollector::finishUpdate( DCCollector *self, Sock* sock, ClassAd* ad1, ClassAd* ad2, StartCommandCallbackType callback_fn, void *miscdata )
{
		// Only send secrets in the case where
		// the collector has been build since 8.9.3 and understands not
		// to share submitter secrets.
	auto *verinfo = sock->get_peer_version();
	bool send_submitter_secrets = false;
	if (verinfo && verinfo->built_since_version(8, 9, 3)) {
		send_submitter_secrets = true;
	}

		// If we are advertising to an admin-configured pool, then
		// we allow the admin to skip authorization; they presumably
		// know what they are doing.  However, if we are acting on
		// behalf of a user, then we assume that secrets must be encrypted!
	if (!self || (!self->getOwner().empty() && !sock->set_crypto_mode(true))) {
		send_submitter_secrets = false;
	}

	int options = send_submitter_secrets ? 0: PUT_CLASSAD_NO_PRIVATE;

	// This is a static function so that we can call it from a
	// nonblocking startCommand() callback without worrying about
	// longevity of the DCCollector instance.

	sock->encode();
	if( ad1 && ! putClassAd(sock, *ad1, options) ) {
		if(self) {
			self->newError( CA_COMMUNICATION_ERROR,
			                "Failed to send ClassAd #1 to collector" );
		}
		if (callback_fn) {
			(*callback_fn)(false, sock, nullptr, sock->getTrustDomain(), sock->shouldTryTokenRequest(), miscdata);
		}
		return false;
	}
		// This is always a private ad.
	if( ad2 && ! putClassAd(sock, *ad2, 0) ) {
		if(self) {
			self->newError( CA_COMMUNICATION_ERROR,
			          "Failed to send ClassAd #2 to collector" );
		}
		if (callback_fn) {
			(*callback_fn)(false, sock, nullptr, sock->getTrustDomain(), sock->shouldTryTokenRequest(), miscdata);
		}
		return false;
	}
	if( ! sock->end_of_message() ) {
		if(self) {
			self->newError( CA_COMMUNICATION_ERROR,
			          "Failed to send EOM to collector" );
		}
		if (callback_fn) {
			(*callback_fn)(false, sock, nullptr, sock->getTrustDomain(), sock->shouldTryTokenRequest(), miscdata);
		}
		return false;
	}
	if (callback_fn) {
		(*callback_fn)(true, sock, nullptr, sock->getTrustDomain(), sock->shouldTryTokenRequest(), miscdata);
	}
	return true;
}

class UpdateData {

private:
	int cmd;
	Stream::stream_type sock_type;
public:
	ClassAd *ad1;
	ClassAd *ad2;
	DCCollector *dc_collector;
	StartCommandCallbackType *m_callback_fn{nullptr};
	void *m_miscdata{nullptr};

	UpdateData(int ad_cmd, Stream::stream_type stype, ClassAd *cad1, ClassAd *cad2, DCCollector *dc_collect, StartCommandCallbackType *callback_fn, void *miscdata)
	  : cmd(ad_cmd),
	    sock_type(stype),
	    ad1(cad1 ? new ClassAd(*cad1) : NULL),
	    ad2(cad2 ? new ClassAd(*cad2) : NULL),
	    dc_collector(dc_collect),
	    m_callback_fn(callback_fn),
	    m_miscdata(miscdata)
	{
			// In case the collector object gets destructed before this
			// update is finished, we need to register ourselves with
			// the dc_collector object so that it can null out our
			// pointer to it.  This is done using a linked-list of
			// UpdateData objects.

		dc_collect->pending_update_list.push_back(this);
	}

	~UpdateData() {
		delete ad1;
		delete ad2;
			// Remove ourselves from the dc_collector's list.
		if(dc_collector) {
			std::deque<UpdateData *>::iterator iter = std::find(dc_collector->pending_update_list.begin(), dc_collector->pending_update_list.end(), this);
			if (iter != dc_collector->pending_update_list.end())
			{
				dc_collector->pending_update_list.erase(iter);
			}
		}
	}

	void DCCollectorGoingAway() {
			// The DCCollector object is being deleted.  We don't
			// need it in order to finish the update.  We only keep
			// a reference to it in order to do non-essential things.

		dc_collector = NULL;
	}

	static void startUpdateCallback(bool success,Sock *sock,CondorError * /* errstack */, const std::string &trust_domain, bool should_try_token_request, void *misc_data) {
		UpdateData *ud = (UpdateData *)misc_data;

			// We got here because a nonblocking call to startCommand()
			// has called us back.  Now we will finish sending the update.

			// NOTE: it is possible that by the time we get here,
			// dc_collector has been deleted.  If that is the case,
			// dc_collector will be NULL.  We will go ahead and finish
			// the update anyway, but we will not do anything that
			// modifies dc_collector (such as saving the TCP sock for
			// future use).

		DCCollector *dc_collector = ud->dc_collector;
		if(!success) {
			char const *who = "unknown";
			if(sock) {
				who = sock->get_sinful_peer();
			}
			if (ud->m_callback_fn) {
				(*ud->m_callback_fn)(false, sock, nullptr, trust_domain, should_try_token_request, ud->m_miscdata);
			}
			dprintf(D_ALWAYS,"Failed to start non-blocking update to %s.\n",who);
			if (dc_collector) {
				while (!dc_collector->pending_update_list.empty()) {
					// UpdateData's dtor removes this from the pending update list
					delete(dc_collector->pending_update_list.front());
				}
				ud = 0;	
			}
		}
		else if(sock && !DCCollector::finishUpdate(ud->dc_collector,sock,ud->ad1,ud->ad2, ud->m_callback_fn, ud->m_miscdata)) {
			char const *who = "unknown";
			if(sock) who = sock->get_sinful_peer();
			dprintf(D_ALWAYS,"Failed to send non-blocking update to %s.\n",who);
			if (dc_collector) {
				while (!dc_collector->pending_update_list.empty()) {
					// UpdateData's dtor removes this from the pending update list
					delete(dc_collector->pending_update_list.front());
				}
				ud = 0;	
			}
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
		if (ud) {
			delete ud;
		}

			// Now that we finished sending the update, we can start sequentially sending
			// the pending updates.  We send these updates synchronously in sequence
			// because if we did it all asynchronously, we may end up with many TCP
			// connections to the collector.  Instead we send the updates one at a time
			// via a single connection.

		if (dc_collector && dc_collector->pending_update_list.size())
		{

			// Here we handle pending updates by sending them over a stashed
			// TCP socket to the collector.
			//
			while (dc_collector->update_rsock && dc_collector->pending_update_list.size())
			{
				UpdateData *ud = dc_collector->pending_update_list.front();
				dc_collector->update_rsock->encode();
					// NOTE: If there's a valid TCP socket available, we always
					// push our updates over that (even if the update requested UDP).
					// I don't think mixing TCP/UDP to the same collector is supported, so
					// I believe this shortcut acceptable.
				if (!dc_collector->update_rsock->put( ud->cmd ) ||
					!DCCollector::finishUpdate(ud->dc_collector,dc_collector->update_rsock,ud->ad1,ud->ad2,ud->m_callback_fn,ud->m_miscdata))
				{
					char const *who = "unknown";
					if(dc_collector->update_rsock) {
						who = dc_collector->update_rsock->get_sinful_peer();
					}
					dprintf(D_ALWAYS,"Failed to send update to %s.\n",who);
					delete dc_collector->update_rsock;
					dc_collector->update_rsock = NULL;
					// Notice we remove the element from the list of pending updates
					// even on failure.
				}
				delete ud;
			}

			// Here we handle pending updates in the event that we do not have
			// a stashed TCP socket to the collector.  This could occur if
			// our TCP socket to the collector was closed for some reason
			// (e.g. the collector was restarted), or it may occur in the
			// case of UDP.  Note that we just start handling the next pending
			// update here, then go back to daemonCore with a callback registered
			// to ensure we do not block in the event we need to re-establish
			// a new TCP socket.
			if (dc_collector->pending_update_list.size())
			{
				UpdateData *ud = dc_collector->pending_update_list.front();
				dc_collector->startCommand_nonblocking(ud->cmd, ud->sock_type, 20, NULL, UpdateData::startUpdateCallback, ud );
			}
		}
	}
};

bool
DCCollector::sendUDPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking, StartCommandCallbackType callback_fn, void *miscdata )
{
		// with UDP it's pretty straight forward.  We always want to
		// use Daemon::startCommand() so we get all the security stuff
		// in every update.  In fact, we want to recreate the SafeSock
		// itself each time, since it doesn't seem to work if we reuse
		// the SafeSock object from one update to the next...

	dprintf( D_FULLDEBUG,
			 "Attempting to send update via UDP to collector %s\n",
			 update_destination );

	bool raw_protocol = false;
	if( cmd == UPDATE_COLLECTOR_AD || cmd == INVALIDATE_COLLECTOR_ADS ) {
			// we *never* want to do security negotiation with the
			// developer collector.
		raw_protocol = true;
	}

	if(nonblocking) {
		UpdateData *ud = new UpdateData(cmd, Sock::safe_sock, ad1, ad2, this, callback_fn, miscdata);
		if (this->pending_update_list.size() == 1)
		{
			startCommand_nonblocking(cmd, Sock::safe_sock, 20, NULL, UpdateData::startUpdateCallback, ud, NULL, raw_protocol );
		}
		return true;
	}

	Sock *ssock = startCommand(cmd, Sock::safe_sock, 20, NULL, NULL, raw_protocol);

	if(!ssock) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send UDP update command to collector" );
		if (callback_fn) {
			(*callback_fn)(false, nullptr, nullptr, "", false, miscdata);
		}
		return false;
	}

	bool success = finishUpdate( this, ssock, ad1, ad2, callback_fn, miscdata );
	delete ssock;

	return success;
}


bool
DCCollector::sendTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking, StartCommandCallbackType callback_fn, void *miscdata )
{
	dprintf( D_FULLDEBUG,
			 "Attempting to send update via TCP to collector %s\n",
			 update_destination );

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
		return initiateTCPUpdate( cmd, ad1, ad2, nonblocking, callback_fn, miscdata );
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
		//
		// also note we do NOT pass the callback_fn to finishUpdate here - the idea
		// is if finishUpdate fails, we turn around and try to start a new connection,
		// and we only want to invoke the callback once.  So we avoid passing the callback to
		// finishUpdate to prevent both finishUpdate and initiateUpdate from invoking the
		// callback function in the case we need to create a new connection.
	update_rsock->encode();
	if (update_rsock->put(cmd) && finishUpdate(this, update_rsock, ad1, ad2, nullptr, nullptr)) {
		if (callback_fn) {
			(*callback_fn)(true, update_rsock, nullptr, update_rsock->getTrustDomain(), update_rsock->shouldTryTokenRequest(), miscdata);
		}
		return true;
	}
	dprintf( D_FULLDEBUG, 
			 "Couldn't reuse TCP socket to update collector, "
			 "starting new connection\n" );
	delete update_rsock;
	update_rsock = NULL;
	return initiateTCPUpdate( cmd, ad1, ad2, nonblocking, callback_fn, miscdata );
}



bool
DCCollector::initiateTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking, StartCommandCallbackType *callback_fn, void *miscdata )
{
	if( update_rsock ) {
		delete update_rsock;
		update_rsock = NULL;
	}
	if(nonblocking) {
		UpdateData *ud = new UpdateData(cmd, Sock::reli_sock, ad1, ad2, this, callback_fn, miscdata);
			// Note that UpdateData automatically adds itself to the pending_update_list.
		if (this->pending_update_list.size() == 1)
		{
			startCommand_nonblocking(cmd, Sock::reli_sock, 20, NULL, UpdateData::startUpdateCallback, ud );
		}
		return true;
	}

	Sock *sock = startCommand(cmd, Sock::reli_sock, 20);

	if(!sock) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send TCP update command to collector" );
		dprintf(D_ALWAYS,"Failed to send update to %s.\n",idStr());
		if (callback_fn) {
			(*callback_fn)(false, nullptr, nullptr, "", false, miscdata);
		}
		return false;
	}
	update_rsock = (ReliSock *)sock;
	return finishUpdate( this, update_rsock, ad1, ad2, callback_fn, miscdata );
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
	return update_destination;
}


void
DCCollector::initDestinationStrings( void )
{
	if( update_destination ) {
		free(update_destination);
		update_destination = NULL;
	}

	std::string dest;

		// Updates will always be sent to whatever info we've got
		// in the Daemon object.  So, there's nothing hard to do for
		// this... just see what useful info we have and use it. 
	if( ! _full_hostname.empty() ) {
		dest = _full_hostname;
		if (! _addr.empty()) {
			dest += ' ';
			dest += _addr;
		}
	} else {
		dest = _addr;
	}
	update_destination = strdup( dest.c_str() );
}


//
// Ad Sequence Number class methods
//

// Get a sequence number class for this classad, creating it if needed.
DCCollectorAdSeq* DCCollectorAdSequences::getAdSeq(const ClassAd & ad)
{
	std::string name, attr;
	ad.LookupString( ATTR_NAME, name );
	ad.LookupString( ATTR_MY_TYPE, attr );
	name += "\n"; name += attr;
	ad.LookupString( ATTR_MACHINE, attr );
	name += "\n"; name += attr;

	DCCollectorAdSeqMap::iterator it = seqs.find(name);
	if (it != seqs.end()) {
		return &(it->second);
	}
	return &(seqs[name]);
}

DCCollector::~DCCollector( void )
{
	if( update_rsock ) {
		delete( update_rsock );
	}
	if( update_destination ) {
		free(update_destination);
	}

		// In case there are any nonblocking updates in progress,
		// let them know this DCCollector object is going away.
	for (std::deque<UpdateData*>::const_iterator it = pending_update_list.begin();
			it != pending_update_list.end();
			it++) {
		if (*it) {
			(*it)->DCCollectorGoingAway();
		}
	}
}

Timeslice &DCCollector::getBlacklistTimeslice()
{
	std::map< std::string, Timeslice >::iterator itr;
	itr = blacklist.find(addr());
	if( itr == blacklist.end() ) {
		Timeslice ts;
		
			// Blacklist this collector if last failed contact took more
			// than 1% of the time that has passed since that operation
			// started.  (i.e. if contact fails quickly, don't worry, but
			// if it takes a long time to fail, be cautious.
		ts.setTimeslice(0.01);
			// Set an upper bound of one hour for the collector to be blacklisted.
		int avoid_time = param_integer("DEAD_COLLECTOR_MAX_AVOIDANCE_TIME",3600);
		ts.setMaxInterval(avoid_time);
		ts.setInitialInterval(0);

		itr = blacklist.insert( std::map< std::string, Timeslice >::value_type(addr(),ts) ).first;
	}
	return itr->second;
}

bool
DCCollector::isBlacklisted() {
	Timeslice &blacklisted = getBlacklistTimeslice();
	return !blacklisted.isTimeToRun();
}

void
DCCollector::blacklistMonitorQueryStarted() {
	condor_gettimestamp( m_blacklist_monitor_query_started );
}

void
DCCollector::blacklistMonitorQueryFinished( bool success ) {
	Timeslice &blacklisted = getBlacklistTimeslice();
	if( success ) {
		blacklisted.reset();
	}
	else {
		struct timeval finished;
		condor_gettimestamp( finished );
		blacklisted.processEvent(m_blacklist_monitor_query_started,finished);

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
