/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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

#include "condor_common.h"
#include "startd.h"

///////////////////////////////////////////////////////////////////////////
// Match
///////////////////////////////////////////////////////////////////////////

Match::Match( Resource* rip )
{
	m_client = new Client;
	m_cap = new Capability;
	m_ad = NULL;
	m_rank = 0;
	m_oldrank = 0;
	m_universe = -1;
	m_agentstream = NULL;
	m_match_tid = -1;
	m_claim_tid = -1;
	m_aliveint = -1;
	m_cluster = -1;
	m_proc = -1;
	m_job_start = -1;
	m_last_pckpt = -1;
	m_rip = rip;
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
	if( m_agentstream ) {
		delete( m_agentstream );
	}

}	
	

void
Match::vacate() 
{
	assert( m_cap );
		// warn the client of this match that it's being vacated
	if( m_client && m_client->addr() ) {
		m_client->vacate( m_cap->capab() );
	}
}


void
Match::publish( ClassAd* ad, amask_t how_much )
{
	char line[256];
	char* tmp;

	if( IS_PRIVATE(how_much) ) {
		return;
	}

	sprintf( line, "%s = %f", ATTR_CURRENT_RANK, m_rank );
	ad->Insert( line );

	if( m_client ) {
		tmp = m_client->user();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_REMOTE_USER, tmp );
			ad->Insert( line );
		}
		tmp = m_client->owner();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_REMOTE_OWNER, tmp );
			ad->Insert( line );
		}
		tmp = m_client->host();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_CLIENT_MACHINE, tmp );
			ad->Insert( line );
		}
	}

	if( (m_cluster > 0) && (m_proc >= 0) ) {
		sprintf( line, "%s=\"%d.%d\"", ATTR_JOB_ID, m_cluster, m_proc );
		ad->Insert( line );
	}

	if( m_job_start > 0 ) {
		sprintf(line, "%s=%d", ATTR_JOB_START, m_job_start );
		ad->Insert( line );
	}

	if( m_last_pckpt > 0 ) {
		sprintf(line, "%s=%d", ATTR_LAST_PERIODIC_CHECKPOINT, m_last_pckpt );
		ad->Insert( line );
	}
}	


void
Match::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	m_rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


void
Match::refuse_agent()
{
	if( !m_agentstream ) return;
	dprintf( D_ALWAYS, "Refusing request from schedd agent.\n" );
	m_agentstream->encode();
	m_agentstream->put(NOT_OK);
	m_agentstream->end_of_message();
}


void
Match::start_match_timer()
{
	if( m_match_tid != -1 ) {
			/*
			  We got matched twice for the same capability.  This
			  must be because we got matched, we sent an update that
			  said we're unavailable, but the collector dropped that
			  update, and we got matched again.  This shouldn't be a
			  fatal error, b/c UDP gets dropped all the time.  We just
			  need to cancel the old timer, print a warning, and then
			  continue. 
			*/
		
	   dprintf( D_FAILURE|D_ALWAYS, "Warning: got matched twice for same capability."
				" Canceling old match timer (%d)\n", m_match_tid );
	   if( daemonCore->Cancel_Timer(m_match_tid) < 0 ) {
		   dprintf( D_ALWAYS, "Failed to cancel old match timer (%d): "
					"daemonCore error\n", m_match_tid );
	   } else {
		   dprintf( D_FULLDEBUG, "Cancelled old match timer (%d)\n", 
					m_match_tid );
	   }
	   m_match_tid = -1;
	}

	m_match_tid = 
		daemonCore->Register_Timer( match_timeout, 0, 
								   (TimerHandlercpp)
								   &Match::match_timed_out,
								   "match_timed_out", this );
	if( m_match_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started match timer (%d) for %d seconds.\n", 
			 m_match_tid, match_timeout );
}


void
Match::cancel_match_timer()
{
	int rval;
	if( m_match_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( m_match_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel match timer (%d): "
					 "daemonCore error\n", m_match_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled match timer (%d)\n", 
					 m_match_tid );
		}
		m_match_tid = -1;
	}
}


