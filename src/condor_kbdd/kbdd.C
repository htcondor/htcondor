/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "XInterface.h"

#include "my_hostname.h"
#include "condor_query.h"
#include "daemon.h"

#include <utmp.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <X11/Xlib.h>

char       *mySubSystem = "KBDD";
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

