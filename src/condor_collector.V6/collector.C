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

#include "condor_classad.h"
#include "condor_parser.h"
#include "sched.h"
#include "condor_status.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "internet.h"
#include "fdprintf.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "my_hostname.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "../condor_status.V6/status_types.h"
#include "../condor_status.V6/totals.h"

#include "condor_collector.h"
#include "collector_engine.h"
#include "HashTable.h"
#include "hashkey.h"

#include "condor_uid.h"

// about self
static char *_FileName_ = __FILE__;		// used by EXCEPT
char* mySubSystem = "COLLECTOR";		// used by Daemon Core

// variables from the config file
char *CondorAdministrator;
char *CondorDevelopers;
int   ClientTimeout;
int   QueryTimeout;
char  *CollectorName = NULL;

// variables to deal with sending out a CollectorAd 
ClassAd *ad;
SafeSock updateSock;

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
int receive_invalidation(Service*, int, Stream*);
int receive_update(Service*, int, Stream*);
int receive_query(Service*, int, Stream*);
void process_invalidation(AdTypes, ClassAd &, Stream *);
void process_query(AdTypes, ClassAd &, Stream *);
int sigint_handler(Service*, int);
int sighup_handler(Service*, int);
void init_classad(int interval);
int sendCollectorAd();

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
	daemonCore->Register_Command(QUERY_STARTD_PVT_ADS,"QUERY_STARTD_PVT_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,NEGOTIATOR);
	daemonCore->Register_Command(QUERY_SCHEDD_ADS,"QUERY_SCHEDD_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	daemonCore->Register_Command(QUERY_MASTER_ADS,"QUERY_MASTER_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	daemonCore->Register_Command(QUERY_CKPT_SRVR_ADS,"QUERY_CKPT_SRVR_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	daemonCore->Register_Command(QUERY_SUBMITTOR_ADS,"QUERY_SUBMITTOR_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,READ);
	daemonCore->Register_Command(QUERY_COLLECTOR_ADS,"QUERY_COLLECTOR_ADS",
		(CommandHandler)receive_query,"receive_query",NULL,ADMINISTRATOR);
	
	// install command handlers for invalidations
	daemonCore->Register_Command(INVALIDATE_STARTD_ADS,"INVALIDATE_STARTD_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,WRITE);
	daemonCore->Register_Command(INVALIDATE_SCHEDD_ADS,"INVALIDATE_SCHEDD_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,WRITE);
	daemonCore->Register_Command(INVALIDATE_MASTER_ADS,"INVALIDATE_MASTER_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,WRITE);
	daemonCore->Register_Command(INVALIDATE_CKPT_SRVR_ADS,
		"INVALIDATE_CKPT_SRVR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,WRITE);
	daemonCore->Register_Command(INVALIDATE_SUBMITTOR_ADS,
		"INVALIDATE_SUBMITTOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,WRITE);
	daemonCore->Register_Command(INVALIDATE_COLLECTOR_ADS,
		"INVALIDATE_COLLECTOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,ALLOW);

	// install command handlers for updates
	daemonCore->Register_Command(UPDATE_STARTD_AD,"UPDATE_STARTD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_SCHEDD_AD,"UPDATE_SCHEDD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_SUBMITTOR_AD,"UPDATE_SUBMITTOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_MASTER_AD,"UPDATE_MASTER_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_CKPT_SRVR_AD,"UPDATE_CKPT_SRVR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,WRITE);
	daemonCore->Register_Command(UPDATE_COLLECTOR_AD,"UPDATE_COLLECTOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,ALLOW);

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
		
	  case QUERY_SUBMITTOR_ADS:
		dprintf (D_ALWAYS, "Got QUERY_SUBMITTOR_ADS\n");
		whichAds = SUBMITTOR_AD;
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

	  case QUERY_COLLECTOR_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_COLLECTOR_ADS\n");
		whichAds = COLLECTOR_AD;
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
receive_invalidation(Service* s, int command, Stream* sock)
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
	  case INVALIDATE_STARTD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_STARTD_ADS\n");
		whichAds = STARTD_AD;
		break;
		
	  case INVALIDATE_SCHEDD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_SCHEDD_ADS\n");
		whichAds = SCHEDD_AD;
		break;
		
	  case INVALIDATE_SUBMITTOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_SUBMITTOR_ADS\n");
		whichAds = SUBMITTOR_AD;
		break;

	  case INVALIDATE_MASTER_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_MASTER_ADS\n");
		whichAds = MASTER_AD;
		break;
		
	  case INVALIDATE_CKPT_SRVR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_CKPT_SRVR_ADS\n");
		whichAds = CKPT_SRVR_AD;	
		break;

	  case INVALIDATE_COLLECTOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_COLLECTOR_ADS\n");
		whichAds = COLLECTOR_AD;
		break;
		
	  default:
		dprintf(D_ALWAYS,"Unknown command %d in receive_invalidation()\n", 
			command);
		whichAds = (AdTypes) -1;
    }
   
    if (whichAds != (AdTypes) -1)
		process_invalidation (whichAds, ad, sock);

	// if the invalidation was for the STARTD ads, also invalidate startd
	// private ads with the same query ad
	if (command == INVALIDATE_STARTD_ADS)
		process_invalidation (STARTD_PVT_AD, ad, sock);

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

	if (ad < THRESHOLD) return 1;

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

int
invalidation_scanFunc (ClassAd *ad)
{
	static char buffer[64];
	
	sprintf( buffer, "%s = -1", ATTR_LAST_HEARD_FROM );

	if (ad < THRESHOLD) return 1;

    if ((*ad) >= (*__query__))
    {
		ad->Insert( buffer );			
        __numAds__++;
    }

    return 1;
}

void
process_invalidation (AdTypes whichAds, ClassAd &query, Stream *sock)
{
	// here we set up a network timeout of a longer duration
	sock->timeout(QueryTimeout);

	// set up for hashtable scan
	__query__ = &query;
	__numAds__ = 0;
	__sock__ = sock;
	sock->encode();
	if (!sock->put(0) || !sock->end_of_message()) {
		dprintf( D_ALWAYS, "Unable to acknowledge invalidation\n" );
		return;
	}

	// first set all the "LastHeardFrom" attributes to low values ...
	collector.walkHashTable (whichAds, invalidation_scanFunc);

	// ... then invoke the housekeeper
	collector.invokeHousekeeper (whichAds);

	dprintf (D_ALWAYS, "(Invalidated %d ads)\n", __numAds__);
}	


static int submittorRunningJobs;
static int submittorIdleJobs;
static int machinesTotal;
static int machinesUnclaimed;
static int machinesClaimed;
static int machinesOwner;

int
reportSubmittorScanFunc( ClassAd *ad )
{
	int tmp1, tmp2;
	if( !ad->LookupInteger( ATTR_RUNNING_JOBS , tmp1 ) ||
		!ad->LookupInteger( ATTR_IDLE_JOBS, tmp2 ) )
			return 0;
	submittorRunningJobs += tmp1;
	submittorIdleJobs	 += tmp2;

	return 1;
}

int
reportMiniStartdScanFunc( ClassAd *ad )
{
	char buf[80];

	if ( !ad->LookupString( ATTR_STATE, buf ) )
		return 0;
	machinesTotal++;
	switch ( buf[0] ) {
		case 'C':
			machinesClaimed++;
			break;
		case 'U':
			machinesUnclaimed++;
			break;
		case 'O':
			machinesOwner++;
			break;
	}

	return 1;
}


#ifndef WIN32
static TrackTotals	*normalTotals;
int
reportStartdScanFunc( ClassAd *ad )
{
	return normalTotals->update( ad );
}

void
reportToDevelopers (void)
{
	char	whoami[128];
	char	buffer[128];
	FILE	*mailer;
	int		mailfd;
	TrackTotals	totals( PP_STARTD_NORMAL );

	if (get_machine_name (whoami) == -1) {
		dprintf (D_ALWAYS, "Unable to get_machine_name()\n");
		return;
	}

	sprintf (buffer, "Condor Collector (%s):  Monthly report\n", whoami);
	if( ( mailfd = email( buffer, CondorDevelopers ) ) < 0	||
		( mailer = fdopen( mailfd, "w" ) ) == NULL ) {
		dprintf (D_ALWAYS, "Didn't send monthly report (couldn't open url)\n");
		close( mailfd );
		return;
	}

	if( ( normalTotals = new TrackTotals( PP_STARTD_NORMAL ) ) == NULL ) {
		dprintf( D_ALWAYS, "Didn't send monthly report (failed totals)\n" );
		return;
	}

	normalTotals = &totals;
	if (!collector.walkHashTable (STARTD_AD, reportStartdScanFunc)) {
		dprintf (D_ALWAYS, "Error making monthly report (startd scan) \n");
	}
		
	// output totals summary to the mailer
	totals.displayTotals( mailer, 20 );

	// now output information about submitted jobs
	submittorRunningJobs = 0;
	submittorIdleJobs = 0;
	if( !collector.walkHashTable( SUBMITTOR_AD, reportSubmittorScanFunc ) ) {
		dprintf( D_ALWAYS, "Error making monthly report (submittor scan)\n" );
	}
	fprintf( mailer , "%20s\t%20s\n" , ATTR_RUNNING_JOBS , ATTR_IDLE_JOBS );
	fprintf( mailer , "%20d\t%20d\n" , submittorRunningJobs,submittorIdleJobs );
	
	fclose( mailer );
	return;
}
#endif  // of ifndef WIN32

int
sendCollectorAd()
{
	// compute submitted jobs information
	submittorRunningJobs = 0;
	submittorIdleJobs = 0;
	if( !collector.walkHashTable( SUBMITTOR_AD, reportSubmittorScanFunc ) ) {
		dprintf( D_ALWAYS, "Error making collector ad (submittor scan)\n" );
	}

	// compute machine information
	machinesTotal = 0;
	machinesUnclaimed = 0;
	machinesClaimed = 0;
	machinesOwner = 0;
	if (!collector.walkHashTable (STARTD_AD, reportMiniStartdScanFunc)) {
		dprintf (D_ALWAYS, "Error making collector ad (startd scan) \n");
	}

	// insert values into the ad
	char line[100];
	sprintf(line,"%s = %d",ATTR_RUNNING_JOBS,submittorRunningJobs);
	ad->Insert(line);
	sprintf(line,"%s = %d",ATTR_IDLE_JOBS,submittorIdleJobs);
	ad->Insert(line);
	sprintf(line,"%s = %d",ATTR_NUM_HOSTS_TOTAL,machinesTotal);
	ad->Insert(line);
	sprintf(line,"%s = %d",ATTR_NUM_HOSTS_CLAIMED,machinesClaimed);
	ad->Insert(line);
	sprintf(line,"%s = %d",ATTR_NUM_HOSTS_UNCLAIMED,machinesUnclaimed);
	ad->Insert(line);
	sprintf(line,"%s = %d",ATTR_NUM_HOSTS_OWNER,machinesOwner);
	ad->Insert(line);

	// send the ad
	int		cmd = UPDATE_COLLECTOR_AD;

	updateSock.encode();
	if(!updateSock.code(cmd))
	{
		dprintf(D_ALWAYS, "Can't send UPDATE_MASTER_AD to the collector\n");
		return 0;
	}
	if(!ad->put(updateSock))
	{
		dprintf(D_ALWAYS, "Can't send ClassAd to the collector\n");
		return 0;
	}
	if(!updateSock.end_of_message())
	{
		dprintf(D_ALWAYS, "Can't send endofrecord to the collector\n");
		return 0;
	}

	return 1;
}

void
init_classad(int interval)
{
	if( ad ) delete( ad );
	ad = new ClassAd();

	ad->SetMyTypeName(COLLECTOR_ADTYPE);
	ad->SetTargetTypeName("");

	char line[100], *tmp;
	sprintf(line, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() );
	ad->Insert(line);

	tmp = param( "CONDOR_ADMIN" );
	if( tmp ) {
		sprintf(line, "%s = \"%s\"", ATTR_CONDOR_ADMIN, tmp );
		ad->Insert(line);
		free( tmp );
	}

	if( CollectorName ) {
		sprintf(line, "%s = \"%s\"", ATTR_NAME, CollectorName );
	} else {
		sprintf(line, "%s = \"%s\"", ATTR_NAME, my_full_hostname() );
	}
	ad->Insert(line);

	sprintf(line, "%s = \"%s\"", ATTR_COLLECTOR_IP_ADDR,
			daemonCore->InfoCommandSinfulString() );
	ad->Insert(line);

	if ( interval > 0 ) {
		sprintf(line,"%s = %d",ATTR_UPDATE_INTERVAL,24*interval);
		ad->Insert(line);
	}

	// In case COLLECTOR_EXPRS is set, fill in our ClassAd with those
	// expressions. 
	config_fill_ad( ad, mySubSystem ); 	
}

	
// signal handlers ...
int
sigint_handler (Service *s, int sig)
{
    dprintf (D_ALWAYS, "Killed by SIGINT\n");
    exit(0);
	return FALSE;	// never will get here; just to satisfy c++
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
    initializeParams();
	return TRUE;
}


int
main_shutdown_fast()
{
	exit(0);
	return TRUE;	// to satisfy c++
}


int
main_shutdown_graceful()
{
	exit(0);
	return TRUE;	// to satisfy c++
}