int
Match::match_timed_out()
{
	char* my_cap = capab();
	if( !my_cap ) {
			// We're all confused.
			// Don't use our dprintf(), use the "real" version, since
			// if we're this confused, our rip pointer might be messed
			// up, too, and we don't want to seg fault.
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Match timed out but there's no capability\n" );
		return FALSE;
	}
		
	Resource* rip = resmgr->get_by_any_cap( my_cap );
	if( !rip ) {
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Can't find resource of expired match\n" );
		return FALSE;
	}

	if( rip->r_cur->cap()->matches( capab() ) ) {
		if( rip->state() != matched_state ) {
				/* 
				   This used to be an EXCEPT(), since it really
				   shouldn't ever happen.  However, it kept happening,
				   and we couldn't figure out why.  For now, just log
				   it and silently ignore it, since there's no real
				   harm done, anyway.  We use D_FULLDEBUG, since we
				   don't want people to worry about it if they see it
				   in D_ALWAYS in the 6.2.X stable series.  However,
				   in the 6.3 series, we should probably try to figure 
				   out what's going on with this, for example, by
				   sending email at this point with the last 300 lines
				   of the log file or something.  -Derek 10/9/00
				*/
			dprintf( D_FAILURE|D_FULLDEBUG, 
					 "WARNING: Current match timed out but in %s state.",
					 state_to_string(rip->state()) );
			return FALSE;
		}
		delete rip->r_cur;
		rip->r_cur = new Match( rip );
		dprintf( D_FAILURE|D_ALWAYS, "State change: match timed out\n" );
		rip->change_state( owner_state );
	} else {
			// The match that timed out was the preempting match.
		assert( rip->r_pre->cap()->matches( capab() ) );
			// We need to generate a new preempting match object,
			// restore our reqexp, and update the CM. 
		delete rip->r_pre;
		rip->r_pre = new Match( rip );
		rip->r_reqexp->restore();
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
	if( m_claim_tid != -1 ) {
	   EXCEPT( "Match::start_claim_timer() called w/ m_claim_tid = %d", 
			   m_claim_tid );
	}
	m_claim_tid =
		daemonCore->Register_Timer( (3 * m_aliveint), 0,
				(TimerHandlercpp)&Match::claim_timed_out,
				"claim_timed_out", this );
	if( m_claim_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started claim timer (%d) w/ %d second "
			 "alive interval.\n", m_claim_tid, m_aliveint );
}


void
Match::cancel_claim_timer()
{
	int rval;
	if( m_claim_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( m_claim_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel claim timer (%d): "
					 "daemonCore error\n", m_claim_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled claim timer (%d)\n",
					 m_claim_tid );
		}
		m_claim_tid = -1;
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

	dprintf( D_FAILURE|D_ALWAYS, "State change: claim timed out (condor_schedd gone?)\n" );

		// Kill the claim.
	rip->kill_claim();
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


char*
Match::capab( void )
{
	if( m_cap ) {
		return m_cap->capab();
	} else {
		return NULL;
	}
}


///////////////////////////////////////////////////////////////////////////
// Client
///////////////////////////////////////////////////////////////////////////

Client::Client()
{
	c_user = NULL;
	c_owner = NULL;
	c_addr = NULL;
	c_host = NULL;
}


Client::~Client() 
{
	if( c_user) free( c_user );
	if( c_owner) free( c_owner );
	if( c_addr) free( c_addr );
	if( c_host) free( c_host );
}


void
Client::setuser( char* user )
{
	if( c_user ) {
		free( c_user);
	}
	if( user ) {
		c_user = strdup( user );
	} else {
		c_user = NULL;
	}
}


void
Client::setowner( char* owner )
{
	if( c_owner ) {
		free( c_owner);
	}
	if( owner ) {
		c_owner = strdup( owner );
	} else {
		c_owner = NULL;
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
	ReliSock* sock;


	if( ! (c_addr || c_host || c_owner ) ) {
			// Client not really set, nothing to do.
		return;
	}

	dprintf(D_FULLDEBUG, "Entered vacate_client %s %s...\n", c_addr, c_host);

	Daemon my_schedd (DT_SCHEDD, NULL, NULL);
	if (!(sock = (ReliSock*)my_schedd.startCommand( RELEASE_CLAIM, Stream::reli_sock, 20 ))) {
		dprintf(D_FAILURE|D_ALWAYS, "Can't connect to schedd (%s)\n", c_addr);
		return;
	}

	if( !sock->put( cap ) ) {
		dprintf(D_ALWAYS, "Can't send capability to client\n");
	} else if( !sock->eom() ) {
		dprintf(D_ALWAYS, "Can't send EOM to client\n");
	}

	sock->close();
	delete sock;
}


///////////////////////////////////////////////////////////////////////////
// Capability
///////////////////////////////////////////////////////////////////////////

char*
new_capability_string()
{
	char cap[128];
	char tmp[128];
	char randbuf[12];
	randbuf[0] = '\0';
	int i, len;

		// Create a really mangled 10 digit random number: The first 6
		// digits are generated as follows: for the ith digit, pull
		// the ith digit off a new random int.  So our 1st slot comes
		// from the 1st digit of 1 random int, the 2nd from the 2nd
		// digit of a 2nd random it, etc...  If we're trying to get a
		// digit from a number that's too small to have that many, we
		// just use the last digit.  The last 4 digits of our number
		// come from the first 4 digits of the current time multiplied
		// by a final random int.  That should keep 'em guessing. :)
		// -Derek Wright 1/8/98
	for( i=0; i<6; i++ ) {
		sprintf( tmp, "%d", get_random_int() );
		len = strlen(tmp);
		if( i < len ) {
			tmp[i+1] = '\0';
			strcat( randbuf, tmp+i );
		} else {
			strcat( randbuf, tmp+(len-1) );
		}
	}
	sprintf( tmp, "%f", (double)((float)time(NULL) * (float)get_random_int()) );
	tmp[4]='\0';
	strcat( randbuf, tmp );

		// Capability string is "<ip:port>#random_number"
	strcpy( cap, daemonCore->InfoCommandSinfulString() );
	strcat( cap, "#" );
	strcat( cap, randbuf );
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

