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
#include "condor_sinful.h"
// for 'param' function
#include "condor_config.h"
// for 'Daemon' class
#include "daemon.h"
// for 'DT_ANY' definition
#include "daemon_types.h"

#include "AbstractReplicatorStateMachine.h"
//#include "ReplicationCommands.h"
#include "FilesOperations.h"

// gcc compilation pecularities demand explicit declaration of template classes
// and functions instantiation
template void utilClearList<AbstractReplicatorStateMachine::ProcessMetadata>
				( List<AbstractReplicatorStateMachine::ProcessMetadata>& );
template void utilClearList<Version>( List<Version>& );
                                                                                
AbstractReplicatorStateMachine::AbstractReplicatorStateMachine()
{
	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine ctor started\n" );
	m_state              = VERSION_REQUESTING;
    m_connectionTimeout  = DEFAULT_SEND_COMMAND_TIMEOUT;
   	m_downloadReaperId   = -1;
   	m_uploadReaperId     = -1;
}

AbstractReplicatorStateMachine::~AbstractReplicatorStateMachine()
{
	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine dtor started\n" );
    finalize();
}
// releasing the dynamic memory and assigning initial values to all the data
// members of the base class
void
AbstractReplicatorStateMachine::finalize()
{
	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::finalize started\n" );
   	m_state             = VERSION_REQUESTING;
   	m_connectionTimeout = DEFAULT_SEND_COMMAND_TIMEOUT;

    utilClearList( m_replicationDaemonsList );
    m_transfererPath = "";

    //utilCancelReaper(m_downloadReaperId);
    //utilCancelReaper(m_uploadReaperId);
	// upon finalizing and/or reinitialiing the existing transferer processes
	// must be killed, otherwise we will temporarily deny creation of new
	// downloading transferers till the older ones are over
    killTransferers( );
}
// passing over REPLICATION_LIST configuration parameter, turning all the 
// addresses into canonical <ip:port> form and inserting them all, except for
// the address of local replication daemon, into 'm_replicationDaemonsList'.
void
AbstractReplicatorStateMachine::initializeReplicationList( char* buffer )
{
    StringList replicationAddressList;
    char*      replicationAddress    = NULL;
    bool       isMyAddressPresent    = false;
	Sinful     my_addr( daemonCore->InfoCommandSinfulString( ) );

    replicationAddressList.initializeFromString( buffer );
    // initializing a list unrolls it, that's why the rewind is needed to bring
    // it to the beginning
    replicationAddressList.rewind( );
    /* Passing through the REPLICATION_LIST configuration parameter, stripping
     * the optional <> brackets off, and extracting the host name out of
     * either ip:port or hostName:port entries
     */
    while( (replicationAddress = replicationAddressList.next( )) ) {
        char* sinfulAddress = utilToSinful( replicationAddress );

        if( sinfulAddress == NULL ) {
            char bufArray[BUFSIZ];

			sprintf( bufArray, 
					"AbstractReplicatorStateMachine::initializeReplicationList"
                    " invalid address %s\n", replicationAddress );
            utilCrucialError( bufArray );

            continue;
        }

		// See comments HADStateMachine::getHadList().
		Sinful s( sinfulAddress );
		free( sinfulAddress );
		s.setSharedPortID( my_addr.getSharedPortID() );

dprintf( D_ALWAYS, "Checking address with shared port ID '%s'...\n", s.getSinful() );
        if( my_addr.addressPointsToMe( s.getSinful() ) ) {
dprintf( D_ALWAYS, "... found myself in list: %s\n", s.getSinful() );
            isMyAddressPresent = true;
        }
        else {
            m_replicationDaemonsList.insert( s.getSinful() );
        }
    }

    if( !isMyAddressPresent ) {
        utilCrucialError( "ReplicatorStateMachine::initializeReplicationList "
                          "my address is not present in REPLICATION_LIST" );
    }
}

