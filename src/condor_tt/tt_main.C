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
#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"

#include "ttmanager.h"

/* Using daemoncore, you get the benefits of a logging system with dprintf
	and you can read config files automatically. To start testing
	your daemon, run it with "-f -t" until you start specifying
	a config file to use(the daemon will automatically look in
	/etc/condor_config, ~condor/condor_config, or the env var
	CONDOR_CONFIG if -t is not specifed).  -f means run in the
	foreground, -t means print the debugging output to the terminal.
*/

//-------------------------------------------------------------

// about self
DECL_SUBSYSTEM( "QUILL", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

// global objects;
TTManager ttManager;



//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	dprintf(D_ALWAYS, "main_init() called\n");

		/*
		  bool init = false;
		  char c;
		  while((c = getopt(argc, argv, "i")) != -1) {
		  switch(c) {
		  case 'i':
		  init = true;
		  break;
		  default:
		  dprintf(D_ALWAYS, "Not Supported Option: [%c]\n", c);
		  break;
		  }
		  }
		*/

	dprintf(D_ALWAYS, "configuring tt options from config file\n");
	ttManager.config();
	ttManager.registerTimers();

	return 0;
}

//-------------------------------------------------------------

int 
main_config( bool is_full )
{
	dprintf(D_ALWAYS, "main_config() called\n");
	ttManager.config(true);
	ttManager.registerTimers();
	return 0;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit(0);
	return 0;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit(0);
	return 0;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

