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

    dprintf( D_FULLDEBUG, "X_EVENT_NOTIFICATION is: %d.\n",
			 X_EVENT_NOTIFICATION);

    if(ssock.ok()) {
		ssock.encode();
		ssock.timeout(3);
		if( !ssock.code(cmd_num) || !ssock.end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send command to startd\n" );
			return -1;
		}
    } else {
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





