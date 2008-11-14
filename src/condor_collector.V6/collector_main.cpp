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
#include "view_server.h"
#include "subsystem_info.h"

#if HAVE_DLOPEN
#include "CollectorPlugin.h"
#endif

//-------------------------------------------------------------

// about self
DECL_SUBSYSTEM("COLLECTOR", SUBSYSTEM_TYPE_COLLECTOR );

// the heart of the collector ...
CollectorDaemon* Daemon;

//-------------------------------------------------------------

void usage(char* name)
{
	dprintf(D_ALWAYS,"Usage: %s [-f] [-b] [-t] [-p <port>]\n",name );
	exit( 1 );
}

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	// handle collector-specific command line args
	if(argc > 2)
	{
		usage(argv[0]);
	}
	
	Daemon=new ViewServer();
	Daemon->Init();

#if HAVE_DLOPEN
	CollectorPluginManager::Load();

	CollectorPluginManager::Initialize();
#endif

	return TRUE;
}

//-------------------------------------------------------------

int main_config( bool is_full )
{
	Daemon->Config();
	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	Daemon->Exit();
#if HAVE_DLOPEN
	CollectorPluginManager::Shutdown();
#endif
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	Daemon->Shutdown();
#if HAVE_DLOPEN
	CollectorPluginManager::Shutdown();
#endif
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}

