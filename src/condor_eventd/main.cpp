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
** Condor Event Daemon: Take action for planned events in the Condor
** pool, including scheduled reboots.
*/

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "eventd.h"

DECL_SUBSYSTEM( "EVENTD", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

int
main_init(int, char *[])
{
	EventD->Config();
	return TRUE;
}

int
main_config( bool is_full )
{
	EventD->Config();
	return TRUE;
}

int
main_shutdown_fast()
{
	DC_Exit(0);
	return TRUE;
}

int
main_shutdown_graceful()
{
	DC_Exit(0);
	return TRUE;
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}

void
main_pre_command_sock_init( )
{
}
