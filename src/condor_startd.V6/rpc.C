/* TODO: magic_string per resource */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#include "sched.h"
#include "condor_types.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_expressions.h"

#include "resource.h"
#include "resmgr.h"

/*
 * Various functions to aid and perform RPCs
 */

extern "C" char *sin_to_string(struct sockaddr_in *);
extern "C" void vacate_client(resource_id_t rid);
extern "C" int create_udpsock(char *service, int port);
extern "C" int create_tcpsock(char *service, int port);
extern "C" int call_incoming(int is, int socktype, resource_id_t rid);
extern "C" int create_port(int *sock);
extern "C" int reply(Sock *sock, int answer);

int command_main(Sock *sock, struct sockaddr_in *from, resource_id_t rid);

static char *_FileName_ = __FILE__;

struct sockaddr_in From;
extern char *IP;
extern char *AccountantHost;

/*
 * Handle an incoming request. Mainly just set up the rpc stream and
 * pass it on to the right function to do the handling proper.
 */
int
call_incoming(int s, int socktype, resource_id_t rid)
{
	struct sockaddr_in from, *ip;
	int len, sd, ret;
	Sock *sock;

	if (socktype == SOCK_STREAM) {
		len = sizeof from;
		memset((char *)&from, 0, len);
		sd = accept(s, (struct sockaddr *)&from, &len);
		dprintf(D_ALWAYS, "handling incoming TCP command from %s\n",
			inet_ntoa(from.sin_addr));
		if (sd < 0 && errno != EINTR) {
			EXCEPT("accept");
		}
		if (sd < 0)
			return -1;
		sock = new ReliSock();
		((ReliSock *)sock)->attach_to_file_desc(sd);
		ip = &from;
	} else {
		dprintf(D_ALWAYS, "handling incoming UDP command\n");
		sock - new SafeSock();	// need to set IP:PORT here???
		((SafeSock *)sock)->attach_to_file_desc(s);
		ip = ((SafeSock *)sock)->endpoint();
	}
	ret = command_main(sock, ip, rid);
	delete sock;
	return ret;
}

/*
 * Do a call to a server. There may already be a connection to the server,
 * in which case 's' is used as the socket. Otherwise a new connection
 * is made using the address 'to' and the port 'port'.
 *
 * As the in call_incoming(): set up the rpc stream and pass things on
 * to a function doing the actual work.
 */
int
call_outgoing(int s, struct sockaddr_in* to, int port, int socktype, int command)
{
	return 0;
}

