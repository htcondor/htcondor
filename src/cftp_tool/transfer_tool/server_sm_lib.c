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
#include <limits.h>

// For disk quota checks
#include <sys/statvfs.h>

#include "server_sm_lib.h"


const int ANNOUNCE_INTERVAL = 5; // As long as we don't have a client, we should announce presence every X seconds

void run_server( ServerArguments* arg)
{
    int status;
    int finished;
    int max_sock;
    int client_used;
    long int max_wait_time;
    char* payload;

    Listener          listener;
    TransferArguments targs;
    Endpoint          OOB_ep;

	struct timeval timeout_tv;
	fd_set accept_set;

    memset( &targs, 0, sizeof( TransferArguments));
    memset( &listener, 0, sizeof( Listener ));
    memset( &OOB_ep, 0, sizeof( Endpoint ));

    status = confirm_arguments( arg );
    if( status != 0 )
        return;

    client_used = 0;

    targs.debug      = arg->debug;
    targs.verbose    = arg->verbose;
    targs.quota      = arg->quota;
    max_wait_time    = arg->itimeout; 
    payload = NULL;

    targs.store_path = (char*)malloc( strlen( arg->tpath)+1 );
    memcpy( targs.store_path, arg->tpath, strlen( arg->tpath)+1 );


    status = makeUDPEndpoint( arg->lhost, arg->lport, &OOB_ep );
    if( status == 0 )
        {
            if( arg->verbose)
                fprintf( stdout, "Out-of-Band channel established.\n");
        }
    else
        {
            fprintf( stdout, "Failed to establish Out-of-Band channel. %s. Quiting.\n", strerror(status));
            return;
        }

    status = establishListener( arg->lhost, arg->lport, &listener );
    if( status == 0 )
        {
            if( arg->verbose)
                fprintf( stdout, "Service established. Waiting for client.\n");
        }
    else
        {
            fprintf( stdout, "Failed to establish service. %s. Quiting.\n", strerror(status));
            return;
        }

    
    finished = 0;
    while( ! finished )
        {

            timeout_tv.tv_sec = ANNOUNCE_INTERVAL;
            timeout_tv.tv_usec = 0;
            
            FD_ZERO(&accept_set);
            FD_SET( listener.socket, &accept_set );
            FD_SET( OOB_ep.socket, &accept_set );

            max_sock = listener.socket > OOB_ep.socket ? listener.socket:OOB_ep.socket;

            status = select( max_sock+1,
                             &accept_set,
                             NULL,
                             NULL,
                             &timeout_tv);

            if( status > 0 )
                {
                    if( FD_ISSET( OOB_ep.socket, &accept_set ) )
                        handle_OOB_communications( &OOB_ep );

                    if( FD_ISSET( listener.socket, &accept_set ) )
                        {
                            handle_client( &listener, &targs, &client_used, &payload );

                            if( client_used )
                                {
                                    if( !arg->single_client )
                                        client_used = 0;
                                    else if ( arg->ptimeout > 0 )
                                        max_wait_time = arg->ptimeout;
                                }
                                    
                        }
                }
            else
                {

                    if( arg->announce )
                        {                     
                            handle_announce( &OOB_ep,
                                             arg->ahost,
                                             arg->aport,
                                             arg->lhost,
                                             arg->lport,
                                             arg->uuid,
                                             payload,
                                             max_wait_time );
                        }


                    // We are using initial timeout mode
                    if( arg->itimeout > 0 && !client_used )
                        {
                            // Need better timing control here.
                            max_wait_time -= ANNOUNCE_INTERVAL;
                            
                            if( max_wait_time <= 0 )
                                return;

                            printf( "Continuing to wait for %d secs.\n", max_wait_time );
                        }

                    // We are using post timeout mode
                    if( arg->ptimeout > 0 && client_used )
                        {
                            // Need better timing control here.
                            max_wait_time -= ANNOUNCE_INTERVAL;
                            
                            if( max_wait_time <= 0 )
                                return;

                            printf( "Continuing to wait for %d secs.\n", max_wait_time );
                        }
                    

                }
            

        }

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
			if( statvfs( arg->tpath, &fiData ) < 0 )
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

   handle_announce 

*/
int handle_announce( Endpoint* e,
                     const char* ahost,
                     int aport,
                     const char* lhost,
                     int lport,
                     const char* uuid,
                     const char* payload,
                     int ttl )
{
    char buf[1024];

    memset( buf, 0, 1024);

    if( !payload )
        {
            // Version 1 of announce message ( pre-transfer )
            
            sprintf( buf, "%s %d %s\n", lhost, lport, uuid );
            return writeUDPEndpoint( e, 
                                     buf, strlen( buf ),
                                     ahost, aport );
            
        }
    else
        {
            // Version 2 of announce message ( post-transfer )
            
            sprintf( buf, "%s %d %s %s %d\n", lhost, lport, uuid,
                                              payload,  ttl);
            return writeUDPEndpoint( e, 
                                     buf, strlen( buf ),
                                     ahost, aport );
        }
         
}






/*

handle_OOB_communications


 */
int handle_OOB_communications( Endpoint* e  )
{

	int byte_count;
	char buf[1024];
	char command[100];
	char arg1[100];
	char arg2[100];
	char arg3[100];
	char arg4[100];
	char arg5[100];
	int num_args;

    char from_addr[512];
    int  from_port;

	fprintf( stderr,  "OOB CHANNEL ACTIVATED\n" );

		// Check to see if we are actually getting a command
	memset( buf, 0, 1024);
    memset( from_addr, 0, 512);
	byte_count = readUDPEndpoint(e,
                                 buf, 1024,
                                 from_addr, &from_port,
								 -1);

    if( byte_count == -1 )
        printf( "Something has gone horribly wrong here.\n" );

    // If this is not a command we will return
	if( strncmp( buf, "COMMAND:", 8 )) 
		{
			fprintf( stderr,  "OOB CHANNEL TERMINATED: No Command Phrase\n" );
			return 0;
		}


	fprintf( stderr,  "Received (%s:%d) Command: %s...", from_addr, from_port, buf+8 );
	num_args = sscanf( buf+8, "%s %s %s %s %s %s", command,
					   arg1, arg2, arg3, arg4, arg5 );

	if( strcmp( command, "SENDTO" ) == 0 && num_args == 3)
		{
			fprintf( stderr,  "Executing.\n");
			
			
			//transfer_file( arg1,
			//			   arg2,
			//			   state->local_file.filename );
		}
	else if( strcmp( command, "PING" ) == 0 && num_args == 1)
        {
            memset( buf, 0, 1024);
            sprintf( buf, "PONG\n" );
            writeUDPEndpoint( e, 
                              buf, strlen( buf ),
                              from_addr, from_port );
        }
    else
		{
			fprintf( stderr,  "Not Recognised.\n" );
		}
			

	fprintf( stderr,  "OOB CHANNEL TERMINATED\n" );
    return 0;
}




/*

handle_client


 */
int handle_client( Listener* l, TransferArguments* targs, int* client_used, char** payload )
{

    int status;
    ReceiveResults    results;
    memset( &results, 0, sizeof( ReceiveResults));

    status = acceptConnection( l, &targs->local_socket, -1 );

    if( status == 0 )
        {
            if( targs->verbose)
                fprintf( stdout, "Client accepted. Beginning Transfer.\n");
        }
    else
        {
            fprintf( stdout, "Failed to aquire client. %s.\n", strerror(status));
            return -1;
        }

    if( (*client_used) )
        {
            closeConnection( &targs->local_socket );
            return 0;
        }

    receive_file( targs, &results);

    if( results.success == 1 )
        {
            (*client_used) = 1;
            if( !(*payload) )
                {
                    (*payload) = malloc( strlen( results.filename)+1 );
                    memset( (*payload), 0, strlen(results.filename)+1 );
                    strcpy( (*payload), results.filename );
                }
        }

    return 0;
}
