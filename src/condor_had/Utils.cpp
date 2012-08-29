/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
#ifdef HAVE_EXT_OPENSSL
#include <openssl/md5.h>
#endif
#include "condor_netdb.h"
#include "ipv6_hostname.h"

#include <fstream>

using namespace std;

// for MD5 blocks computation
#define FILE_CHUNK_SIZE               (100)

extern void main_shutdown_graceful();

MyString
utilNoParameterError( const char* parameter, const char* daemonName )
{
	MyString error;

	if( ! strcasecmp( daemonName, "HAD" ) ) {
		error.formatstr( "HAD configuration error: no %s in config file",
                        parameter );
	} else if( ! strcasecmp( daemonName, "REPLICATION" ) ) {
		error.formatstr( "Replication configuration error: no %s in config file",
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
		error.formatstr("HAD configuration error: %s is not valid in config file",
					   parameter );
	} else if( ! strcasecmp( daemonName, "REPLICATION" ) ) {
    	error.formatstr( "Replication configuration error: %s is not valid "
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

	condor_sockaddr addr;
	if (!addr.from_ip_string(ipAddress)) {
		std::vector<condor_sockaddr> addrs = resolve_hostname(hostName);
		if (addrs.empty()) {
			free(hostName);
			return 0;
		}
		free(ipAddress);
		MyString ipaddr_str = addrs.front().to_ip_string();
		ipAddress = strdup(ipaddr_str.Value());
	}
	MyString sinfulString;

	sinfulString = generate_sinful(ipAddress, port);
    free( ipAddress );
    
    return strdup( sinfulString.Value( ) );
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

#ifdef HAVE_EXT_OPENSSL
bool
utilSafePutFile( ReliSock& socket, const MyString& filePath )
{
	REPLICATION_ASSERT( filePath != "" );
	// bool       successValue = true;
	filesize_t bytes        = 0;
    dprintf( D_ALWAYS, "utilSafePutFile %s started\n", filePath.Value( ) );

	ifstream file( filePath.Value( ) );
	
	if( file.fail( ) ){
		dprintf( D_ALWAYS, "utilSafePutFile failed to open file %s\n",
				 filePath.Value( ) ); 
		return false;
	}
	int bytesTotal = 0;
	int bytesRead  = 0;
	
	MD5_CTX  md5Context;
	// initializing MD5 structures
    MD5_Init( & md5Context );

	char* md     = ( char *) malloc( ( MAC_SIZE ) * sizeof( char ) );
	char* buffer = ( char *) malloc( ( FILE_CHUNK_SIZE ) * sizeof( char ) );

	while( file.good() ) {
		// reading next chunk of file
		file.read( buffer, FILE_CHUNK_SIZE );
		bytesRead = file.gcount();
		// generating MAC gradually, chunk by chunk
		MD5_Update( & md5Context, buffer, bytesRead );

		bytesTotal += bytesRead;
	}
	MD5_Final( (unsigned char*) md, & md5Context );
	free( buffer );

	dprintf( D_ALWAYS,
			 "utilSafePutFile MAC created, total bytes read %d\n",
			 bytesTotal );
	socket.encode( );
    
	if(	! socket.code_bytes( md, MAC_SIZE ) ||
        socket.put_file( &bytes, filePath.Value( ) ) < 0  ||
	    ! socket.end_of_message( ) ) {
        dprintf( D_ALWAYS,
                "utilSafePutFile unable to send file %s, MAC "
                "or to code the end of the message\n", filePath.Value( ) );
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
	char*       md      = ( char *) malloc( ( MAC_SIZE ) * sizeof( char ) );

	dprintf( D_ALWAYS, "utilSafeGetFile %s started\n", filePath.Value( ) );	
	socket.decode( );

    if( ! socket.code_bytes( md, MAC_SIZE ) ||
	    socket.get_file( &bytes, filePath.Value( ), true ) < 0 ||
	    ! socket.end_of_message( ) ) {
        dprintf( D_ALWAYS, "utilSafeGetFile unable to get file %s, MAC or the "
							"end of the message\n", 
				 filePath.Value( ) );
        free( md );
		
		return false;
    }
	dprintf( D_ALWAYS, "utilSafeGetFile MAC received\n" );
	ifstream file( filePath.Value( ) );

    if( file.fail( ) ){
        dprintf( D_ALWAYS, "utilSafeGetFile failed to open file %s\n",
                 filePath.Value( ) );
		free( md );

        return false;
    }
	int bytesRead  = 0;

	char* buffer  = (char *) malloc( ( FILE_CHUNK_SIZE ) * sizeof( char ) );
	char* localMd = (char *) malloc( ( MAC_SIZE ) * sizeof( char ) );
	
    MD5_CTX  md5Context;
	// initializing MD5 structures
    MD5_Init( & md5Context );

	while( file.good() ) {
		// reading next chunk of file
		file.read( buffer, FILE_CHUNK_SIZE );
		bytesRead = file.gcount();
		// generating MAC gradually, chunk by chunk
		MD5_Update( & md5Context, buffer, bytesRead );
	}
	// generating MAC
	MD5_Final( (unsigned char*) localMd, & md5Context );
	free( buffer );

	if( memcmp(md, localMd, MAC_SIZE) ) {
		dprintf( D_ALWAYS, "utilSafeGetFile %s received with errors: "
						   "local MAC does not match remote MAC\n",
                 filePath.Value( ) );
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
    dprintf( D_ALWAYS, "utilSafePutFile %s started\n", filePath.Value( ) );

    socket.encode( );
    // sending the temporary file through the opened socket
    if( socket.put_file( &bytes, filePath.Value( ) ) < 0  ||
        ! socket.end_of_message( ) ) {
        dprintf( D_ALWAYS,
                "utilSafePutFile unable to send file %s "
                "or to code the end of the message\n", filePath.Value( ) );
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

	dprintf( D_ALWAYS, "utilSafeGetFile %s started\n", filePath.Value( ) );
    socket.decode( );

    if(  socket.get_file( &bytes, filePath.Value( ), true ) < 0 ||
       ! socket.end_of_message( ) ) {
        dprintf( D_ALWAYS, "utilSafeGetFile unable to get file %s or the "
                            "end of the message\n",
                 filePath.Value( ) );
        return false;
    }
    dprintf( D_ALWAYS, "utilSafeGetFile finished successfully\n" );

    return true;
}
#endif
