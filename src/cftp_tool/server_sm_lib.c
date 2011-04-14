#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// For disk quota checks
#include <sys/statvfs.h>

#include "server_sm_lib.h"

#include "server_sm/discovery.h"
#include "server_sm/negotiation.h"
#include "server_sm/transfer.h"
#include "server_sm/teardown.h"
#include "server_sm/error.h"


const int STATE_NUM = 100; // Will need to change this later
const int ANNOUNCE_INTERVAL = 5; // As long as we don't have a client, we should announce presence every X seconds

void run_server( ServerArguments* arg)
{

	StateAction SM_States[STATE_NUM];

	ServerRecord master_server;
	ServerState  session_state;

		//-------------- Initialize SM States -----------------
	
	SM_States[S_UNKNOWN_ERROR]              = &State_UnknownError;
	SM_States[S_SEND_SESSION_CLOSE]         = &State_SendSessionClose;

	SM_States[S_RECV_SESSION_PARAMETERS]    = &State_ReceiveSessionParameters;
	SM_States[S_CHECK_SESSION_PARAMETERS]   = &State_CheckSessionParameters;
	SM_States[S_ACK_SESSION_PARAMETERS]     = &State_AcknowledgeSessionParameters;
	SM_States[S_RECV_CLIENT_READY]          = &State_ReceiveClientReady;

	SM_States[S_RECV_DATA_BLOCK]            = &State_ReceiveDataBlock;
	SM_States[S_ACK_DATA_BLOCK]             = &State_AcknowledgeDataBlock;

	SM_States[S_RECV_FILE_FINISH]           = &State_ReceiveFileFinish;
	SM_States[S_ACK_FILE_FINISH]            = &State_AcknowledgeFileFinish;

      //------------------------------------------------------- 

	memset( &master_server, 0, sizeof( ServerRecord ));
	memset( &session_state, 0, sizeof( ServerState ));

	master_server.server_name = (char*)malloc(256);
	master_server.server_port = (char*)malloc(16);
	
	memcpy( master_server.server_name, arg->lhost, 256);
	memcpy( master_server.server_port, arg->lport, 16);

	master_server.server_socket = -1;

	if( confirm_arguments( arg ) != 0)
	  {
	    return;
	  }

	session_state.arguments = arg;
	session_state.data_buffer = NULL;

	start_server( &master_server );
	
	if( master_server.server_socket == -1 )
		{
				//Error has happened, TODO: handle here
			return;
		}
	

	handle_client( &master_server, &session_state );

	if( session_state.last_error )
		{
				//Error happened while accepting client, TODO: handle here
			return;
		}



		//Prep structure for initial state
	session_state.state = S_RECV_SESSION_PARAMETERS;	
	session_state.phase = NEGOTIATION;
	session_state.last_error = 0;
	memset( session_state.error_string, 0, 256 );


	while( run_state_machine( SM_States, &session_state )==0 )
		{
				// Do nothing here, we wait till the SM kills us.
		}

		// We may want some clean up based on
	    // the final condition of session_state.
	


		// Clean up allocated memory

	if( master_server.server_name )	
		free( master_server.server_name );

	if( master_server.server_port )	
		free( master_server.server_port );

	if( session_state.client_info.client_name)
		free( session_state.client_info.client_name );

	if( session_state.client_info.client_port )
		free( session_state.client_info.client_port );

	if( session_state.data_buffer )
		free( session_state.data_buffer );

	if( session_state.local_file.filename )
		free( session_state.local_file.filename );

	if( session_state.session_parameters )
		free( session_state.session_parameters );

	return;
}



/*

  confirm_arguments


 */

