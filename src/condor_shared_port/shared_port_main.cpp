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
#include "shared_port_server.h"

SharedPortServer *shared_port_server = NULL;

//-------------------------------------------------------------

// about self
DECL_SUBSYSTEM("SHARED_PORT", SUBSYSTEM_TYPE_SHARED_PORT );	// used by Daemon Core

//-------------------------------------------------------------

static void CleanUp()
{
	delete shared_port_server;
	shared_port_server = NULL;
}

//-------------------------------------------------------------

int main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_ALWAYS, "main_init() called\n");
	shared_port_server = new SharedPortServer();
	shared_port_server->InitAndReconfig();
	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool /* is_full */ )
{
	dprintf(D_ALWAYS, "main_config() called\n");
	shared_port_server->InitAndReconfig();
	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	CleanUp();
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	CleanUp();
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int /* argc */, char* /* argv */ [] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

