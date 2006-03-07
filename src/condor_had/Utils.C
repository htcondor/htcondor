#include "condor_common.h"
#include "Utils.h"
//#include "HadCommands.h"
//#include "ReplicationCommands.h"
#include "FilesOperations.h"
// for 'CHILD_ON' and 'CHILD_OFF_FAST'
#include "condor_commands.h"
// for 'getHostFromAddr' and 'getPortFromAddr'
#include "internet.h"
// for MD5
#include "condor_md.h"
#ifdef CONDOR_MD
#include <openssl/md5.h>
#endif

// for MD5 blocks computation
#define FILE_CHUNK_SIZE               (100)

extern int main_shutdown_graceful();

//#undef CONDOR_MD

MyString
utilNoParameterError( const char* parameter, const char* daemonName )
{
	MyString error;

	if( ! strcasecmp( daemonName, "HAD" ) ) {
		error.sprintf( "HAD configuration error: no %s in config file",
                        parameter );
	} else if( ! strcasecmp( daemonName, "REPLICATION" ) ) {
		error.sprintf( "Replication configuration error: no %s in config file",
                       parameter );
	} else {
		dprintf( D_ALWAYS, "utilNoParameterError no such daemon name %s\n", 
				 daemonName );
	}

	return error;
}

MyString
utilConfigurationError( const char* parameter, const char* daemonName )
{
	MyString error;

    if( ! strcasecmp( daemonName, "HAD" ) ) {
		error.sprintf("HAD configuration error: %s is not valid in config file",
					   parameter );
	} else if( ! strcasecmp( daemonName, "REPLICATION" ) ) {
    	error.sprintf( "Replication configuration error: %s is not valid "
                       "in config file", parameter );
	} else {
        dprintf( D_ALWAYS, "utilConfigurationError no such daemon name %s\n", 
                 daemonName );
    }

    return error;
}

void
utilCrucialError( const char* message )
{
    dprintf( D_ALWAYS, "%s\n", message );
    main_shutdown_graceful( );
}

void
utilCancelTimer(int& timerId)
{
     if (timerId >= 0) {                    
        daemonCore->Cancel_Timer( timerId );
        timerId = -1;
     }
}

void
utilCancelReaper(int& reaperId)
{
    if(reaperId >= 0) {
        daemonCore->Cancel_Reaper( reaperId );
        reaperId = -1;
    }
}

const char*
utilToString( int command )
{
    switch( command ) {
        case REPLICATION_LEADER_VERSION:
            return "REPLICATION_LEADER_VERSION";
        case REPLICATION_TRANSFER_FILE:
            return "REPLICATION_TRANSFER_FILE";
        case REPLICATION_NEWLY_JOINED_VERSION:
            return "REPLICATION_NEWLY_JOINED_VERSION";
        case REPLICATION_GIVING_UP_VERSION:
            return "REPLICATION_GIVING_UP_VERSION";
        case REPLICATION_SOLICIT_VERSION:
            return "REPLICATION_SOLICIT_VERSION";
        case REPLICATION_SOLICIT_VERSION_REPLY:
            return "REPLICATION_SOLICIT_VERSION_REPLY";
        case HAD_BEFORE_PASSIVE_STATE:
            return "HAD_BEFORE_PASSIVE_STATE";
        case HAD_AFTER_ELECTION_STATE:
            return "HAD_AFTER_ELECTION_STATE";
        case HAD_AFTER_LEADER_STATE:
            return "HAD_AFTER_LEADER_STATE";
        case HAD_IN_LEADER_STATE:
            return "HAD_IN_LEADER_STATE";
        case HAD_ALIVE_CMD:
            return "HAD_ALIVE_CMD";
        case HAD_SEND_ID_CMD:
            return "HAD_SEND_ID_CMD";
        case CHILD_ON:
            return "CHILD_ON";
        case CHILD_OFF_FAST:
            return "CHILD_OFF_FAST";
        default:
            return "unknown command";
    }
}

