#include "condor_common.h"
#include "_condor_fix_resource.h"
#include <iostream.h>
#include <time.h>

#ifndef WIN32
#include <netinet/in.h>
#include <sys/param.h>
#endif  // ifndef WIN32

#include "condor_classad.h"
#include "condor_parser.h"
#include "sched.h"
#include "condor_status.h"
#include "manager.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "internet.h"
#include "fdprintf.h"
#include "condor_io.h"
#include "condor_attributes.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condor_collector.h"
#include "collector_engine.h"
#include "HashTable.h"
#include "hashkey.h"

#include "condor_uid.h"

// about self
static char *_FileName_ = __FILE__;		// used by EXCEPT
char* mySubSystem = "COLLECTOR";		// used by Daemon Core

// variables from the config file
char *Log;
char *CollectorLog;
char *CondorAdministrator;
char *CondorDevelopers;
int   MaxCollectorLog;
int   ClientTimeout; 
int   QueryTimeout;
int   MachineUpdateInterval;
int	  MasterCheckInterval;

// the heart of the collector ...
CollectorEngine collector;

// misc functions
#ifndef WIN32
extern	   void initializeReporter (void);
#endif
extern "C" int  SetSyscalls () {return 0;}
extern     void initializeParams();

// misc external variables
extern int errno;

// internal function prototypes
void houseKeeper(void);
int receive_update(Service*, int, Stream*);
int receive_query(Service*, int, Stream*);
void process_query(AdTypes, ClassAd &, Stream *);
int sigint_handler(Service*, int);
int sighup_handler(Service*, int);
#ifndef WIN32
void sigpipe_handler();
void unixsigint_handler();
void unixsighup_handler();
#endif

void usage(char* name)
{
	dprintf(D_ALWAYS,"Usage: %s [-f] [-b] [-t] [-p <port>]\n",name );
	exit( 1 );
}

