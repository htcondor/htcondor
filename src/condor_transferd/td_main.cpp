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
#include "condor_td.h"
#include "subsystem_info.h"

// The transferd object which does the work requested by the parent
TransferD g_td;

void main_init(int argc, char *argv[])
{
	// parse the command line attributes
	g_td.init(argc, argv);

	dprintf(D_ALWAYS, "Registering timers...\n");
	g_td.register_timers();

	dprintf(D_ALWAYS, "Registering handlers...\n");
	g_td.register_handlers();
}

//-------------------------------------------------------------

void 
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");

	g_td.reconfig();
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");

	g_td.shutdown_fast();

	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");

	g_td.shutdown_graceful();

	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem( "TRANSFERD", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
