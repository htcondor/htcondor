#include "condor_common.h"
#include "_condor_fix_resource.h"
#include <iostream.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/param.h>

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

#include "condor_daemon_core.h"
#include "condor_timer_manager.h"

#include "condor_collector.h"
#include "collector_engine.h"
#include "HashTable.h"
#include "hashkey.h"

#include "condor_uid.h"

// about self
char *MyName;
static char *_FileName_ = __FILE__;

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

// timer services
TimerManager timer;
int clientTimeoutHandler (Service *);  // if client takes too long to request
int queryTimeoutHandler  (Service *);  // if a query+response takes too long

// the heart of the collector ...
CollectorEngine collector(&timer);

// communication
int ClientSocket  = -1;

// new style
int CollectorCOMM_UDPSocket;  // used for updates
int CollectorCOMM_TCPSocket;  // used for queries

#if defined(USE_XDR)
// old style
int CollectorXDR_UDPSocket;  // used for updates
int CollectorXDR_TCPSocket;  // used for queries
#endif

// the shipping socket abstractions
SafeSock   COMM_UDP_sock;

// control and display
int		Foreground = 0;
int		Termlog;

// misc functions
#if defined(USE_XDR)
extern "C" XDR *xdr_Init ();
extern "C" XDR *xdr_Udp_Init ();
extern "C" int  xdr_context (XDR *, CONTEXT *);
extern "C" int  xdr_mywrapstring (XDR *, char **);
extern "C" int  xdr_mach_rec (XDR *, MACH_REC *);
#endif
extern "C" int  SetSyscalls () {}
extern "C" void event_mgr (void);
extern     void initializeParams (void);
extern     int  initializeTCPSocket (const char *, int);
extern     int  initializeUDPSocket (const char *, int);
extern	   void initializeReporter (void);

// misc external variables
extern int errno;
extern struct sockaddr_in From;

// information about signal handlers
typedef void (* SIG_HANDLER) ();
extern "C" void install_sig_handler (int, SIG_HANDLER);
void sigint_handler ();
void sighup_handler ();
void sigpipe_handler();

// internal function prototypes
#if defined(USE_XDR)
void giveStatus              (XDR *);
bool sendTerminatingMachRec  (XDR *);
void processXDR_TCP_Command  (void);
void processXDR_UDP_Command  (void);
#if 0
void processXDR_query        (AdTypes, ClassAd &, XDR *);
#endif
int  xdr_send_classad_as_mach_rec (XDR *, ClassAd *);
#endif
void houseKeeper   		     (void);
void processCOMM_TCP_Command (void);
void processCOMM_UDP_Command (void);
void processCOMM_query       (AdTypes, ClassAd &, Sock *);

void usage(char* name)
{
	dprintf(D_ALWAYS,"Usage: %s [-f] [-b] [-t] [-c config_file_name]\n",name );
	exit( 1 );
}