void
AbstractReplicatorStateMachine::reinitialize()
{
	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::reinitialize "
					   "started\n" );
    char* buffer = NULL;

    buffer = param( "REPLICATION_LIST" );

    if ( buffer ) {
        initializeReplicationList( buffer );
        free( buffer );
    } else {
        utilCrucialError( utilNoParameterError("REPLICATION_LIST", 
		                                       "REPLICATION").c_str( ) );
    }

	m_connectionTimeout = param_integer("HAD_CONNECTION_TIMEOUT",
										DEFAULT_SEND_COMMAND_TIMEOUT,
										0); // min value

	buffer = param( "TRANSFERER" );
	if (!buffer) {
		utilCrucialError(utilConfigurationError("TRANSFERER",
												"REPLICATION").c_str());
	}
	m_transfererPath = buffer ? buffer : "";
	free( buffer );

	char* spoolDirectory = param( "SPOOL" );
    
    if( spoolDirectory ) {
        m_versionFilePath = spoolDirectory;
        m_versionFilePath += "/";
        m_versionFilePath += VERSION_FILE_NAME;
        
        buffer = param( "STATE_FILE" );
        if ( !buffer ) {
            buffer = param( "NEGOTIATOR_STATE_FILE" );
        }
    
        if( buffer ) {
            m_stateFilePath = buffer;

            free( buffer );
        } else {
            m_stateFilePath  = spoolDirectory;
            m_stateFilePath += "/";
            m_stateFilePath += "Accountantnew.log";
        }

        free( spoolDirectory );
    } else {
        utilCrucialError( utilNoParameterError("SPOOL", "REPLICATION").
                          c_str( ) );
    }
	printDataMembers( );
}
// Canceling all the data regarding the downloading process that has just 
// finished, i.e. its pid, the last time of creation. Then replacing the state
// file and the version file with the temporary files, that have been uploaded.
// After all this is finished, loading the received version file's data into
// 'm_myVersion'
int
AbstractReplicatorStateMachine::downloadReplicaTransfererReaper(
    int pid, int exitStatus)
{
    dprintf( D_ALWAYS,
            "AbstractReplicatorStateMachine::downloadReplicaTransfererReaper "
            "called for process no. %d\n", pid );
    AbstractReplicatorStateMachine* replicatorStateMachine =
         static_cast<AbstractReplicatorStateMachine*>( this );
    // setting the downloading reaper process id to initialization value to
    // know whether the downloading has finished or not
	// NOTE: upon stalling the downloader, the transferer is being killed before
	// 		 the reaper is called, so that the application fails in the assert,
	//		 this is the reason for commenting it out
	//REPLICATION_ASSERT(replicatorStateMachine->
	//					m_downloadTransfererMetadata.isValid());
	replicatorStateMachine->m_downloadTransfererMetadata.set( );   

    // the function ended due to the operating system signal, the numeric
    // value of which is stored in exitStatus
    if( WIFSIGNALED( exitStatus ) ) {
        dprintf( D_PROC,
            "AbstractReplicatorStateMachine::downloadReplicaTransfererReaper "
            "process %d failed by signal number %d\n",
            pid, WTERMSIG( exitStatus ) );
        return TRANSFERER_FALSE;
    }
    // exit function real return value can be retrieved by dividing the
    // exit status by 256
    else if( WEXITSTATUS( exitStatus ) != 0 ) {
        dprintf( D_PROC,
            "AbstractReplicatorStateMachine::downloadReplicaTransfererReaper "
            "process %d ended unsuccessfully "
            "with exit status %d\n",
            pid, WEXITSTATUS( exitStatus ) );
        return TRANSFERER_FALSE;
    }
    std::string temporaryFilesExtension;

    formatstr( temporaryFilesExtension, "%d.%s", pid,
               DOWNLOADING_TEMPORARY_FILES_EXTENSION );
    // the rotation and the version synchronization appear in the code
    // sequentially, trying to make the gap between them as less as possible;
    // upon failure we do not synchronize the local version, since such
	// downloading is considered invalid
	if( ! FilesOperations::safeRotateFile(
						  replicatorStateMachine->m_versionFilePath.c_str(),
										  temporaryFilesExtension.c_str()) ||
		! FilesOperations::safeRotateFile(
						    replicatorStateMachine->m_stateFilePath.c_str(),
										  temporaryFilesExtension.c_str()) ) {
		return TRANSFERER_FALSE;
	}
    replicatorStateMachine->m_myVersion.synchronize( false );

    return TRANSFERER_TRUE;
}
// Scanning the list of uploading transferers and deleting all the data
// regarding the process that has just finished and called the reaper, i.e.
// its pid and time of creation
int
AbstractReplicatorStateMachine::uploadReplicaTransfererReaper(
    int pid, int exitStatus)
{
    dprintf( D_ALWAYS,
        "AbstractReplicatorStateMachine::uploadReplicaTransfererReaper "
        "called for process no. %d\n", pid );
    AbstractReplicatorStateMachine* replicatorStateMachine =
        static_cast<AbstractReplicatorStateMachine*>( this );
// TODO: Atomic operation
	replicatorStateMachine->m_uploadTransfererMetadataList.Rewind( );

	ProcessMetadata* uploadTransfererMetadata = NULL;

	// Scanning the list of uploading transferers to remove the pid of the
    // process that has just finished
    while( replicatorStateMachine->m_uploadTransfererMetadataList.Next( 
										uploadTransfererMetadata ) ) {
        // deleting the finished uploading transferer process pid from the list
        if( pid == uploadTransfererMetadata->m_pid ) { 
            dprintf( D_FULLDEBUG,
                "AbstractReplicatorStateMachine::uploadReplicaTransfererReaper"
                " removing process no. %d from the uploading "
                "condor_transferers list\n",
                uploadTransfererMetadata->m_pid );
			delete uploadTransfererMetadata;
        	replicatorStateMachine->
				  m_uploadTransfererMetadataList.DeleteCurrent( );
		}
		// for debugging purposes only
		//dprintf( D_FULLDEBUG, 
		//		"AbstractReplicatorStateMachine::uploadReplicaTransfererReaper"
    	//		" uploading condor_transferers list size = %d\n", 
		//		replicatorStateMachine->m_uploadTransfererMetadataList.Number() );
	}
	replicatorStateMachine->m_uploadTransfererMetadataList.Rewind( );
// End of TODO: Atomic operation

    // the function ended due to the operating system signal, the numeric
    // value of which is stored in exitStatus
    if( WIFSIGNALED( exitStatus ) ) {
        dprintf( D_PROC,
            "AbstractReplicatorStateMachine::uploadReplicaTransfererReaper "
            "process %d failed by signal number %d\n",
            pid, WTERMSIG( exitStatus ) );
        return TRANSFERER_FALSE;
    }
    // exit function real return value can be retrieved by dividing the
    // exit status by 256
    else if( WEXITSTATUS( exitStatus ) != 0 ) {
        dprintf( D_PROC,
            "AbstractReplicatorStateMachine::uploadReplicaTransfererReaper "
            "process %d ended unsuccessfully "
            "with exit status %d\n",
                   pid, WEXITSTATUS( exitStatus ) );
        return TRANSFERER_FALSE;
    }
    return TRANSFERER_TRUE;
}

