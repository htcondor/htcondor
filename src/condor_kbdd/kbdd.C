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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "my_hostname.h"
#include "condor_query.h"

#include <utmp.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <X11/Xlib.h>


extern "C" char *get_startd_addr(const char *);

char       *mySubSystem = "KBDD";
XInterface *xinter;

int
update_startd()
{
    static int cmd_num = X_EVENT_NOTIFICATION;
    static char *my_addr = get_startd_addr(NULL);
    static SafeSock ssock(my_addr, 0, 0);

    if(ssock.ok()) 
    {
	ssock.encode();
	ssock.timeout(3);
	if( !ssock.code(cmd_num) || !ssock.end_of_message() ) 
	{
	    dprintf( D_ALWAYS, "Can't send command to startd\n" );
	    return -1;
	}
    } 
    else 
    {
		return -1;
    }
    dprintf( D_FULLDEBUG, "Sent update to startd at: %s.\n", my_addr);
    return 0;		
}

int 
PollActivity()
{
    if(xinter->CheckActivity())
    {
	update_startd();
    }
    return TRUE;
}

int 
main_shutdown_graceful()
{
    delete xinter;
    exit(EXIT_SUCCESS);
}

int 
main_shutdown_fast()
{
    exit(EXIT_SUCCESS);
}

int 
main_config()
{
    return TRUE;
}

int
main_init(int, char **)
{
    xinter = new XInterface();

    //Poll for X activity every second.
    daemonCore->Register_Timer(5, 5, (Event)PollActivity, "PollActivity");
    return TRUE;
}





