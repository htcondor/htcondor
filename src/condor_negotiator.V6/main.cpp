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
#include "subsystem_info.h"
#include "matchmaker.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#include "NegotiatorPlugin.h"
#endif

// the matchmaker object
Matchmaker matchMaker;

void usage(char* name)
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n negotiator_name]", name);
	exit( 1 );
}

void main_init (int argc, char *argv[])
{
	const char *neg_name = NULL;

	bool dryrun = false;

	for ( int i = 1; i < argc; i++ ) {
		if ( argv[i][0] == '-' && argv[i][1] == 'n' && (i + 1) < argc ) {
			neg_name = argv[i + 1];
			i++;
		} else if (strcmp(argv[i], "-z") == 0)  {
			dryrun = true;
		} else {
			usage(argv[0]);
		}
	}

	// read in params
	matchMaker.initialize (neg_name);

	if (dryrun) {
		matchMaker.setDryRun(dryrun);
		dprintf(D_ALWAYS, " ----------------------------  BEGIN DRYRUN NEGOTIATION TEST --------------------\n");
		matchMaker.negotiationTime();
		dprintf(D_ALWAYS, " ----------------------------  END DRYRUN NEGOTIATION TEST --------------------\n");
		exit(1);
	}
}

void main_shutdown_graceful()
{
	matchMaker.invalidateNegotiatorAd();
#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	NegotiatorPluginManager::Shutdown();
#endif
	DC_Exit(0);
}


void main_shutdown_fast()
{
	matchMaker.invalidateNegotiatorAd();
#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	NegotiatorPluginManager::Shutdown();
#endif
	DC_Exit(0);
}

void
main_config()
{
	matchMaker.reinitialize ();
}


int
main( int argc, char **argv )
{
	set_mySubSystem( "NEGOTIATOR", true, SUBSYSTEM_TYPE_NEGOTIATOR );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
