#include "utilities.h"

#include "sha1-c/sha1.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>

/*

Establish a TCP listener

 */

int establishListener( const char* address, int port, Listener* l )
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

    char port_s[10];

    // Initialize the Listener structure
    if( l->address )
        free( l->address );
    l->address = malloc( strlen( address )+1 );
    strcpy( l->address, address );
    l->port = port; 
    l->socket = -1;

	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    sprintf( port_s, "%d", port );

	if ((status = getaddrinfo(address,
							  port_s,
							  &hints,
							  &servinfo)) != 0) {
		return 1;
	}

	sock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if( sock == -1)
		{
			freeaddrinfo(servinfo);
			return 2;
		}	


	// Clear up any old binds on the port
    // lose the pesky "Address already in use" error message
    yes = 1;
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		freeaddrinfo(servinfo);	
		return 3;
	} 


	results = bind( sock, servinfo->ai_addr, servinfo->ai_addrlen);
	if( results == -1)
		{
			freeaddrinfo(servinfo);	
			return 4;
		}

	if( port == 0 )
		{
				// We grabbed a random port, so now we determine what it is
			addr_size = sizeof( localsock_name );
			getsockname( sock, &localsock_name, &addr_size );
			if( localsock_name.sa_family == AF_INET)
				l->port = ntohs( ((struct sockaddr_in*)(&localsock_name))->sin_port); 
			if( localsock_name.sa_family == AF_INET6 )
				l->port = ntohs( ((struct sockaddr_in6*)(&localsock_name))->sin6_port); 
		}
	
	results = listen( sock, 1 ); // Using a backlog of 1 for this simple server
	if( results == -1)
		{
			freeaddrinfo(servinfo);
			return 5;
		}	

    l->socket = sock;
	//We fill in a ServerRecord so we don't need this anymore.
	freeaddrinfo(servinfo);

	return 0;
}

/*

   Accept incoming Connection from Listener

*/

int acceptConnection( Listener* l, Connection* c, int timeout )
{

	int results;
    int flags;

	struct sockaddr_storage client_addr;
    socklen_t addr_size;	
	fd_set accept_set;
	struct timeval timeout_tv;
	unsigned long int max_wait;

    // Initialize the Connection structure
    if( c->address )
        free( c->address );
    c->address = malloc( 512 );
    c->port = 0; 
    c->socket = -1;


    flags = fcntl(l->socket, F_GETFL, 0);
	//fcntl(l->socket, F_SETFL, flags | O_NONBLOCK);

    if( timeout == -1 )
        max_wait = 10000;
    else
        max_wait = timeout;

	results = 0;

	FD_ZERO(&accept_set);
	FD_SET( l->socket, &accept_set );
	timeout_tv.tv_sec = max_wait;
	timeout_tv.tv_usec = 0;

	results = select(l->socket+1, &accept_set,
                     NULL, NULL, &timeout_tv);
			

	if( results == 0 )
		{
			return -1;
		}

    if( results == -1 )
        {
            return errno;
        }

	addr_size = sizeof( client_addr );
	results = accept( l->socket,
					  (struct sockaddr *)&client_addr,
					  &addr_size );

	if( results == -1)
		{
			return errno;
		}	

    flags = fcntl(l->socket, F_GETFL, 0);
	//fcntl(l->socket, F_SETFL, flags ^ O_NONBLOCK);

	
	c->socket = results;

    getAddressPort( &client_addr, c->address, &c->port );

	return 0;
}




/*

Establish a TCP connection to a remote service

*/

int makeConnection( const char* address, int port, Connection* c )
{

    int status;
    status = startConnection(  address, port, c);
    if( status == 0 )
        return 0;

    if( status > 0 )
        return status;

    status = finishConnection( c, -1, 1);

    return status;
}