// creating downloading transferer process and remembering its pid and creation
// time
bool
AbstractReplicatorStateMachine::download( const char* daemonSinfulString )
{
	ArgList  processArguments;
	processArguments.AppendArg( m_transfererPath );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "down" );
	processArguments.AppendArg( daemonSinfulString );
	processArguments.AppendArg( m_versionFilePath );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( m_stateFilePath );
	// Get arguments from this ArgList object for descriptional purposes.
	std::string	s;
	processArguments.GetArgsStringForDisplay( s );
    dprintf( D_FULLDEBUG,
			 "AbstractReplicatorStateMachine::download creating "
			 "downloading condor_transferer process: \n \"%s\"\n",
			 s.c_str( ) );

	// PRIV_ROOT privilege is necessary here to create the process
	// so we can read GSI certs <sigh>
	priv_state privilege = PRIV_ROOT;

	int transfererPid = daemonCore->Create_Process(
        m_transfererPath.c_str( ),    // name
        processArguments,             // args
        privilege,                    // priv
        m_downloadReaperId,           // reaper id
        FALSE,                        // command port needed?
        FALSE,                        // command port needed?
        NULL,                         // env
        NULL,                         // cwd
        NULL                          // process family info
        );
    if( transfererPid == FALSE ) {
        dprintf( D_ALWAYS,
            "AbstractReplicatorStateMachine::download unable to create "
            "downloading condor_transferer process\n" );
        return false;
    } else {
        dprintf( D_FULLDEBUG,
            "AbstractReplicatorStateMachine::download downloading "
            "condor_transferer process created with pid = %d\n",
             transfererPid );
		REPLICATION_ASSERT( ! m_downloadTransfererMetadata.isValid() );
       /* Remembering the last time, when the downloading 'condor_transferer'
        * was created: the monitoring might be useful in possible prevention
        * of stuck 'condor_transferer' processes. Remembering the pid of the
        * downloading process as well: to terminate it when the downloading
        * process is stuck
        */
		m_downloadTransfererMetadata.set(transfererPid, time( NULL ) );
    }

    return true;
}