int confirm_arguments( ServerArguments* arg)
{
	struct statvfs fiData;	
	long int disk_space;

		// Do we have a quota to check for?
	if( arg->quota != -1 )
		{
			if( statvfs( "./", &fiData ) < 0 )
				{
					fprintf( stderr, "Unable to confirm quota. Stat failed for './' \n" );
					return -1;
				}

			disk_space = fiData.f_bsize*fiData.f_bavail; 
			
			if( arg->debug )
				fprintf( stderr, "Diskspace: %ld, Quota: %ld \n", disk_space, arg->quota*1024l );

			if( disk_space < arg->quota*1024l )
				{
					fprintf( stderr, "Unable to satisfy quota. Insufficent disk space available. \n");
					return -1;
				}		
		}



		// Check for transfer path	

	if( access(arg->tpath, R_OK | W_OK ) != 0 )
		{
			fprintf(stderr, "Unable to access transfer path with READ/WRITE permissions.\n");
			fprintf(stderr, "Error: %s\n", strerror( errno ) );
			return -1;
		}



	return 0;
}



/*

  start_server


 */
void start_server( ServerRecord* master_server )
{

//Really rusty on socket api - refered to Beej's guide for this
//http://beej.us/guide/bgnet/output/html/multipage/syscalls.html	

	int status;	
	int sock;
	int results;
	int yes;
	struct addrinfo hints;
	struct addrinfo *servinfo;  // will point to the results
	struct sockaddr localsock_name;
	socklen_t addr_size;

	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	if ((status = getaddrinfo(master_server->server_name,
							  master_server->server_port,
							  &hints,
							  &servinfo)) != 0) {
		fprintf(stderr, "Unable to resolve server: %s\n", gai_strerror(status));
		return;
	}

	sock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if( sock == -1)
		{
			fprintf( stderr, "Error on creating socket: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);
			return;
		}	


	// Clear up any old binds on the port
    // lose the pesky "Address already in use" error message
    yes = 1;
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		fprintf( stderr, "Error on clearing old port bindings: %s\n", strerror( errno ) );
		freeaddrinfo(servinfo);	
		return;
	} 


	results = bind( sock, servinfo->ai_addr, servinfo->ai_addrlen);
	if( results == -1)
		{
			fprintf( stderr, "Error on binding socket: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);	
			return;
		}

	if( strcmp( master_server->server_port , "0" ) == 0 )
		{
				// We grabbed a random port, so now we determine what it is
			addr_size = sizeof( localsock_name );
			getsockname( sock, &localsock_name, &addr_size );
			memset( master_server->server_port, 0, 16 );
			if( localsock_name.sa_family == AF_INET)
					// extract the address from the raw value
				sprintf( master_server->server_port, "%d", 
						 ntohs( ((struct sockaddr_in*)(&localsock_name))->sin_port) ); 
			if( localsock_name.sa_family == AF_INET6 )
				sprintf( master_server->server_port, "%d", 
						 ntohs( ((struct sockaddr_in6*)(&localsock_name))->sin6_port) ); 
		}

	
	results = listen( sock, 1 ); // Using a backlog of 1 for this simple server
	if( results == -1)
		{
			fprintf( stderr, "Error on listening: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);
			return;
		}	

	master_server->server_socket = sock;
	//We fill in a ServerRecord so we don't need this anymore.
	freeaddrinfo(servinfo);

	return;
}


/*



announce_server

*/

void announce_server( ServerRecord* master_server, ServerState* state )
{
	
	int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
	char message[512];

	VERBOSE( "Announcing server presence..." )

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo( state->arguments->ahost,
						   state->arguments->aport,
						   &hints, &servinfo)) != 0)
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return;
		}

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "announce server: failed to bind socket\n");
        return;
    }
	
	memset( message, 0, 512 );
	sprintf( message, "%s %s %s",
			 master_server->server_name,
			 master_server->server_port,
			 state->arguments->uuid
			 );

    if ((numbytes = sendto(sockfd, message, strlen( message ), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("announce server: sendto");
        return;
    }

    freeaddrinfo(servinfo);

    close(sockfd);

	VERBOSE( "Complete!\n\n" )

    return;

}




/*

handle_client

*/
void handle_client( ServerRecord* master_server, ServerState* state )
{

	int results;

	struct sockaddr_in* caddr_ip4;
	struct sockaddr_in6* caddr_ip6;

	struct sockaddr_storage client_addr;
    socklen_t addr_size;	
	fd_set accept_set;
	struct timeval timeout_tv;
	unsigned long int total_time_waited;
	unsigned long int max_wait;

	VERBOSE( "Waiting for client connection...\n")

	fcntl(master_server->server_socket, F_SETFL, O_NONBLOCK);
	
	total_time_waited = 0;
	if( state->arguments->itimeout == -1 )
		max_wait = ULONG_MAX;
	else
		max_wait = state->arguments->itimeout;
	results = 0;


	while( total_time_waited < max_wait && results == 0)
		{
			printf( "Total Time: %d\n", total_time_waited );
			FD_ZERO(&accept_set);
			FD_SET( master_server->server_socket, &accept_set );
			timeout_tv.tv_sec = ANNOUNCE_INTERVAL;
			timeout_tv.tv_usec = 0;

			results = select(master_server->server_socket+1, &accept_set,
							 NULL, NULL, &timeout_tv);
			
			if( state->arguments->announce )
				announce_server( master_server, state );
			
			total_time_waited += ANNOUNCE_INTERVAL;
		}

	if( results == -1 )	
		{	
			fprintf( stderr, "Error on accepting: %s\n", strerror( errno ) );
			state->last_error = 1;
			return;
		}	
	if( results == 0 )
		{
			fprintf( stderr, "No client connected within initial wait period. Exiting.\n" );
			state->last_error = 1;
			return;
		}

	addr_size = sizeof( client_addr );
	results = accept( master_server->server_socket,
					  (struct sockaddr *)&client_addr,
					  &addr_size );

	if( results == -1)
		{
			fprintf( stderr, "Error on accepting: %s\n", strerror( errno ) );
			state->last_error = 1;
			return;
		}	
	
	state->client_info.client_socket = results;
	state->client_info.client_name = NULL;
	state->client_info.client_port = NULL;

		// Test if the client is using a IP4 address
	if( client_addr.ss_family == AF_INET )
		{
			if( state->arguments->debug )
				fprintf( stderr, "Client is using IP4.\n" );

			caddr_ip4 = (struct sockaddr_in*)(&client_addr);

			// Allocate enough space for an IP4 address
			state->client_info.client_name = (char*) malloc( INET_ADDRSTRLEN ); 
			memset( state->client_info.client_name, 0, INET_ADDRSTRLEN );
			
			inet_ntop( AF_INET, &(caddr_ip4->sin_addr),
					   state->client_info.client_name, INET_ADDRSTRLEN);

			// 10 chars will hold a IP4 port
			state->client_info.client_port = (char*) malloc( 10 ); 
			memset( state->client_info.client_port, 0, 10 );
			
			// extract the address from the raw value
			sprintf( state->client_info.client_port, "%d", 
					 ntohs( caddr_ip4->sin_port) );   // port number
		}

		// Test if the client is using a IP6 address
	if( client_addr.ss_family == AF_INET6 )
		{

			if( state->arguments->debug )
				fprintf( stderr, "Client is using IP6.\n" );


			caddr_ip6 = (struct sockaddr_in6*)(&client_addr);

			// Allocate enough space for an IP6 address
			state->client_info.client_name = (char*) malloc( INET6_ADDRSTRLEN ); 
			memset( state->client_info.client_name, 0, INET6_ADDRSTRLEN );
			
			inet_ntop( AF_INET6, &(caddr_ip6->sin6_addr),
					   state->client_info.client_name, INET6_ADDRSTRLEN);

			// 10 chars will hold a IP6 port
			state->client_info.client_port = (char*) malloc( 10 ); 
			memset( state->client_info.client_port, 0, 10 );
			
			// extract the address from the raw value
			sprintf( state->client_info.client_port, "%d", 
					 ntohs( caddr_ip6->sin6_port) );   // port number
		}


	if( state->arguments->debug )
		if( state->client_info.client_name && state->client_info.client_port )
			fprintf( stderr, "The client is %s at port %s.\n\n",
					 state->client_info.client_name, state->client_info.client_port );


	VERBOSE( "Client accepted.\n\n" )
	state->last_error = 0;
	return;

}








/*


run_state_machine 

*/
int run_state_machine( StateAction* states, ServerState* state )
{
	int condition;


	if( state->state == -1 )
		return 1; // We are done here
	else	
		{
			condition = states[state->state]( state );
			recv_cftp_frame( state );
			state->state = transition_table( state, condition );
			return 0;
		}
}


/*


transition_table

*/
int transition_table( ServerState* state, int condition )
{

	switch( state->state )
		{


		case S_RECV_SESSION_PARAMETERS:
			if( state->frecv_buf.MessageType != SIF )
				{
					sprintf( state->error_string, 
							 "No frame received from client or bad frame type.");
					return S_UNKNOWN_ERROR;
				}
			else		
				return S_CHECK_SESSION_PARAMETERS;
			


		case S_CHECK_SESSION_PARAMETERS:
			if( condition == -1 ) // Something bad happened here
				return S_UNKNOWN_ERROR;

				//TODO: Need to handle parameter constraint failures

			return S_ACK_SESSION_PARAMETERS;



		case S_ACK_SESSION_PARAMETERS:
			if( state->frecv_buf.MessageType == SRF )
				return S_RECV_CLIENT_READY;
			
			if( state->frecv_buf.MessageType == SCF )
				return S_UNKNOWN_ERROR; // TODO: Create a state for client connection close

			return S_UNKNOWN_ERROR; 



		case S_RECV_CLIENT_READY:
			if( condition == -1 ) //Something bad happened with the file pointer
				return S_UNKNOWN_ERROR;
					
			return S_RECV_DATA_BLOCK;



		case S_RECV_DATA_BLOCK:
			if( state->frecv_buf.MessageType == DTF )
				return S_ACK_DATA_BLOCK;
			
			if( state->frecv_buf.MessageType == FFF )
				return S_ACK_FILE_FINISH;

			return S_UNKNOWN_ERROR;


		case S_ACK_DATA_BLOCK:
			if( condition == -1 )
				return S_UNKNOWN_ERROR;
			if( condition == 1 )
				return S_RECV_FILE_FINISH;

			return S_RECV_DATA_BLOCK;

			
		case S_RECV_FILE_FINISH:
			if( state->frecv_buf.MessageType == FFF )
				return S_ACK_FILE_FINISH;


		case S_ACK_FILE_FINISH:
			return -1; // Probably need to do more here, but thats for later

					

			
		case S_UNKNOWN_ERROR:
			return -1; // If we have completed an unknown error, we should die.



		default:
			return S_UNKNOWN_ERROR;
			break;
		}
}



/*


recv_cftp_frame


*/
int recv_cftp_frame( ServerState* state )
{
	if( !state->recv_rdy )
		return -1;
	state->recv_rdy = 0;

	memset( &state->frecv_buf, 0, sizeof( cftp_frame ) );

	recv( state->client_info.client_socket, 	
		  &state->frecv_buf,	
		  sizeof( cftp_frame ),
		  MSG_WAITALL );

	#ifdef SERVER_DEBUG
	fprintf( stderr, "[DEBUG] Read %ld bytes from client on frame_recv.\n", sizeof( cftp_frame ) );
	desc_cftp_frame(state, 0 );
	#endif

	return sizeof( cftp_frame );
}



/*


recv_data_frame


*/
int recv_data_frame( ServerState* state )
{
	if( !state->recv_rdy )
		return -1;
	state->recv_rdy = 0;

	int recv_bytes;
		//int i;

	if( state->data_buffer_size <= 0 )
		return 0;

	if( state->data_buffer )
		free( state->data_buffer );

	state->data_buffer = (char*) malloc( state->data_buffer_size );
	memset( state->data_buffer, 0, state->data_buffer_size );

	recv_bytes = recv( state->client_info.client_socket, 	
					   state->data_buffer,	
					   state->data_buffer_size,
					   MSG_WAITALL );

	#ifdef SERVER_DEBUG
	fprintf( stderr, "[DEBUG] Read %d bytes from client on data_recv.\n", recv_bytes );
		/*
    for( i = 0; i < recv_bytes; i ++ )
		{
			if( i % 32 == 0 )
				fprintf( stderr, "\n" );
			fprintf( stderr, "%c", (char)*(state->data_buffer+i));
		}
	fprintf( stderr, "\n" );
		*/
	#endif

	return recv_bytes;
}




/*


send_cftp_frame

*/
int send_cftp_frame( ServerState* state )
{
	int length;
	int status;

	if( !state->send_rdy )
		return -1;
	state->send_rdy = 0;

	length = sizeof( cftp_frame );
	status = sendall( state->client_info.client_socket,
					  (char*)(&state->fsend_buf),
					  &length );

	if( status == -1 )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send CFTP frame: %s", strerror(errno));
			return 0;
		}
	else
		{

    #ifdef SERVER_DEBUG
	fprintf( stderr, "[DEBUG] Sent %d bytes to client on frame_send.\n", length );
	desc_cftp_frame(state, 1 );
	#endif

	
	fprintf( stderr, "MARK!\n" );

			return length;
		}
}


