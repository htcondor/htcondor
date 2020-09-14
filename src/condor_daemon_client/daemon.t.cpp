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


/* 
   This test program tries to exhaustively test the Daemon object.
   The test program itself is fairly ugly.  It basically tries to
   instantiate and manipulate a bunch of different Daemon objects.  To
   truly test everything, you'll need to run it twice: once with a
   reasonable config file, and once with NEGOTIATOR_HOST commented
   out, and a SCHEDD_NAME defined.  In addition, you need to give an
   argument (anything will do, we just look at the number of args, not
   what they are) to test all the socket/command stuff.

   Written on 5/12/99 by Derek Wright <wright@cs.wisc.edu>
*/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "daemon.h"
#include "internet.h"
#include "my_hostname.h"

const int dflag = D_ALWAYS | D_NOHEADER;

void
makeAndDisplayCM( char* name, char* pool )
{
	Daemon collector( DT_COLLECTOR, name, pool );
	Daemon negotiator( DT_NEGOTIATOR, name, pool );
	Daemon view_collector( DT_VIEW_COLLECTOR, name, pool );

	if( ! collector.locate() ) {
		dprintf( dflag, "%s\n", collector.error() );
	} else { 
		collector.display( dflag );
	} 
	dprintf( dflag, "\n" );
	if( ! negotiator.locate() ) {
		dprintf( dflag, "%s\n", negotiator.error() );
	} else { 
		negotiator.display( dflag );
	} 
	dprintf( dflag, "\n" );
	if( ! view_collector.locate() ) {
		dprintf( dflag, "%s\n", view_collector.error() );
	} else { 
		view_collector.display( dflag );
	} 
	dprintf( dflag, "\n" );
}

void
makeAndDisplayRegular( char* name, char* pool )
{
	Daemon startd( DT_STARTD, name, pool );
	Daemon schedd( DT_SCHEDD, name, pool );
	Daemon master( DT_MASTER, name, pool );

	if( ! startd.locate() ) {
		dprintf( dflag, "%s\n", startd.error() );
	} else { 
		startd.display( dflag );
	} 
	dprintf( dflag, "\n" );
	if( ! schedd.locate() ) {
		dprintf( dflag, "%s\n", schedd.error() );
	} else { 
		schedd.display( dflag );
	} 
	dprintf( dflag, "\n" );
	if( ! master.locate() ) {
		dprintf( dflag, "%s\n", master.error() );
	} else { 
		master.display( dflag );
	} 
	dprintf( dflag, "\n" );
}