// main code sequence starts ....
int main (int argc, char *argv[])
{
	fd_set readfds;
	int    count, timerID;
	int	   numIterations;
	char** ptr;
	
	if(argc > 5)
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
		case 'f':
			Foreground = 1;
			break;
		case 't':
			Termlog = 1;
			break;
		case 'b':
			Foreground = 0;
			break;
		default:
			usage(argv[0]);
		}
	}
	
    // initialize
    MyName = *argv;
	set_condor_priv();
	config( 0 );
	
    initializeParams ();
    dprintf_config ("COLLECTOR", STDERR_FILENO);

    // print out a banner
    dprintf (D_ALWAYS, "+--------------------------------------------+\n");
    dprintf (D_ALWAYS, "|       [ Condor Collector:  Startup ]       |\n");
    dprintf (D_ALWAYS, "+--------------------------------------------+\n");

    // if core is dumped, it will be found in the log directory
	{
		int result = chdir (Log);
		if (result < 0) 
		{
			EXCEPT ("failed chdir(%s): ERRNO %d", Log, errno);
		}
	}
 
    // install signal handlers
	install_sig_handler (SIGINT, sigint_handler);
	install_sig_handler (SIGHUP, sighup_handler);
	install_sig_handler (SIGPIPE, sigpipe_handler);

	// setup routine to report to condor developers
	initializeReporter ();

    // setup communication sockets
	{
	  const char *coll = "condor_collector";

#if defined(USE_XDR)
	  CollectorXDR_TCPSocket =initializeTCPSocket (coll, COLLECTOR_PORT);
	  CollectorXDR_UDPSocket =initializeUDPSocket (coll, COLLECTOR_UDP_PORT);
#endif
	  CollectorCOMM_TCPSocket=initializeTCPSocket (coll, COLLECTOR_COMM_PORT);
	  CollectorCOMM_UDPSocket=initializeUDPSocket(coll,COLLECTOR_UDP_COMM_PORT);

	  if (!COMM_UDP_sock.attach_to_file_desc(CollectorCOMM_UDPSocket))
	  {
		EXCEPT("Unable to attach to file descriptor");
	  }
	}

	// set up housekeeper
	if (!collector.scheduleHousekeeper (MachineUpdateInterval))
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

	// forever process loop
	numIterations = 0;
	for (;;)
	{
		// register sockets 
		FD_ZERO (&readfds);
		FD_SET  (CollectorCOMM_TCPSocket, &readfds);
		FD_SET	(CollectorCOMM_UDPSocket, &readfds);
#if defined(USE_XDR)
		FD_SET  (CollectorXDR_TCPSocket, &readfds);
		FD_SET  (CollectorXDR_UDPSocket, &readfds);
#endif

		// await communication activity
		count = select (FD_SETSIZE, (fd_set *) &readfds, (fd_set *) 0,
						(fd_set *) 0, (struct timeval *) 0);

		// check if return value was ok
		if (count <= 0)
		{
			dprintf (D_ALWAYS, "Select returned value: %d <= 0\n", count);
			if (errno == EINTR)
			{
				dprintf (D_ALWAYS, "(errno was EINTR --- redoing loop)\n");
				continue;
			}
			else
			{
				EXCEPT ("Bad return value from select(): %d", count);
			}
		}

		// process given command
#if defined(USE_XDR)
		if (FD_ISSET (CollectorXDR_TCPSocket, &readfds))
			processXDR_TCP_Command ();

		if (FD_ISSET (CollectorXDR_UDPSocket, &readfds))
			processXDR_UDP_Command ();
#endif

		if (FD_ISSET (CollectorCOMM_TCPSocket, &readfds))
			processCOMM_TCP_Command ();

		if (FD_ISSET (CollectorCOMM_UDPSocket, &readfds))
			processCOMM_UDP_Command ();

		// after 32768 iterations (arbitrary), check with event manager
		// (we don't wan't to call event_mgr on every iteration to check 
		// for events which are scheduled four times a month)
		++numIterations;
		if (numIterations % 32768 == 0) {
			event_mgr ();
		}
	}

	// we should never quit the above loop
	EXCEPT ("Should never reach here");
	return 1;
}

#if defined(USE_XDR)
void 
processXDR_TCP_Command (void)
{
	struct sockaddr_in from;
	int    len, timerID;
	XDR    xdr, *xdrs;
	int    command, insert;
	AdTypes whichAds;
	ClassAd ad;

	len = sizeof (from);
	memset ((char *) &from, 0, len);
	ClientSocket=accept(CollectorXDR_TCPSocket,(struct sockaddr *)&from,&len);

	if (ClientSocket < 0)
	{
		EXCEPT ("failed accept()");
	}

	// set up alarm for getting the command
	timerID=timer.NewTimer (NULL,ClientTimeout,(void *)&clientTimeoutHandler);

	xdrs = xdr_Init (&ClientSocket, &xdr);

	xdrs->x_op = XDR_DECODE;
//	if (!xdr_int (xdrs, &command) || (command != GIVE_STATUS && !ad.get(xdrs)))
	if (!xdr_int (xdrs, &command))
	{
		dprintf(D_ALWAYS,"Failed to receive command on XDR TCP: aborting\n");
		xdr_destroy (xdrs);
		(void) close (ClientSocket);
		ClientSocket = -1;
		(void) timer.CancelTimer (timerID);
		return;
	}

	// cancel timer --- collector engine sets up its own timer for
	// collecting further information
	(void) timer.CancelTimer (timerID);

	// TCP commands should not allow for classad updates. In fact the collector
	// will not collect on TCP to discourage use of TCP for classad updates
	// check for GIVE_STATUS command
	if (command == GIVE_STATUS)
	{
		dprintf (D_ALWAYS, "Got GIVE_STATUS\n");
		giveStatus(xdrs);
	}
	else
	{
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
			dprintf(D_ALWAYS,"Unknown command %d\n", command);
			whichAds = (AdTypes) -1;	
        }
		
#if 0
 		if (whichAds != (AdTypes) -1)
        	processXDR_query (whichAds, ad, xdrs);
#endif
	}

	// clean up connection
	xdr_destroy (xdrs);
	(void) close (ClientSocket);
	ClientSocket = -1;
}
#endif