bool
AbstractReplicatorStateMachine::downloadNew( const char* daemonSinfulString )
{
	ArgList  processArguments;
	processArguments.AppendArg( m_transfererPath );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "down-new" );
	processArguments.AppendArg( daemonSinfulString );
	processArguments.AppendArg( m_versionFilePath );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( m_stateFilePath );
	// Get arguments from this ArgList object for descriptional purposes.
	std::string	s;
	processArguments.GetArgsStringForDisplay( s );
	dprintf( D_FULLDEBUG,
		"AbstractReplicatorStateMachine::downloadNew creating "
		"downloading condor_transferer process: \n \"%s\"\n",
		s.c_str( ) );

	// PRIV_ROOT privilege is necessary here to create the process
	// so we can read GSI certs <sigh>
	priv_state privilege = PRIV_ROOT;

	int transfererPid = daemonCore->Create_Process(
		m_transfererPath.c_str( ),    // name
		processArguments,             // args
		privilege,                    // priv
		m_downloadReaperId,           // reaper id
		FALSE,                        // command port needed?
		FALSE,                        // command port needed?
		NULL,                         // env
		NULL,                         // cwd
		NULL                          // process family info
		);
	if( transfererPid == FALSE ) {
		dprintf( D_ALWAYS,
			"AbstractReplicatorStateMachine::downloadNew unable to create "
			"downloading condor_transferer process\n" );
		return false;
	} else {
		dprintf( D_FULLDEBUG,
			"AbstractReplicatorStateMachine::downloadNew downloading "
			"condor_transferer process created with pid = %d\n",
			transfererPid );
		REPLICATION_ASSERT( ! m_downloadTransfererMetadata.isValid() );
		/* Remembering the last time, when the downloading 'condor_transferer'
		 * was created: the monitoring might be useful in possible prevention
		 * of stuck 'condor_transferer' processes. Remembering the pid of the
		 * downloading process as well: to terminate it when the downloading
		 * process is stuck
		 */
		m_downloadTransfererMetadata.set(transfererPid, time( NULL ) );
	}

	return true;
}

