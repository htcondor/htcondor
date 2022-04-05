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
#include "condor_daemon_core.h"
// for 'Daemon' class
#include "daemon.h"
// for 'DT_ANY'
#include "daemon_types.h"
// for 'getHostFromAddr' function
#include "internet.h"
// for unlink
#include "FilesOperations.h"
// for rotate_file
#include "util_lib_proto.h"

#include "Utils.h"
#include "DownloadReplicaTransferer.h"

// timeout for accepting connection from uploading transferer
//#define ACCEPT_TIMEOUT                  (2)

int
DownloadReplicaTransferer::initialize( )
{
	reinitialize( );

	int result;
	if ( m_command == "down-new" ) {
		result = transferFileCommandNew();
	} else {
		result = transferFileCommand();
	}
	if( result == TRANSFERER_FALSE ) {
		return TRANSFERER_FALSE;
	}
	return download( );
}

/* Function    : transferFileCommand
 * Return value: TRANSFERER_TRUE - upon success,
 *               TRANSFERER_FALSE - upon failure
 * Description : sends a transfer command to the remote replication daemon,
 *               which creates a uploading 'condor_transferer' process
 * Notes       : sends to the replication daemon a port number, on which it
 *               will be listening to the files uploading requests
 */
int
DownloadReplicaTransferer::transferFileCommand( )
{
    dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
             "to %s started\n", m_daemonSinfulString.c_str() );
    Daemon daemon( DT_ANY, m_daemonSinfulString.c_str() );
    ReliSock temporarySocket;
    
    // no retries after 'm_connectionTimeout' seconds of unsuccessful connection
    temporarySocket.timeout( m_connectionTimeout );
    temporarySocket.doNotEnforceMinimalCONNECT_TIMEOUT( );

    if( ! temporarySocket.connect( m_daemonSinfulString.c_str(), 0, false ) ) {
        dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
                 "unable to connect to %s, reason: %s\n",
                 m_daemonSinfulString.c_str(), strerror( errno ) );
        temporarySocket.close( );

		return TRANSFERER_FALSE;
    }
    if( ! daemon.startCommand( REPLICATION_TRANSFER_FILE, &temporarySocket,
                                                 m_connectionTimeout ) ) {
        dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
                 "unable to start command to addr %s\n",
                 m_daemonSinfulString.c_str() );
		temporarySocket.close( );

        return TRANSFERER_FALSE;
    }
    std::string sinfulString;
    // find and bind port of the socket, to which the uploading
    // 'condor_transferer' process will send the important files
    ReliSock listeningSocket;

	listeningSocket.timeout( m_maxTransferLifetime / 2);
	//listeningSocket.timeout( ACCEPT_TIMEOUT );
	//listeningSocket.timeout( m_connectionTimeout );
	// this setting is practically unnecessary, since we do not connect to
	// remote sockets with 'listeningSocket'
    listeningSocket.doNotEnforceMinimalCONNECT_TIMEOUT( );

	// TODO: HAD is probably IPv4-only for the foreseeable future, if for
	// no other reason than it transfers the sinful as a string rather than
	// a classad (and so rewriting can't happen).
	//
	// This may have, but probably did not, work in IPv6-only mode.  It's
	// easy to check for that and try IPv6 instead, just to see...
    if( ! listeningSocket.bind( CP_IPV4, false, 0, false ) || ! listeningSocket.listen( ) ) {
		temporarySocket.close( );
		listeningSocket.close( );

        return TRANSFERER_FALSE;
    }
	const char *val = listeningSocket.get_sinful_public();
    sinfulString = val ? val : "";
    // after the socket for the downloading/uploading process is occupied,
    // its number is sent to the remote replication daemon
    if( ! temporarySocket.code( sinfulString ) ||
        ! temporarySocket.end_of_message( ) ) {
        dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
                 "unable to code the sinful string %s\n", sinfulString.c_str() );
		temporarySocket.close( );
		listeningSocket.close( );

        return TRANSFERER_FALSE;
    } else {
        dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
                 "sinful string %s coded successfully\n", sinfulString.c_str() );
    }
	temporarySocket.close( );
    m_socket = listeningSocket.accept( );
	if ( m_socket == NULL ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
		         "timed out waiting for peer to connect\n" );
		listeningSocket.close( );

		return TRANSFERER_FALSE;
	}
//	m_socket->set_timeout_multiplier( 1 );
    m_socket->timeout( INT_MAX ); //m_connectionTimeout );
    m_socket->doNotEnforceMinimalCONNECT_TIMEOUT( );

	listeningSocket.close( );
    dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
                       "sent transfer command successfully and accepted "
					   "request on port no. %d\n", m_socket->get_port( ) );
    return TRANSFERER_TRUE;
}

