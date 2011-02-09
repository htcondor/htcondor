#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server_sm_lib.h"

#include "server_sm/discovery.h"
#include "server_sm/negotiation.h"
#include "server_sm/transfer.h"
#include "server_sm/teardown.h"
#include "server_sm/error.h"


const int STATE_NUM = 100; // Will need to change this later


void run_server( char* server_name,  char* server_port)
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

	master_server.server_name = server_name;
	master_server.server_port = server_port;
	master_server.server_socket = -1;

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

	return;
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

handle_client

*/
void handle_client( ServerRecord* master_server, ServerState* session_state )
{

	int results;

	struct sockaddr_in* caddr_ip4;
	struct sockaddr_in6* caddr_ip6;

	struct sockaddr_storage client_addr;
    socklen_t addr_size;	

	addr_size = sizeof( client_addr );
	results = accept( master_server->server_socket,
					  (struct sockaddr *)&client_addr,
					  &addr_size );

	if( results == -1)
		{
			fprintf( stderr, "Error on accepting: %s\n", strerror( errno ) );
			session_state->last_error = 1;
			return;
		}	
	
	session_state->client_info.client_socket = results;
	session_state->client_info.client_name = NULL;
	session_state->client_info.client_port = NULL;

		// Test if the client is using a IP4 address
	if( client_addr.ss_family == AF_INET )
		{
#ifdef SERVER_DEBUG
			fprintf( stderr, "Client is using IP4.\n" );
#endif

			caddr_ip4 = (struct sockaddr_in*)(&client_addr);

			// Allocate enough space for an IP4 address
			session_state->client_info.client_name = (char*) malloc( INET_ADDRSTRLEN ); 
			memset( session_state->client_info.client_name, 0, INET_ADDRSTRLEN );
			
			inet_ntop( AF_INET, &(caddr_ip4->sin_addr),
					   session_state->client_info.client_name, INET_ADDRSTRLEN);

			// 10 chars will hold a IP4 port
			session_state->client_info.client_port = (char*) malloc( 10 ); 
			memset( session_state->client_info.client_port, 0, 10 );
			
			// extract the address from the raw value
			sprintf( session_state->client_info.client_port, "%d", 
					 ntohs( caddr_ip4->sin_port) );   // port number
		}

		// Test if the client is using a IP6 address
	if( client_addr.ss_family == AF_INET6 )
		{
#ifdef SERVER_DEBUG
			fprintf( stderr, "Client is using IP6.\n" );
#endif

			caddr_ip6 = (struct sockaddr_in6*)(&client_addr);

			// Allocate enough space for an IP6 address
			session_state->client_info.client_name = (char*) malloc( INET6_ADDRSTRLEN ); 
			memset( session_state->client_info.client_name, 0, INET6_ADDRSTRLEN );
			
			inet_ntop( AF_INET6, &(caddr_ip6->sin6_addr),
					   session_state->client_info.client_name, INET6_ADDRSTRLEN);

			// 10 chars will hold a IP6 port
			session_state->client_info.client_port = (char*) malloc( 10 ); 
			memset( session_state->client_info.client_port, 0, 10 );
			
			// extract the address from the raw value
			sprintf( session_state->client_info.client_port, "%d", 
					 ntohs( caddr_ip6->sin6_port) );   // port number
		}

#ifdef SERVER_DEBUG
	if( session_state->client_info.client_name && session_state->client_info.client_port )
		fprintf( stderr, "The client is %s at port %s.\n",
				 session_state->client_info.client_name, session_state->client_info.client_port );
#endif


	session_state->last_error = 0;
	return;

}








/*


run_state_machine 

*/
int run_state_machine( StateAction* states, ServerState* session_state )
{
	int condition;


	if( session_state->state == -1 )
		return 1; // We are done here
	else	
		{
			condition = states[session_state->state]( session_state );
			recv_cftp_frame( session_state );
			session_state->state = transition_table( session_state, condition );
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

			return S_ACK_SESSION_PARAMETERS;



		case S_ACK_SESSION_PARAMETERS:
			return S_RECV_CLIENT_READY;

			

			
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
	fprintf( stderr, "[DEBUG] Read %d bytes from client on frame_recv.\n", sizeof( cftp_frame ) );
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
	#endif

		
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
