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

// variables from the config file
extern char *Log;
extern char *CollectorLog;
extern char *CondorAdministrator;
extern char *CondorDevelopers;
extern int   MaxCollectorLog;
extern int   ClientTimeout; 
extern int   QueryTimeout;
extern int   MachineUpdateInterval;

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
		sin.sin_port = port;
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
		
    MaxCollectorLog = (tmp = param ("MAX_COLLECTOR_LOG")) ? atoi (tmp) : 64000;

    ClientTimeout = (tmp = param ("CLIENT_TIMEOUT")) ? atoi (tmp) : 30;

	QueryTimeout = (tmp = param ("QUERY_TIMEOUT")) ? atoi (tmp) : 60;

    MachineUpdateInterval = (tmp = param ("MACHINE_UPDATE_INTERVAL")) ?
		atoi (tmp) : 300;
    
    CondorDevelopers = param ("CONDOR_DEVELOPERS");
    CondorAdministrator = param ("CONDOR_ADMIN");
}




