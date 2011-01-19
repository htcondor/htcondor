#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include <string>

/*
 * The GAHP uses the following functions from the Query API:
 *
 *      RunInstances
 *      TerminateInstances
 *      DescribeInstances
 *      CreateKeyPair
 *      DeleteKeyPair
 *      DescribeKeyPairs
 *
 * We also attempt to verify that the request was properly signed.
 *
 */

std::string handleRequest( const std::string & request ) {
    // FIXME.
    std::string reply = "HTTP/1.1 200 OK\r\n"
                      . "
}

void handleConnection( int sockfd ) {
    ssize_t bytesRead;
    char buffer[1024+1];
    std::string request;

    while( 1 ) {
        bytesRead = read( sockfd, (void *) buffer, 1024 );
        
        if( bytesRead == -1 ) {
            if( errno == EINTR ) {
                fprintf( stderr, "read() interrupted, retrying.\n" );
                continue;
            }
            fprintf( stderr, "read() failed (%d): '%s'\n", errno, strerror( errno ) );
            return;
        }
        
        if( bytesRead == 0 ) {
            close( sockfd );
            return;
        }
        
        buffer[bytesRead] = '\0';
        request += buffer;
        // fprintf( stderr, "DEBUG: request is now '%s'.\n", request.c_str() );
        
        // A given HTTP request is terminated by blank line.
        // FIXME: we need to check if we got more than one HTTP request
        // in this read().
        std::string::size_type index = request.find( "\r\n\r\n" );
        if( index != std::string::npos ) {
            std::string firstRequest = request.substr( 0, index + 4 );
            request = request.substr( index + 4 );
            if( ! request.empty() ) {
                fprintf( stderr, "DEBUG: request remainder '%s'\n", request.c_str() );
            }
            
            fprintf( stderr, "DEBUG: handling request '%s'\n", firstRequest.c_str() );
            std::string reply = handleRequest( firstRequest );
            if( ! reply.empty() ) {
                fprintf( stderr, "DEBUG: writing reply '%s'\n", reply.c_str() );
                ssize_t totalBytesWritten = 0;
                ssize_t bytesToWrite = reply.size();
                const char * outBuf = reply.c_str();
                while( 1 ) {
                    ssize_t bytesWritten = write( sockfd,
                        outBuf + totalBytesWritten,
                        bytesToWrite - totalBytesWritten );
                    if( bytesWritten == -1 ) {
                        fprintf( stderr, "write() failed (%d), '%s'; aborting reply.\n", errno, strerror( errno ) );
                        close( sockfd );
                        return;
                    }
                    totalBytesWritten += bytesWritten;
                }
            }
        }
    }
}

int main( int argc, char ** argv ) {

    int listenSocket = socket( PF_INET, SOCK_STREAM, 0 );
    if( listenSocket == -1 ) {
        fprintf( stderr, "socket() failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 1 );
    }
    
    struct sockaddr_in listenAddr;
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons( 21737 );
    listenAddr.sin_addr.s_addr = INADDR_ANY;
    int rv = bind( listenSocket, (struct sockaddr *)(& listenAddr), sizeof( listenAddr ) );
    if( rv != 0 ) {
        fprintf( stderr, "bind() failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 2 );
    }

    rv = listen( listenSocket, 0 );
    if( rv != 0 ) {
        fprintf( stderr, "listen() failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 3 );
    }

    while( 1 ) {
        struct sockaddr_in remoteAddr;
        socklen_t raSize = sizeof( remoteAddr );
        int remoteSocket = accept( listenSocket, (struct sockaddr *)(& remoteAddr), & raSize );
        if( remoteSocket == -1 ) {
            fprintf( stderr, "accept() failed(%d): '%s'; aborting.\n", errno, strerror( errno ) );
            exit( 4 );
        }
        
        handleConnection( remoteSocket );
    }
    
    return 0;
} // end main()