// creating uploading transferer process and remembering its pid and
// creation time
bool
AbstractReplicatorStateMachine::upload( const char* daemonSinfulString )
{
	ArgList  processArguments;
	processArguments.AppendArg( m_transfererPath );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "up" );
	processArguments.AppendArg( daemonSinfulString );
	processArguments.AppendArg( m_versionFilePath );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( m_stateFilePath );

	// Get arguments from this ArgList object for descriptional purposes.
	std::string	s;
	processArguments.GetArgsStringForDisplay( s );
    dprintf( D_FULLDEBUG,
			 "AbstractReplicatorStateMachine::upload creating "
			 "uploading condor_transferer process: \n \"%s\"\n",
			 s.c_str( ) );

	// PRIV_ROOT privilege is necessary here to create the process
	// so we can read GSI certs <sigh>
	priv_state privilege = PRIV_ROOT;

    int transfererPid = daemonCore->Create_Process(
        m_transfererPath.c_str( ),    // name
        processArguments,             // args
        privilege,                    // priv
        m_uploadReaperId,             // reaper id
        FALSE,                        // tcp command port needed?
        FALSE,                        // udp command port needed?
        NULL,                         // envs
        NULL,                         // cwd
        NULL                          // process family info
        );
    if ( transfererPid == FALSE ) {
		dprintf( D_PROC,
            "AbstractReplicatorStateMachine::upload unable to create "
            "uploading condor_transferer process\n");
        return false;
    } else {
        dprintf( D_FULLDEBUG,
            "AbstractReplicatorStateMachine::upload uploading "
            "condor_transferer process created with pid = %d\n",
            transfererPid );
        /* Remembering the last time, when the uploading 'condor_transferer'
         * was created: the monitoring might be useful in possible prevention
         * of stuck 'condor_transferer' processes. Remembering the pid of the
         * uploading process as well: to terminate it when the uploading
         * process is stuck
         */
// TODO: Atomic operation
		// dynamically allocating the memory for the pid, since it is going to
    	// be inserted into Condor's list which stores the pointers to the data,
    	// rather than the data itself, that's why it is impossible to pass a 
		// local integer variable to append to this list
		ProcessMetadata* uploadTransfererMetadata = 
				new ProcessMetadata( transfererPid, time( NULL ) );
		m_uploadTransfererMetadataList.Append( uploadTransfererMetadata );
// End of TODO: Atomic operation
    }

    return true;
}

bool
AbstractReplicatorStateMachine::uploadNew( Stream *stream )
{
	// TODO take ReliSock instead of sinful
	//   have child inherit ReliSock
	ArgList  processArguments;
	processArguments.AppendArg( m_transfererPath );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "up-new" );
	processArguments.AppendArg( "" );
	processArguments.AppendArg( m_versionFilePath );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( m_stateFilePath );

	Stream *inherit_list[] =
		{ stream /*connection to client*/,
		  0 /*terminal NULL*/ };

	// Get arguments from this ArgList object for descriptional purposes.
	std::string	s;
	processArguments.GetArgsStringForDisplay( s );
	dprintf( D_FULLDEBUG,
		"AbstractReplicatorStateMachine::uploadNew creating "
		"uploading condor_transferer process: \n \"%s\"\n",
		s.c_str( ) );

	// PRIV_ROOT privilege is necessary here to create the process
	// so we can read GSI certs <sigh>
	priv_state privilege = PRIV_ROOT;

	int transfererPid = daemonCore->Create_Process(
		m_transfererPath.c_str( ),    // name
		processArguments,             // args
		privilege,                    // priv
		m_uploadReaperId,             // reaper id
		FALSE,                        // tcp command port needed?
		FALSE,                        // udp command port needed?
		NULL,                         // envs
		NULL,                         // cwd
		NULL,                         // process family info
		inherit_list                  // inherited socks
		);
	if ( transfererPid == FALSE ) {
		dprintf( D_PROC,
			"AbstractReplicatorStateMachine::uploadNew unable to create "
			"uploading condor_transferer process\n");
		return false;
	} else {
		dprintf( D_FULLDEBUG,
			"AbstractReplicatorStateMachine::uploadNew uploading "
			"condor_transferer process created with pid = %d\n",
			transfererPid );
		/* Remembering the last time, when the uploading 'condor_transferer'
		 * was created: the monitoring might be useful in possible prevention
		 * of stuck 'condor_transferer' processes. Remembering the pid of the
		 * uploading process as well: to terminate it when the uploading
		 * process is stuck
		 */
// TODO: Atomic operation
		// dynamically allocating the memory for the pid, since it is going to
		// be inserted into Condor's list which stores the pointers to the data,
		// rather than the data itself, that's why it is impossible to pass a
		// local integer variable to append to this list
		ProcessMetadata* uploadTransfererMetadata =
				new ProcessMetadata( transfererPid, time( NULL ) );
		m_uploadTransfererMetadataList.Append( uploadTransfererMetadata );
// End of TODO: Atomic operation
	}

	return true;
}

