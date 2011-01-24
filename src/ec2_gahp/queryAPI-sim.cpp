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

#include <ext/hash_map>
namespace ext = __gnu_cxx;

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

/*
 * Inexplicably, this isn't one of the standard specializations.
 * Even less explicably, you can't use namespace aliases to open a namespace.
 *
 * However, by supplying this specialization, all of the ext::hash_maps using
 * std::string as a key work without further ado.
 */

namespace __gnu_cxx {
    template<> struct hash< std::string > {
        size_t operator()( const std::string & s ) const {
            return __stl_hash_string( s.c_str() );
        }
    };
}

typedef struct {
} Group;
typedef ext::hash_map< std::string, Group > NameToGroupMap;

typedef struct {
    std::string privateKey;
} Keypair;
typedef ext::hash_map< std::string, Keypair > NameToKeypairMap;

typedef struct {
    std::string imageID;
    std::string privateDNSName;
    std::string publicDNSName;
    std::string instanceType;
    std::string instanceState;
    std::string keyName;
    
    ext::hash_map< std::string, const Group * > groups;
} Instance;
typedef ext::hash_map< std::string, Instance > InstanceIDToInstanceMap;

typedef struct {
    NameToKeypairMap keypairs;
    NameToGroupMap groups;
    InstanceIDToInstanceMap instances;    
} User;
typedef ext::hash_map< std::string, User > AccessKeyIDToUserMap;

// Global.  Eww.
AccessKeyIDToUserMap users;

void registerTestUsers() {
    users[ "1" ] = User();
    users[ "2" ] = User();
}

// The compiler seems unwilling or unable to infer return types; GregT
// speculated that this might be because templated functions are in their
// own namespace, but getObject<>() doesn't infer the return type, either.
template< class V, class T, class K > V getObject( const T & map, const K & key, bool & found ) {
    class T::const_iterator ci = map.find( key );
    if( map.end() == ci ) {
        found = false;
        return V();
    }
    found = true;
    return ci->second;
}

bool validateAndAcquireUser( AttributeValueMap & avm, std::string & userID, User & user, std::string & reply ) {
    bool found = false;

    userID = getObject< std::string >( avm, "AWSAccessKeyId", found );
    if( (!found) || userID.empty() ) {
        fprintf( stderr, "DEBUG: failed to find AWSAccessKeyId in query.\n" );
        reply = "Required parameter AWSAccessKeyId missing or empty.\n";
        return false;
    }

    user = getObject< User >( users, userID, found );
    if( ! found ) {
        fprintf( stderr, "Failed to find user identified by '%s'.\n", userID.c_str() );
        reply = "Required parameter ASWAccessKeyId invalid.\n";
        return false;
    }
    
    return true;
}

bool handleRunInstances( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleRunInstances()\n" );

    std::string userID;
    User user;
    bool found = validateAndAcquireUser( avm, userID, user, reply );
    if( ! found ) { return false; }

    // Validate the ImageId, MinCount, and MaxCount parameters, as well
    // as the optional parameters KeyName, InstanceType, SecurityGroup*,
    // and UserData.
    
    // We presently assume all imageIDs are valid.
    std::string imageID = getObject< std::string >( avm, "ImageId", found );
    if( (! found) || imageID.empty() ) {
        fprintf( stderr, "Failed to find imageID in query.\n" );
        reply = "Required parameter ImageId missing or empty.\n";
        return false;
    }

    std::string minCount = getObject< std::string >( avm, "MinCount", found );
    if( (! found) || minCount.empty() ) {
        fprintf( stderr, "Failed to find minCount in query.\n" );
        reply = "Required parameter MinCount missing or empty.\n";
        return false;
    }
    
    std::string maxCount = getObject< std::string >( avm, "MaxCount", found );
    if( (! found) || maxCount.empty() ) {
        fprintf( stderr, "Failed to find maxCount in query.\n" );
        reply = "Required parameter MaxCount missing or empty.\n";
        return false;
    }
    
    // FIXME: Verify that maxCount >= minCount.

    std::string keyName = getObject< std::string >( avm, "KeyName", found );
    if( ! keyName.empty() ) {
        // FIXME: Verify that this keypair exists.
    }

    // We presently assume all instanceTypes are valid.
    std::string instanceType = getObject< std::string >( avm, "InstanceType", found );

    std::string userData = getObject< std::string >( avm, "UserData", found );
    if( ! userData.empty() ) {
        // FIXME: Verify that the user data is Base64-encoded.
    }

    std::vector< std::string > securityGroupNames;
    for( int i = 1; ; ++i ) {
        std::ostringstream sgParameterName;
        sgParameterName << "SecurityGroup." << i;
        std::string sgName = getObject< std::string >( avm, sgParameterName.str(), found );
        if( ! found ) { break; }
        if( sgName.empty() ) {
            std::ostringstream error;
            error << "Optional parameter " << sgName << " must not be empty." << std::endl;
            reply = error.str();
            fprintf( stderr, "%s", reply.c_str() );
            return false;
        }
        
        // FIXME: Verify that the group sgName exists.
    }

    // FIXME: create the corresponding Instance.

    return true;
}

