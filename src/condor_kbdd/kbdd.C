/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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