/*


send_data_frame

*/
int send_data_frame( ServerState* state )
{
	int length;
	int status;

	if( !state->send_rdy )
		return -1;
	state->send_rdy = 0;

	length = state->data_buffer_size;
	if( length <= 0 )
		return 0;
	
	if( !state->data_buffer )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send data: Data buffer null");
			return 0;
		}

	status = sendall( state->client_info.client_socket,
					  (char*)(&state->data_buffer),
					  &length );

	if( status == -1 )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send data: %s", strerror(errno));
			return 0;
		}
	else
		{

    #ifdef SERVER_DEBUG
	fprintf( stderr, "[DEBUG] Sent %d bytes to client on data_send.\n", length );
	#endif


			return length;
		}
}




/*

desc_cftp_frame




 */
void desc_cftp_frame( ServerState* state, int send_or_recv)
{
	cftp_frame* frame;

	if( send_or_recv == 1 )
		{
			frame = &state->fsend_buf;
			fprintf( stderr, "\tMessage type of Send Frame is " );
		}
	else
		{
			frame = &state->frecv_buf;
			fprintf( stderr, "\tMessage type of Recv Frame is " );
		}
	
	switch( frame->MessageType )
		{
		case DSF:
			fprintf( stderr, "DSF frame.\n" );
			break;
		case DRF:
			fprintf( stderr, "DRF frame.\n" );
			break;
		case SIF:
			fprintf( stderr, "SIF frame.\n" );
			break;
		case SAF:
			fprintf( stderr, "SAF frame.\n" );
			break;
		case SRF:
			fprintf( stderr, "SRF frame.\n" );
			break;
		case SCF:
			fprintf( stderr, "SCF frame.\n" );
			break;
		case DTF:
			fprintf( stderr, "DTF frame.\n" );
			fprintf( stderr, "\tChunk Number: %ld\n" , ntohll(((cftp_dtf_frame*)frame)->BlockNum) );
			break;
		case DAF:
			fprintf( stderr, "DAF frame.\n" );
			fprintf( stderr, "\tChunk Number: %ld\n" , ntohll(((cftp_daf_frame*)frame)->BlockNum) );	
			break;
		case FFF:
			fprintf( stderr, "FFF frame.\n" );
			break;
		case FAF:
			fprintf( stderr, "FAF frame.\n" );
			break;
		default:
			fprintf( stderr, "Unknown frame.\n" );
			break;
		}


}