bool handleTerminateInstances( AttributeValueMap & avm, std::string & reply ) {
    fprintf( stderr, "handleTerminateInstances()\n" );

    std::string userID;
    User user;
    bool found = validateAndAcquireUser( avm, userID, user, reply );
    if( ! found ) { return false; }
    
    std::string instanceID = getObject< std::string >( avm, "InstanceId.1", found );
    if( (! found) || instanceID.empty() ) {
        fprintf( stderr, "DEBUG: failed to find instanceID in query.\n" );
        reply = "Required parameter InstanceId.1 missing or empty.\n";
        return false;
    }
    
    Instance instance = getObject< Instance >( user.instances, instanceID, found );
    if( ! found ) {
        std::ostringstream error;
        error << "Instance ID '" << instanceID << "' does not exist." << std::endl;
        reply = error.str();
        fprintf( stderr, "%s", reply.c_str() );
        return false;
    }        

    reply = "// FIXME: spam out some XML.\n";
    return true;
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

// Global.  Eww.
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
 *
 * Validates parameters:
 *      Signature,
 *      SignatureVersion
 *      SignatureMethod
 *      Timestamp
 *      Version
 * 
 */
bool validateSignature( std::string & method,
                        const std::string & host,
                        const std::string & URL,
                        const AttributeValueMap & queryParameters ) {
    return true;
}

std::string constructReply( const std::string & statusLine, const std::string & response ) {
    std::ostringstream reply;

    // The ec2_gahp doesn't ever look at the headers.
    reply << statusLine << "\r\n";
    reply << "Content-Length: " << response.size() << "\r\n";
    reply << "\r\n";
    reply << response;
    reply << "\r\n";

    return reply.str();
}

std::string handleRequest( const std::string & request ) {
    std::string URL;
    if( ! extractURL( request, URL ) ) {
        return constructReply( "HTTP/1.1 400 Bad Request", "" );
    }
    
    std::string host;
    if( ! extractHost( request, host ) ) {
        return constructReply( "HTTP/1.1 400 Bad Request", "" );
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
            std::ostringstream error;
            error << "Malformed URL '" << URL << "': attribute without value; failing" << std::endl;
            fprintf( stderr, error.str().c_str() );
            return constructReply( "HTTP/1.1 400 Bad Request", error.str() );
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
        return constructReply( "HTTP/1.1 401 Unauthorized", "Failed signature validation." );
    }

    std::string action = queryParameters[ "Action" ];
    if( action.empty() ) {
        return constructReply( "HTTP/1.1 400 Bad Request", "No action specified." );
    }
    
    std::string response;
    ActionToHandlerMap::const_iterator ci = simulatorActions.find( action );
    if( simulatorActions.end() == ci ) {
        std::ostringstream error;
        error << "Action '" << action << "' not found." << std::endl;
        fprintf( stderr, error.str().c_str() );
        return constructReply( "HTTP/1.1 404 Not Found", error.str() );
    }
    
    if( (*(ci->second))( queryParameters, response ) ) {
        return constructReply( "HTTP/1.1 200 OK", response );
    } else {
        return constructReply( "HTTP/1.1 406 Not Acceptable", response );
    }
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
        
        // A given HTTP request is terminated by a blank line.
        std::string::size_type index = request.find( "\r\n\r\n" );
        while( index != std::string::npos ) {
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
                while( totalBytesWritten != bytesToWrite ) {
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
            
            index = request.find( "\r\n\r\n" );            
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
    registerTestUsers();

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
