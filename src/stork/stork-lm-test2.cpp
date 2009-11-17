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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "dc_collector.h"
#include "get_daemon_name.h"
#include "condor_config.h"
#include "subsystem_info.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "conversion.h"
using namespace std;

#include "newclassad_stream.h"

#include "OrderedSet.h"
#include "stork-lm.h"
#include <list>

typedef pair<time_t, const char *> Dest;

class StorkMatchTest: public Service
{
  public:
	// ctor/dtor
	StorkMatchTest( void );
	~StorkMatchTest( void );
	int init( void );
	int config( bool );
	int stop( void );

  private:

	// Timer handlers
	int timerHandler( void );

	StorkLeaseManager		*lm;
	multimap<time_t, const char *, less<time_t> >dests;
	unsigned	target;
	//list<Dest *>			dests;
	char					*my_name;

	int TimerId;
	int Interval;
};

StorkMatchTest::StorkMatchTest( void )
{
}

StorkMatchTest::~StorkMatchTest( void )
{
}

int
StorkMatchTest::init( void )
{
	my_name = NULL;
	target = 200;

	// Read our Configuration parameters
	config( true );

	// Create teh match maker object
	lm = new StorkLeaseManager();
	dprintf( D_FULLDEBUG, "lm size %d @ %p\n", sizeof(*lm), lm );

	TimerId = daemonCore->Register_Timer(
		1, 15, (TimerHandlercpp)&StorkMatchTest::timerHandler,
		"StorkMatchTest::timerHandler()", this );
	if ( TimerId <= 0 ) {
		dprintf( D_FULLDEBUG, "Register_Timer() failed\n" );
		return -1;
	}

	return 0;
}

int
StorkMatchTest::config( bool /*init*/ )
{
	return 0;
}

void
StorkMatchTest::timerHandler ( void )
{
	time_t	now = time( NULL );
	dprintf( D_FULLDEBUG, "Timer handler starting @ %ld\n", now );

	// Release some destinations
	multimap<time_t, const char *>::iterator iter, tmp;
	for( iter = dests.begin(); iter != dests.end(); iter++ ) {
		if ( iter->first > now ) {
			break;
			//continue;
		}
		tmp = iter;
		iter++;
		if ( random() % 20 ) {
			//dprintf( D_FULLDEBUG, "Pre return dump #1\n" );
			//lm->Dump();
			//dprintf( D_FULLDEBUG, "Pre return dump #2\n" );
			//lm->Dump( false );
			dprintf( D_FULLDEBUG, "Returning xfer %s\n", tmp->second );
			if ( !lm->returnTransferDestination( tmp->second ) ) {
				dprintf( D_ALWAYS, "Return of xfer %s failed\n",
						 tmp->second );
			}
			//dprintf( D_FULLDEBUG, "Post return dump\n" );
			//lm->Dump();
		} else {
			dprintf( D_FULLDEBUG, "Failing xfer %s\n", tmp->second );
			if ( !lm->failTransferDestination( tmp->second ) ) {
				dprintf( D_ALWAYS, "Fail of xfer %s failed\n",
						 tmp->second );
			}
		}
		free( const_cast<char *>(tmp->second) );
		dests.erase( tmp );
	}

	// Grab some destinations
	while ( dests.size() < target ) {
		if ( 0 == (random()%15) ) {
			break;
		}
		const char	*d = lm->getTransferDirectory( "gsiftp" );
		if ( !d ) {
			break;
		}
		int		duration = 1 + ( random() % 300 );
		time_t	end = now + duration;
		dests.insert( make_pair( end, d ) );
		dprintf( D_FULLDEBUG, "Added %s duration %d end %ld\n",
				 d, duration, end );
	}

	dprintf( D_FULLDEBUG, "Dump:\n" );
	for( iter = dests.begin(); iter != dests.end(); iter++ ) {
		dprintf( D_FULLDEBUG, "  %s %ld\n", iter->second, iter->first );
	}

	//lm->Dump();

	dprintf( D_FULLDEBUG, "Timer handler done\n" );
	return;
}

int
StorkMatchTest::stop ( void )
{
	dprintf( D_FULLDEBUG, "Shutting down test\n" );

	// Release all destinations
	multimap<time_t, const char *>::iterator iter, tmp;
	for( iter = dests.begin(); iter != dests.end(); iter++ ) {
		tmp = iter;
		iter++;
		dprintf( D_FULLDEBUG, "Returning xfer %s\n", tmp->second );
		if ( !lm->returnTransferDestination( tmp->second ) ) {
			dprintf( D_ALWAYS, "Return of xfer %s failed\n",
					 tmp->second );
		}
		free( const_cast<char *>(tmp->second) );
		dests.erase( tmp );
	}

	dprintf( D_FULLDEBUG, "Stop done\n" );
	return 0;
}


//-------------------------------------------------------------

// about self
DECL_SUBSYSTEM( "StorkMatchTest", SUBSYSTEM_TYPE_TOOL);

StorkMatchTest	smt;

//-------------------------------------------------------------

int main_init(int /*argc*/, char * /*argv*/ [])
{
	DebugFlags = D_FULLDEBUG | D_ALWAYS;
	dprintf(D_ALWAYS, "main_init() called\n");

	smt.init( );


	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool /*is_full*/ )
{
	dprintf(D_ALWAYS, "main_config() called\n");
	smt.config( true );
	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast(void)
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	smt.stop( );
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful(void)
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	smt.stop( );
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int /*argc*/, char* /*argv*/ [] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init(void )
{
}
