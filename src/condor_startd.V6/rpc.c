/* TODO: magic_string per resource */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <errno.h>
#include <netdb.h>

#include "cdefs.h"
#include "sched.h"
#include "condor_types.h"
#include "condor_debug.h"
#include "condor_xdr.h"
#include "condor_expressions.h"

#include "resource.h"
#include "resmgr.h"

/*
 * Various functions to aid and perform RPCs
 */

char	*MySockName;

static char *_FileName_;

extern struct sockaddr_in From;
extern char *IP;
extern char *AccountantHost;
extern char *client;

extern XDR *xdr_Udp_Init __P((int *, XDR *));	/* XXX */
extern char *sin_to_string __P((struct sockaddr_in *));

/*
 * Handle an incoming request. Mainly just set up the XDR stream and
 * pass it on to the right function to do the handling proper.
 */
int
call_incoming(s, socktype, rid)
	int s;
	int socktype;
	resource_id_t rid;
{
	struct sockaddr_in from, *ip;
	int len, new, ret;
	XDR xdr, *xdrs;

	if (socktype == SOCK_STREAM) {
		len = sizeof from;
		memset((char *)&from, 0, len);
		new = accept(s, (struct sockaddr *)&from, &len);
		dprintf(D_ALWAYS, "call_incoming(SOCK_STREAM) from %s\n",
			inet_ntoa(from.sin_addr));
		if (new < 0 && errno != EINTR) {
			EXCEPT("accept");
		}
		if (new < 0)
			return -1;
		xdrs = xdr_Init(&new, &xdr);
		ip = &from;
	} else {
		dprintf(D_ALWAYS, "call_incoming(SOCK_DGRAM)\n");
		xdrs = xdr_Udp_Init(&s, &xdr);
		ip = &From;	/* XXX UGH! */
	}
	dprintf(D_ALWAYS, "call_incoming: call command_main\n");
	ret = command_main(xdrs, ip, rid);
	xdr_destroy(xdrs);
	if (socktype == SOCK_STREAM)
		close(new);
	return ret;
}

/*
 * Do a call to a server. There may already be a connection to the server,
 * in which case 's' is used as the socket. Otherwise a new connection
 * is made using the address 'to' and the port 'port'.
 *
 * As the in call_incoming(): set up the XDR stream and pass things on
 * to a function doing the actual work.
 */
int
call_outgoing(s, to, port, socktype, command)
	int s;
	struct sockaddr_in *to;
	int port, socktype, command;
{
	return 0;
}

int
create_udpsock(service, port)
	char *service;
	int port;
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
create_tcpsock(service, port)
	char *service;
	int port;
{
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
create_port(sock)
	int *sock;
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
reply(xdrs, answer)
	XDR *xdrs;
	int answer;
{
	xdrs->x_op = XDR_ENCODE;
	if (!xdr_int(xdrs,&answer)) {
		return FALSE;
	}
	if (!xdrrec_endofrecord(xdrs,TRUE)) {
		return FALSE;
	}
	dprintf(D_ALWAYS, "Replied %s\n", answer ? "ACCEPTED" : "REFUSED");
	return TRUE;
}

void
vacate_client(rid)
	resource_id_t rid;
{
	int sock;
	XDR xdr, *xdrs;
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return;

	dprintf(D_FULLDEBUG, "vacate_client %s...\n", client);
	if ((sock = do_connect(client, "condor_schedd", 0)) < 0)
	{
		dprintf(D_ALWAYS, "Can't connect to schedd\n");
	}
	else
	{
		xdrs = xdr_Init(&sock, &xdr);
		if(!snd_int(xdrs, VACATE_SERVICE, FALSE))
		{
			dprintf(D_ALWAYS, "Can't send VACATE_SERVICE command\n");
		}
		else if(!snd_string(xdrs, rip->r_capab, TRUE))
		{
			dprintf(D_ALWAYS, "Can't send capability\n");
		}
		xdr_destroy(xdrs);
		close(sock);
	}

	/* send VACATE_SERVICE and capability to accountant */
	if (!AccountantHost)
	{
		free(client);
		client = NULL;
		free(rip->r_capab);
		rip->r_capab = NULL;
		rip->r_claimed = FALSE;
		dprintf(D_FULLDEBUG, "Done vacate_client\n");
		return;
	}
	if ((sock = do_connect(AccountantHost, "condor_accountant", ACCOUNTANT_PORT)) < 0)
	{
		dprintf(D_ALWAYS, "Couldn't connect to accountant\n");
	}
	else
	{
		xdrs = xdr_Init(&sock, &xdr);
		if(!snd_int(xdrs, VACATE_SERVICE, FALSE))
		{
			dprintf(D_ALWAYS, "Can't send VACATE_SERVICE command\n");
		}
		else if(!snd_string(xdrs, rip->r_capab, TRUE))
		{
			dprintf(D_ALWAYS, "Can't send capability\n");
		}
		xdr_destroy(xdrs);
		close(sock);
	}
	free(rip->r_capab);
	rip->r_capab = NULL;
	rip->r_claimed = FALSE;
	dprintf(D_ALWAYS, "Done vacating client %s\n", client);
	free(client);
	client = NULL;
}