void
processCOMM_TCP_Command (void)
{
    struct sockaddr_in from;
    int    len, timerID;
	Sock 	*sock;
    int    command, insert;
	AdTypes whichAds;
	ClassAd ad;

    len = sizeof (from);
    memset ((char *) &from, 0, len);
    ClientSocket=accept(CollectorCOMM_TCPSocket,(struct sockaddr *)&from,&len);

    if (ClientSocket < 0)
    {
        EXCEPT ("failed accept()");
    }

    // set up alarm for communication
    timerID = timer.NewTimer (NULL, ClientTimeout,(void*)&clientTimeoutHandler);

	// set up sock
	sock = new ReliSock();
	sock->assign(ClientSocket);

	// TCP commands should not allow for classad updates.  In fact the collector
	// will not collect on TCP to discourage use of TCP for classad updates

	sock->decode();
    if (!sock->code(command) 		|| 
		!ad.get((Stream &)*sock) 	|| 
		!sock->end_of_message())
    {
        dprintf(D_ALWAYS,"Failed to receive query on COMM TCP: aborting\n");
		sock->end_of_message();
		delete sock;
        (void) close (ClientSocket);
        ClientSocket = -1;
        (void) timer.CancelTimer (timerID);
        return;
    }

    // cancel timer --- collector engine sets up its own timer for
    // collecting further information
    (void) timer.CancelTimer(timerID);

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
		dprintf(D_ALWAYS,"Unknown command %d\n", command);
		whichAds = (AdTypes) -1;
    }
   
    if (whichAds != (AdTypes) -1)
		processCOMM_query (whichAds, ad, sock);

    // clean up connection
	delete sock;
    (void) close (ClientSocket);
    ClientSocket = -1;
}


#if defined(USE_XDR)
void
processXDR_UDP_Command (void)
{
	XDR     xdr, *xdrs;
	int     command, timerID, insert;
	sockaddr_in *from;
	ClassAd *ad;

	// set up alarm for communication
	timerID = timer.NewTimer (NULL, ClientTimeout,(void*)&clientTimeoutHandler);

	xdrs = xdr_Udp_Init (&CollectorXDR_UDPSocket, &xdr);
	xdrs->x_op = XDR_DECODE;
	if (!xdr_int (xdrs, &command))
	{
		dprintf(D_ALWAYS,"Failed to receive command on XDR UDP: aborting\n");
		xdr_destroy (xdrs);
		(void) close (ClientSocket);
		ClientSocket = -1;
		(void) timer.CancelTimer (timerID);
		return;
	}

    // cancel timer --- collector engine sets up its own timer for
    // collecting further information
    (void) timer.CancelTimer (timerID);

	// get endpoint
	from = &From;

	// process the given command
	if (!(ad = collector.collect (command, xdrs, from, insert)))
	{
		if (insert == -2)
		{
			dprintf(D_ALWAYS,"Got QUERY command (%d); not supported for UDP\n",
					 command);
		}
	}

	// clean up connection
	xdr_destroy (xdrs);
	(void) close (ClientSocket);
	ClientSocket = -1;
}
#endif

void
processCOMM_UDP_Command (void)
{
    int		 command, timerID, insert;
	sockaddr_in *from;
	ClassAd *ad;

    // set up alarm for communication
    timerID = timer.NewTimer (NULL, ClientTimeout,(void*)&clientTimeoutHandler);

	// set up safe socket
	COMM_UDP_sock.decode();
    if (!COMM_UDP_sock.code(command))
    {
        dprintf(D_ALWAYS,"Failed to receive command on COMM UDP: aborting\n");
        (void) timer.CancelTimer (timerID);
		COMM_UDP_sock.end_of_message();
        return;
    }

    // cancel timer --- collector engine sets up its own timer for
    // collecting further information
    (void) timer.CancelTimer (timerID);

	// get endpoint
	from = COMM_UDP_sock.endpoint();

    // process the given command
	if (!(ad = collector.collect (command,(Sock*)&COMM_UDP_sock,from,insert)))
	{
		if (insert == -2)
		{
			dprintf (D_ALWAYS,"Got QUERY command (%d); not supported for UDP\n",
						command);
		}
	}
}

static ClassAd *__query__;
#if defined(USE_XDR)
static XDR *__xdrs__;
#endif
static Sock *__sock__;
static int __numAds__;
static int __failed__;

#if defined(USE_XDR) && 0
int
XDR_query_scanFunc (ClassAd *ad)
{
	int more = 1;

	if ((*ad) >= (*__query__))
	{
		if (!xdr_int (__xdrs__, &more) || !ad->put(__xdrs__))
		{
			dprintf (D_ALWAYS, 
					"Error sending query result to client -- aborting\n");
			return 0;
		}
		__numAds__++;
	}

	return 1;
}
#endif

int
COMM_query_scanFunc (ClassAd *ad)
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

