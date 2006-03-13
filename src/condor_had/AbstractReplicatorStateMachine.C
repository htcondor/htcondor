#include "../condor_daemon_core.V6/condor_daemon_core.h"
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
template class Item<Version>;
template class List<Version>;
template class Item<AbstractReplicatorStateMachine::ProcessMetadata>;
template class List<AbstractReplicatorStateMachine::ProcessMetadata>;
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
    m_releaseDirectoryPath = "";

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
            char buffer[BUFSIZ];

			sprintf( buffer, 
					"AbstractReplicatorStateMachine::initializeReplicationList"
                    " invalid address %s\n", replicationAddress );
            utilCrucialError( buffer );

            continue;
        }
        if( strcmp( sinfulAddress,
                    daemonCore->InfoCommandSinfulString( ) ) == 0 ) {
            isMyAddressPresent = true;
        }
        else {
            m_replicationDaemonsList.insert( sinfulAddress );
        }
        // pay attention to release memory allocated by malloc with free and by
        // new with delete here utilToSinful returns memory allocated by malloc
        free( sinfulAddress );
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
		  								       "REPLICATION").GetCStr( ) );
    }
    buffer = param( "HAD_CONNECTION_TIMEOUT" );

    if( buffer ) {
        m_connectionTimeout = strtol( buffer, 0, 10 );

        if( errno == ERANGE || m_connectionTimeout <= 0 ) {
        	utilCrucialError( utilConfigurationError( "HAD_CONNECTION_TIMEOUT",
													  "HAD" ).GetCStr( ) );
        }
        free( buffer );
    } else {
        utilCrucialError( utilNoParameterError("HAD_CONNECTION_TIMEOUT",
										       "HAD").GetCStr( ) );
    }
    buffer = param( "RELEASE_DIR" );

    if( buffer ) {
        m_releaseDirectoryPath.sprintf( "%s/bin", buffer );

        free( buffer );
    } else {
        utilCrucialError( utilConfigurationError("RELEASE_DIR", 
											     "REPLICATION").GetCStr( ) );
    }
	char* spoolDirectory = param( "SPOOL" );
    
    if( spoolDirectory ) {
        m_versionFilePath = spoolDirectory;
        m_versionFilePath += "/";
        m_versionFilePath += VERSION_FILE_NAME;
        
        buffer = param( "NEGOTIATOR_STATE_FILE" );
    
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
                          GetCStr( ) );
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
    Service* service, int pid, int exitStatus)
{
    dprintf( D_ALWAYS,
            "AbstractReplicatorStateMachine::downloadReplicaTransfererReaper "
            "called for process no. %d\n", pid );
    AbstractReplicatorStateMachine* replicatorStateMachine =
         static_cast<AbstractReplicatorStateMachine*>( service );
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
    MyString temporaryFilesExtension( pid );

    temporaryFilesExtension += ".";
    temporaryFilesExtension += DOWNLOADING_TEMPORARY_FILES_EXTENSION;
    // the rotation and the version synchronization appear in the code
    // sequentially, trying to make the gap between them as less as possible;
    // upon failure we do not synchronize the local version, since such
	// downloading is considered invalid
	if( ! FilesOperations::safeRotateFile(
						  replicatorStateMachine->m_versionFilePath.GetCStr(),
										  temporaryFilesExtension.GetCStr()) ||
		! FilesOperations::safeRotateFile(
						    replicatorStateMachine->m_stateFilePath.GetCStr(),
										  temporaryFilesExtension.GetCStr()) ) {
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
    Service* service, int pid, int exitStatus)
{
    dprintf( D_ALWAYS,
        "AbstractReplicatorStateMachine::uploadReplicaTransfererReaper "
        "called for process no. %d\n", pid );
    AbstractReplicatorStateMachine* replicatorStateMachine =
        static_cast<AbstractReplicatorStateMachine*>( service );
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
    MyString executable;

    executable.sprintf( "%s/condor_transferer",
                                m_releaseDirectoryPath.GetCStr( ) );
# if 0
    MyString processArguments;
    processArguments.sprintf( "%s -f down %s %s 1 %s",
                              executable.GetCStr( ),
                              daemonSinfulString,
                              m_versionFilePath.GetCStr( ),
                              m_stateFilePath.GetCStr( ) );
    dprintf( D_FULLDEBUG, "AbstractReplicatorStateMachine::download creating "
                          "downloading condor_transferer process: \n \"%s\"\n",
			 processArguments.GetCStr( ) );
# else
	ArgList  processArguments;
	processArguments.AppendArg( executable.GetCStr() );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "down" );
	processArguments.AppendArg( daemonSinfulString );
	processArguments.AppendArg( m_versionFilePath.GetCStr() );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( m_stateFilePath.GetCStr() );

	// Get arguments from this ArgList object for descriptional purposes.
	MyString	s;
	processArguments.GetArgsStringForDisplay( &s );
    dprintf( D_FULLDEBUG,
			 "AbstractReplicatorStateMachine::download creating "
			 "downloading condor_transferer process: \n \"%s\"\n",
			 s.GetCStr( ) );
# endif
    // PRIV_USER_FINAL privilege is necessary here to create a user process,
    // after setting it to PRIV_UNKNOWN, the transferer process failed to
    // create when the pool was started with real uid of 'root'
	priv_state privilege;

	if( ! getProcessPrivilege(privilege) ) {
		dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::download unable to "
						   "grant to the transferer the necessary privilege\n");
		return false;
	}
	int transfererPid = daemonCore->Create_Process(
        executable.GetCStr( ),        // name
        processArguments,             // args
        privilege,                    // priv
        m_downloadReaperId,           // reaper id
        FALSE,                        // command port needed?
        NULL,                         // env
        NULL,                         // cwd
        FALSE                         // new process group
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

// creating uploading transferer process and remembering its pid and
// creation time
bool
AbstractReplicatorStateMachine::upload( const char* daemonSinfulString )
{
    MyString executable;

    executable.sprintf( "%s/condor_transferer",
                                m_releaseDirectoryPath.GetCStr( ) );

# if 0
    MyString processArguments;
    processArguments.sprintf( "%s -f up %s %s 1 %s",
							  executable.GetCStr( ),
                              daemonSinfulString,
                              m_versionFilePath.GetCStr( ),
                              m_stateFilePath.GetCStr( ) );
    dprintf( D_FULLDEBUG,
			 "AbstractReplicatorStateMachine::upload creating uploading "
			 "condor_transferer process:\n\"%s\"\n",
			 processArguments.GetCStr( ) );
# else
	ArgList  processArguments;
	processArguments.AppendArg( executable.GetCStr() );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "up" );
	processArguments.AppendArg( daemonSinfulString );
	processArguments.AppendArg( m_versionFilePath.GetCStr() );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( m_stateFilePath.GetCStr() );

	// Get arguments from this ArgList object for descriptional purposes.
	MyString	s;
	processArguments.GetArgsStringForDisplay( &s );
    dprintf( D_FULLDEBUG,
			 "AbstractReplicatorStateMachine::upload creating "
			 "uploading condor_transferer process: \n \"%s\"\n",
			 s.GetCStr( ) );
# endif

	// PRIV_USER_FINAL privilege is necessary here to create a user process,
	// after setting it to PRIV_UNKNOWN, the transferer process failed to
	// create when the pool was started with real uid of 'root'
	priv_state privilege;

	if( ! getProcessPrivilege(privilege) ) {
        dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::upload unable to "
                           "grant to the transferer the necessary privilege\n");
        return false;
    }

    int transfererPid = daemonCore->Create_Process(
        executable.GetCStr( ),        // name
        processArguments,             // args
        privilege,                    // priv
        m_uploadReaperId,             // reaper id
        FALSE,                        // command port needed?
        NULL,                         // envs
        NULL,                         // cwd
        FALSE                         // new process group
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
         newVersion.toString( ).GetCStr( ) );
    m_versionsList.Append( newVersion );
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
               utilToString( command ), daemonSinfulString );
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
                   utilToString( command ), daemonSinfulString );
		socket.close( );

        return ;
    }

    char* sinfulString = daemonCore->InfoCommandSinfulString();
    if(! socket.code( sinfulString )/* || ! socket.eom( )*/) {
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
	if( ! socket.eom( ) ) {
		socket.close( );
       	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand "
                            "unable to code the end of message\n" );
       	return ;
   	}

	socket.close( );
   	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::sendCommand "
                       "%s command sent to %s successfully\n",
             utilToString( command ), daemonSinfulString );
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

    if( ! socket.code( stateAsInteger ) /*|| ! socket.eom( )*/ ) {
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
			delete uploadTransfererMetadata;
			// after deletion the iterator is moved to the previous member
			// so advancing the iterator twice and missing one entry does not
			// happen
        	m_uploadTransfererMetadataList.DeleteCurrent( );
		}
    }
	m_uploadTransfererMetadataList.Rewind( );
}

bool 
AbstractReplicatorStateMachine::getProcessPrivilege(priv_state& privilege)
{
	// Create the priv state for the process
	//priv_state priv;
# ifdef WIN32
	// WINDOWS
	privilege = PRIV_CONDOR;
# else
	// UNIX
	privilege = PRIV_USER_FINAL;
	uid_t uid = get_condor_uid( );
	
	if ( uid == (uid_t) -1 )
	{
		dprintf( D_ALWAYS, "Cron: Invalid UID -1\n" );
		
		return false;
	}
	gid_t gid = get_condor_gid( );
	
	if ( gid == (uid_t) -1 )
	{
		dprintf( D_ALWAYS, "Cron: Invalid GID -1\n" );
		
		return false;
	}
	// tells DaemonCore what uid/gid to use for PRIV_USER_FINAL
	set_user_ids( uid, gid );
# endif

	return true;
	//return priv;
}
