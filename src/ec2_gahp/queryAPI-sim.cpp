#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include <signal.h>

#include <unistd.h>

#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <algorithm>

typedef std::map< std::string, std::string > AttributeValueMap;
typedef bool (*ActionHandler)( AttributeValueMap & avm, std::string & reply, unsigned requestNumber );
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
 *      CreateTags
 */

std::string xmlTag( const char * tagName, const std::string & tagValue ) {
    std::ostringstream os;
    os << "<" << tagName << ">" << tagValue << "</" << tagName << ">";
    return os.str();
}

typedef struct Group_t {
    std::string groupID;

    Group_t() { }
    Group_t( const std::string & gID ) : groupID( gID ) { }
} Group;
typedef std::map< std::string, Group > NameToGroupMap;

typedef struct Keypair_t {
    std::string keyName;
    std::string fingerprint;
    std::string privateKey;

    Keypair_t() { }
    Keypair_t( const std::string & kn,
               const std::string & fp,
               const std::string & pk ) : keyName( kn ),
               fingerprint( fp ), privateKey( pk ) { }
} Keypair;
typedef std::map< std::string, Keypair > NameToKeypairMap;

std::ostream & operator << ( std::ostream & os, const Keypair & kp ) {
    os << "<item>" << std::endl;
    os << xmlTag( "keyName", kp.keyName ) << std::endl;
    os << xmlTag( "keyFingerprint", kp.fingerprint ) << std::endl;
    os << "</item" << std::endl;
    return os;
}

class InstanceState {
    public:
        enum InstanceStatus {
            PENDING = 0,
            RUNNING = 16,
            SHUTTING_DOWN = 32,
            TERMINATED = 48,
            STOPPING = 64,
            STOPPED = 80,
            INVALID = -1
        };

        InstanceState() : statusCode( INVALID ) { }
        InstanceState( InstanceStatus state ) : statusCode( state ) { }
        InstanceState( const std::string & state ) {
            if( state == "pending" ) { this->statusCode = PENDING; }
            else if( state == "running" ) { this->statusCode = RUNNING; }
            else if( state == "shutting-down" ) { this->statusCode = SHUTTING_DOWN; }
            else if( state == "terminated" ) { this->statusCode = TERMINATED; }
            else if( state == "stopping" ) { this->statusCode = STOPPING; }
            else if( state == "stopped" ) { this->statusCode = STOPPED; }
            else { this->statusCode = INVALID; }
        }

        void progress() {
            if( rand() % 2 ) {
                switch( this->statusCode ) {
                    case PENDING:
                        this->statusCode = RUNNING;
                        break;
                    case RUNNING:
                        this->statusCode = SHUTTING_DOWN;
                        break;
                    case SHUTTING_DOWN:
                        this->statusCode = TERMINATED;
                        break;
                    case TERMINATED:
                        this->statusCode = TERMINATED;
                        break;
                    case STOPPING:
                        this->statusCode = STOPPED;
                        break;
                    case STOPPED:
                        this->statusCode = STOPPED;
                        break;
                    case INVALID:
                        fprintf( stderr, "Attempting to progress the INVALID state implies a bug.\n" );
                        break;
                }
            }
        }

        InstanceStatus code() const { return this->statusCode; }

        std::string name() const {
            switch( this->statusCode ) {
                case PENDING:
                    return "pending";
                case RUNNING:
                    return "running";
                case SHUTTING_DOWN:
                    return "shutting-down";
                case TERMINATED:
                    return "terminated";
                case STOPPING:
                    return "stopping";
                case STOPPED:
                    return "stopped";
                case INVALID:
                default:
                    return "invalid";
            }
        }

    private:
        InstanceStatus statusCode;
};

std::ostream & operator << ( std::ostream & os, const InstanceState & is ) {
    os << "<code>" << is.code() << "</code>" << std::endl;
    os << xmlTag( "name", is.name() ) << std::endl;
    return os;
}

typedef struct Instance_t {
    std::string instanceID;

    std::string imageID;
    std::string privateDNSName;
    std::string publicDNSName;
    std::string instanceType;
    std::string keyName;

    InstanceState instanceState;
    std::vector< std::string > groupNames;

    Instance_t() { }
    Instance_t( const std::string & inID, const std::string & imID,
                const std::string & piDN, const std::string & puDN,
                const std::string & iT,   const std::string & iS,
                const std::string & kN,
                const std::vector< std::string > & gN ) : instanceID(inID),
                imageID(imID), privateDNSName( piDN ), publicDNSName( puDN ),
                instanceType( iT ), keyName( kN ), instanceState( iS ),
                groupNames( gN ) { }
} Instance;
typedef std::map< std::string, Instance > InstanceIDToInstanceMap;

