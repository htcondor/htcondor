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

/*
static int
getFileSize( const MyString& filePath )
{
	int begin, end;
	
	ifstream file ( filePath.Value( ) );

	begin = file.tellg( );
	file.seekg( 0, ios::end );
	end = file.tellg( );
	file.close( );

	return end - begin;
}*/

/*static void
safeUnlinkStateAndVersionFiles( const MyString& stateFilePath,
							    const MyString& versionFilePath,
							    const MyString& extension )
{
	FilesOperations::safeUnlinkFile( versionFilePath.Value( ),
                                     extension.Value( ) );
    FilesOperations::safeUnlinkFile( stateFilePath.Value( ),
                                     extension.Value( ) );
}*/

int
UploadReplicaTransferer::initialize( )
{
    reinitialize( );

    m_socket = new ReliSock( );

	// enable timeouts, we set them to the maximal possible value because the
	// maximal life policy of transferers is managed from within the code of
	// replication daemon, so that transferers behave as if they are given the
	// infinite time to upload the files
    //m_socket->set_timeout_multiplier( 1 );
    m_socket->timeout( INT_MAX ); // m_connectionTimeout );
    // no retries after 'm_connectionTimeout' seconds of unsuccessful connection
    m_socket->doNotEnforceMinimalCONNECT_TIMEOUT( );

    if( ! m_socket->connect( 
		const_cast<char*>( m_daemonSinfulString.Value( ) ), 0, false ) ) {
        dprintf( D_ALWAYS, 
				"UploadReplicaTransferer::initialize cannot connect to %s\n",
                 m_daemonSinfulString.Value( ) );
        return TRANSFERER_FALSE;
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
// stress testing
// 	dprintf( D_FULLDEBUG, "UploadReplicaTransferer::upload stalling "
//                       	  "uploading process\n" );
//	sleep(300);
//	int bytesTotal = getFileSize( m_stateFilePath );
	// setting the timeout depending on the file size and aknowledging the 
	// receiving side about that in order to copy the file to the temporary one
	// and not to get the socket closed at the end of copy operation
//	int newTimeout = std::max( int( bytesTotal * TRANSFER_SAFETY_COEFFICIENT / 
//												 DISK_WRITE_RATIO + 0.5 ),
//                               10 );
//	int oldTimeout = m_socket->timeout( newTimeout );
//	dprintf( D_ALWAYS, "UploadReplicaTransferer::upload set new timeout %d, "
//					   "old timeout %d, file size %d\n",
//			 newTimeout, oldTimeout, bytesTotal );
//	m_socket->encode( );
//	if( ! m_socket->code( bytesTotal ) || 
//		! m_socket->eom( ) ) {
//		 dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to send "
//							"the state file size (%d) or to code the end of "
//							"message\n", bytesTotal );
//		return TRANSFERER_FALSE;
//	}
    MyString extension( daemonCore->getpid( ) );
	// the .up ending is needed in order not to confuse between upload and
    // download processes temporary files
    extension += ".";
    extension += UPLOADING_TEMPORARY_FILES_EXTENSION;

	char* temporaryVersionFilePath =
			const_cast<char*>(m_versionFilePath.Value());
	/*char* temporaryStateFilePath   = 
			const_cast<char*>(m_stateFilePath.Value());*/
	char* temporaryExtension       = const_cast<char*>(extension.Value());

    if( ! FilesOperations::safeCopyFile( temporaryVersionFilePath,
										 temporaryExtension ) ) {
		dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to copy "
						   "version file %s\n", temporaryVersionFilePath );
		FilesOperations::safeUnlinkFile( temporaryVersionFilePath,
										 temporaryExtension );
		return TRANSFERER_FALSE;
	}
	char* stateFilePath = NULL;

	m_stateFilePathsList.rewind( );

	while( ( stateFilePath = m_stateFilePathsList.next( ) ) ) {
		if( ! FilesOperations::safeCopyFile( stateFilePath,
		                                     temporaryExtension ) ) {
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
	
	/*if( ! FilesOperations::safeCopyFile( temporaryStateFilePath,
                                         temporaryExtension ) ) {
        dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to copy "
                           "state file %s\n", temporaryStateFilePath );
		safeUnlinkStateAndVersionFiles( temporaryStateFilePath,
                                        temporaryVersionFilePath,
									    temporaryExtension );
        return TRANSFERER_FALSE;
    }*/
    // upload version file
    if( uploadFile( m_versionFilePath, extension ) == TRANSFERER_FALSE){
		dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to upload "
						   "version file %s\n", temporaryVersionFilePath );
		//FilesOperations::safeUnlinkFile( temporaryStateFilePath, 
		//								 temporaryExtension );
		safeUnlinkStateAndVersionFiles( m_stateFilePathsList,
		                                m_versionFilePath,
									    extension);
		return TRANSFERER_FALSE;
    }
	// trying to unlink the temporary files; upon failure we still return the
    // status of uploading the files, since the most important thing here is
    // that the files were uploaded successfully
	//FilesOperations::safeUnlinkFile( temporaryVersionFilePath, 
	//								 temporaryExtension );
    m_stateFilePathsList.rewind( );
	
	while( ( stateFilePath = m_stateFilePathsList.next( ) ) ) {
		MyString stateFilePathString = stateFilePath;

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
	/*if( uploadFile( m_stateFilePath, extension ) == TRANSFERER_FALSE ){
		return TRANSFERER_FALSE;
	}*/
	//FilesOperations::safeUnlinkFile( temporaryStateFilePath,
	//								 temporaryExtension );
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
UploadReplicaTransferer::uploadFile( MyString& filePath, MyString& extension )
{
    dprintf( D_ALWAYS, "UploadReplicaTransferer::uploadFile %s.%s started\n", 
			 filePath.Value( ), extension.Value( ) );
    // sending the temporary file through the opened socket
	if( ! utilSafePutFile( *m_socket, filePath + "." + extension ) ){
		dprintf( D_ALWAYS, "UploadReplicaTransferer::uploadFile failed, "
                "unlinking %s.%s\n", filePath.Value(), extension.Value());
		FilesOperations::safeUnlinkFile( filePath.Value( ), 
										 extension.Value( ) );
		return TRANSFERER_FALSE;
	}
	return TRANSFERER_TRUE;
}
