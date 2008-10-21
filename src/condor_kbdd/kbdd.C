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
#include "XInterface.h"

#include "my_hostname.h"
#include "condor_query.h"
#include "daemon.h"
#include "subsystem_info.h"

#include <utmp.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <X11/Xlib.h>

DECL_SUBSYSTEM( "KBDD" );
XInterface *xinter;


bool
update_startd()
{
    static SafeSock ssock;
	static bool first_time = true;
	static Daemon startd( DT_STARTD );
	if( first_time ) {
		if( ! startd.locate() ) {
			dprintf( D_ALWAYS, "Can't locate startd, aborting (%s)\n",
					 startd.error() );
			return false;
		}
		if( !ssock.connect(startd.addr(), 0) ) {
			dprintf( D_ALWAYS, "Can't connect to startd at: %s, "
					 "aborting\n", startd.addr() );
			return false;
		}
		first_time = false;
	}
	if( !startd.sendCommand(X_EVENT_NOTIFICATION, &ssock, 3) ) {
		dprintf( D_ALWAYS, "Can't send X_EVENT_NOTIFICATION command "
				 "to startd at: %s, aborting\n", startd.addr() );
		return false;
	}
	dprintf( D_FULLDEBUG, "Sent update to startd at: %s\n", startd.addr() );
	return true;		
}


int 
PollActivity()
{
    if(xinter != NULL)
    {
	if(xinter->CheckActivity())
	{
	    update_startd();
	}
    }
    return TRUE;
}

int 
main_shutdown_graceful()
{
    delete xinter;
    DC_Exit(EXIT_SUCCESS);
	return TRUE;
}

int 
main_shutdown_fast()
{
	DC_Exit(EXIT_SUCCESS);
	return TRUE;
}

int 
main_config( bool is_full )
{
    return TRUE;
}

int
main_init(int, char **)
{
    
    int id;
    xinter = NULL;

    //Poll for X activity every second.
    id = daemonCore->Register_Timer(5, 5, (Event)PollActivity, "PollActivity");
    xinter = new XInterface(id);
    return TRUE;
}


void
main_pre_dc_init( int, char** )
{
}


void
main_pre_command_sock_init( )
{
}

