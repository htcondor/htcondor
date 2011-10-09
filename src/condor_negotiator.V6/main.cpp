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
#if defined(HAVE_DLOPEN) || defined(WIN32)
#include "NegotiatorPlugin.h"
#endif
#endif

// the matchmaker object
Matchmaker matchMaker;

void main_init (int, char *[])
{
	// read in params
	matchMaker.initialize ();
}

void main_shutdown_graceful()
{
	matchMaker.invalidateNegotiatorAd();
#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
	NegotiatorPluginManager::Shutdown();
#endif
#endif
	DC_Exit(0);
}


void main_shutdown_fast()
{
	matchMaker.invalidateNegotiatorAd();
#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
	NegotiatorPluginManager::Shutdown();
#endif
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
	set_mySubSystem( "NEGOTIATOR", SUBSYSTEM_TYPE_NEGOTIATOR );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