/*

  Initiate a non-blocking connection start

*/
int startConnection( const char* address, int port, Connection* c)
{
	int	flags;
	int status;	
	int sock;
	struct addrinfo hints;
	struct addrinfo *servinfo;  // will point to the results
    char port_s[10];


    // Initialize the Connection structure
    if( c->address )
        free( c->address );
    c->address = malloc( strlen( address )+1 );
    strcpy( c->address, address );
    c->port = port; 
    c->socket = -1;


    // Establish the connection

	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    sprintf( port_s, "%d", port );
	if ((status = getaddrinfo(address,
							  port_s,
							  &hints,
							  &servinfo)) != 0) {
		return errno;
	}

	sock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if( sock == -1)
		{
			freeaddrinfo(servinfo);
			return errno;
		}	

    c->socket = sock;

	flags = fcntl(c->socket, F_GETFL, 0);
	//fcntl(c->socket, F_SETFL, flags | O_NONBLOCK);

	if ( (status = connect(sock, servinfo->ai_addr, servinfo->ai_addrlen)) < 0)
        {
		if (errno != EINPROGRESS)
            {
                freeaddrinfo(servinfo);
                return(errno);
            }
        else
            {
                freeaddrinfo(servinfo);
                return(-1);
            }
        }

    // Connection done now
    return 0;
}



/*

  Attempt to finish a previous non-blocking connection start

*/
int finishConnection( Connection* c, int timeout, int final)
{
    struct timeval timeout_tv;
    fd_set 	wset, rset;
    int status;
    unsigned int len;
	int flags;

	FD_ZERO(&rset);
	FD_SET(c->socket, &rset);
	wset = rset;
	timeout_tv.tv_sec = timeout;
	timeout_tv.tv_usec = 0;

	if ( (status = select(c->socket+1, &rset, &wset, NULL,
					 timeout<0 ? &timeout_tv : NULL)) == 0) {
        if(final)
            close(c->socket);		/* final timeout */

		errno = ETIMEDOUT;
		return(-1);
	}

	if (FD_ISSET(c->socket, &rset) || FD_ISSET(c->socket, &wset)) {
		len = sizeof(status);
		if (getsockopt(c->socket, SOL_SOCKET, SO_ERROR, &status, &len) < 0)
			return(-1);			/* Solaris pending error */
	} else
		return(-2);

	flags = fcntl(c->socket, F_GETFL, 0);
	//fcntl(c->socket, F_SETFL, flags ^ O_NONBLOCK);

	if (status) {
		close(c->socket);		/* just in case */
		errno = status;
		return(errno);
	}

    return(0);
}