// sending command, along with the local replication daemon's version and state
void
AbstractReplicatorStateMachine::broadcastVersion( int command )
{
    char* replicationDaemon = NULL;

    m_replicationDaemonsList.rewind( );

    while( (replicationDaemon = m_replicationDaemonsList.next( )) ) {
        sendVersionAndStateCommand( command,  replicationDaemon );
    }
    m_replicationDaemonsList.rewind( );
}

void
AbstractReplicatorStateMachine::requestVersions( )
{
    char* replicationDaemon = NULL;

    m_replicationDaemonsList.rewind( );

    while( (replicationDaemon = m_replicationDaemonsList.next( )) ) {
        sendCommand( REPLICATION_SOLICIT_VERSION,  replicationDaemon,
                                &AbstractReplicatorStateMachine::noCommand);
    }
    m_replicationDaemonsList.rewind( );
}

// inserting/replacing version from specific remote replication daemon
// into/in 'm_versionsList'
void
AbstractReplicatorStateMachine::updateVersionsList( Version& newVersion )
{
    Version* oldVersion;
// TODO: Atomic operation
    m_versionsList.Rewind( );

    while( m_versionsList.Next( oldVersion ) ) {
        // deleting all occurences of replica belonging to the same host name
        if( oldVersion->getHostName( ) == newVersion.getHostName( ) ) {
            delete oldVersion;
            m_versionsList.DeleteCurrent( );
        }
    }
    dprintf( D_FULLDEBUG,
        "AbstractReplicatorStateMachine::updateVersionsList appending %s\n",
         newVersion.toString( ).c_str( ) );
    m_versionsList.Append( &newVersion );
    m_versionsList.Rewind( );
// End of TODO: Atomic operation
}

void
AbstractReplicatorStateMachine::cancelVersionsListLeader( )
{
    Version* version;
    
    m_versionsList.Rewind( );

    while( m_versionsList.Next( version ) ) {
        version->setState( BACKUP );
    }
    
    m_versionsList.Rewind( );
}

// sending command to remote replication daemon; specified command function
// allows to specify which data is to be sent to the remote daemon
void
AbstractReplicatorStateMachine::sendCommand(
    int command, char* daemonSinfulString, CommandFunction function )
{
    dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand %s to %s\n",
               getCommandStringSafe( command ), daemonSinfulString );
    Daemon  daemon( DT_ANY, daemonSinfulString );
    ReliSock socket;

    // no retries after 'm_connectionTimeout' seconds of unsuccessful connection
    socket.timeout( m_connectionTimeout );
    socket.doNotEnforceMinimalCONNECT_TIMEOUT( );

    if( ! socket.connect( daemonSinfulString, 0, false ) ) {
        dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand "
                            "unable to connect to %s\n",
                   daemonSinfulString );
		socket.close( );

        return ;
    }
// General actions for any command sending
    if( ! daemon.startCommand( command, &socket, m_connectionTimeout ) ) {
        dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand "
                            "cannot start command %s to %s\n",
                   getCommandStringSafe( command ), daemonSinfulString );
		socket.close( );

        return ;
    }

    char const* sinfulString = daemonCore->InfoCommandSinfulString();
    if(! socket.put( sinfulString )/* || ! socket.end_of_message( )*/) {
        dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand "
                            "unable to code the local sinful string or eom%s\n",
                   sinfulString );
		socket.close( );

        return ;
    }
    else {
        dprintf( D_FULLDEBUG, "AbstractReplicatorStateMachine::sendCommand "
                              "local sinful string coded successfully\n" );
    }
// End of General actions for any command sending

// Command-specific actions
	if( ! ((*this).*(function))( socket ) ) {
    	socket.close( );

		return ;
	}
// End of Command-specific actions
	if( ! socket.end_of_message( ) ) {
		socket.close( );
       	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand "
                            "unable to code the end of message\n" );
       	return ;
   	}

	socket.close( );
   	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand "
                       "%s command sent to %s successfully\n",
             getCommandStringSafe( command ), daemonSinfulString );
}