void
testSocks( Daemon* d )
{
	ReliSock reli;
	ReliSock* reli_p;
	SafeSock safe;
	SafeSock* safe_p;
	char* state = NULL;

	dprintf( dflag, "Using reliSock() method...\n" );
	reli_p = d->reliSock();
	if( reli_p ) {
		reli_p->encode();
		if( ! reli_p->put( GIVE_STATE ) ) {
			dprintf( dflag, "Can't encode GIVE_STATE\n" );
		}
		if( ! reli_p->end_of_message() ) {
			dprintf( dflag, "Can't send eom\n" );
		}
		reli_p->decode();
		if( ! reli_p->code( state ) ) {
			dprintf( dflag, "Can't decode state\n" );
		} else {
			dprintf( dflag, "State of %s is %s\n", d->idStr(), state );
		}
		if( state ) {
			free( state );
			state = NULL;
		}
		delete reli_p;
	}

	dprintf( dflag, "Using ReliSock startCommand() method #1...\n" );
	reli_p = (ReliSock*)d->startCommand( GIVE_STATE );
	if( reli_p ) {
		if( ! reli_p->end_of_message() ) {
			dprintf( dflag, "Can't send eom\n" );
		}
		reli_p->decode();
		if( ! reli_p->code( state ) ) {
			dprintf( dflag, "Can't decode state\n" );
		} else {
			dprintf( dflag, "State of %s is %s\n", d->idStr(), state );
		}
		if( state ) {
			free( state );
			state = NULL;
		}
		delete reli_p;
	}

	dprintf( dflag, "Using ReliSock startCommand() method #2...\n" );
	if( ! d->addr() || ! reli.connect(d->addr()) ) {
		dprintf( dflag, "Can't connect to %s\n", d->idStr() );
	} else {
		reli_p = (ReliSock*)d->startCommand( GIVE_STATE, &reli );
		if( reli_p ) {
			if( ! reli_p->end_of_message() ) {
				dprintf( dflag, "Can't send eom\n" );
			}
			reli_p->decode();
			if( ! reli_p->code( state ) ) {
				dprintf( dflag, "Can't decode state\n" );
			} else {
				dprintf( dflag, "State of %s is %s\n", d->idStr(), state );
			}
			if( state ) {
				free( state );
				state = NULL;
			}
			delete reli_p;
		}
	}

	dprintf( dflag, "Using sendCommand() method #1...\n" );
	if( ! d->sendCommand(X_EVENT_NOTIFICATION) ) {
		dprintf( dflag, "Couldn't send X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	} else {
		dprintf( dflag, "Sent X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	}

	reli_p = new ReliSock();
	dprintf( dflag, "Using sendCommand() method #2 (should fail)...\n" );
	if( ! d->sendCommand(X_EVENT_NOTIFICATION, reli_p) ) {
		dprintf( dflag, "Couldn't send X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	} else {
		dprintf( dflag, "Sent X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	}

	if( d->addr() ) {
		reli_p->connect( d->addr() );
		dprintf( dflag, "Using sendCommand() method #2 (should work)...\n" );
		if( ! d->sendCommand(X_EVENT_NOTIFICATION) ) {
			dprintf( dflag, "Couldn't send X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
		} else {
			dprintf( dflag, "Sent X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
		}
	}

	delete reli_p;

	dprintf( dflag, "Using safeSock() method...\n" );
	safe_p = d->safeSock( 3 );
	if( safe_p ) {
		safe_p->encode();
		if( ! safe_p->put( GIVE_STATE ) ) {
			dprintf( dflag, "Can't encode GIVE_STATE\n" );
		}
		if( ! safe_p->end_of_message() ) {
			dprintf( dflag, "Can't send eom\n" );
		}
		safe_p->decode();
		if( ! safe_p->code( state ) ) {
			dprintf( dflag, "Can't decode state\n" );
		} else {
			dprintf( dflag, "State of %s is %s\n", d->idStr(), state );
		}
		if( state ) {
			free( state );
			state = NULL;
		}
		delete safe_p;
	}

	dprintf( dflag, "Using SafeSock startCommand() method #1...\n" );
	safe_p = (SafeSock*)d->startCommand( GIVE_STATE, Stream::safe_sock, 3 );
	if( safe_p ) {
		if( ! safe_p->end_of_message() ) {
			dprintf( dflag, "Can't send eom\n" );
		}
		safe_p->decode();
		if( ! safe_p->code( state ) ) {
			dprintf( dflag, "Can't decode state\n" );
		} else {
			dprintf( dflag, "State of %s is %s\n", d->idStr(), state );
		}
		if( state ) {
			free( state );
			state = NULL;
		}
		delete safe_p;
	}

	dprintf( dflag, "Using SafeSock startCommand() method #2...\n" );
	if( ! d->addr() || ! safe.connect(d->addr(), 3) ) {
		dprintf( dflag, "Can't connect to %s\n", d->idStr() );
	} else {
		safe_p = (SafeSock*)d->startCommand( GIVE_STATE, &safe );
		if( safe_p ) {
			if( ! safe_p->end_of_message() ) {
				dprintf( dflag, "Can't send eom\n" );
			}
			safe_p->decode();
			if( ! safe_p->code( state ) ) {
				dprintf( dflag, "Can't decode state\n" );
			} else {
				dprintf( dflag, "State of %s is %s\n", d->idStr(), state );
			}
			if( state ) {
				free( state );
				state = NULL;
			}
			delete safe_p;
		}
	}

	dprintf( dflag, "Using SafeSock sendCommand() method #1...\n" );
	if( ! d->sendCommand(X_EVENT_NOTIFICATION, Stream::safe_sock, 3) ) {
		dprintf( dflag, "Couldn't send X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	} else {
		dprintf( dflag, "Sent X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	}

	safe_p = new SafeSock();
	dprintf( dflag, "Using SafeSock sendCommand() method #2 (should fail)...\n" );
	if( ! d->sendCommand(X_EVENT_NOTIFICATION, safe_p) ) {
		dprintf( dflag, "Couldn't send X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	} else {
		dprintf( dflag, "Sent X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
	}

	if( d->addr() ) {
		safe_p->connect( d->addr() );
		dprintf( dflag, "Using SafeSock sendCommand() method #2 (should work)...\n" );
		if( ! d->sendCommand(X_EVENT_NOTIFICATION) ) {
			dprintf( dflag, "Couldn't send X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
		} else {
			dprintf( dflag, "Sent X_EVENT_NOTIFICATION to %s\n", d->idStr() ); 
		}	
	}
	delete safe_p;

	d->startCommand( GIVE_STATE, NULL, 0 );
	d->sendCommand( X_EVENT_NOTIFICATION, NULL, 0 );
}

void
testAPI( char* my_name, bool do_socks )
{
	char *name, *addr, *fullhost, *host, *pool, *error, *id;

	Daemon startd( DT_STARTD, my_name );
	if( ! startd.locate() ) {
		dprintf( dflag, "%s\n", startd.error() );
	}
	name = startd.name();
	addr = startd.addr();
	fullhost = startd.fullHostname();
	host = startd.hostname();
	pool = startd.pool();
	error = (char*)startd.error();
	id = startd.idStr();

	dprintf( dflag, "Type: %d (%s), Name: %s, Addr: %s\n", 
			 (int)startd.type(), daemonString(startd.type()), 
			 name ? name : "(null)", 
			 addr ? addr : "(null)" );
	dprintf( dflag, "FullHost: %s, Host: %s, Pool: %s, Port: %d\n", 
			 fullhost ? fullhost : "(null)",
			 host ? host : "(null)", 
			 pool ? pool : "(null)", startd.port() );
	dprintf( dflag, "IsLocal: %s, IsFound: %s, IdStr: %s, Error: %s\n", 
			 startd.isLocal() ? "Y" : "N",
			 startd.isFound ? "Y" : "N",
			 id ? id : "(null)",
			 error ? error : "(null)" );
	if( do_socks ) {
		testSocks( &startd );
	}
}


main(int argc, char* argv) 
{
	config();
	bool do_socks = false;

	if( argc > 1 ) {
		do_socks = true;
	}

	dprintf( dflag, "\nExternal interface (local daemon)\n\n" );
	testAPI( NULL, do_socks );

	dprintf( dflag, "\nExternal interface (bad hostname)\n\n" );
	testAPI( "bazzle", do_socks );

	dprintf( dflag, "\nLocal daemons\n\n" );
	makeAndDisplayCM( NULL, NULL );
	makeAndDisplayRegular( NULL, NULL );

	dprintf( dflag, "\nRemote daemons we should find\n\n" );
	makeAndDisplayCM( "condor", "condor" );
	makeAndDisplayRegular( "puck.cs.wisc.edu", "condor" );

	dprintf( dflag, "\nWe should find the startd, but not the others\n\n" );
	makeAndDisplayRegular( "slot1@raven.cs.wisc.edu", NULL );

	dprintf( dflag, "\nUsing a bogus sinful string\n\n" );
	Daemon d( DT_NEGOTIATOR, "<128.105.232.240:23232>" );
	if( d.locate() ) {
		dprintf( dflag, "Found %s\n", d.idStr() );
		d.display( dflag );
	} else {
		dprintf( dflag, "%s\n", d.error() );
	}

	dprintf( dflag, "\nMake a local STARTD w/ explicit name\n\n" );
	Daemon d2( DT_STARTD, get_local_fqdn().Value() );
	if( d2.locate() ) {
		d2.display( dflag );
	} else {
		dprintf( dflag, "%s\n", d2.error() );
	}

	dprintf( dflag, "\nMake a local COLLECTOR w/ explicit name\n\n" );
	Daemon d3( DT_COLLECTOR, "turkey.cs.wisc.edu" );
	if( d3.locate() ) {
		d3.display( dflag );
	} else {
		dprintf( dflag, "%s\n", d3.error() );
	}

	dprintf( dflag, "\nUse idStr for a remote STARTD\n\n" );
	Daemon d4( DT_STARTD, "puck.cs.wisc.edu" );
	if( d4.locate() ) {
		dprintf( dflag, "Found %s\n", d4.idStr() );
	} else {
		dprintf( dflag, "%s\n", d4.error() );
	}

	dprintf( dflag, "\nTest socks on a valid Daemon that isn't up\n\n" );
	Daemon d5( DT_SCHEDD, "cabernet.cs.wisc.edu" );
	testSocks( &d5 );

	dprintf( dflag, "\nCM where you specify pool and not name\n\n" );	
	Daemon d6( DT_COLLECTOR, NULL, "condor" );
	if( d6.locate() ) {
		d6.display( dflag );
	} else {
		dprintf( dflag, "%s\n", d6.error() );
	}
	
	dprintf( dflag, "\nUsing sinful string for the collector\n\n" );
	Daemon d7( DT_COLLECTOR, "<128.105.143.16:9618>" );
	if( d7.locate() ) {
		dprintf( dflag, "Found %s\n", d7.idStr() );
		d7.display( dflag );
	} else {
		dprintf( dflag, "%s\n", d7.error() );
	}

	dprintf( dflag, "\nRemote daemons we should NOT find\n\n" );
	makeAndDisplayRegular( "bazzle.cs.wisc.edu", "condor" );
	makeAndDisplayCM( "bazzle", "bazzle" );
}
