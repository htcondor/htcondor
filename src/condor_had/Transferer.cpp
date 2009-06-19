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
#include "subsystem_info.h"

// replication classes
#include "UploadReplicaTransferer.h"
#include "DownloadReplicaTransferer.h"
#include "FilesOperations.h"
#include "Utils.h"

extern char* myName;
// for daemon core
DECL_SUBSYSTEM( "TRANSFERER", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

// single 'condor_transferer' object
BaseReplicaTransferer* replicaTransferer = NULL; 

#if 0
int
uploadTerminateSignalHandler(Service* service, int signalNumber)
{
    dprintf( D_ALWAYS, "uploadTerminateSignalHandler started\n" );
    REPLICATION_ASSERT(signalNumber == 100);

    MyString extension( daemonCore->getpid( ) );
    // the .up ending is needed in order not to confuse between upload and
    // download processes temporary files
    extension += ".";
    extension += UPLOADING_TEMPORARY_FILES_EXTENSION;

	FilesOperations::safeUnlinkFile(
        replicaTransferer->getVersionFilePath( ).Value( ),
        extension.Value( ) );
    FilesOperations::safeUnlinkFile(
        replicaTransferer->getStateFilePath( ).Value( ),
        extension.Value( ) );
    exit(1);
	return 0;
}

int
downloadTerminateSignalHandler(Service* service, int signalNumber)
{
    dprintf( D_ALWAYS, "downloadTerminateSignalHandler started\n" );
    REPLICATION_ASSERT(signalNumber == 100);

    MyString extension( daemonCore->getpid( ) );
    // the .up ending is needed in order not to confuse between upload and
    // download processes temporary files
    extension += ".";
    extension += DOWNLOADING_TEMPORARY_FILES_EXTENSION;

	FilesOperations::safeUnlinkFile(
        replicaTransferer->getVersionFilePath( ).Value( ),
        extension.Value( ) );
    FilesOperations::safeUnlinkFile(
        replicaTransferer->getStateFilePath( ).Value( ),
        extension.Value( ) );
    exit(1);

	return 0;
}
#endif

/* Function   : cleanTemporaryFiles 
 * Description: cleans all temporary files of the transferer: be it the
 * 				uploading one or the downloading one
 */
void
cleanTemporaryFiles()
{
	dprintf( D_ALWAYS, "cleanTemporaryFiles started\n" );

	MyString downloadingExtension( daemonCore->getpid( ) );
	MyString uploadingExtension( daemonCore->getpid( ) );
	char*    stateFilePath = NULL;	

	downloadingExtension += ".";
	downloadingExtension += DOWNLOADING_TEMPORARY_FILES_EXTENSION;
	uploadingExtension   += ".";
	uploadingExtension   += UPLOADING_TEMPORARY_FILES_EXTENSION;

	// we first delete the possible temporary version files
	FilesOperations::safeUnlinkFile(
        replicaTransferer->getVersionFilePath( ).Value( ),
        downloadingExtension.Value( ) );
    FilesOperations::safeUnlinkFile(
        replicaTransferer->getVersionFilePath( ).Value( ),
        uploadingExtension.Value( ) );

	// then the possible temporary state files
    StringList& stateFilePathsList = 
		const_cast<StringList& >( replicaTransferer->getStateFilePathsList( ) );

	stateFilePathsList.rewind( );

    while( ( stateFilePath = stateFilePathsList.next( ) ) ) {
		if( ! FilesOperations::safeUnlinkFile( stateFilePath, 
										downloadingExtension.Value( ) ) ) {
            dprintf( D_ALWAYS, "cleanTemporaryFiles unable to unlink "
                               "state file %s with .down extension\n", 
					 stateFilePath );
        }
		if( ! FilesOperations::safeUnlinkFile( stateFilePath,
										uploadingExtension.Value( ) ) ) {
			dprintf( D_ALWAYS, "cleanTemporaryFiles unable to unlink "
                               "state file %s with .up extension\n",
					 stateFilePath );
		}									   
    }
    stateFilePathsList.rewind( );

	dprintf( D_ALWAYS, "cleanTemporaryFiles finished\n" );
}

/* Function: main_init
 * Arguments: argv[0] - name,
 *            argv[1] - up | down,
 *            argv[2] - another communication side daemon's sinful string,
 *            argv[3] - version file,
 *            argv[4] - number of state files
 *            argv[5], argv[6], ... argv[5 + argv[4] - 1] - state files
 * Remarks: flags (like '-f') are being stripped off before this function call
 */
int
main_init( int argc, char *argv[] )
{
    /*if( argc != 5 ) {
         dprintf( D_ALWAYS, "Transferer error: arguments number differs "
		 					"from 5\n" );
         DC_Exit( 1 );
    }*/
	if( argc < 6 ) {
		dprintf( D_ALWAYS, "Transferer error: arguments number is less "
						   "than 6\n");
		DC_Exit( 1 );
	}
	int stateFilesNumber = atoi(argv[4]);
	
	if( stateFilesNumber != argc - 5 ) {
		dprintf( D_ALWAYS, "Transferer error: number of state files, specified "
						   "as the fourth argument differs from their actual "
						   "number: %d vs. %d\n", stateFilesNumber, argc - 4 );

		DC_Exit( 1 );
	}
	StringList stateFilePathsList;
	
	for( int stateFileIndex = 0;
		 stateFileIndex < stateFilesNumber;
		 stateFileIndex ++ ) {
		stateFilePathsList.append( argv[5 + stateFileIndex] );
	}

    if( ! strncmp( argv[1], "down", strlen( "down" ) ) ) {
        replicaTransferer = new DownloadReplicaTransferer( 
								argv[2], 
								argv[3], 
								//argv[4] );
								stateFilePathsList );
	} else if( ! strncmp( argv[1], "up", strlen( "up" ) ) ) {
        replicaTransferer = new UploadReplicaTransferer( 
								argv[2], 
								argv[3], 
								//argv[4] );
								stateFilePathsList );
    } else {
        dprintf( D_ALWAYS, "Transfer error: first parameter must be "
                         "either up or down\n" );
        DC_Exit( 1 );
    }

    int result = replicaTransferer->initialize( );

    DC_Exit( result );

    return TRUE; // compilation reason
}

int
main_shutdown_graceful( )
{
	cleanTemporaryFiles( );
    delete replicaTransferer;
    DC_Exit( 0 );

    return 0; // compilation reason
}

int
main_shutdown_fast( )
{
	cleanTemporaryFiles( );
	delete replicaTransferer;
	DC_Exit( 0 );

	return 0; // compilation reason
}

// no reconfigurations enabled
int
main_config( bool is_full )
{
    return 1;
}

void
main_pre_dc_init( int argc, char* argv[] )
{
}

void
main_pre_command_sock_init( )
{
}