// specific command function - sends local daemon's version over the socket
bool
AbstractReplicatorStateMachine::versionCommand( ReliSock& socket )
{
    dprintf( D_ALWAYS,
        "AbstractReplicatorStateMachine::versionCommand started\n" );

    if( ! m_myVersion.code( socket ) ) {
        dprintf( D_NETWORK, "AbstractReplicatorStateMachine::versionCommand "
                            "unable to code the replica\n");
        return false;
    } 
    dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::versionCommand "
                       "sent command successfully\n" );
    return true;
}

// specific command function - sends local daemon's version and state over 
// the socket
bool
AbstractReplicatorStateMachine::versionAndStateCommand(ReliSock& socket)
{
    if( ! versionCommand( socket ) ) {
            return false;
    }
	int stateAsInteger = int( m_state );

    if( ! socket.code( stateAsInteger ) /*|| ! socket.end_of_message( )*/ ) {
        dprintf( D_NETWORK,
            "AbstractReplicatorStateMachine::versionAndStateCommand "
            "unable to code the state or eom%d\n", m_state );
        return false;
    }
    dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::versionAndStateCommand "
                       "sent command successfully\n" );
    return true;
}

// cancels all the data, considering transferers, both uploading and downloading
// such as pids and last times of creation, and sends SIGKILL signals to the
// stuck processes
void
AbstractReplicatorStateMachine::killTransferers()
{
    if( m_downloadTransfererMetadata.isValid() ) {
       /* Beware of sending SIGKILL with download transferer's pid = -1, because
        * according to POSIX it will be sent to every process that the
        * current process is able to sent signals to
        */
        dprintf( D_FULLDEBUG,
            "AbstractReplicatorStateMachine::killTransferers "
            "killing downloading condor_transferer pid = %d\n",
                   m_downloadTransfererMetadata.m_pid );
        //kill( m_downloadTransfererMetadata.m_pid, SIGKILL );
        daemonCore->Send_Signal( m_downloadTransfererMetadata.m_pid, SIGKILL );
		// when the process is killed, it could have not yet erased its
        // temporary files, this is why we ensure it by erasing it in killer
        // function
        std::string extension;
        // the .down ending is needed in order not to confuse between upload and
        // download processes temporary files
        formatstr( extension, "%d.%s", m_downloadTransfererMetadata.m_pid,
                   DOWNLOADING_TEMPORARY_FILES_EXTENSION );

        FilesOperations::safeUnlinkFile( m_versionFilePath.c_str( ),
                                         extension.c_str( ) );
        FilesOperations::safeUnlinkFile( m_stateFilePath.c_str( ),
                                         extension.c_str( ) );
		m_downloadTransfererMetadata.set();
    }

	m_uploadTransfererMetadataList.Rewind( );

	ProcessMetadata* uploadTransfererMetadata = NULL;    

    while( m_uploadTransfererMetadataList.Next( uploadTransfererMetadata ) ) {
        if( uploadTransfererMetadata->isValid( ) ) {
            dprintf( D_FULLDEBUG,
                "AbstractReplicatorStateMachine::killTransferers "
                "killing uploading condor_transferer pid = %d\n",
                uploadTransfererMetadata->m_pid );
            //kill( uploadTransfererMetadata->m_pid, SIGKILL );
			daemonCore->Send_Signal( uploadTransfererMetadata->m_pid, SIGKILL );
			
			            // when the process is killed, it could have not yet
			            // erased its
            // temporary files, this is why we ensure it by erasing it in killer
            // function
            std::string extension;
            // the .up ending is needed in order not to confuse between
            // upload and download processes temporary files
            formatstr( extension, "%d.%s", uploadTransfererMetadata->m_pid,
                       UPLOADING_TEMPORARY_FILES_EXTENSION );

            FilesOperations::safeUnlinkFile( m_versionFilePath.c_str( ),
                                             extension.c_str( ) );
            FilesOperations::safeUnlinkFile( m_stateFilePath.c_str( ),
                                             extension.c_str( ) );
			delete uploadTransfererMetadata;
			// after deletion the iterator is moved to the previous member
			// so advancing the iterator twice and missing one entry does not
			// happen
        	m_uploadTransfererMetadataList.DeleteCurrent( );
		}
    }
	m_uploadTransfererMetadataList.Rewind( );
}