std::ostream & operator << ( std::ostream & os, const Instance & i ) {
    os << "<item>" << std::endl;
        os << xmlTag( "instanceId", i.instanceID ) << std::endl;
        os << xmlTag( "imageId", i.imageID ) << std::endl;
        os << xmlTag( "privateDnsName", i.privateDNSName ) << std::endl;
        os << xmlTag( "dnsName", i.publicDNSName ) << std::endl;
        os << xmlTag( "instanceType", i.instanceType ) << std::endl;
        os << "<instanceState>" << std::endl;
            os << i.instanceState;
        os << "</instanceState>" << std::endl;
        if( ! i.keyName.empty() ) { os << xmlTag( "keyName", i.keyName ); }
    os << "</item>" << std::endl;
    return os;
}

typedef struct {
    NameToKeypairMap keypairs;
    NameToGroupMap groups;
    InstanceIDToInstanceMap instances;
} User;
typedef std::map< std::string, User > AccessKeyIDToUserMap;

// Global.  Eww.
AccessKeyIDToUserMap users;

// Since we don't support CreateSecurityGroup, insert a few for testing.
void registerTestUsers() {
    users[ "1" ] = User();
    users[ "1" ].groups[ "sg-name-1" ] = Group( "sg-name-1" );
    users[ "1" ].groups[ "sg-name-2" ] = Group( "sg-name-2" );
    users[ "1" ].groups[ "sg-name-3" ] = Group( "sg-name-3" );

    // The Amazon EC2 ID we actually use for testing.
    users[ "1681MPTCV7E5BF6TWE02" ] = User();

    // The Magellan (Eucalyptus) ID we actually use for testing.
    users[ "HOq1jQmeoswWXoJTq44DbTuD8jchkWfXmbsEg" ] = User();
}

// The compiler seems unwilling or unable to infer return types; GregT
// speculated that this might be because templated functions are in their
// own namespace, but getObject<>() doesn't infer the return type, either.
template< class V, class T, class K > V getObject( const T & map, const K & key, bool & found ) {
    typename T::const_iterator ci = map.find( key );
    if( map.end() == ci ) {
        found = false;
        return V();
    }
    found = true;
    return ci->second;
}

template< class V, class T, class K > V & getReference( T & map, const K & key, bool & found ) {
    static V dummyValue = V();

    typename T::iterator ci = map.find( key );
    if( map.end() == ci ) {
        found = false;
        return dummyValue;
    }
    found = true;
    return map[ key ];
}

User & validateAndAcquireUser( AttributeValueMap & avm, std::string & userID, std::string & reply, bool & found ) {
    static User dummyUser = User();

    userID = getObject< std::string >( avm, "AWSAccessKeyId", found );
    if( (! found) || userID.empty() ) {
        fprintf( stderr, "Failed to find user ID in query.\n" );
        reply = "Required parameter 'AWSAccessKeyId' missing or empty.\n";
        found = false;
        return dummyUser;
    }

    User & user = getReference< User >( users, userID, found );
    if( ! found ) {
        std::ostringstream os;
        os << "Unknown AWSAccessKeyId '" << userID << "'." << std::endl;
        reply = os.str();
        fprintf( stderr, "%s", reply.c_str() );
        found = false;
        return dummyUser;
    }

    found = true;
    return user;
}

