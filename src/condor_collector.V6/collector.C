#include "condor_common.h"
#include "_condor_fix_resource.h"
#include <iostream.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <netinet/in.h>

#include "condor_classad.h"
#include "condor_parser.h"
#include "sched.h"
#include "condor_status.h"
#include "manager.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"
#include "proc_obj.h"

#include "condor_daemon_core.h"
#include "condor_timer_manager.h"

#include "condor_collector.h"
#include "collector_engine.h"
#include "HashTable.h"
#include "hashkey.h"

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

// old style
int CollectorXDR_UDPSocket;  // used for updates
int CollectorXDR_TCPSocket;  // used for queries

// the shipping socket abstractions
SafeSock   COMM_UDP_sock;

// misc functions
extern "C" XDR *xdr_Init ();
extern "C" XDR *xdr_Udp_Init ();
extern "C" int  xdr_context (XDR *, CONTEXT *);
extern "C" int  xdr_mywrapstring (XDR *, char **);
extern "C" int  xdr_mach_rec (XDR *, MACH_REC *);
extern "C" int  SetSyscalls () {}
extern     void initializeParams (void);
extern     int  initializeTCPSocket (const char *, int);
extern     int  initializeUDPSocket (const char *, int);

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
void giveStatus              (XDR *);
bool sendTerminatingMachRec  (XDR *);
void houseKeeper   		     (void);
void processXDR_TCP_Command  (void);
void processXDR_UDP_Command  (void);
void processCOMM_TCP_Command (void);
void processCOMM_UDP_Command (void);
void processXDR_query        (AdTypes, ClassAd &, XDR *);
void processCOMM_query       (AdTypes, ClassAd &, Sock *);
int  xdr_send_classad_as_mach_rec (XDR *, ClassAd *);

// main code sequence starts ....
int main (int argc, char *argv[])
{
	fd_set readfds;
	int    count, timerID;
 
    // initialize
    MyName = *argv;
    config (MyName, NULL);
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

    // setup communication sockets
	{
	  const char *coll = "condor_collector";

	  CollectorXDR_TCPSocket =initializeTCPSocket (coll, COLLECTOR_PORT);
	  CollectorXDR_UDPSocket =initializeUDPSocket (coll, COLLECTOR_UDP_PORT);
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

	// forever process loop
	for (;;)
	{
		// register sockets 
		FD_ZERO (&readfds);
		FD_SET  (CollectorCOMM_TCPSocket, &readfds);
		FD_SET	(CollectorCOMM_UDPSocket, &readfds);
		FD_SET  (CollectorXDR_TCPSocket, &readfds);
		FD_SET  (CollectorXDR_UDPSocket, &readfds);

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
				EXCEPT ("Bad return value from select(): %d\n", count);
			}
		}

		// process given command
		if (FD_ISSET (CollectorXDR_TCPSocket, &readfds))
			processXDR_TCP_Command ();

		if (FD_ISSET (CollectorXDR_UDPSocket, &readfds))
			processXDR_UDP_Command ();

		if (FD_ISSET (CollectorCOMM_TCPSocket, &readfds))
			processCOMM_TCP_Command ();

		if (FD_ISSET (CollectorCOMM_UDPSocket, &readfds))
			processCOMM_UDP_Command ();
	}

	// we should never quit the above loop
	EXCEPT ("Should never reach here");
	return -1;
}

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
	timerID = timer.NewTimer (NULL,ClientTimeout,(void *)&clientTimeoutHandler);

	xdrs = xdr_Init (&ClientSocket, &xdr);

	xdrs->x_op = XDR_DECODE;
	if (!xdr_int (xdrs, &command) || (command != GIVE_STATUS && !ad.get(xdrs)))
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

	// TCP commands should not allow for classad updates.  In fact the collector
	// will not collect on TCP to discourage use of TCP for classad updates

	// check for GIVE_STATUS command
	if (command == GIVE_STATUS)
	{
		giveStatus(xdrs);
	}
	else
	{
		switch (command)
		{
       		case QUERY_STARTD_ADS:  
				whichAds = STARTD_AD; 
				break;

            case QUERY_SCHEDD_ADS:  
				whichAds = SCHEDD_AD; 
				break;
            case QUERY_MASTER_ADS:  
				whichAds = MASTER_AD; 
				break;

			default:
				dprintf(D_ALWAYS,"Unknown command %d\n", command);
				whichAds = (AdTypes) -1;	
        }
		
		if (whichAds != (AdTypes) -1)
        	processXDR_query (whichAds, ad, xdrs);
	}

	// clean up connection
	xdr_destroy (xdrs);
	(void) close (ClientSocket);
	ClientSocket = -1;
}


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
    if (!sock->code(command) || !ad.get((Stream &)*sock) || !sock->eom())
    {
        dprintf(D_ALWAYS,"Failed to receive query on COMM TCP: aborting\n");
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
            whichAds = STARTD_AD;
            break;

        case QUERY_SCHEDD_ADS:
            whichAds = SCHEDD_AD;
            break;

        case QUERY_MASTER_ADS:
            whichAds = MASTER_AD;
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
static XDR *__xdrs__;
static Sock *__sock__;
static int __numAds__;
static int __failed__;

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
	if (!sock->eom())
	{
		dprintf (D_ALWAYS, "Error flushing COMM sock\n");
	}

	// cancel alarm
	(void) timer.CancelTimer (timerID);
	dprintf (D_ALWAYS, "(Sent %d ads in response to query)\n", __numAds__);
}	

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
		EXCEPT ("Got SIGPIPE, but have no client socket\n");
	
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
