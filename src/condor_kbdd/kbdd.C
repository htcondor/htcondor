#include "condor_common.h"
#include "XInterface.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "my_hostname.h"
#include "condor_query.h"

#include <netdb.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <utmp.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <rpc/types.h>

#include <stdio.h>
#include <X11/Xlib.h>


extern "C" char *get_startd_addr(const char *);

char *mySubSystem = "KBDD";
XInterface *xinter;


int
update_startd()
{
    char *my_name;
    //SafeSock *ssock;
    my_name = my_hostname();


    
    dprintf( D_FULLDEBUG, "Activity on display.\n");
    //my_addr = get_startd_addr(my_name);
    
    //dprintf( D_FULLDEBUG, "My hostname is: %s.\n", my_name);
    //dprintf( D_FULLDEBUG, "Sent update to startd at: %s.\n", my_addr);
    
    //ssock = new SafeSock(my_addr, 0, 3);
    //ssock->encode();
    //ssock->snd_int(X_EVENT_NOTIFICATION, TRUE);
    
    //delete ssock;
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