bool handleRunInstances( AttributeValueMap & avm, std::string & reply, unsigned requestNumber ) {
    // fprintf( stdout, "handleRunInstances()\n" );

    bool found = false;
    std::string userID;
    User & user = validateAndAcquireUser( avm, userID, reply, found );
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

    if( minCount != "1" || maxCount != "1" ) {
        fprintf( stderr, "The simulator presently only supports starting one instance at a time.\n" );
        reply = "Counts must be '1' at present.\n";
        return false;
    }

    std::string keyName = getObject< std::string >( avm, "KeyName", found );
    if( ! keyName.empty() ) {
        Keypair kp = getObject< Keypair >( user.keypairs, keyName, found );
        if( ! found ) {
            std::ostringstream error;
            error << "The requested keypair, '" << keyName << "', does not exist." << std::endl;
            reply = error.str();
            fprintf( stderr, "%s", reply.c_str() );
            return false;
        }
    }

    // We presently assume all instanceTypes are valid.
    std::string instanceType = getObject< std::string >( avm, "InstanceType", found );
    if( instanceType.empty() ) {
        instanceType = "m1.small";
    }

    std::string userData = getObject< std::string >( avm, "UserData", found );
    if( ! userData.empty() ) {
        // We should validate that the user data is properly Base64-encoded.
    }

    std::vector< std::string > groupNames;
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

        NameToGroupMap::const_iterator ci = user.groups.find( sgName );
        if( ci != user.groups.end() ) {
            groupNames.push_back( sgName );
        } else {
            std::ostringstream error;
            error << "Group '" << sgName << "' does not exist.\n";
            reply = error.str();
            fprintf( stderr, "%s", reply.c_str() );
            return false;
        }
    }

    if( groupNames.empty() ) {
        groupNames.emplace_back("default" );
    }

    // Create the (unique) corresponding Instance.
    char edh[] = "12345678";
    snprintf( edh, sizeof( edh ), "%.8x", (unsigned)user.instances.size() );

    std::string instanceID = "i-"; instanceID += edh;
    std::string privateDNSName = "private.dns."; privateDNSName += edh;
    std::string publicDNSName = "public.dns."; publicDNSName += edh;
    std::string instanceState = "pending";
    user.instances[ instanceID ] = Instance( instanceID, imageID, privateDNSName, publicDNSName, instanceType, instanceState, keyName, groupNames );
    std::string reservationID = "r-"; reservationID += edh;

    // Construct the XML reply.
    std::ostringstream xml;
    xml << "<RunInstancesResponse xmlns=\"http://ec2.amazonaws.com/doc/2010-11-15/\">" << std::endl;

    char rID[] = "1234";
    snprintf( rID, sizeof( rID ), "%.4x", requestNumber );
    std::string requestID = rID;

    xml << "<requestId>" << requestID << "</requestId>" << std::endl;
    xml << "<reservationId>" << reservationID << "</reservationId>" << std::endl;
    xml << "<ownerId>" << userID << "</ownerId>" << std::endl;

    xml << "<groupSet>" << std::endl;
        for( unsigned i = 0; i < groupNames.size(); ++i ) {
            xml << xmlTag( "item", xmlTag( "groupId", groupNames[i] ) ) << std::endl;
        }
    xml << "</groupSet>" << std::endl;

    xml << "<instancesSet>" << std::endl;
        xml << user.instances[instanceID];
    xml << "</instancesSet>" << std::endl;

    xml << "</RunInstancesResponse>" << std::endl;

    reply = xml.str();
    return true;
}