/*

   makeUDPEndpoint

*/
int makeUDPEndpoint( const char* address, int port, Endpoint* e )
{

	int sockfd;
    struct addrinfo hints, *servinfo;
    int rv;
	int yes;
 	int results;
    char port_s[10];

    // Initialize the Endpoint structure
    if( e->address )
        free( e->address );
    e->address = malloc( strlen( address )+1 );
    strcpy( e->address, address );
    e->port = port; 
    e->socket = -1;



	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;


    sprintf( port_s, "%d", port );
    if ((rv = getaddrinfo( address,
						   port_s,
						   &hints, &servinfo)) != 0)
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return -1;
		}

	sockfd = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if( sockfd == -1)
		{
			fprintf( stderr, "Error on creating UDP socket: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);
			return -1;
		}	


	// Clear up any old binds on the port
    // lose the pesky "Address already in use" error message
    yes = 1;
	if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
		{
			fprintf( stderr, "Error on clearing old port bindings: %s\n", strerror(errno));
			freeaddrinfo(servinfo);	
			return -1;
		} 
	

	results = bind( sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
	if( results == -1)
		{
			fprintf( stderr, "Error on binding UDP socket: %s\n", strerror(errno));
			freeaddrinfo(servinfo);	
			return -1;
		}
	
	e->socket = sockfd;
	//We fill in a ServerRecord so we don't need this anymore.
	freeaddrinfo(servinfo);
    
    return 0;
}


/*

   readUDPEndpoint

*/
int readUDPEndpoint( Endpoint* e, void* buffer, int len,
                     char* address, int* port, int timeout)
{

	int rv;
    struct sockaddr_storage from;
    socklen_t fromlen;
	fd_set accept_set;
	struct timeval timeout_tv;
	unsigned long int total_time_waited;
	unsigned long int max_wait;
	unsigned long int next_wait;
	int results;
	int total_recvd;
	int terminate;

	total_time_waited = 0;
	results = 0;
	total_recvd = 0;
	terminate = 0;

	memset( buffer, 0, len );

    FD_ZERO(&accept_set);
    FD_SET( e->socket, &accept_set );
    next_wait = max_wait - total_time_waited;
    timeout_tv.tv_sec = timeout;
    timeout_tv.tv_usec = 0;
    results = select(e->socket+1, &accept_set, NULL, NULL, &timeout_tv);
	
    if( results )
        {
            fromlen = sizeof( from );
            rv = recvfrom(e->socket,
                          buffer,
                          len,
                          0,
                          (struct sockaddr*)&from,
                          &fromlen);

            getAddressPort( &from, address, port );   
            fprintf( stderr, "d[%s]  ->   %d\n", address, strlen(address) );
            return total_recvd;
        }
    else
        {
            return 0;
        }



}


/*

   writeUDPEndpoint

*/
int writeUDPEndpoint( Endpoint* e, void* buffer, int len,
                     const char* address, int port)
{
    
	int numbytes;
    struct addrinfo hints, *servinfo;
	int rv;
    char port_s[10];

    sprintf( port_s, "%d", port );

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
	if ((rv = getaddrinfo( address,
						   port_s,
						   &hints, &servinfo)) != 0)
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return -1;
		}
    
    
    
    
	if ((numbytes = sendto(e->socket,
						   buffer, len,
                           0,
						   servinfo->ai_addr,
						   servinfo->ai_addrlen)) == -1)
		{
			freeaddrinfo(servinfo);
			return -1;
		}
    else
        {
            return numbytes;
        }
    

}



/*


  getAddressPort


 */
void getAddressPort( struct sockaddr_storage* ss, char* address, int* port )
{

	struct sockaddr_in* caddr_ip4;
	struct sockaddr_in6* caddr_ip6;

    fprintf( stderr, "a%d -> [%d] [%d]\n", ss->ss_family, AF_INET, AF_INET6 );

	// Test if the client is using a IP4 address
	if( ss->ss_family == AF_INET )
		{
			caddr_ip4 = (struct sockaddr_in*)(ss);

			// Allocate enough space for an IP4 address
			//address = (char*) malloc( INET_ADDRSTRLEN+1 ); 
			memset( address, 0, INET_ADDRSTRLEN+1 );
			
			inet_ntop( AF_INET, &(caddr_ip4->sin_addr),
					   address, INET_ADDRSTRLEN);

            fprintf( stderr, "b[%s]  ->   %d\n", address, strlen(address) );
            
			// extract the address from the raw value
			*port = ntohs( caddr_ip4->sin_port);// port number
            return;
		}

		// Test if the client is using a IP6 address
	if( ss->ss_family == AF_INET6 )
		{
			caddr_ip6 = (struct sockaddr_in6*)(ss);

			// Allocate enough space for an IP6 address
			//address = (char*) malloc( INET6_ADDRSTRLEN+1 ); 
			memset( address, 0, INET6_ADDRSTRLEN+1 );
			
			inet_ntop( AF_INET6, &(caddr_ip6->sin6_addr),
					   address, INET6_ADDRSTRLEN);


            fprintf( stderr, "c[%s]  ->   %d\n", address, strlen(address) );

            if( strlen( address ) == 0 )
                {
                    //Set to a default value if ntop 
                    //returns a blank result.
                    // ( I believe this means its localhost )
                    sprintf( address, "::1/128" );
                }

			// 10 chars will hold a IP6 port
			*port = ntohs( caddr_ip6->sin6_port); // port number
            return;
		}

}




/*
  open_file
  
  Opens a given filename and returns a FileRecord pointer.

  Returns NULL if the file is unable to be opened.

 */
