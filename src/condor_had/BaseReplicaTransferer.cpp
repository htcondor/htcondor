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


BaseReplicaTransferer::BaseReplicaTransferer(
								 const std::string&  pDaemonSinfulString,
								 const std::string&  pVersionFilePath,
								 const StringList& pStateFilePathsList ):
   m_daemonSinfulString( pDaemonSinfulString ),
   m_versionFilePath( pVersionFilePath ),
   m_socket( 0 ),
   m_connectionTimeout( DEFAULT_SEND_COMMAND_TIMEOUT ),
   m_maxTransferLifetime( DEFAULT_MAX_TRANSFER_LIFETIME )
{
	// 'malloc'-allocated string
	char* stateFilePathsAsString = pStateFilePathsList.print_to_string();
	// copying the string lists
	if ( stateFilePathsAsString ) {
		m_stateFilePathsList.initializeFromString( stateFilePathsAsString );
	}
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

	m_connectionTimeout = param_integer("HAD_CONNECTION_TIMEOUT",
										DEFAULT_SEND_COMMAND_TIMEOUT,
										0); // min value

	m_maxTransferLifetime = param_integer( "MAX_TRANSFER_LIFETIME",
											DEFAULT_MAX_TRANSFER_LIFETIME );
    return TRANSFERER_TRUE;
}

void
BaseReplicaTransferer::safeUnlinkStateAndVersionFiles(
	StringList& stateFilePathsList,
    const std::string& versionFilePath,
    const std::string& extension)
{
	FilesOperations::safeUnlinkFile( versionFilePath.c_str( ),
	                                 extension.c_str( ) );
	stateFilePathsList.rewind();

	char* stateFilePath = NULL;

	while( ( stateFilePath = stateFilePathsList.next( ) ) ) {
		FilesOperations::safeUnlinkFile( stateFilePath,
		                                 extension.c_str( ) );
	}
	stateFilePathsList.rewind();
}