// returns allocated by 'malloc' string upon success or NULL upon failure
// to be deallocated by 'free' function only
char*
utilToSinful( char* address )
{
    int port = getPortFromAddr( address );
    
    if( port == 0 ) {
        return 0;
    }

    // getHostFromAddr returns dynamically allocated buffer
    char* hostName = getHostFromAddr( address );
    char* ipAddress = hostName;
    
    if( hostName == 0) {
      return 0;
    }

    struct in_addr sin;
    
    if( ! is_ipaddr( ipAddress, &sin ) ) {
        struct hostent *entry = gethostbyname( hostName );
        
        if( entry == 0 ) {
            free( hostName );
            
            return 0;
        }
        char* temporaryIpAddress = inet_ntoa(*((struct in_addr*)entry->h_addr));
        
        free( ipAddress );
        ipAddress = strdup( temporaryIpAddress );
    }
    MyString sinfulString;

    sinfulString.sprintf( "<%s:%d>", ipAddress, port );

    free( ipAddress );
    
    return strdup( sinfulString.GetCStr( ) );
}

int
utilAtoi(const char* string, bool* result)
{
    if ( *string == '\0' ){
        *result = false;

        return -1;
    } else {
        char* endptr;
        int   returnValue = (int) strtol( string, &endptr, 10 );

        if ( endptr != NULL && *endptr != '\0' || errno == ERANGE ){
            // any warning is considered as an error
            // and the entire string is considered invalid
            *result = false;

            return -1;
        }
        *result = true;

        return returnValue;
    }
}

void
utilClearList( StringList& list )
{
    char* element = NULL;
    list.rewind ();

    while ( (element = list.next( )) ) {
        list.deleteCurrent( );
    }
}

#ifdef CONDOR_MD
bool
utilSafePutFile( ReliSock& socket, const MyString& filePath )
{
	REPLICATION_ASSERT( filePath != "" );
	// bool       successValue = true;
	filesize_t bytes        = 0;
    dprintf( D_ALWAYS, "utilSafePutFile %s started\n", filePath.GetCStr( ) );

	ifstream file( filePath.GetCStr( ) );
	
	if( file.fail( ) ){
		dprintf( D_ALWAYS, "utilSafePutFile failed to open file %s\n",
				 filePath.GetCStr( ) ); 
		return false;
	}
	int bytesTotal = 0;
	int bytesRead  = 0;
	
	MD5_CTX  md5Context;
	// initializing MD5 structures
    MD5_Init( & md5Context );

	char* md     = ( char *) malloc( ( MAC_SIZE + 1 ) * sizeof( char ) );
	char* buffer = ( char *) malloc( ( FILE_CHUNK_SIZE + 1 ) * sizeof( char ) );

	while( file.good() ) {
		// reading next chunk of file
		file.read( buffer, FILE_CHUNK_SIZE );
		bytesRead = file.gcount();
		buffer[bytesRead] = '\0';
		// generating MAC gradually, chunk by chunk
		MD5_Update( & md5Context, buffer, bytesRead );
		//dprintf( D_ALWAYS, "utilSafePutFile buffer read %s\n", buffer );

		bytesTotal += bytesRead;
	}
	MD5_Final( (unsigned char*) md, & md5Context );
	md[MAC_SIZE] = '\0';
	free( buffer );

	dprintf( D_ALWAYS, "utilSafePutFile MAC created %s with "
                       "actual length %d, total bytes read %d\n",
           md, strlen( md ), bytesRead );
	socket.encode( );
    
	if(	! socket.code( md ) ||
        socket.put_file( &bytes, filePath.GetCStr( ) ) < 0  ||
	    ! socket.eom( ) ) {
        dprintf( D_ALWAYS,
                "utilSafePutFile unable to send file %s, MAC "
                "or to code the end of the message\n", filePath.GetCStr( ) );
        free( md );
		return false;
    }
    dprintf( D_ALWAYS, "utilSafePutFile finished successfully\n" );
    free( md );
    
	return true;
}