bool handleTerminateInstances( AttributeValueMap & avm, std::string & reply, unsigned requestNumber ) {
    // fprintf( stdout, "handleTerminateInstances()\n" );

    bool found = false;
    std::string userID;
    User & user = validateAndAcquireUser( avm, userID, reply, found );
    if( ! found ) { return false; }

    // The ec2_gahp will never request more than one.
    std::string instanceID = getObject< std::string >( avm, "InstanceId.1", found );
    if( (! found) || instanceID.empty() ) {
        fprintf( stderr, "Failed to find instanceID in query.\n" );
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

    // Change the state of the instance.
    InstanceState oldInstanceState = instance.instanceState;
    InstanceState newInstanceState = InstanceState( "terminated" );
    user.instances[ instanceID ].instanceState = newInstanceState;

    // Construct the XML reply.
    std::ostringstream xml;
    xml << "<TerminateInstancesResponse xmlns=\"http://ec2.amazonaws.com/doc/2010-11-15/\">" << std::endl;

    char rID[] = "1234";
    snprintf( rID, sizeof( rID ), "%.4x", requestNumber );
    std::string requestID = rID;

    xml << xmlTag( "requestId", rID ) << std::endl;
    xml << "<instancesSet>" << std::endl;
        xml << xmlTag( "instanceId", instanceID ) << std::endl;
        xml << "<currentState>" << std::endl;
            xml << newInstanceState;
        xml << "</currentState>" << std::endl;
        xml << "<previousState>" << std::endl;
            xml << oldInstanceState;
        xml << "</previousState>" << std::endl;
    xml << "</instancesSet>" << std::endl;
    xml << "</TerminateInstancesResponse>" << std::endl;

    reply = xml.str();
    return true;
}

bool handleDescribeInstances( AttributeValueMap & avm, std::string & reply, unsigned requestNumber ) {
    // fprintf( stdout, "handleDescribeInstances()\n" );

    bool found = false;
    std::string userID;
    User & user = validateAndAcquireUser( avm, userID, reply, found );
    if( ! found ) { return false; }

    std::ostringstream xml;
    xml << "<DescribeInstancesResponse xmlns=\"http://ec2.amazonaws.com/doc/2010-11-15/\">" << std::endl;

    char rID[] = "1234";
    snprintf( rID, sizeof( rID ), "%.4x", requestNumber );
    std::string requestID = rID;

    // Progress the state all of VMs.
    for( InstanceIDToInstanceMap::iterator i = user.instances.begin(); i != user.instances.end(); ++i ) {
        user.instances[ i->first ].instanceState.progress();
    }

    // Because the ec2_gahp only requests a single VM at a time, we don't
    // maintain reservation records; instead, each VM is in its own.
    xml << xmlTag( "requestID", rID ) << std::endl;
    xml << "<reservationSet>" << std::endl;

    for( InstanceIDToInstanceMap::iterator i = user.instances.begin(); i != user.instances.end(); ++i ) {
        Instance currentInstance = i->second;
        std::string reservationID = "r-" + currentInstance.instanceID.substr( 2, 8 );

        xml << "<item>" << std::endl;

            xml << "<reservationId>" << reservationID << "</reservationId>" << std::endl;
            xml << "<ownerId>" << userID << "</ownerId>" << std::endl;

            xml << "<groupSet>" << std::endl;
                for( unsigned j = 0; j < currentInstance.groupNames.size(); ++j ) {
                    xml << xmlTag( "item", xmlTag( "groupId", currentInstance.groupNames[j] ) ) << std::endl;
                }
            xml << "</groupSet>" << std::endl;

            xml << "<instancesSet>" << std::endl;
                xml << currentInstance;
            xml << "</instancesSet>" << std::endl;

        xml << "</item>" << std::endl;
    }

    xml << "</reservationSet>" << std::endl;
    xml << "</DescribeInstancesResponse>" << std::endl;

    reply = xml.str();
    return true;
}

bool handleCreateTags( AttributeValueMap & avm, std::string & reply, unsigned requestNumber ) {

    bool found = false;
    std::string userID;
    validateAndAcquireUser( avm, userID, reply, found );
    if( ! found ) { return false; }

    std::string tagName = getObject< std::string >( avm, "Tag.0.Key", found );
    if( (! found) || tagName.empty() ) {
        fprintf( stderr, "Failed to find TagName in query.\n" );
        reply = "Required parameter TagName missing or empty.\n";
        return false;
    }

    std::string tagValue = getObject< std::string >( avm, "Tag.0.Value", found );
    if( (! found) || tagValue.empty() ) {
        fprintf( stderr, "Failed to find tagValue in query.\n" );
        reply = "Required parameter tagValue missing or empty.\n";
        return false;
    }

    char rID[] = "1234";
    snprintf( rID, sizeof( rID ), "%.4x", requestNumber );
    std::string requestID = rID;

    std::ostringstream xml;
    xml << "<CreateTagsResponse xmlns=\"http://ec2.amazonaws.com/doc/2010-11-15/\">" << std::endl;
    xml << xmlTag( "requestId", rID ) << std::endl;
    xml << xmlTag( "tagName", tagName ) << std::endl;
    xml << xmlTag( "tagValue", tagValue ) << std::endl;
    xml << "/<CreateTagsResponse>" << std::endl;

    reply = xml.str();
    return true;
}

bool handleCreateKeyPair( AttributeValueMap & avm, std::string & reply, unsigned requestNumber ) {
    // fprintf( stdout, "handleCreateKeyPair()\n" );

    bool found = false;
    std::string userID;
    User & user = validateAndAcquireUser( avm, userID, reply, found );
    if( ! found ) { return false; }

    std::string keyName = getObject< std::string >( avm, "KeyName", found );
    if( (! found) || keyName.empty() ) {
        fprintf( stderr, "Failed to find KeyName in query.\n" );
        reply = "Required parameter KeyName missing or empty.\n";
        return false;
    }

    Keypair kp( keyName, "key-fingerprint", "private-key" );
    user.keypairs[ keyName ] = kp;

    char rID[] = "1234";
    snprintf( rID, sizeof( rID ), "%.4x", requestNumber );
    std::string requestID = rID;

    std::ostringstream xml;
    xml << "<CreateKeyPairResponse xmlns=\"http://ec2.amazonaws.com/doc/2010-11-15/\">" << std::endl;
    xml << xmlTag( "requestId", rID ) << std::endl;
    xml << xmlTag( "keyName", kp.keyName ) << std::endl;
    xml << xmlTag( "keyFingerprint", kp.fingerprint ) << std::endl;
    xml << xmlTag( "keyMaterial", kp.privateKey ) << std::endl;
    xml << "/<CreateKeyPairResponse>" << std::endl;

    reply = xml.str();
    return true;
}

bool handleDeleteKeyPair( AttributeValueMap & avm, std::string & reply, unsigned requestNumber ) {
    // fprintf( stdout, "handleDeleteKeyPair()\n" );

    bool found = false;
    std::string userID;
    User & user = validateAndAcquireUser( avm, userID, reply, found );
    if( ! found ) { return false; }

    std::string keyName = getObject< std::string >( avm, "KeyName", found );
    if( (! found) || keyName.empty() ) {
        fprintf( stderr, "Failed to find KeyName in query.\n" );
        reply = "Required parameter KeyName missing or empty.\n";
        return false;
    }

    Keypair kp = getObject< Keypair >( user.keypairs, keyName, found );
    if( found ) {
        user.keypairs.erase( keyName );
    } else {
        // Amazon's EC2 blithely succeeds when this happen, but we'll
        // let the developer(s) know that something's probably amiss.
        std::ostringstream error;
        error << "Keypair named '" << keyName << "' does not exist." << std::endl;
        reply = error.str();
        fprintf( stderr, "%s", reply.c_str() );
    }

    char rID[] = "1234";
    snprintf( rID, sizeof( rID ), "%.4x", requestNumber );
    std::string requestID = rID;

    std::ostringstream xml;
    xml << "<DeleteKeyPairsResponse xmlns=\"http://ec2.amazonaws.com/doc/2010-11-15/\">" << std::endl;
    xml << xmlTag( "requestId", requestID ) << std::endl;
    xml << xmlTag( "return", "true" ) << std::endl;
    xml << "</DeleteKeyPairsResponse>" << std::endl;

    reply = xml.str();
    return true;
}

bool handleDescribeKeyPairs( AttributeValueMap & avm, std::string & reply, unsigned requestNumber ) {
    // fprintf( stdout, "handleDescribeKeyPairs()\n" );

    bool found = false;
    std::string userID;
    User & user = validateAndAcquireUser( avm, userID, reply, found );
    if( ! found ) { return false; }

    char rID[] = "1234";
    snprintf( rID, sizeof( rID ), "%.4x", requestNumber );
    std::string requestID = rID;

    std::ostringstream xml;

    xml << "<DescribeKeyPairsResponse xmlns=\"http://ec2.amazonaws.com/doc/2010-11-15/\">" << std::endl;
    xml << "<keySet>" << std::endl;
        for( NameToKeypairMap::const_iterator i = user.keypairs.begin(); i != user.keypairs.end(); ++i ) {
            xml << i->second;
        }
    xml << "</keySet>" << std::endl;
    xml << "</DescribeKeyPairsResponse>" << std::endl;

    reply = xml.str();
    return true;
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
    simulatorActions[ "CreateTags" ] = & handleCreateTags;
}

// m/^Authorization: <algorithm> Credential=<accessKeyID>/....
bool extractAccessKeyID( const std::string & header, std::string & accessKeyID ) {
	std::string::size_type i = header.find( "\r\nAuthorization: " );
	if( std::string::npos == i ) {
		fprintf( stderr, "Malformed header '%s': contains no Authorization header; failing.\n", header.c_str() );
		return false;
	}

	std::string::size_type j = header.find( "\r\n", i + 2 );
	if( std::string::npos == j ) {
        fprintf( stderr, "Malformed request '%s': Authorization field not CR/LF terminated; failing.\n", header.c_str() );
		return false;
	}

	std::string credential = "Credential=";
	std::string::size_type k = header.find( credential, i );
	if( std::string::npos == j || k >= j ) {
		fprintf( stderr, "Malformed header '%s': Authorization field does not contain Credential.\n", header.c_str() );
		return false;
	}

	std::string::size_type l = header.find( "/", k + credential.length() );
	if( std::string::npos == j || l >= j ) {
		fprintf( stderr, "Malformed header '%s': Credential does not contain '/'.\n", header.c_str() );
		return false;
	}

	accessKeyID = header.substr( k + credential.length(), l - (k + credential.length()) );
	// fprintf( stderr, "Found accessKeyID '%s'\n", accessKeyID.c_str() );
	return true;
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

// m/^Content-Length: \d+/
bool extractContentLength( const std::string & header, long int & contentLength ) {
	std::string::size_type i = header.find( "\r\nContent-Length: " );
	if( std::string::npos != i ) {
		std::string clString = header.substr( i + 18 );
		// fprintf( stderr, "clString = '%s'\n", clString.c_str() );

		std::string::size_type j = clString.find( "\r\n" );
		if( std::string::npos == j ) {
			fprintf( stderr, "Malformed request '%s': no newline after Content-Length; failing.\n", header.c_str() );
			return false;
		}

		char * endptr = NULL;
		clString = clString.substr( 0, j );
		// fprintf( stderr, "clString = '%s'\n", clString.c_str() );
		long int candidateLength = strtol( clString.c_str(), & endptr, 10 );
		if( * endptr == '\0' ) { contentLength = candidateLength; return true; }

		fprintf( stderr, "Failed to convert '%s' into a number.\n", clString.c_str() );
		return false;
	} else {
		// Could be a GET request.
		return false;
	}
}

// m/^GET <URL> HTTP/
// m/^POST <URL> HTTP/
bool extractURL( const std::string & request, std::string & URL ) {
    if( request.find( "POST " ) != 0 ) {
        fprintf( stderr, "Malformed request '%s': did not begin with 'POST '; failing.\n", request.c_str() );
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
bool validateSignature( std::string & /* method */,
                        const std::string & /* host */,
                        const std::string & /* URL */,
                        const AttributeValueMap & /* queryParameters */ ) {
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

std::string handleRequest( const std::string & header, std::string & request ) {
    static unsigned requestCount = -1; ++requestCount;

	std::string content;
	long int contentLength = 0;
	// fprintf( stderr, "Handling header '%s'\n", header.c_str() );
	if( extractContentLength( header, contentLength ) ) {
		// fprintf( stderr, "content = '%s'\n", content.c_str() );
		content = request.substr( 0, contentLength );
		request = request.substr( contentLength );
		// fprintf( stderr, "content = '%s', request = '%s'\n", content.c_str(), request.c_str() );
	}

    std::string URL;
    if( ! extractURL( header, URL ) ) {
        return constructReply( "HTTP/1.1 400 Bad Request", "" );
    }

    std::string host;
    if( ! extractHost( header, host ) ) {
        return constructReply( "HTTP/1.1 400 Bad Request", "" );
    }
    std::transform( host.begin(), host.end(), host.begin(), & tolower );

    // fprintf( stderr, "DEBUG: found 'http://%s%s'\n", host.c_str(), URL.c_str() );

    //
    // Rather than rewrite the parser, just change URL to the appropriate
    // string from the body of the PUT.
    //
    URL = "?" + content;

    AttributeValueMap queryParameters;
    std::string::size_type i = URL.find( "?" );
    while( i < URL.size() ) {
        // Properly encoded URLs will only have ampersands between
        // the key-value pairs, and equals between keys and values.
        std::string::size_type equalsIdx = URL.find( "=", i + 1 );
        if( std::string::npos == equalsIdx ) {
            std::ostringstream error;
            error << "Malformed URL '" << URL << "': attribute without value; failing" << std::endl;
            fprintf( stderr, "%s", error.str().c_str() );
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


	//
	// Quick hack to handle signature V4.
	//
	std::string accessKeyID = queryParameters[ "AWSAccessKeyId" ];
	if( accessKeyID.empty() ) {
    	if( ! extractAccessKeyID( header, accessKeyID ) ) {
	        return constructReply( "HTTP/1.1 400 Bad Request", "Unable to extract accessKeyID" );
    	}
		queryParameters[ "AWSAccessKeyId" ] = accessKeyID;
	}


    std::string method = "POST";
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
        fprintf( stderr, "%s", error.str().c_str() );
        return constructReply( "HTTP/1.1 404 Not Found", error.str() );
    }

    if( (*(ci->second))( queryParameters, response, requestCount ) ) {
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
                // fprintf( stdout, "read() interrupted, retrying.\n" );
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

        // A given HTTP header is terminated by a blank line.
        std::string::size_type index = request.find( "\r\n\r\n" );
        while( index != std::string::npos ) {
            std::string header = request.substr( 0, index + 4 );
            request = request.substr( index + 4 );
            // if( ! request.empty() ) { fprintf( stderr, "DEBUG: request remainder '%s'\n", request.c_str() ); }

            // fprintf( stderr, "DEBUG: handling request '%s'\n", firstRequest.c_str() );
            std::string reply = handleRequest( header, request );
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

int printLeakSummary() {
    int rv = 0;

    AccessKeyIDToUserMap::const_iterator u = users.begin();
    for( ; u != users.end(); ++u ) {
        // You can leak keys...
        if( ! u->second.keypairs.empty() ) {
            fprintf( stdout, "User '%s' leaked keys.\n", u->first.c_str() );
            rv = 6;
        }

        // But since the ec2_gahp doesn't manage groups, you can't leak them.

        // We never garbage-collect terminated instances (since EC2 leaves
        // them around for some time as well), but we should verify that
        // they're all terminated.
        InstanceIDToInstanceMap::const_iterator i = u->second.instances.begin();
        for( ; i != u->second.instances.end(); ++i ) {
            if( i->second.instanceState.code() != InstanceState::TERMINATED ) {
                fprintf( stdout, "Instance '%s' in in state '%s', not terminated.\n", i->second.instanceID.c_str(), i->second.instanceState.name().c_str() );
                rv = 6;
            }
        }
    }

    if( rv == 0 ) { fprintf( stdout, "No leaks detected.\n" ); }
    return rv;
}

void sigterm( int sig ) {
    fprintf( stdout, "Caught signal %d, exiting.\n", sig );
    exit( printLeakSummary() );
}

int main( int /* argc */, char ** /* argv */ ) {

    struct sigaction sa;
    sa.sa_handler = & sigterm;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = 0;

    int rv = sigaction( SIGTERM, & sa, NULL );
    if( rv != 0 ) {
        fprintf( stderr, "sigaction() failed (%d): '%s', aborting.\n", errno, strerror( errno ) );
        exit( 5 );
    }

    rv = sigaction( SIGINT, & sa, NULL );
    if( rv != 0 ) {
        fprintf( stderr, "sigaction() failed (%d): '%s', aborting.\n", errno, strerror( errno ) );
        exit( 5 );
    }

    int listenSocket = socket( PF_INET, SOCK_STREAM, 0 );
    if( listenSocket == -1 ) {
        fprintf( stderr, "socket() failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 1 );
    }

/*
 * listen()ing to an unbound port selects a random free port automagically.
 *
    struct sockaddr_in listenAddr;
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons( 21737 );
    listenAddr.sin_addr.s_addr = INADDR_ANY;
    rv = bind( listenSocket, (struct sockaddr *)(& listenAddr), sizeof( listenAddr ) );
    if( rv != 0 ) {
        fprintf( stderr, "bind() failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 2 );
    }
*/

    rv = listen( listenSocket, 0 );
    if( rv != 0 ) {
        fprintf( stderr, "listen() failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 3 );
    }

    struct sockaddr_in listenAddr;
    socklen_t listenAddrLen = sizeof( struct sockaddr_in );
    rv = getsockname( listenSocket, (struct sockaddr *)(& listenAddr), & listenAddrLen );
    if( rv != 0 ) {
        fprintf( stderr, "getsockname() failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 5 );
    }
    if( listenAddrLen != sizeof( struct sockaddr_in ) ) {
        fprintf( stderr, "getsockname() returned bogus address, aborting.\n" );
        exit( 6 );
    }
    fprintf( stdout, "listen_port = %d\n", ntohs( listenAddr.sin_port ) );
    rv = fflush( stdout );
    if( rv != 0 ) {
        fprintf( stderr, "fflush( stdout ) failed (%d): '%s'; aborting.\n", errno, strerror( errno ) );
        exit( 7 );
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
