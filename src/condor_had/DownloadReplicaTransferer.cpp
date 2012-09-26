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
//#include "util_lib_proto.h"

#include "Utils.h"
#include "DownloadReplicaTransferer.h"

// timeout for accepting connection from uploading transferer
//#define ACCEPT_TIMEOUT                  (2)

int
DownloadReplicaTransferer::initialize( )
{
    reinitialize( );

    if( transferFileCommand( ) == TRANSFERER_FALSE ) {
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
    char* temporaryDaemonSinfulString =
        const_cast<char*>( m_daemonSinfulString.Value( ) );

    dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
                       "to %s started\n", temporaryDaemonSinfulString );
    Daemon daemon( DT_ANY, temporaryDaemonSinfulString );
    ReliSock temporarySocket;
    
    // no retries after 'm_connectionTimeout' seconds of unsuccessful connection
    temporarySocket.timeout( m_connectionTimeout );
    temporarySocket.doNotEnforceMinimalCONNECT_TIMEOUT( );

    if( ! temporarySocket.connect( temporaryDaemonSinfulString, 0, false ) ) {
        dprintf( D_NETWORK, "DownloadReplicaTransferer::transferFileCommand "
                            "unable to connect to %s, reason: %s\n",
                   temporaryDaemonSinfulString, strerror( errno ) );
        temporarySocket.close( );

		return TRANSFERER_FALSE;
    }
    if( ! daemon.startCommand( REPLICATION_TRANSFER_FILE, &temporarySocket,
                                                 m_connectionTimeout ) ) {
        dprintf( D_COMMAND, "DownloadReplicaTransferer::transferFileCommand "
							"unable to start command to addr %s\n",
                   temporaryDaemonSinfulString );
		temporarySocket.close( );

        return TRANSFERER_FALSE;
    }
    MyString sinfulString;
    // find and bind port of the socket, to which the uploading
    // 'condor_transferer' process will send the important files
    ReliSock listeningSocket;

	listeningSocket.timeout( m_maxTransferLifetime / 2);
	//listeningSocket.timeout( ACCEPT_TIMEOUT );
	//listeningSocket.timeout( m_connectionTimeout );
	// this setting is practically unnecessary, since we do not connect to
	// remote sockets with 'listeningSocket'
    listeningSocket.doNotEnforceMinimalCONNECT_TIMEOUT( );

    if( ! listeningSocket.bind( FALSE ) || ! listeningSocket.listen( ) ) {
		temporarySocket.close( );
		listeningSocket.close( );

        return TRANSFERER_FALSE;
    }
    sinfulString = listeningSocket.get_sinful_public();
    // after the socket for the downloading/uploading process is occupied,
    // its number is sent to the remote replication daemon
    char* temporarySinfulString = const_cast<char*>( sinfulString.Value( ) );
    if( ! temporarySocket.code( temporarySinfulString ) ||
        ! temporarySocket.end_of_message( ) ) {
        dprintf( D_NETWORK, "DownloadReplicaTransferer::transferFileCommand "
               "unable to code the sinful string %s\n", temporarySinfulString );
		temporarySocket.close( );
		listeningSocket.close( );

        return TRANSFERER_FALSE;
    } else {
        dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
               "sinful string %s coded successfully\n", temporarySinfulString );
    }
	temporarySocket.close( );
    m_socket = listeningSocket.accept( );
//	m_socket->set_timeout_multiplier( 1 );
    m_socket->timeout( INT_MAX ); //m_connectionTimeout );
    m_socket->doNotEnforceMinimalCONNECT_TIMEOUT( );

	listeningSocket.close( );
    dprintf( D_ALWAYS, "DownloadReplicaTransferer::transferFileCommand "
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
	MyString extension;

    extension.formatstr( "%d.%s",
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
		MyString stateFilePathString = stateFilePath;
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
//	if( downloadFile( m_stateFilePath, extension ) == TRANSFERER_FALSE ) {
//		FilesOperations::safeUnlinkFile( m_versionFilePath.Value( ), 
//										 extension.Value( ) );
//		return TRANSFERER_FALSE;	
//	}
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
DownloadReplicaTransferer::downloadFile(MyString& filePath, MyString& extension)
{
// stress testing
//    dprintf( D_FULLDEBUG, "DownloadReplicaTransferer::downloadFile "
//						  "purposedly stalling the downloading process\n" );
//    sleep( 300 );
    dprintf( D_ALWAYS, "DownloadReplicaTransferer::downloadFile %s.%s\n", 
			 filePath.Value( ), extension.Value( ) );

	if( ! utilSafeGetFile( *m_socket, filePath + "." + extension) ) {
		dprintf( D_ALWAYS, "DownloadReplicaTransferer::downloadFile failed, "
				"unlinking %s.%s\n", filePath.Value(), extension.Value());
		FilesOperations::safeUnlinkFile( filePath.Value( ), 
										 extension.Value( ) );
		return TRANSFERER_FALSE;
	}
	return TRANSFERER_TRUE;
}