int
create_udpsock(char* service, int port)
{
	struct sockaddr_in  sin;
	struct servent *servp;
	int sock;

	memset((char *)&sin,0, sizeof sin );
	servp = getservbyname(service, "udp");
	if (servp) {
		sin.sin_port = htons((u_short)servp->s_port);
	} else {
		sin.sin_port = htons((u_short)port);
	}

	if ((sock=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
		EXCEPT( "socket" );
	}

	if (bind(sock,(struct sockaddr *)&sin,sizeof(sin)) < 0) {
	        if (errno == EADDRINUSE) {
			EXCEPT("CONDOR_STARTD ALREADY RUNNING");
		} else {
			EXCEPT( "bind" );
 		}
	}
	return sock;
}

int
create_tcpsock(char* service, int port)
{
	char   	*MySockName;
	struct sockaddr_in	sin;
	struct servent *servp;
#ifdef SO_LINGER
	struct linger linger = { 0, 0 };        /* Don't linger */
#endif
	int		sock;
	int		status;
	int		len, yes = 1;

	memset((char *)&sin,0, sizeof sin);
	servp = getservbyname(service, "tcp");
	if (servp) {
		sin.sin_port = htons((u_short)servp->s_port);
	} else {
		sin.sin_port = htons((u_short)port);
	}

	if ((sock=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT("socket");
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes,
		       sizeof(yes)) < 0) {
		EXCEPT("setsockopt");
	}

#ifdef SO_LINGER
	if (setsockopt(sock,SOL_SOCKET,SO_LINGER,(char *)&linger,sizeof(linger))
	    < 0) {
		EXCEPT( "setsockopt" );
	}
#endif

	if ((status = bind(sock,(struct sockaddr *)&sin,sizeof(sin))) < 0) {
		dprintf(D_ALWAYS, "Bind returns %d, errno = %d\n", status,
			errno);
		if (errno == EADDRINUSE) {
			EXCEPT( "CONDOR_STARTD ALREADY RUNNING" );
		} else {
			EXCEPT("bind");
		}
	}

	get_inet_address(&(sin.sin_addr));
	MySockName = sin_to_string( &sin );
	dprintf(D_ALWAYS, "Listening at address %s\n", MySockName);

	if (listen(sock,5) < 0) {
		EXCEPT("listen");
	}

	return sock;
}

int
create_port(int* sock)
{
	struct sockaddr_in sin;
	int len = sizeof sin;
	char address[100];

	memset((char *)&sin,0,sizeof sin);
	sin.sin_family = AF_INET;

	if (IP) {
		sprintf(address, "<%s:%d>", IP, 0);
		string_to_sin(address, &sin);
	} else {
		sin.sin_port = 0;
	}

	if ((*sock=socket(AF_INET,SOCK_STREAM,0)) < 0) {
		EXCEPT( "socket" );
	}

	if (bind(*sock,(struct sockaddr *)&sin, sizeof sin) < 0) {
		EXCEPT( "bind" );
	}

	if (listen(*sock,1) < 0) {
		EXCEPT( "listen" );
	}

	if (getsockname(*sock,(struct sockaddr *)&sin, &len) < 0) {
		EXCEPT("getsockname");
	}

	return (int)ntohs((u_short)sin.sin_port);
}

int
reply(Sock *sock, int answer)
{
	sock->encode();
	if (!sock->code(answer)) {
		return FALSE;
	}
	if (!sock->eom()) {
		return FALSE;
	}
	dprintf(D_ALWAYS, "Replied %s\n", answer ? "ACCEPTED" : "REFUSED");
	return TRUE;
}

 
// This function simply sends the VACATE_SERVICE command to the schedd
// and accountant (if it exists).
void
vacate_client(resource_id_t rid)
{
	int sd;
	ReliSock *sock;
	resource_info_t *rip;

	if( !(rip = resmgr_getbyrid(rid)) ) {
		dprintf( D_ALWAYS, "vacate:client: Can't find resource.\n" );
		return;
	}

	dprintf(D_FULLDEBUG, "vacate_client %s...\n", rip->r_client);
	if( (sd = do_connect(rip->r_client, "condor_schedd", 0)) < 0 ) {
		dprintf(D_ALWAYS, "Can't connect to schedd (%s)\n", rip->r_client);
	} else {
		sock = new ReliSock();
		sock->attach_to_file_desc(sd);
		if( !sock->put(VACATE_SERVICE) ) {
			dprintf(D_ALWAYS, "Can't send VACATE_SERVICE command\n");
		} else if( !sock->put(rip->r_capab) ) {
			dprintf(D_ALWAYS, "Can't send capability\n");
		} else if( !sock->eom() ) {
			dprintf(D_ALWAYS, "Can't send EOM to schedd\n");
		}
		delete sock;
	}

	/* send VACATE_SERVICE and capability to accountant */
	if( AccountantHost ) {
		if( (sd = do_connect(AccountantHost, "condor_accountant",
							 ACCOUNTANT_PORT)) < 0 ) {
			dprintf(D_ALWAYS, "Couldn't connect to accountant\n");
		} else {
			sock = new ReliSock();
			sock->attach_to_file_desc(sd);
			if( !sock->put(VACATE_SERVICE) ) {
				dprintf(D_ALWAYS, "Can't send VACATE_SERVICE command\n");
			} else if( !sock->put(rip->r_capab) ) {
				dprintf(D_ALWAYS, "Can't send capability\n");
			} else if( !sock->eom() ) {
				dprintf(D_ALWAYS, "Can't send EOM to accountant\n");
			}
			delete sock;
		}
	}

	dprintf(D_ALWAYS, "Done vacating client %s\n", rip->r_client);
}
