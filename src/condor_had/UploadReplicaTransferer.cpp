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
// for copy_file
#include "util_lib_proto.h"

#include "UploadReplicaTransferer.h"
#include "FilesOperations.h"
#include "Utils.h"

int
UploadReplicaTransferer::initialize( )
{
	reinitialize( );

	if ( m_command == "up-new" ) {
		// Try to get inherited socket
		Stream **socks = daemonCore->GetInheritedSocks();
		if (socks[0] == NULL ||
			socks[0]->type() != Stream::reli_sock) {
			dprintf( D_ALWAYS,
				"UploadReplicaTransferer::initialize failed to get inherited socket\n" );
			return TRANSFERER_FALSE;
		}
		m_socket = (ReliSock *)socks[0];

		int reply = 1;
		m_socket->encode();
		if( ! m_socket->code( reply ) ||
			! m_socket->end_of_message( ) ) {
			dprintf( D_ALWAYS,
				"UploadReplicaTransferer::initialize failed to send reply\n" );
			return TRANSFERER_FALSE;
		}
		m_socket->timeout( INT_MAX ); // m_connectionTimeout );

	} else {
		m_socket = new ReliSock( );

		// enable timeouts, we set them to the maximal possible value because the
		// maximal life policy of transferers is managed from within the code of
		// replication daemon, so that transferers behave as if they are given the
		// infinite time to upload the files
		//m_socket->set_timeout_multiplier( 1 );
		m_socket->timeout( INT_MAX ); // m_connectionTimeout );
		// no retries after 'm_connectionTimeout' seconds of unsuccessful connection
		m_socket->doNotEnforceMinimalCONNECT_TIMEOUT( );

		if( ! m_socket->connect( m_daemonSinfulString.c_str( ), 0, false ) ) {
			dprintf( D_ALWAYS,
				"UploadReplicaTransferer::initialize cannot connect to %s\n",
				m_daemonSinfulString.c_str( ) );
			return TRANSFERER_FALSE;
		}
	}
	// send accounting information and version files
	return upload( );
}

/* Function    : upload
 * Return value: TRANSFERER_TRUE  - upon success
 *               TRANSFERER_FALSE - upon failure
 * Description : uploads state and version files to the
 *               downloading 'condor_transferer' process
 */
int
UploadReplicaTransferer::upload( )
{
    dprintf( D_ALWAYS, "UploadReplicaTransferer::upload started\n" );
    std::string extension;
	// the .up ending is needed in order not to confuse between upload and
    // download processes temporary files
	formatstr( extension, "%d.%s", daemonCore->getpid( ),
	           UPLOADING_TEMPORARY_FILES_EXTENSION );

    if( ! FilesOperations::safeCopyFile( m_versionFilePath.c_str(),
										 extension.c_str() ) ) {
		dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to copy "
				 "version file %s\n", m_versionFilePath.c_str() );
		FilesOperations::safeUnlinkFile( m_versionFilePath.c_str(),
										 extension.c_str() );
		return TRANSFERER_FALSE;
	}
	char* stateFilePath = NULL;

	m_stateFilePathsList.rewind( );

	while( ( stateFilePath = m_stateFilePathsList.next( ) ) ) {
		if( ! FilesOperations::safeCopyFile( stateFilePath,
		                                     extension.c_str() ) ) {
			dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to copy "
							   "state file %s\n", stateFilePath );
			// we return anyway, so that we should not worry that we operate
			// on 'm_stateFilePathsList' while iterating on it in the outer loop
			safeUnlinkStateAndVersionFiles( m_stateFilePathsList,
											m_versionFilePath,
											extension );
			return TRANSFERER_FALSE;
		}
	}
	m_stateFilePathsList.rewind( );
	
    // upload version file
    if( uploadFile( m_versionFilePath, extension ) == TRANSFERER_FALSE){
		dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to upload "
				 "version file %s\n", m_versionFilePath.c_str() );
		safeUnlinkStateAndVersionFiles( m_stateFilePathsList,
		                                m_versionFilePath,
									    extension);
		return TRANSFERER_FALSE;
    }
	// trying to unlink the temporary files; upon failure we still return the
    // status of uploading the files, since the most important thing here is
    // that the files were uploaded successfully
	//FilesOperations::safeUnlinkFile( m_versionFilePath.c_str(),
	//								 extension.c_str() );
    m_stateFilePathsList.rewind( );
	
	while( ( stateFilePath = m_stateFilePathsList.next( ) ) ) {
		std::string stateFilePathString = stateFilePath;

		if( uploadFile( stateFilePathString , extension ) ==
				TRANSFERER_FALSE ) {
			safeUnlinkStateAndVersionFiles( m_stateFilePathsList,
			                                m_versionFilePath,
											extension );
			return TRANSFERER_FALSE;
		}
	}
	m_stateFilePathsList.rewind( );
	safeUnlinkStateAndVersionFiles( m_stateFilePathsList,
	                                m_versionFilePath,
									extension );
	// if the uploading failed, the temporary file is deleted, so there is no
	// need to delete it in the caller
	return TRANSFERER_TRUE;
}

/* Function    : uploadFile
 * Arguments   : filePath   - the name of uploaded file
 *               extension  - temporary file extension
 * Return value: TRANSFERER_TRUE  - upon success
 *               TRANSFERER_FALSE - upon failure
 * Description : uploads the specified file to the downloading process through
 *               the transferer socket
 */
int
UploadReplicaTransferer::uploadFile( const std::string& filePath, const std::string& extension )
{
    dprintf( D_ALWAYS, "UploadReplicaTransferer::uploadFile %s.%s started\n", 
			 filePath.c_str( ), extension.c_str( ) );

	int fips_mode = param_integer("HAD_FIPS_MODE", 0);

    // sending the temporary file through the opened socket
	if( ! utilSafePutFile( *m_socket, filePath + "." + extension, fips_mode ) ){
		dprintf( D_ALWAYS, "UploadReplicaTransferer::uploadFile failed, "
                "unlinking %s.%s\n", filePath.c_str(), extension.c_str());
		FilesOperations::safeUnlinkFile( filePath.c_str( ), 
										 extension.c_str( ) );
		return TRANSFERER_FALSE;
	}
	return TRANSFERER_TRUE;
}
