#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include <string>
#include <map>
#include <sstream>

typedef std::map< std::string, std::string > AttributeValueMap;
typedef bool (*ActionHandler)( AttributeValueMap & avm, std::string & reply );
typedef std::map< std::string, ActionHandler > ActionToHandlerMap;

/*
 * The GAHP uses the following functions from the Query API:
 *
 *      RunInstances
 *      TerminateInstances
 *      DescribeInstances
 *      CreateKeyPair
 *      DeleteKeyPair
 *      DescribeKeyPairs
 */

bool handleRunInstances( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleRunInstances()\n" );
    return false;
}

bool handleTerminateInstances( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleTerminateInstances()\n" );
    return false;
}

bool handleDescribeInstances( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleDescribeInstances()\n" );
    return false;
}

bool handleCreateKeyPair( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleCreateKeyPair()\n" );
    return false;
}

bool handleDeleteKeyPair( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleDeleteKeyPair()\n" );
    return false;
}

bool handleDescribeKeyPairs( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleDescribeKeyPairs()\n" );
    return false;
}

ActionToHandlerMap simulatorActions;

void registerAllHandlers() {
    simulatorActions[ "RunInstances" ] = & handleRunInstances;
    simulatorActions[ "TerminateInstances" ] = & handleTerminateInstances;
    simulatorActions[ "DescribeInstances" ] = & handleDescribeInstances;
    simulatorActions[ "CreateKeyPair" ] = & handleCreateKeyPair;
    simulatorActions[ "DeleteKeyPair" ] = & handleDeleteKeyPair;
    simulatorActions[ "DescribeKeyPairs" ] = & handleDescribeKeyPairs;
}

// m/^Host: <host>\r\n/
bool extractHost( const std::string & request, std::string & host ) {
    std::string::size_type i = request.find( "\r\nHost: " );
    if( std::string::npos == i ) {
        fprintf( stderr, "Malformed request '%s': contains no Host header; failing.\n", request.c_str() );
        return false;
    }

    std::string::size_type j = request.find( "\r\n", i + 2 );
    if( std::string::npos == j ) {
        fprintf( stderr, "Malformed request '%s': Host field not CR/LF terminated; failing.\n", request.c_str() );
        return false;
    }

    host = request.substr( i + 8, j - (i + 8)  );
    return true;
}

// m/^GET <URL> HTTP/
bool extractURL( const std::string & request, std::string & URL ) {
    if( request.find( "GET " ) != 0 ) {
        fprintf( stderr, "Malformed request '%s': did not begin with 'GET '; failing.\n", request.c_str() );
        return false;
    }
    
    URL = request.substr( 4, request.find( "HTTP" ) - 5 );
    return true;
}

/*
 * The most fragile part of the EC2 GAHP is the query signing, so
 * we'd like to validate it.  See the comments in amazonCommands.cpp
 * for more details.
 *
 * This function is presently a stub because I haven't solved the
 * problem of key distribution between the tester and the simulator.
 */
bool validateSignature( std::string & method,
                        const std::string & host,
                        const std::string & URL,
                        const AttributeValueMap & queryParameters ) {
    return true;
}

std::string handleRequest( const std::string & request ) {
    std::string URL;
    if( ! extractURL( request, URL ) ) {
        return "HTTP/1.1 400 Bad Request\r\n";
    }
    
    std::string host;
    if( ! extractHost( request, host ) ) {
        return "HTTP/1.1 400 Bad Request\r\n";
    }
    std::transform( host.begin(), host.end(), host.begin(), & tolower );
    
    // fprintf( stderr, "DEBUG: found 'http://%s%s'\n", host.c_str(), URL.c_str() );
    
    AttributeValueMap queryParameters;
    std::string::size_type i = URL.find( "?" );
    while( i < URL.size() ) {
        // Properly encoded URLs will only have ampersands between
        // the key-value pairs, and equals between keys and values.
        std::string::size_type equalsIdx = URL.find( "=", i + 1 );
        if( std::string::npos == equalsIdx ) {
            fprintf( stderr, "Malformed URL '%s': attribute without value; failing.\n", URL.c_str() );
            return "HTTP/1.1 400 Bad Request\r\n";
        }        
        std::string::size_type ampersandIdx = URL.find( "&", i + 1 );
        if( std::string::npos == ampersandIdx ) {
            ampersandIdx = URL.size();
        }

        std::string key = URL.substr( i + 1, equalsIdx - (i + 1) );
        std::string value = URL.substr( equalsIdx + 1, ampersandIdx - (equalsIdx + 1 ) );
        // fprintf( stderr, "DEBUG: key = '%s', value = '%s'\n", key.c_str(), value.c_str() );
        queryParameters[ key ] = value;
        
        i = ampersandIdx;
    }

    std::string method = "GET";
    if( ! validateSignature( method, host, URL, queryParameters ) ) {
            return "HTTP/1.1 401 Unauthorized\r\n";
    }


    std::string action = queryParameters[ "Action" ];
    if( action.empty() ) {
        return "HTTP/1.1 400 Bad Request\r\n";
    }        

    std::ostringstream reply;
    std::string response;
    ActionToHandlerMap::const_iterator ci = simulatorActions.find( action );
    if( simulatorActions.end() == ci ) {
        fprintf( stderr, "Action '%s' not found.\n", action.c_str() );
        return "HTTP/1.1 404 Action Not Found\r\n";
    }
    
    if( (*(ci->second))( queryParameters, response ) ) {
        reply << "HTTP/1.1 200 OK\r\n";
    } else {
        reply << "HTTP/1.1 406 Not Acceptable\r\n";
    }
    
    // FIXME: With which headers do EC2 and Eucalytpus reply?
    reply << "Content-Length: " << response.size() << "\r\n";
    reply << "Content-Type: text/xml\r\n";
    reply << "\r\n";

    return reply.str();
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
            // if( ! request.empty() ) { fprintf( stderr, "DEBUG: request remainder '%s'\n", request.c_str() ); }
            
            // fprintf( stderr, "DEBUG: handling request '%s'\n", firstRequest.c_str() );
            std::string reply = handleRequest( firstRequest );
            if( ! reply.empty() ) {
                // fprintf( stderr, "DEBUG: writing reply '%s'\n", reply.c_str() );
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

// FIXME: sigterm handler to dump diagnostics and gracefully exit.  See
// the condor_amazon simulator.
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

    registerAllHandlers();

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