// main initialization code... the real main() is in DaemonCore
main_init(int argc, char *argv[])
{
	char** ptr;
	
	// handle collector-specific command line args
	if(argc > 1)
	{
		usage(argv[0]);
	}
	for(ptr = argv + 1; *ptr; ptr++)
	{
		if(ptr[0][0] != '-')
		{
			usage(argv[0]);
		}
		switch(ptr[0][1])
		{
		// place collector-specific command line args here
		default:
			usage(argv[0]);
		}
	}
	
	// read in various parameters from condor_config
    initializeParams ();

    // install signal handlers
	daemonCore->Register_Signal(DC_SIGINT,"SIGINT",(SignalHandler)sigint_handler,"sigint_handler()");
	daemonCore->Register_Signal(DC_SIGHUP,"SIGHUP",(SignalHandler)sighup_handler,"sighup_handler()");

#ifndef WIN32
	install_sig_handler (SIGINT, unixsigint_handler);
#endif	// of ifndef WIN32

#ifndef WIN32
	// setup routine to report to condor developers
	initializeReporter ();
#endif

	// install command handlers for queries
	daemonCore->Register_Command(QUERY_STARTD_ADS,"QUERY_STARTD_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	daemonCore->Register_Command(QUERY_SCHEDD_ADS,"QUERY_SCHEDD_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	daemonCore->Register_Command(QUERY_MASTER_ADS,"QUERY_MASTER_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	daemonCore->Register_Command(QUERY_CKPT_SRVR_ADS,"QUERY_CKPT_SRVR_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	
	// install command handlers for updates
	daemonCore->Register_Command(UPDATE_STARTD_AD,"UPDATE_STARTD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_SCHEDD_AD,"UPDATE_SCHEDD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_MASTER_AD,"UPDATE_MASTER_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_CKPT_SRVR_AD,"UPDATE_CKPT_SRVR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);

	// set up housekeeper
	if (!collector.scheduleHousekeeper(MachineUpdateInterval))
	{
		EXCEPT ("Could not initialize housekeeper");
	}

	// set up routine to check on masters
	if (!collector.scheduleDownMasterCheck (MasterCheckInterval))
	{
		EXCEPT ("Could not initialize master check routine");
	}

	// set up so that private ads from startds are collected as well
	collector.wantStartdPrivateAds (true);

	return TRUE;
}

int
receive_query(Service* s, int command, Stream* sock)
{
    struct sockaddr_in *from;
	AdTypes whichAds;
	ClassAd ad;

	from = sock->endpoint();

	sock->decode();
	sock->timeout(ClientTimeout);
    if (!ad.get((Stream &)*sock) || !sock->eom())
    {
        dprintf(D_ALWAYS,"Failed to receive query on TCP: aborting\n");
        return FALSE;
    }

    // cancel timeout --- collector engine sets up its own timeout for
    // collecting further information
    sock->timeout(0);

    switch (command)
    {
	  case QUERY_STARTD_ADS:
		dprintf (D_ALWAYS, "Got QUERY_STARTD_ADS\n");
		whichAds = STARTD_AD;
		break;
		
	  case QUERY_SCHEDD_ADS:
		dprintf (D_ALWAYS, "Got QUERY_SCHEDD_ADS\n");
		whichAds = SCHEDD_AD;
		break;
		
	  case QUERY_MASTER_ADS:
		dprintf (D_ALWAYS, "Got QUERY_MASTER_ADS\n");
		whichAds = MASTER_AD;
		break;
		
	  case QUERY_CKPT_SRVR_ADS:
		dprintf (D_ALWAYS, "Got QUERY_CKPT_SRVR_ADS\n");
		whichAds = CKPT_SRVR_AD;	
		break;
		
	  case QUERY_STARTD_PVT_ADS:
		dprintf (D_ALWAYS, "Got QUERY_STARTD_PVT_ADS\n");
		whichAds = STARTD_PVT_AD;
		break;

	  default:
		dprintf(D_ALWAYS,"Unknown command %d in process_query()\n", command);
		whichAds = (AdTypes) -1;
    }
   
    if (whichAds != (AdTypes) -1)
		process_query (whichAds, ad, sock);

    // all done; let daemon core will clean up connection
	return TRUE;
}

int
receive_update(Service *s, int command, Stream* sock)
{
    int	insert;
	sockaddr_in *from;
	ClassAd *ad;

	
	// TCP commands should not allow for classad updates.  In fact the collector
	// will not collect on TCP to discourage use of TCP for classad updates.
	if ( sock->type() == Stream::reli_sock ) {
		// update via tcp; sorry buddy, use udp or you're outa here!
		dprintf(D_ALWAYS,"Received UPDATE command via TCP; ignored\n");
		// let daemon core clean up the socket
		return TRUE;
	}

	// get endpoint
	from = sock->endpoint();

    // process the given command
	if (!(ad = collector.collect (command,(Sock*)sock,from,insert)))
	{
		if (insert == -2)
		{
			// this should never happen assuming we never register QUERY
			// commands with daemon core, but it cannot hurt to check...
			dprintf (D_ALWAYS,"Got QUERY command (%d); not supported for UDP\n",
						command);
		}
	}

	// let daemon core clean up the socket
	return TRUE;
}

static ClassAd *__query__;
static Stream *__sock__;
static int __numAds__;
static int __failed__;

int
query_scanFunc (ClassAd *ad)
{
    int more = 1;

    if ((*ad) >= (*__query__))
    {
        if (!__sock__->code(more) || !ad->put(*__sock__))
        {
            dprintf (D_ALWAYS, 
                    "Error sending query result to client -- aborting\n");
            return 0;
        }
        __numAds__++;
    }

    return 1;
}

void
process_query (AdTypes whichAds, ClassAd &query, Stream *sock)
{
	int		more;

	// here we set up a network timeout of a longer duration
	sock->timeout(QueryTimeout);

	// set up for hashtable scan
	__query__ = &query;
	__numAds__ = 0;
	__sock__ = sock;
	sock->encode();
	if (!collector.walkHashTable (whichAds, query_scanFunc))
	{
		dprintf (D_ALWAYS, "Error sending query response\n");
	}

	// end of query response ...
	more = 0;
	if (!sock->code(more))
	{
		dprintf (D_ALWAYS, "Error sending EndOfResponse (0) to client\n");
	}

	// flush the output
	if (!sock->end_of_message())
	{
		dprintf (D_ALWAYS, "Error flushing CEDAR socket\n");
	}

	dprintf (D_ALWAYS, "(Sent %d ads in response to query)\n", __numAds__);
}	

#ifndef WIN32
int 	__mailer__;

int
reportScanFunc (ClassAd *ad)
{
	char buffer[2048];
	int	 x;

	if (!ad->LookupString (ATTR_NAME, buffer)) return 0;
	fdprintf (__mailer__, "%15s", buffer);

	if (!ad->LookupString (ATTR_ARCH, buffer)) return 0;
	fdprintf (__mailer__, "%8s", buffer);

	if (!ad->LookupString (ATTR_OPSYS, buffer)) return 0;
	fdprintf (__mailer__, "%14s", buffer);

	if (!ad->LookupInteger (ATTR_MIPS, x)) x = -1;
	fdprintf (__mailer__, "%4d", x);

	if (!ad->LookupInteger (ATTR_KFLOPS, x)) x = -1;
	fdprintf (__mailer__, "%7d", x);

	if (!ad->LookupString (ATTR_STATE, buffer)) return 0;
	fdprintf (__mailer__, "%10s\n", buffer);

	return 1;
}

void
reportToDevelopers (void)
{
	char	whoami[128];
	char	buffer[128];

	if (get_machine_name (whoami) == -1) {
		dprintf (D_ALWAYS, "Unable to get_machine_name()\n");
		return;
	}

	sprintf (buffer, "Condor Collector (%s):  Monthly report\n", whoami);
	if ((__mailer__ = email (buffer, CondorDevelopers)) < 0) {
		dprintf (D_ALWAYS, "Didn't send monthly report (couldn't open url)\n");
		return;
	}

	if (!collector.walkHashTable (STARTD_AD, reportScanFunc)) {
		dprintf (D_ALWAYS, "Error sending monthly report\n");
	}	
		
	close (__mailer__);
	return;
}
#endif  // of ifndef WIN32
	
// signal handlers ...
int
sigint_handler (Service *s, int sig)
{
    dprintf (D_ALWAYS, "Killed by SIGINT\n");
    exit(0);
	return FALSE;	// never will get here; just to satisfy c++
}


int
sighup_handler (Service *s, int sig)
{
    dprintf (D_ALWAYS, "Got SIGHUP; re-reading config file ...\n");
    initializeParams();
	return TRUE;
}


#ifndef WIN32
void
unixsigint_handler ()
{
    dprintf (D_ALWAYS, "Killed by SIGINT\n");
    exit(0);
	return;	// never will get here; just to satisfy c++
}

#endif	// of ifndef WIN32



int
main_config()
{
	return TRUE;
}


int
main_shutdown_fast()
{
	return TRUE;
}


int
main_shutdown_graceful()
{
	return TRUE;
}

