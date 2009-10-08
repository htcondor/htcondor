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
// for 'param' function
#include "condor_config.h"

#include "BaseReplicaTransferer.h"
#include "FilesOperations.h"
#include "Utils.h"

extern int main_shutdown_graceful();

BaseReplicaTransferer::BaseReplicaTransferer(
                                 const MyString&  pDaemonSinfulString,
                                 const MyString&  pVersionFilePath,
                                 //const MyString&  pStateFilePath ):
								 const StringList& pStateFilePathsList ):
   m_daemonSinfulString( pDaemonSinfulString ),
   m_versionFilePath( pVersionFilePath ),
   //m_stateFilePathsList( pStateFilePathsList ), 
   m_socket( 0 ),
   m_connectionTimeout( DEFAULT_SEND_COMMAND_TIMEOUT ),
   m_maxTransferLifetime( DEFAULT_MAX_TRANSFER_LIFETIME )
{
	// 'malloc'-allocated string
	char* stateFilePathsAsString = 
		const_cast<StringList&>(pStateFilePathsList).print_to_string();
	// copying the string lists
	m_stateFilePathsList.initializeFromString( stateFilePathsAsString );
	free( stateFilePathsAsString );
}

BaseReplicaTransferer::~BaseReplicaTransferer()
{
        delete m_socket;
}

int
BaseReplicaTransferer::reinitialize( )
{
    dprintf( D_ALWAYS, "BaseReplicaTransferer::reinitialize started\n" );
    
    char* buffer = param( "HAD_CONNECTION_TIMEOUT" );
    
    if( buffer ) {
        bool result = false;

		m_connectionTimeout = utilAtoi( buffer, &result ); 
		//strtol( buffer, 0, 10 );
        
        if( ! result || m_connectionTimeout <= 0 ) {
            utilCrucialError(
				utilConfigurationError("HAD_CONNECTION_TIMEOUT", 
									   "HAD").Value( ) );
        }
        free( buffer );
    } else {
		utilCrucialError(
       		utilNoParameterError("HAD_CONNECTION_TIMEOUT", "HAD").Value( ) );
    }

	m_maxTransferLifetime = param_integer( "MAX_TRANSFER_LIFETIME",
											DEFAULT_MAX_TRANSFER_LIFETIME );
    return TRANSFERER_TRUE;
}

void
BaseReplicaTransferer::safeUnlinkStateAndVersionFiles(
	const StringList& stateFilePathsList,
    const MyString&   versionFilePath,
    const MyString&   extension)
{
	FilesOperations::safeUnlinkFile( versionFilePath.Value( ),
                                     extension.Value( ) );
	StringList& stateFilePathsListRef =
		const_cast<StringList&>(stateFilePathsList);
	stateFilePathsListRef.rewind();

	char* stateFilePath = NULL;

	while( ( stateFilePath = stateFilePathsListRef.next( ) ) ) {
		FilesOperations::safeUnlinkFile( stateFilePath,
                                     	 extension.Value( ) );
	}
	stateFilePathsListRef.rewind();
}