int
DownloadReplicaTransferer::transferFileCommandNew( )
{
	dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommandNew "
	         "to %s started\n", m_daemonSinfulString.c_str() );
	Daemon daemon( DT_ANY, m_daemonSinfulString.c_str() );
	ReliSock *temporarySocket = new ReliSock;

	// no retries after 'm_connectionTimeout' seconds of unsuccessful connection
	temporarySocket->timeout( m_connectionTimeout );
	temporarySocket->doNotEnforceMinimalCONNECT_TIMEOUT( );

	if( ! temporarySocket->connect( m_daemonSinfulString.c_str(), 0, false ) ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommandNew "
			"unable to connect to %s, reason: %s\n",
			m_daemonSinfulString.c_str(), strerror( errno ) );
		delete temporarySocket;

		return TRANSFERER_FALSE;
	}
	if( ! daemon.startCommand( REPLICATION_TRANSFER_FILE_NEW, temporarySocket,
	                           m_connectionTimeout ) ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommandNew "
			"unable to start command to addr %s\n",
			m_daemonSinfulString.c_str() );
		delete temporarySocket;

		return TRANSFERER_FALSE;
	}

	temporarySocket->encode();
	if( ! temporarySocket->put( "" ) ||
	    ! temporarySocket->end_of_message( ) ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommandNew "
			"unable to send empty sinful string\n" );
		delete temporarySocket;

		return TRANSFERER_FALSE;
	}

	int reply = 0;
	temporarySocket->decode();
	if( ! temporarySocket->code( reply ) ||
	    ! temporarySocket->end_of_message( ) ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommandNew "
			"unable to receive reply\n" );
		delete temporarySocket;

		return TRANSFERER_FALSE;
	} else if ( reply <= 0 ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommandNew "
			"server sent failure code %d\n", reply );
		delete temporarySocket;
		return TRANSFERER_FALSE;
	} else {
		dprintf( D_NETWORK, "DownloadReplicaTransferer::transferFileCommandNew "
			"server sent success code %d\n", reply );
	}
	m_socket = temporarySocket;
//	m_socket->set_timeout_multiplier( 1 );
	m_socket->timeout( INT_MAX ); //m_connectionTimeout );
	m_socket->doNotEnforceMinimalCONNECT_TIMEOUT( );

	dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommandNew "
		"sent transfer command successfully and accepted "
		"request on port no. %d\n", m_socket->get_port( ) );
	return TRANSFERER_TRUE;
}

/* Function    : download
 * Return value: TRANSFERER_TRUE - upon success, 
 *               TRANSFERER_FALSE - upon failure
 * Description : downloads accounting information and version files
 */
int
DownloadReplicaTransferer::download( ) {
	std::string extension;

    formatstr( extension, "%d.%s",
                       daemonCore->getpid( ),
                       DOWNLOADING_TEMPORARY_FILES_EXTENSION );
	// download version file                                                    
    if( downloadFile( m_versionFilePath, extension) == TRANSFERER_FALSE ) {
    	return TRANSFERER_FALSE;
    }
    // download state file
	char* stateFilePath = NULL;

	m_stateFilePathsList.rewind( );

	while( ( stateFilePath = m_stateFilePathsList.next( ) ) ) {
		std::string stateFilePathString = stateFilePath;
		if( downloadFile( stateFilePathString, extension ) ==
				TRANSFERER_FALSE ) {
			// we return anyway, so that we should not worry that we operate
			// on 'm_stateFilePathsList' while iterating on it in the outer loop
			safeUnlinkStateAndVersionFiles( m_stateFilePathsList,
											m_versionFilePath,
											extension );
			return TRANSFERER_FALSE;
		}
	}
	m_stateFilePathsList.rewind( );	
    return TRANSFERER_TRUE;
}

/* Function    : downloadFile
 * Arguments   : filePath   - a name of the downloaded file
 *				 extension  - extension of temporary file
 * Return value: TRANSFERER_TRUE  - upon success,
 *               TRANSFERER_FALSE - upon failure
 * Description : downloads 'filePath' from the uploading 'condor_transferer'
 *               process
 */
int
DownloadReplicaTransferer::downloadFile(const std::string& filePath, const std::string& extension)
{
    dprintf( D_ALWAYS, "DownloadReplicaTransferer::downloadFile %s.%s\n", 
			 filePath.c_str( ), extension.c_str( ) );

	int fips_mode = param_integer("HAD_FIPS_MODE", 0);

	if( ! utilSafeGetFile( *m_socket, filePath + "." + extension, fips_mode) ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::downloadFile failed, "
				"unlinking %s.%s\n", filePath.c_str(), extension.c_str());
		FilesOperations::safeUnlinkFile( filePath.c_str( ), 
										 extension.c_str( ) );
		return TRANSFERER_FALSE;
	}
	return TRANSFERER_TRUE;
}
