/*  
	This file implements the classes defined in Match.h:

	Match, Capability, and Client

	A Match object contains all of the information a startd needs
    about a given match, such as the capability, the client of this
    match, etc.  The capability in the match is just a pointer to a
    Capability object.  The client is also just a pointer to a Client
    object.  The startd maintains two Match objects in the "rip", the
    per-resource information structure.  One for the current match,
    and one for the possibly preempting match that is pending.
 
	A Capability object contains the capability string, and some
	functions to manipulate and compare against this string.  The
	constructor generates a new capability with the following form:
	<ip:port>#random_integer

	A Client object contains all the info about a given client of a
	startd.  In particular, the client name (a.k.a. "user"), the
	address ("<www.xxx.yyy.zzz:pppp>" formed ip/port), and the
	hostname.

	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>	
*/

#include "startd.h"
static char *_FileName_ = __FILE__;

///////////////////////////////////////////////////////////////////////////
// Match
///////////////////////////////////////////////////////////////////////////

Match::Match()
{
	m_client = new Client;
	m_cap = new Capability;
	m_ad = NULL;
	m_rank = -1;
	m_oldrank = -1;
	m_universe = -1;
	m_agentstream = NULL;
	m_match_tid = -1;
	m_claim_tid = -1;
	m_aliveint = -1;
}


Match::~Match()
{	
		// Cancel any timers associated with this match
	this->cancel_match_timer();
	this->cancel_claim_timer();

		// Free up memory that's been allocated
	if( m_ad ) {
		delete( m_ad );
	}
	delete( m_cap );
	if( m_client ) {
		delete( m_client );
	}
		// We don't want to delete m_agentstream, since DaemonCore
		// will delete it for us.  
//	delete( m_agentstream );
}	
	

// Vacate the current match
void
Match::vacate() 
{
	assert( m_cap );

		// warn the client of this match that it's being vacated
	if( m_client && m_client->addr() ) {
		m_client->vacate( m_cap->capab() );
	}

		// send RELEASE_CLAIM and capability to accountant 
	if( accountant_host ) {
		ReliSock sock;
		sock.timeout( 30 );
		if( sock.connect( accountant_host, ACCOUNTANT_PORT ) < 0 ) {
			dprintf(D_ALWAYS, "Couldn't connect to accountant\n");
		} else {
			if( !sock.put( RELEASE_CLAIM ) ) {
				dprintf(D_ALWAYS, "Can't send RELEASE_CLAIM command\n");
			} else if( !sock.put( m_cap->capab() ) ) {
				dprintf(D_ALWAYS, "Can't send capability to accountant\n");
			} else if( !sock.eom() ) {
				dprintf(D_ALWAYS, "Can't send EOM to accountant\n");
			}
			sock.close();
		}
	}
}


void
Match::start_match_timer()
{
	m_match_tid = 
		daemonCore->Register_Timer( match_timeout, 0, 
								   (TimerHandlercpp)match_timed_out,
								   "match_timed_out", this );
	if( m_match_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started match timer for %d seconds.\n", 
			 match_timeout );
}


void
Match::cancel_match_timer()
{
	if( m_match_tid != -1 ) {
		daemonCore->Cancel_Timer( m_match_tid );
		m_match_tid = -1;
		dprintf( D_FULLDEBUG, "Canceled match timer.\n" );
	}
}


int
Match::match_timed_out()
{
	Resource* rip = resmgr->get_by_any_cap( capab() );
	if( !rip ) {
		EXCEPT( "Can't find resource of expired match." );
	}

	if( rip->r_cur->cap()->matches( capab() ) ) {
		if( rip->state() != matched_state ) {
			EXCEPT( "Match timed out but not in matched state." );
		}
		delete rip->r_cur;
		rip->r_cur = new Match;
		rip->change_state( owner_state );
	} else {
			// The match that timed out was the preempting match.
		assert( rip->r_pre->cap()->matches( capab() ) );
			// We need to generate a new preempting match object,
			// restore our reqexp, and update the CM. 
		delete rip->r_pre;
		rip->r_pre = new Match;
		rip->r_reqexp->pub();
		rip->update();
	}		
	return TRUE;
}