int open_file( char* filename, FileRecord* record)
{
	SHA1Context hashRecord;
	unsigned char chunk;
	unsigned int len;
    char* str_ptr;	
	char* old_str_ptr;
	

	memset( record, 0, sizeof( FileRecord ));


		// Extract the base filename from the filepath given
	old_str_ptr = str_ptr = filename;
	while( str_ptr )
		{
			old_str_ptr = str_ptr;
			str_ptr = strchr( str_ptr+1, '/' );
			if( str_ptr )
				str_ptr = str_ptr + 1;
		}
	str_ptr = old_str_ptr;
    
	record->filename = (char*)malloc( strlen(str_ptr)+1 );
    memset( record->filename, 0, strlen(str_ptr)+1 );
	strcpy( record->filename, str_ptr );

	record->path = (char*)malloc( str_ptr - filename );
    memset( record->path, 0, str_ptr - filename );
	memcpy( record->path, filename, str_ptr - filename );


	record->fp = fopen( filename, "rb" );
	if( record->fp == NULL )
		{
			return 1;
		}


		// Calculate the hash code for the file data
	SHA1Reset( &hashRecord );
	chunk = fgetc( record->fp );
	while( !feof(record->fp) )
		{
			SHA1Input( &hashRecord, &chunk, 1);			
			chunk = fgetc( record->fp );
		}


	len = SHA1Result( &hashRecord );
	if( len == 0 )
		fprintf(stderr, "Error while constructing hash. Hash is not right!\n" );

	rewind( record->fp);
	memcpy( record->hash, hashRecord.Message_Digest, 5*sizeof(int) );
	
	fseek( record->fp, 0, SEEK_END );
	record->file_size = ftell( record->fp );
	rewind( record->fp);

	record->chunk_size = 100; // Really bad hack
	
		// Count how many chunks there will be.
	record->num_chunks = record->file_size/record->chunk_size;
	if( record->file_size % record->chunk_size != 0 )
	    record->num_chunks = record->num_chunks + 1;


	return 0;
}


//sendall - taken from the Beej guide to socket programming
//http://beej.us/guide/bgnet/output/html/multipage/advanced.html#sendall

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n = 0;

	if( buf == NULL )
		return -1;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 



// readOOB
int readOOB(int s, void* buf, int len, int flags,
			struct sockaddr *from, socklen_t *fromlen,
			int timeout, char termChar)
{
	int rv;
	fd_set accept_set;
	struct timeval timeout_tv;
	unsigned long int total_time_waited;
	unsigned long int max_wait;
	unsigned long int next_wait;
	int results;
	int total_recvd;
	int terminate;

	max_wait = timeout * 1000000;
	fcntl(s, F_SETFL, O_NONBLOCK);
	total_time_waited = 0;
	results = 0;
	total_recvd = 0;
	terminate = 0;
	
	memset( buf, 0, len );

	while( total_recvd < len && total_time_waited < max_wait && !terminate )
		{
			printf( "TICK %d %ld %d\n", total_recvd, total_time_waited, terminate );
			FD_ZERO(&accept_set);
			FD_SET( s, &accept_set );
			next_wait = max_wait - total_time_waited;
			timeout_tv.tv_sec = next_wait / 1000000;
			timeout_tv.tv_usec = next_wait % 1000000;
			printf( "\tNext Wait: %ld %ld\n" , next_wait/1000000, next_wait%1000000 );
			results = select(s+1, &accept_set, NULL, NULL, &timeout_tv);
			
			if( results )
				{
					rv = recvfrom(s,
								  buf+total_recvd,
								  len-total_recvd,
								  0,
								  from,
								  fromlen);

					if( strchr( buf+total_recvd, termChar) )
						terminate = 1;

					total_recvd += rv;
				}
			total_time_waited += next_wait - (timeout_tv.tv_sec * 1000000 + timeout_tv.tv_usec);
		}

	return total_recvd;
}


/*

closeConnection

*/
int closeConnection( Connection* c )
{
    if( c == NULL )
        return 0;
    
    if( c->address )
        free( c->address );

    return 0;
}