#if defined(USE_XDR) && 0
void
processXDR_query (AdTypes whichAds, ClassAd &query, XDR *xdrs)
{
	int	timerID;
	int	more;

	// here we set up a timer of longer duration
	timerID = timer.NewTimer (NULL, QueryTimeout,(void*)&queryTimeoutHandler);

	// set up for hashtable scan
	__query__ = &query;
	__numAds__ = 0;
	__xdrs__ = xdrs;
	xdrs->x_op = XDR_ENCODE;
	if (!collector.walkHashTable (whichAds, XDR_query_scanFunc))
	{
		dprintf (D_ALWAYS, "Error sending query response on XDR\n");
	}

	// end of query response ...
	more = 0;
	if (!xdr_int (xdrs, &more))
	{
		dprintf (D_ALWAYS, "Error sending EndOfResponse (0) to client\n");
	}

	// flush the output
	if (!xdrrec_endofrecord (xdrs, TRUE))
	{
		dprintf (D_ALWAYS, "Error flushing xdr sock\n");
	}

	// cancel alarm
	(void) timer.CancelTimer (timerID);
	dprintf (D_ALWAYS, "(Sent %d ads in response to query)\n", __numAds__);
}	
#endif

void
processCOMM_query (AdTypes whichAds, ClassAd &query, Sock *sock)
{
	int     timerID;
	int		more;

	// here we set up a timer of longer duration
	timerID = timer.NewTimer (NULL, QueryTimeout, (void *)&queryTimeoutHandler);

	// set up for hashtable scan
	__query__ = &query;
	__numAds__ = 0;
	__sock__ = sock;
	sock->encode();
	if (!collector.walkHashTable (whichAds, COMM_query_scanFunc))
	{
		dprintf (D_ALWAYS, "Error sending query response on COMM\n");
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
		dprintf (D_ALWAYS, "Error flushing COMM sock\n");
	}

	// cancel alarm
	(void) timer.CancelTimer (timerID);
	dprintf (D_ALWAYS, "(Sent %d ads in response to query)\n", __numAds__);
}	

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


#if defined(USE_XDR)
int
XDR_give_status_scanFunc (ClassAd *ad)
{
	if (!xdr_send_classad_as_mach_rec (__xdrs__, ad)) __failed__++;
	__numAds__++;
	return 1;
}


void
giveStatus (XDR *xdrs)
{
	ClassAd *ad;
	int total = 0, failed = 0;
	char *ch = NULL;

	// skip record
	if (!xdrrec_skiprecord (xdrs))
	{
		dprintf (D_ALWAYS, "Could not skiprecord after GIVE_STATUS\n");
		return;
	}

	// prepare to send
	xdrs->x_op = XDR_ENCODE;

	// send list of classads
	__xdrs__ = xdrs;
	__failed__ = 0;
	__numAds__ = 0;

  	(void) collector.walkHashTable (STARTD_AD, XDR_give_status_scanFunc);
 
	dprintf (D_ALWAYS, "Response: successfully sent %d of %d MACH_REC's\n", 
			 __numAds__-__failed__, __numAds__);

	// terminate list of ads ...
	if (!sendTerminatingMachRec (xdrs))
	{  
		dprintf (D_ALWAYS, "Could not send terminating MACH_REC\n");
	}

	// flush stream
	if (!xdrrec_endofrecord (xdrs, TRUE))
	{
		dprintf (D_ALWAYS, "Could not flush xdr stream\n");
	}
}


bool
sendTerminatingMachRec (XDR *xdrs)
{
	MACH_REC mach_rec, *rec = &mach_rec;

	memset ((void *) rec, 0, sizeof (MACH_REC));
	rec->machine_context = create_context ();
	if (!xdr_mach_rec (xdrs, rec)) {return false;}
	free_context (rec->machine_context);

	return true;
}
#endif

	
// signal handlers ...
void
sigint_handler ()
{
    dprintf (D_ALWAYS, "Killed by SIGINT\n");
    exit (0);
}


void
sighup_handler ()
{
    dprintf (D_ALWAYS, "Got SIGHUP; re-reading config file ...\n");
    initializeParams ();
}


void
sigpipe_handler ()
{
    if (ClientSocket == -1)
		EXCEPT ("Got SIGPIPE, but have no client socket");
	
	(void) close (ClientSocket);
    ClientSocket = -1;
}


int
clientTimeoutHandler (Service *x)
{
    if (ClientSocket == -1)
	{
		dprintf (D_ALWAYS, "Alarm: Got alarm, but have no client socket\n");
		return 0;
	}
    
    dprintf (D_ALWAYS, 
			 "Alarm: Client took too long (over %d secs) -- aborting\n",
			 ClientTimeout);

	(void) close (ClientSocket);
    ClientSocket = -1;

	return 0;
}


int
queryTimeoutHandler (Service *x)
{
	if (ClientSocket == -1)
	{
		dprintf (D_ALWAYS, "Alarm: Got alarm, but have no query socket\n");
		return 0;
	}

    dprintf (D_ALWAYS, 
			 "Alarm: Query took too long (over %d secs) -- aborting\n",
			 QueryTimeout);

	(void) close (ClientSocket);
    ClientSocket = -1;

	return 0;
}
