#include <iostream.h>
#include <stdio.h>

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_io.h"
#include "sched.h"

// about self
static char *_FileName_ = __FILE__;

// prototypes
extern void reportToDevelopers (void);
extern "C" int schedule_event( int , int , int , int , int , void (*)(void));

// variables from the config file
extern char *Log;
extern char *CollectorLog;
extern char *CondorAdministrator;
extern char *CondorDevelopers;
extern int   MaxCollectorLog;
extern int   ClientTimeout; 
extern int   QueryTimeout;
extern int   MachineUpdateInterval;
extern int   MasterCheckInterval;

// communication
extern int ClientSocket;


int
initializeTCPSocket (const char *service, int port)
{
	struct sockaddr_in sin;
	struct servent     *servp;
	int    on = 1;
	int    sock;

	// get the port of the collector
	memset ((char *) &sin, 0, sizeof (sin));
	servp = getservbyname (service, "tcp");
	if (servp)
	{
		sin.sin_port = htons ((u_short) servp->s_port);
	}
	else
	{
		sin.sin_port = htons (port);
	}

	// create a socket
	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		EXCEPT ("failed socket()");
	}
	
	// allow reuse of local addresses
	if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
		< 0)
	{
		EXCEPT ("failed setsockopt()");
	}

	// bind to the collector's address
	if (bind (sock, (struct sockaddr *)&sin, sizeof (sin)) < 0)
	{
		// check if address is in use ...
		if (errno == EADDRINUSE)
		{
			EXCEPT ("CONDOR_COLLECTOR ALREADY RUNNING");
		}
		else
		{
			EXCEPT ("failed bind()");
		}
	}

	// allow up to five connections
	if (listen (sock, 5) < 0)	
	{
		EXCEPT ("failed listen()");
	}
	
	return sock;
}


int
initializeUDPSocket (const char *service, int port)
{
	struct sockaddr_in sin;
	struct servent     *servp;
	int    sock;

	memset ((char *)&sin, 0, sizeof (sin));
	servp = getservbyname (service, "udp");
	if (servp)
	{
		sin.sin_port = htons ((u_short) servp->s_port);
	}
	else
	{
		sin.sin_port = htons (port);
	}

	sock = socket (AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		EXCEPT ("failed socket()");
	}

	if (bind (sock, (struct sockaddr *)&sin, sizeof (sin)) < 0)
	{
		if (errno == EADDRINUSE)
		{
			EXCEPT ("CONDOR_COLLECTOR ALREADY RUNNING");
		}
		else
		{
			EXCEPT ("failed bind()");
		}
	}

	return sock;
}

void
initializeParams (void)
{
    char *tmp;

	Log = param ("LOG");
	if (Log == NULL) 
	{
		EXCEPT ("Variable 'LOG' not found in config file.");
    }

    CollectorLog = param ("COLLECTOR_LOG");
	if (CollectorLog == NULL) 
	{
		EXCEPT ("Variable 'COLLECTOR_LOG' not found in config file.");
	}
		
    if ((CondorAdministrator = param ("CONDOR_ADMIN")) == NULL)
	{
		EXCEPT ("Variable 'CONDOR_ADMIN' not found in condfig file.");
	}

    MaxCollectorLog = (tmp = param ("MAX_COLLECTOR_LOG")) ? atoi (tmp) : 64000;

    ClientTimeout = (tmp = param ("CLIENT_TIMEOUT")) ? atoi (tmp) : 30;

	QueryTimeout = (tmp = param ("QUERY_TIMEOUT")) ? atoi (tmp) : 60;

    MachineUpdateInterval = (tmp = param ("MACHINE_UPDATE_INTERVAL")) ?
		atoi (tmp) : 300;
    
	MasterCheckInterval = (tmp = param ("MASTER_CHECK_INTERVAL")) ? 
		atoi (tmp) : 10800; 	// three hours

	tmp = param ("CONDOR_DEVELOPERS");
	if (tmp == NULL) {
		tmp = "condor@cs.wisc.edu";
	} else
	if (strcmp (tmp, "NONE") == 0) {
		tmp = NULL;
	}
	CondorDevelopers = tmp;
}



void
initializeReporter (void)
{
	const int STAR = -1;

	// schedule reports to developers
	schedule_event( STAR, 1,  0, 0, 0, reportToDevelopers );
	schedule_event( STAR, 8,  0, 0, 0, reportToDevelopers );
	schedule_event( STAR, 15, 0, 0, 0, reportToDevelopers );
	schedule_event( STAR, 23, 0, 0, 0, reportToDevelopers );
}
