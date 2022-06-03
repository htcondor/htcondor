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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "defrag.h"

//-------------------------------------------------------------

Defrag *defrag = NULL;

//-------------------------------------------------------------

void main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_FULLDEBUG, "main_init() called\n");

	defrag = new Defrag();
	defrag->init();
}

//-------------------------------------------------------------

void 
main_config()
{
	dprintf(D_FULLDEBUG, "main_config() called\n");

	defrag->config();
}

static void Stop()
{
	defrag->stop();
	delete defrag;
	defrag = NULL;
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_FULLDEBUG, "main_shutdown_fast() called\n");
	Stop();
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_FULLDEBUG, "main_shutdown_graceful() called\n");
	Stop();
	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem("DEFRAG", true, SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}