void
Match::start_claim_timer()
{
	if( m_aliveint < 0 ) {
		dprintf( D_ALWAYS, 
				 "Warning: starting claim timer before alive interval set.\n" );
		m_aliveint = 300;
	}
	m_claim_tid =
		daemonCore->Register_Timer( (3 * m_aliveint), 0,
									(TimerHandlercpp)claim_timed_out,
									"claim_timed_out", this );
	if( m_claim_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started claim timer w/ %d second alive interval.\n", 
			 m_aliveint );
}


void
Match::cancel_claim_timer()
{
	if( m_claim_tid != -1 ) {
		daemonCore->Cancel_Timer( m_claim_tid );
		m_claim_tid = -1;
		dprintf( D_FULLDEBUG, "Canceled claim timer.\n" );
	}
}


int
Match::claim_timed_out()
{
	Resource* rip = resmgr->get_by_cur_cap( capab() );
	if( !rip ) {
		EXCEPT( "Can't find resource of expired claim." );
	}
		// Note that this claim timed out so we don't try to send a 
		// command to our client.
	if( m_client ) {
		delete m_client;
		m_client = NULL;
	}
		// Release the claim.
	rip->release_claim();
	return TRUE;
}


void
Match::alive()
{
		// Process a keep alive command
	daemonCore->Reset_Timer( m_claim_tid, (3 * m_aliveint), 0 );
}


// Set our ad to the given pointer
void
Match::setad(ClassAd *ad) 
{
	if( m_ad ) {
		delete( m_ad );
	}
	m_ad = ad;
}


void
Match::deletead(void)
{
	if( m_ad ) {
		delete( m_ad );
		m_ad = NULL;
	}
}


void
Match::setagentstream(Stream* stream)
{
	if( m_agentstream ) {
		delete( m_agentstream );
	}
	m_agentstream = stream;
}


///////////////////////////////////////////////////////////////////////////
// Client
///////////////////////////////////////////////////////////////////////////

Client::Client()
{
	c_name = NULL;
	c_addr = NULL;
	c_host = NULL;
}


Client::~Client() 
{
	free( c_name );
	free( c_addr );
	free( c_host );
}


void
Client::setname(char* name)
{
	if( c_name ) {
		free( c_name);
	}
	if( name ) {
		c_name = strdup( name );
	} else {
		c_name = NULL;
	}
}


void
Client::setaddr(char* addr)
{
	if( c_addr ) {
		free( c_addr);
	}
	if( addr ) {
		c_addr = strdup( addr );
	} else {
		c_addr = NULL;
	}
}


void
Client::sethost(char* host)
{
	if( c_host ) {
		free( c_host);
	}
	if( host ) {
		c_host = strdup( host );
	} else {
		c_host = NULL;
	}
}


void
Client::vacate(char* cap)
{
	ReliSock sock;
	sock.timeout( 30 );

	if( ! (c_addr || c_host || c_name ) ) {
			// Client not really set, nothing to do.
		return;
	}

	dprintf(D_FULLDEBUG, "Entered vacate_client %s %s...\n", c_addr, c_host);
	
	if(	sock.connect( c_addr, 0 ) < 0 ) {
		dprintf(D_ALWAYS, "Can't connect to schedd (%s)\n", c_addr);
	} else {
		if( !sock.put( RELEASE_CLAIM ) ) {
			dprintf(D_ALWAYS, "Can't send RELEASE_CLAIM command to client\n");
		} else if( !sock.put( cap ) ) {
			dprintf(D_ALWAYS, "Can't send capability to client\n");
		} else if( !sock.eom() ) {
			dprintf(D_ALWAYS, "Can't send EOM to client\n");
		}
		sock.close();
	}
}


///////////////////////////////////////////////////////////////////////////
// Capability
///////////////////////////////////////////////////////////////////////////

char*
new_capability_string()
{
	char cap[256];
	char tmp[128];
	sprintf( tmp, "%d", get_random_int() );
	strcpy( cap, daemonCore->InfoCommandSinfulString() );
	strcat( cap, "#" );
	strcat( cap, tmp );
	return strdup( cap );
}


Capability::Capability()
{
	c_capab = new_capability_string();
}


Capability::~Capability()
{
	free( c_capab );
}


int
Capability::matches(char* capab)
{
	return( strcmp(capab, c_capab) == 0 );
}