bool
utilSafeGetFile( ReliSock& socket, const MyString& filePath )
{
	REPLICATION_ASSERT( filePath != "" );
	filesize_t  bytes      = 0;
	char*       md      = (char *) malloc( ( MAC_SIZE + 1 ) * sizeof( char ) );

	dprintf( D_ALWAYS, "utilSafeGetFile %s started\n", filePath.GetCStr( ) );	
	socket.decode( );

    if( ! socket.code( md ) ||
	    socket.get_file( &bytes, filePath.GetCStr( ), true ) < 0 ||
	    ! socket.eom( ) ) {
        dprintf( D_ALWAYS, "utilSafeGetFile unable to get file %s, MAC or the "
							"end of the message\n", 
				 filePath.GetCStr( ) );
        free( md );
		
		return false;
    }
	md[MAC_SIZE] = '\0';
	dprintf( D_ALWAYS, "utilSafeGetFile MAC received %s\n", md );
	ifstream file( filePath.GetCStr( ) );

    if( file.fail( ) ){
        dprintf( D_ALWAYS, "utilSafeGetFile failed to open file %s\n",
                 filePath.GetCStr( ) );
		free( md );

        return false;
    }
	int bytesTotal = 0;
	int bytesRead  = 0;

	char* buffer  = (char *) malloc( ( FILE_CHUNK_SIZE + 1 ) * sizeof( char ) );
	char* localMd = (char *) malloc( ( MAC_SIZE + 1 ) * sizeof( char ) );
	
    MD5_CTX  md5Context;
	// initializing MD5 structures
    MD5_Init( & md5Context );

	while( file.good() ) {
		// reading next chunk of file
		file.read( buffer, FILE_CHUNK_SIZE );
		bytesRead = file.gcount();
		buffer[bytesRead] = '\0';
		// generating MAC gradually, chunk by chunk
		MD5_Update( & md5Context, buffer, bytesRead );

		// dprintf( D_ALWAYS, "utilSafePutFile buffer read %s\n", buffer );
		bytesTotal += bytesRead;
	}
	// generating MAC
	MD5_Final( (unsigned char*) localMd, & md5Context );
	localMd[MAC_SIZE] = '\0';
	free( buffer );

	if( strcmp(md, localMd) ) {
		dprintf( D_ALWAYS, "utilSafeGetFile %s received with errors: "
						   "local MAC - %s, remote MAC - %s\n",
                 filePath.GetCStr( ), localMd, md );
		free( localMd );
		free( md );

		return false;
	}

	free( localMd );
	free( md );
	dprintf( D_ALWAYS, "utilSafeGetFile finished successfully\n" );

	return true;
}
#else
bool
utilSafePutFile( ReliSock& socket, const MyString& filePath )
{
    REPLICATION_ASSERT( filePath != "" );
    filesize_t bytes = 0;
    dprintf( D_ALWAYS, "utilSafePutFile %s started\n", filePath.GetCStr( ) );

    socket.encode( );
    // sending the temporary file through the opened socket
    if( socket.put_file( &bytes, filePath.GetCStr( ) ) < 0  ||
        ! socket.end_of_message( ) ) {
        dprintf( D_ALWAYS,
                "utilSafePutFile unable to send file %s "
                "or to code the end of the message\n", filePath.GetCStr( ) );
        return false;
    }
    dprintf( D_ALWAYS, "utilSafePutFile finished successfully\n" );

    return true;
}

bool
utilSafeGetFile( ReliSock& socket, const MyString& filePath )
{
    REPLICATION_ASSERT( filePath != "" );
    filesize_t  bytes = 0;

	dprintf( D_ALWAYS, "utilSafeGetFile %s started\n", filePath.GetCStr( ) );
    socket.decode( );

    if(  socket.get_file( &bytes, filePath.GetCStr( ), true ) < 0 ||
       ! socket.end_of_message( ) ) {
        dprintf( D_ALWAYS, "utilSafeGetFile unable to get file %s or the "
                            "end of the message\n",
                 filePath.GetCStr( ) );
        return false;
    }
    dprintf( D_ALWAYS, "utilSafeGetFile finished successfully\n" );

    return true;
}
#endif
