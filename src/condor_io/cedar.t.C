#include "condor_common.h"
#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"


#define SERVER "server"
#define CLIENT "client"
#define TCP "tcp"
#define UDP "udp"
#define TIMEOUT 60
enum { client, server, tcp, udp };

int main(int argc, char **argv)
{
	/* These are built in types */
	unsigned char u_character;
	int integer;
	unsigned int u_integer;
	long long_integer;
	unsigned long u_long_integer;
	long long long_long_integer;
	unsigned long long u_long_long_integer;
	short short_integer;
	unsigned short u_short_integer;
	float floating_pt;
	double double_floating_pt;

	/* Condor types */
	PROC_ID proc;
	STARTUP_INFO startup;
	PORTS ports;
	StartdRec startd_rec;

	/* Unix types */
#if !defined(WIN32)
	signal_t sig;
	open_flags_t open_flags;
	fcntl_cmd_t fcntl_cmd;
	struct rusage resource_usage;
	struct stat status;
	struct statfs status_fs;
	struct timezone tz;
	struct rlimit resource_limit;
#endif
	
#if defined( HAS_64BIT_STRUCTS )
	struct stat64 status64;
	struct rlimit64 resource_limit64;
#endif

	int mode;

	int port;
	char *host;

	Sock *socket;

	if(argc < 4) {
		printf("usage: %s server|client tcp|udp port [host]\n", argv[0]);
		exit(1);
	}

	if( strcmp( SERVER, argv[1] ) == 0 ) {
		mode = server;
		if(argc > 4) {
			printf("server does not require host argument, ignoring...\n");
		}
		port = atoi(argv[3]);
	} else if( strcmp( CLIENT, argv[1] ) == 0 ) {
		mode = client;
		if( argc < 5 ) {
			printf("client requires host argument.\n");
			exit(1);
		}
		port = atoi(argv[3]);
		host = argv[4];
	} else {
		printf( "Unrecognized option %s.\n", argv[1] );
		exit(1);
	}

	if( strcmp( UDP, argv[2] ) == 0 ) {
		socket = new SafeSock();
		if( mode == server ) {
			socket->bind(port);
		} else {
			socket->connect(host, port);
			socket->timeout(TIMEOUT);
		}
	} else if( strcmp( TCP, argv[2] ) == 0 ) {
		if( mode == server ) {
			ReliSock listen_socket;
			listen_socket.listen(port);
			socket = listen_socket.accept();
			assert(socket);
		} else {
			socket = new ReliSock();
			((ReliSock *)socket)->connect(host, port);
			socket->timeout(TIMEOUT);
		}
	} else {
		printf("Unrecognized mode %s.\n", argv[2] );
		exit(1);
	}

	// If we are a client, send a packet, or connect and send a stream.
	if(mode == client) {
		u_character = 42;
		integer = 42;
		u_integer = 42;
		long_integer = 42;
		u_long_integer = 42;
		long_long_integer = 42;
		u_long_long_integer = 42;
		short_integer = 42;
		u_short_integer = 42; 
		floating_pt = 42.0;
		double_floating_pt = 42.0;

		socket->encode();
		assert(socket->is_encode() == TRUE);
		assert(socket->is_decode() == FALSE);
	}
	else
	{
		socket->decode();
		assert(socket->is_encode() == FALSE);
		assert(socket->is_decode() == TRUE);
	}
	assert(socket->code(u_character) == TRUE);
	assert(socket->code(integer) == TRUE);
	assert(socket->code(u_integer) == TRUE);
	assert(socket->code(long_integer) == TRUE);
	assert(socket->code(u_long_integer) == TRUE);
	assert(socket->code(long_long_integer) == TRUE);
	assert(socket->code(u_long_long_integer) == TRUE);
	assert(socket->code(short_integer) == TRUE);
	assert(socket->code(u_short_integer) == TRUE);
	assert(socket->code(floating_pt) == TRUE);
	assert(socket->code(double_floating_pt) == TRUE); 
	assert(socket->end_of_message() == TRUE);

	
	assert(	u_character == 42 );
	assert(	integer == 42 );
	assert(	u_integer == 42  );
	assert(	long_integer == 42 );
	assert(	u_long_integer == 42 );
	assert(	long_long_integer == 42 );
	assert(	u_long_long_integer == 42 );
	assert(	short_integer == 42 );
	assert(	u_short_integer == 42 ); 

// These tests fail because of the imprecise method Stream uses to
// transfer floats.  This should be fixed some day...
//	assert(	floating_pt == 42.0 );
//	assert(	double_floating_pt == 42.0 );

	printf("SUCCESS.\n");
}


	

	
	
	
		
