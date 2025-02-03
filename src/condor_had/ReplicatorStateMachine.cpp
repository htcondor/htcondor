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
// for 'daemonCore'
#include "condor_daemon_core.h"
// for 'param' function
#include "condor_config.h"
#include "my_username.h"

#include "ReplicatorStateMachine.h"
//#include "HadCommands.h"
//#include "ReplicationCommands.h"
#include "FilesOperations.h"

#include "ipv6_hostname.h"

// multiplicative factor, determining how long the active HAD, that does not 
// send the messages to the replication daemon, is considered alive
#define HAD_ALIVE_TOLERANCE_FACTOR      (2)
// multiplicative factor, determining how long the newly joining machine is
// allowed to download the version and state files of other pool machines
#define NEWLY_JOINED_TOLERANCE_FACTOR   (2)

ReplicatorStateMachine::ReplicatorStateMachine()
{
	dprintf( D_ALWAYS, "ReplicatorStateMachine ctor started\n" );
   	m_state                       = VERSION_REQUESTING;
   	m_replicationTimerId          = -1;
   	m_versionRequestingTimerId    = -1;
   	m_versionDownloadingTimerId   = -1;
   	m_replicationInterval         = -1;
   	m_hadAliveTolerance           = -1;
   	m_maxTransfererLifeTime       = -1;
   	m_newlyJoinedWaitingVersionInterval = -1;
   	m_lastHadAliveTime          = -1;
   	srand( time(nullptr) & 0xfffffff);
	m_classAd = NULL;
	m_updateCollectorTimerId = -1;
	m_updateInterval = -1;
}
// finalizing the delta, belonging to this class only, since the data, belonging
// to the base class is finalized implicitly
ReplicatorStateMachine::~ReplicatorStateMachine( )
{
	dprintf( D_ALWAYS, "ReplicatorStateMachine dtor started\n" );
    finalizeDelta( );
}
/* Function   : finalize
 * Description: clears and resets all inner structures and data members
 */
void
ReplicatorStateMachine::finalize()
{
	dprintf( D_ALWAYS, "ReplicatorStateMachine::finalize started\n" );
    finalizeDelta( );
    AbstractReplicatorStateMachine::finalize( );
}
/* Function   : finalizeDelta
 * Description: clears and resets all inner structures and data members declared
 *				in this class only (excluding inherited classes)
 */
void
ReplicatorStateMachine::finalizeDelta( )
{
    ClassAd invalidate_ad;
    std::string line;

    dprintf( D_ALWAYS, "ReplicatorStateMachine::finalizeDelta started\n" );
    utilCancelTimer(m_replicationTimerId);
    utilCancelTimer(m_versionRequestingTimerId);
    utilCancelTimer(m_versionDownloadingTimerId);
    utilCancelTimer(m_updateCollectorTimerId);
    m_replicationInterval               = -1;
    m_hadAliveTolerance                 = -1;
    m_maxTransfererLifeTime             = -1;
    m_newlyJoinedWaitingVersionInterval = -1;
    m_lastHadAliveTime                  = -1;
    m_updateInterval                    = -1;

    if ( m_classAd != NULL ) {
        delete m_classAd;
        m_classAd = NULL;
    }

    SetMyTypeName( invalidate_ad, QUERY_ADTYPE );
    invalidate_ad.Assign(ATTR_TARGET_TYPE, REPLICATION_ADTYPE );
    formatstr( line, "%s == \"%s\"", ATTR_NAME, m_name.c_str( ) );
    invalidate_ad.AssignExpr( ATTR_REQUIREMENTS, line.c_str( ) );
	invalidate_ad.Assign( ATTR_NAME, m_name );
    daemonCore->sendUpdates( INVALIDATE_ADS_GENERIC, &invalidate_ad, NULL, false );
}
void
ReplicatorStateMachine::initialize( )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::initialize started\n" );

    reinitialize( );
    // register commands that the service responds to
    registerCommand(HAD_BEFORE_PASSIVE_STATE);
    registerCommand(HAD_AFTER_ELECTION_STATE);
    registerCommand(HAD_AFTER_LEADER_STATE);
    registerCommand(HAD_IN_LEADER_STATE);
    registerCommand(REPLICATION_LEADER_VERSION);
    registerCommand(REPLICATION_TRANSFER_FILE);
    registerCommand(REPLICATION_TRANSFER_FILE_NEW);
    registerCommand(REPLICATION_NEWLY_JOINED_VERSION);
    registerCommand(REPLICATION_GIVING_UP_VERSION);
    registerCommand(REPLICATION_SOLICIT_VERSION);
    registerCommand(REPLICATION_SOLICIT_VERSION_REPLY);
}
// clears all the inner structures and loads the configuration parameters'
// values again
void
ReplicatorStateMachine::reinitialize()
{
    // delete all configurations and start everything over from the scratch
    finalize( );
    AbstractReplicatorStateMachine::reinitialize( );

    m_myVersion.initialize( m_stateFilePath, m_versionFilePath );

    m_replicationInterval =
		param_integer("REPLICATION_INTERVAL",
					  5 * MINUTE,
					  0); // min value, must be positive
    // deduce HAD alive tolerance
    int hadConnectionTimeout =
		param_integer("HAD_CONNECTION_TIMEOUT",
					  DEFAULT_SEND_COMMAND_TIMEOUT,
					  0); // min value, must be positive
    m_maxTransfererLifeTime =
		param_integer("MAX_TRANSFER_LIFETIME",
					  5 * MINUTE,
					  0); // min value, must be positive
    m_newlyJoinedWaitingVersionInterval =
		param_integer("NEWLY_JOINED_WAITING_VERSION_INTERVAL",
					  NEWLY_JOINED_TOLERANCE_FACTOR * (hadConnectionTimeout + 1),
					  0); // min value, must be positive

    char* buffer = param( "HAD_LIST" );

    if ( buffer ) {
        int count = 0;
        for (const auto& item : StringTokenIterator(buffer)) {
            count++; (void)item;
        }

        free( buffer );
        m_hadAliveTolerance = HAD_ALIVE_TOLERANCE_FACTOR *
                            ( 2 * hadConnectionTimeout * count + 1 );

        dprintf( D_FULLDEBUG, "ReplicatorStateMachine::reinitialize %s=%d\n",
                "HAD_LIST", m_hadAliveTolerance );
    } else {
        utilCrucialError( utilNoParameterError( "HAD_LIST", "HAD" ).c_str( ));
    }

    initializeClassAd();
    int updateInterval = param_integer ( "REPLICATION_UPDATE_INTERVAL", 300 );
    if ( m_updateInterval != updateInterval ) {
        m_updateInterval = updateInterval;

        utilCancelTimer(m_updateCollectorTimerId);

        m_updateCollectorTimerId = daemonCore->Register_Timer ( 0,
               m_updateInterval,
               (TimerHandlercpp) &ReplicatorStateMachine::updateCollectors,
               "ReplicatorStateMachine::updateCollectors", this );
    }

    // set a timer to replication routine
    dprintf( D_ALWAYS, "ReplicatorStateMachine::reinitialize setting "
                                      "replication timer\n" );
    m_replicationTimerId = daemonCore->Register_Timer( m_replicationInterval,
            (TimerHandlercpp) &ReplicatorStateMachine::replicationTimer,
            "Time to replicate file", this );
    // register the download/upload reaper for the transferer process
    if( m_downloadReaperId == -1 ) {
		m_downloadReaperId = daemonCore->Register_Reaper(
        	"downloadReplicaTransfererReaper",
        (ReaperHandlercpp)&ReplicatorStateMachine::downloadReplicaTransfererReaper,
        	"downloadReplicaTransfererReaper", this );
	}
    if( m_uploadReaperId == -1 ) {
		m_uploadReaperId = daemonCore->Register_Reaper(
        	"uploadReplicaTransfererReaper",
        (ReaperHandlercpp) &ReplicatorStateMachine::uploadReplicaTransfererReaper,
        	"uploadReplicaTransfererReaper", this );
    }
	// for debugging purposes only
	printDataMembers( );
	
	beforePassiveStateHandler( );
}

void
ReplicatorStateMachine::initializeClassAd()
{
    if( m_classAd != NULL) {
        delete m_classAd;
        m_classAd = NULL;
    }

    m_classAd = new ClassAd();

    SetMyTypeName(*m_classAd, REPLICATION_ADTYPE);

    formatstr( m_name, "replication@%s -p %d", get_local_fqdn().c_str(),
				  daemonCore->InfoCommandPort( ) );
    m_classAd->Assign( ATTR_NAME, m_name );
    m_classAd->Assign( ATTR_MY_ADDRESS,
					   daemonCore->InfoCommandSinfulString( ) );

    // publish list of replication nodes
    char* buffer = param( "REPLICATION_LIST" );
	if ( NULL == buffer ) {
		EXCEPT( "ReplicatorStateMachine: No replication list!!" );
	}
    std::string attrReplList;
    std::string comma;

    for (const auto& replAddress : StringTokenIterator(buffer)) {
        attrReplList += comma;
        attrReplList += replAddress;
        comma = ",";
    }
    m_classAd->Assign( ATTR_REPLICATION_LIST, attrReplList );

    // publish DC attributes
    daemonCore->publish(m_classAd);
	free(buffer);
}
// sends the version of the last execution time to all the replication daemons,
// then asks the pool replication daemons to send their own versions to it,
// sets a timer to wait till the versions are received
void
ReplicatorStateMachine::beforePassiveStateHandler()
{
    REPLICATION_ASSERT(m_state == VERSION_REQUESTING);
    
    dprintf( D_ALWAYS, 
			"ReplicatorStateMachine::beforePassiveStateHandler started\n" ); 
    broadcastVersion( REPLICATION_NEWLY_JOINED_VERSION );
    requestVersions( );

    dprintf( D_FULLDEBUG, "ReplicatorStateMachine::beforePassiveStateHandler "
			"registering version requesting timer\n" );
    m_versionRequestingTimerId = daemonCore->Register_Timer( 
		m_newlyJoinedWaitingVersionInterval,
       (TimerHandlercpp) &ReplicatorStateMachine::versionRequestingTimer,
       "Time to pass to VERSION_DOWNLOADING state", this );
}

void
ReplicatorStateMachine::afterElectionStateHandler()
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::afterElectionStateHandler "
			"started\n" );
    REPLICATION_ASSERT(m_state != REPLICATION_LEADER);
   
	// we stay in VERSION_REQUESTING or VERSION_DOWNLOADING state
    // of newly joining node, we will go to LEADER_STATE later
    // upon receiving of IN_LEADER message from HAD 
    if( m_state == VERSION_REQUESTING || m_state == VERSION_DOWNLOADING ) {
        return ;
    }

	becomeLeader( );
}

void
ReplicatorStateMachine::afterLeaderStateHandler( )
{
   // REPLICATION_ASSERT(state != BACKUP)
    
    if( m_state == VERSION_REQUESTING || m_state == VERSION_DOWNLOADING ) {
        return ;
    }
	// receiving this notification message in BACKUP state means, that the
    // pool version downloading took more time than it took for the HAD to
    // become active and to give up the leadership, in this case we ignore 
	// this notification message from HAD as well, since we do not want 
	// to broadcast our newly downloaded version to others, because it is 
	// too new
	if( m_state == BACKUP ) {
		return ;
	}
    dprintf( D_ALWAYS, 
			"ReplicatorStateMachine::afterLeaderStateHandler started\n" );
    broadcastVersion( REPLICATION_GIVING_UP_VERSION );
    m_state = BACKUP;
}

void
ReplicatorStateMachine::inLeaderStateHandler( )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::inLeaderStateHandler started "
			"with state = %d\n", int( m_state ) );
    // REPLICATION_ASSERT(m_state != BACKUP)
 
    if( m_state == VERSION_REQUESTING || m_state == VERSION_DOWNLOADING) {
        return ;
    }
	// receiving this notification message in BACKUP state means, that the
    // pool version downloading took more time than it took for the HAD to
    // become active, in this case we act as if we received AFTER_ELECTION
	// message
    if( m_state == BACKUP ) {
		becomeLeader( );

		return ;
	}
	m_lastHadAliveTime = time( NULL );

    dprintf( D_FULLDEBUG,
            "ReplicatorStateMachine::inLeaderStateHandler last HAD alive time "
            "is set to %s", ctime( &m_lastHadAliveTime ) );
    //if( downloadTransferersNumber( ) == 0 && 
	// 	  replicaSelectionHandler( newVersion ) ) {
    //    download( newVersion.getSinfulString( ).c_str( ) );
    //}
}

bool
ReplicatorStateMachine::replicaSelectionHandler( Version& newVersion )
{
    REPLICATION_ASSERT( m_state == VERSION_DOWNLOADING || m_state == BACKUP );
    dprintf( D_ALWAYS, "ReplicatorStateMachine::replicaSelectionHandler "
             "started with my version = %s, #versions = %zu\n",
             m_myVersion.toString( ).c_str( ), m_versionsList.size( ) );
    Version myVersionCopy = m_myVersion;
    
	// in BACKUP state compares the received version with the local one
    if( m_state == BACKUP ) {        
		// compares the versions, taking only 'gid' and 'logicalClock' into
		// account - this is the reason for making the states equal
        myVersionCopy.setState( newVersion );

        return ! newVersion.isComparable( myVersionCopy ) || 
				 newVersion > myVersionCopy;
    }
	/* in VERSION_DOWNLOADING state selecting the best version from the list of
	 * received versions according to the policy defined by 
	 * 'replicaSelectionHandler', i.e. selecting the version with greatest
	 * 'logicalClock' value amongst a group of versions with the same gid
	 */
    if( m_versionsList.empty( ) ) {
        return false;
    }
    // taking the first actual version as the best version in the meantime
    Version bestVersion = *m_versionsList.begin();
    dprintf( D_ALWAYS, "ReplicatorStateMachine::replicaSelectionHandler best "
			"version = %s\n", bestVersion.toString( ).c_str( ) );

    for (const auto& version : m_versionsList) {
        dprintf( D_ALWAYS, "ReplicatorStateMachine::replicaSelectionHandler "
				"actual version = %s\n", version.toString( ).c_str( ) );
        if( version.isComparable( bestVersion ) && version > bestVersion ) {
            bestVersion = version;
        }
    }
    
	// compares the versions, taking only 'gid' and 'logicalClock' into
    // account - this is the reason for making the states equal
    myVersionCopy.setState( bestVersion );

	// either when the versions are incomparable or when the local version
	// is worse, the remote version must be downloaded
    if( myVersionCopy.isComparable( bestVersion ) && 
		myVersionCopy >= bestVersion ) {
        return false;
    }
    newVersion = bestVersion;
    dprintf( D_ALWAYS, "ReplicatorStateMachine::replicaSelectionHandler "
			"best version selected: %s\n", newVersion.toString().c_str()); 
    return true;
}
// until the state files merging utility is ready, the function is not really
// interesting, it selects 0 as the next gid each time
void
ReplicatorStateMachine::gidSelectionHandler( )
{
    REPLICATION_ASSERT( m_state == BACKUP || m_state == REPLICATION_LEADER );
    dprintf( D_ALWAYS, "ReplicatorStateMachine::gidSelectionHandler started\n");
    
    bool          areVersionsComparable = true;

    for (const auto& actualVersion : m_versionsList) {
        if( ! m_myVersion.isComparable( actualVersion ) ) {
            areVersionsComparable = false;
            
            break;
        }    
    }

    if( areVersionsComparable ) {
        dprintf( D_ALWAYS, "ReplicatorStateMachine::gidSelectionHandler no "
				"need to select new gid\n" );
        return ;
    }
    int temporaryGid = 0;

    while( ( temporaryGid = rand( ) ) == m_myVersion.getGid( ) ) { }
    m_myVersion.setGid( temporaryGid );

    dprintf( D_ALWAYS, "ReplicatorStateMachine::gidSelectionHandler "
			"new gid selected: %d\n", temporaryGid );
    //myVersion.setSinfulString( daemonCore->InfoCommandSinfulString( ) );
}
/* Function   : decodeVersionAndState
 * Arguments  : stream - socket, through which the data is received 
 * Description: receives remote replication daemon version and state from the
 *				given socket
 */
bool
ReplicatorStateMachine::decodeVersionAndState( Stream* stream, Version& newVersion )
{
	// decode remote replication daemon version
	if( ! newVersion.decode( stream ) ) {
		dprintf( D_ALWAYS, "ReplicatorStateMachine::decodeVersionAndState "
		         "cannot read remote daemon version\n" );

		return false;
	}
	int remoteReplicatorState;

	stream->decode( );
	// decode remore replication daemon state
	if( ! stream->code( remoteReplicatorState ) ) {
		dprintf( D_ALWAYS, "ReplicatorStateMachine::decodeVersionAndState "
		         "unable to decode the state\n" );

		return false;
	}
	newVersion.setState( ReplicatorState( remoteReplicatorState ) );

	return true;
}
/* Function   : becomeLeader
 * Description: passes to leader state, sets the last time, that HAD sent its
 * 				message and chooses a new gid
 */
void
ReplicatorStateMachine::becomeLeader( )
{
	// sets the last time, when HAD sent a HAD_IN_STATE_STATE
	m_lastHadAliveTime = time( NULL );
    dprintf( D_FULLDEBUG, "ReplicatorStateMachine::becomeLeader "
            "last HAD alive time is set to %s", ctime( &m_lastHadAliveTime ) );       // selects new gid for the pool
    gidSelectionHandler( );
    m_state = REPLICATION_LEADER;
}
/* Function   : onLeaderVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_LEADER_VERSION command; comparing the
 *				received version to the local one and downloading the replica
 *				from the remote replication daemon when the received version is
 *				better than the local one and there is no downloading
 *				'condor_transferer' running at the same time
 */
void
ReplicatorStateMachine::onLeaderVersion( Stream* stream )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::onLeaderVersion started\n" );
    
    if( m_state != BACKUP ) {
        return ;
    }
	checkVersionSynchronization( );

    Version newVersion;
    bool rc = decodeVersionAndState( stream, newVersion );
	// comparing the received version to the local one
    bool downloadNeeded = replicaSelectionHandler( newVersion );
    // downloading the replica from the remote replication daemon, when the
	// received version is better and there is no running downloading
	// 'condor_transferers'  
    if( downloadTransferersNumber( ) == 0 && rc && downloadNeeded ) {
        dprintf( D_FULLDEBUG, "ReplicatorStateMachine::onLeaderVersion "
				"downloading from %s\n", 
				newVersion.getSinfulString( ).c_str( ) );
		if ( newVersion.knowsNewTransferProtocol() ) {
			downloadNew( newVersion.getSinfulString( ).c_str( ) );
		} else {
			download( newVersion.getSinfulString( ).c_str( ) );
		}
    }
    // replication leader must not send a version which hasn't been updated
    //assert(downloadNeeded);
    //REPLICATION_ASSERT(downloadNeeded);
}
/* Function   : onTransferFile
 * Arguments  : daemonSinfulString - the address of remote replication daemon,
 *				which sent the REPLICATION_TRANSFER_FILE command
 * Description: handler of REPLICATION_TRANSFER_FILE command; starting uploading
 * 				the replica from specified replication daemon
 */
void
ReplicatorStateMachine::onTransferFile( char* daemonSinfulString )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::onTransferFile %s started\n",
             daemonSinfulString );
    if( m_state == REPLICATION_LEADER ) {
        upload( daemonSinfulString );
    }
}
void
ReplicatorStateMachine::onTransferFileNew( Stream *stream )
{
	dprintf( D_ALWAYS, "ReplicatorStateMachine::onTransferFileNew started\n" );
	if( m_state == REPLICATION_LEADER ) {
		uploadNew( stream );
	}
}

/* Function   : onSolicitVersion
 * Arguments  : daemonSinfulString - the address of remote replication daemon,
 *              which sent the REPLICATION_SOLICIT_VERSION command 
 * Description: handler of REPLICATION_SOLICIT_VERSION command; sending local
 *				version along with current replication daemon state
 */
void
ReplicatorStateMachine::onSolicitVersion( char* daemonSinfulString )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::onSolicitVersion %s started\n",
             daemonSinfulString );
    if( m_state == BACKUP || m_state == REPLICATION_LEADER ) {
        sendVersionAndStateCommand( REPLICATION_SOLICIT_VERSION_REPLY,
                                    daemonSinfulString );
   }
}
/* Function   : onSolicitVersionReply
 * Arguments  : stream - socket, through which the data is received and sent 
 * Description: handler of REPLICATION_SOLICIT_VERSION_REPLY command; updating
 * 				versions list with newly received remote version
 */
void
ReplicatorStateMachine::onSolicitVersionReply( Stream* stream )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::onSolicitVersionReply "
					   "started\n" );
    Version newVersion;
    
    if( m_state == VERSION_REQUESTING && 
	    decodeVersionAndState( stream, newVersion ) ) {
        updateVersionsList( newVersion );
    }
}
/* Function   : onNewlyJoinedVersion 
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_NEWLY_JOINED_VERSION command; void by now
 */
void
ReplicatorStateMachine::onNewlyJoinedVersion( Stream* /*stream*/ )
{
    dprintf(D_ALWAYS, "ReplicatorStateMachine::onNewlyJoinedVersion started\n");
    
    if( m_state == REPLICATION_LEADER ) {
        // eventually merging files
        //decodeAndAddVersion( stream );
    }
}
/* Function   : onGivingUpVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_GIVING_UP_VERSION command; void by now
 * 				initiating merging two reconciled replication leaders' state 
 *				files and new gid selection (for replication daemon in leader
 *				state)
 */
void
ReplicatorStateMachine::onGivingUpVersion( Stream* /*stream*/ )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::onGivingUpVersion started\n" );
    
    if( m_state == BACKUP) {
        // eventually merging files
    }
    if( m_state == REPLICATION_LEADER ){
        // eventually merging files
        gidSelectionHandler( );
    }
}

int
ReplicatorStateMachine::downloadReplicaTransfererReaper(
	int pid, int exitStatus)
{
    ReplicatorStateMachine* replicatorStateMachine =
    	static_cast<ReplicatorStateMachine*>( this );
    int returnValue = AbstractReplicatorStateMachine::
						downloadReplicaTransfererReaper(pid, 
														exitStatus);
    if( returnValue == TRANSFERER_TRUE && 
		replicatorStateMachine->m_state == VERSION_DOWNLOADING ) {
        replicatorStateMachine->versionDownloadingTimer( );
    }
    return returnValue;
}
/* Function   : commandHandler 
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
						  arrived
 * Description: handles various commands sent to this replication daemon
 */
int
ReplicatorStateMachine::commandHandler( int command, Stream* stream )
{
    char* daemonSinfulString = 0;
   
    stream->decode( );

    if( ! stream->code( daemonSinfulString ) 
		/*|| ! stream->end_of_message( )*/ ) 
	{
        dprintf( D_NETWORK, "ReplicatorStateMachine::commandHandler "
                            "cannot read remote daemon sinful string for %s\n",
                 getCommandStringSafe( command ) );
	    free( daemonSinfulString );

		return FALSE;
    }

    dprintf( /*D_COMMAND*/
             D_FULLDEBUG, "ReplicatorStateMachine::commandHandler received "
			"command %s from %s\n", getCommandStringSafe(command), daemonSinfulString );
    switch( command ) {
        case REPLICATION_LEADER_VERSION:
            onLeaderVersion( stream );
            
            break;
        case REPLICATION_TRANSFER_FILE:
            onTransferFile( daemonSinfulString );
            
            break;
        case REPLICATION_TRANSFER_FILE_NEW:
            onTransferFileNew( stream );
            break;
        case REPLICATION_SOLICIT_VERSION:
            onSolicitVersion( daemonSinfulString );

            break;
        case REPLICATION_SOLICIT_VERSION_REPLY:
            onSolicitVersionReply( stream );

            break;
        case REPLICATION_NEWLY_JOINED_VERSION:
            onNewlyJoinedVersion( stream );
            
            break;
        case REPLICATION_GIVING_UP_VERSION:
            onGivingUpVersion( stream );
            
            break;
        case HAD_BEFORE_PASSIVE_STATE:
            beforePassiveStateHandler();

            break;
        case HAD_AFTER_ELECTION_STATE:
            afterElectionStateHandler();

            break;
        case HAD_AFTER_LEADER_STATE:
            afterLeaderStateHandler();

            break;
        case HAD_IN_LEADER_STATE:
            inLeaderStateHandler();

            break;
    }
	free( daemonSinfulString );

    if( ! stream->end_of_message( ) ) {
        dprintf( D_NETWORK, "ReplicatorStateMachine::commandHandler "
                            "cannot read the end of the message\n" );
    }
    return FALSE;
}
/* Function   : registerCommand 
 * Arguments  : command - id to register
 * Description: register command with given id in daemon core
 */
void
ReplicatorStateMachine::registerCommand(int command)
{
    daemonCore->Register_Command(
        command, getCommandStringSafe( command ),
        (CommandHandlercpp) &ReplicatorStateMachine::commandHandler,
        "commandHandler", this, DAEMON );
}
/* Function   : killStuckDownloadingTransferer
 * Description: kills downloading transferer process, if its working time
 *				exceeds MAX_TRANSFER_LIFETIME seconds, and clears all the data
 *				regarding it, i.e. pid and last time of creation 
 */
void 
ReplicatorStateMachine::killStuckDownloadingTransferer( time_t currentTime )
{
    // killing stuck downloading 'condor_transferer'
    if( m_downloadTransfererMetadata.isValid( ) &&
        currentTime - m_downloadTransfererMetadata.m_lastTimeCreated >
            m_maxTransfererLifeTime ) {
       /* Beware of sending signal with downloadTransfererPid = -1, because
        * according to POSIX it will be sent to every process that the
        * current process is able to sent signals to
        */
        dprintf( D_FULLDEBUG, 
				"ReplicatorStateMachine::killStuckDownloadingTransferer "
                "killing downloading condor_transferer pid = %d\n",
                 m_downloadTransfererMetadata.m_pid );
		// sending SIGKILL signal, wrapped in daemon core function for
		// portability
    	if( !daemonCore->Send_Signal( m_downloadTransfererMetadata.m_pid, 
									 SIGKILL ) ) {
        	dprintf( D_ALWAYS, 
                     "ReplicatorStateMachine::killStuckDownloadingTransferer"
                     " kill signal failed, reason = %s\n", strerror(errno));
        }
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
		m_downloadTransfererMetadata.set( );
	}
}
/* Function   : killStuckUploadingTransferers
 * Description: kills uploading transferer processes, the working time of which
 *              exceeds MAX_TRANSFER_LIFETIME seconds, and clears all the data,
 *				regarding them, i.e. pids and last times of creation
 */
void 
ReplicatorStateMachine::killStuckUploadingTransferers( time_t currentTime )
{
	// killing stuck uploading 'condor_transferers'
	auto stuck_check = [&](const ProcessMetadata& uploadTransfererMetadata) {
		if( uploadTransfererMetadata.isValid( ) &&
			currentTime - uploadTransfererMetadata.m_lastTimeCreated >
              m_maxTransfererLifeTime ) {
            dprintf( D_FULLDEBUG, 
					"ReplicatorStateMachine::killStuckUploadingTransferers "
                    "killing uploading condor_transferer pid = %d\n",
                    uploadTransfererMetadata.m_pid );
			// sending SIGKILL signal, wrapped in daemon core function for
        	// portability
			if( !daemonCore->Send_Signal( 
				uploadTransfererMetadata.m_pid, SIGKILL ) ) {
				dprintf( D_ALWAYS, 
						 "ReplicatorStateMachine::killStuckUploadingTransferers"
						 " kill signal failed, reason = %s\n", strerror(errno));
			}
			// when the process is killed, it could have not yet erased its
        	// temporary files, this is why we ensure it by erasing it in killer
        	// function	
			std::string extension;
            // the .up ending is needed in order not to confuse between
            // upload and download processes temporary files
			formatstr( extension, "%d.%s", uploadTransfererMetadata.m_pid,
			           UPLOADING_TEMPORARY_FILES_EXTENSION );

            FilesOperations::safeUnlinkFile( m_versionFilePath.c_str( ),
                                             extension.c_str( ) );
            FilesOperations::safeUnlinkFile( m_stateFilePath.c_str( ),
                                             extension.c_str( ) );
			return true;
		} else {
			return false;
		}
    };
	std::erase_if(m_uploadTransfererMetadataList, stuck_check);
}
/* Function   : replicationTimer 
 * Description: replication daemon life cycle handler
 * Remarks    : fired upon expiration of timer, designated by 
 *              'm_replicationTimerId' each 'REPLICATION_INTERVAL' seconds
 */
void
ReplicatorStateMachine::replicationTimer( int /* timerID */ )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::replicationTimer "
					   "cancelling timer\n" );
    utilCancelTimer(m_replicationTimerId);

    dprintf( D_ALWAYS, "ReplicatorStateMachine::replicationTimer "
                       "registering timer once again\n" );
    m_replicationTimerId = daemonCore->Register_Timer ( m_replicationInterval,
                    (TimerHandlercpp) &ReplicatorStateMachine::replicationTimer,
                    "Time to replicate file", this );
    if( m_state == VERSION_REQUESTING ) {
        return ;
    }
	time_t currentTime = time( NULL );
    /* Killing stuck uploading/downloading processes: allowing downloading/
     * uploading for about several replication intervals only
     */
// TODO: Atomic operation
	killStuckDownloadingTransferer( currentTime );    
// End of TODO: Atomic operation
	if( m_state == VERSION_DOWNLOADING ) {
        return ;
    }
// TODO: Atomic operation
	killStuckUploadingTransferers( currentTime );
// End of TODO: Atomic operation
    dprintf( D_FULLDEBUG, "ReplicatorStateMachine::replicationTimer "
						  "# downloading condor_transferer = %d, "
						  "# uploading condor_transferer = %zu\n",
             downloadTransferersNumber( ), 
			 m_uploadTransfererMetadataList.size( ) );
    if( m_state == BACKUP ) {
		checkVersionSynchronization( );

		return ;
    }
    dprintf( D_ALWAYS, "ReplicatorStateMachine::replicationTimer "
					   "synchronizing the "
					   "local version with actual state file\n" );
	// if after the version synchronization, the file update was tracked, the
	// local version is broadcasted to the entire pool
    if( m_myVersion.synchronize( true ) ) {
        broadcastVersion( REPLICATION_LEADER_VERSION );
    }
    dprintf( D_FULLDEBUG, "ReplicatorStateMachine::replicationTimer %d seconds "
						  "without HAD_IN_LEADER_STATE\n",
             int( currentTime - m_lastHadAliveTime ) );
	// allowing to remain replication leader without HAD_IN_LEADER_STATE
	// messages for about 'HAD_ALIVE_TOLERANCE' seconds only
    if( currentTime - m_lastHadAliveTime > m_hadAliveTolerance) {
        broadcastVersion( REPLICATION_GIVING_UP_VERSION );
        m_state = BACKUP;
    }
}
/* Function   : versionRequestingTimer
 * Description: timer, expiration of which means stopping collecting the pool
 *				versions in VERSION_REQUESTING state, passing to
 *				VERSION_DOWNLOADING state and starting downloading from the
 *				machine with the best version
 */
void
ReplicatorStateMachine::versionRequestingTimer( int /* timerID */ )
{
    dprintf( D_ALWAYS, 
			"ReplicatorStateMachine::versionRequestingTimer started\n" );
    utilCancelTimer(m_versionRequestingTimerId);
    dprintf( D_FULLDEBUG, "ReplicatorStateMachine::versionRequestingTimer "
			"cancelling version requesting timer\n" );
    m_state = VERSION_DOWNLOADING;
    // selecting the best version amongst all the versions that have been sent
    // by other replication daemons
    Version updatedVersion;

    if( replicaSelectionHandler( updatedVersion ) ) {
		if ( updatedVersion.knowsNewTransferProtocol() ) {
			downloadNew( updatedVersion.getSinfulString( ).c_str( ) );
		} else {
			download( updatedVersion.getSinfulString( ).c_str( ) );
		}
        dprintf( D_FULLDEBUG, "ReplicatorStateMachine::versionRequestingTimer "
				"registering version downloading timer\n" );
        m_versionDownloadingTimerId = daemonCore->Register_Timer( 
			m_maxTransfererLifeTime,
            (TimerHandlercpp) &ReplicatorStateMachine::versionDownloadingTimer,
            "Time to pass to BACKUP state", this );
    } else {
        versionDownloadingTimer( );
    }
}
/* Function   : versionDownloadingTimer
 * Description: timer, expiration of which means stopping downloading the best
 *              pool version in VERSION_DOWNLOADING state and passing to
 *              BACKUP state
 */
void
ReplicatorStateMachine::versionDownloadingTimer( int /* timerID */ )
{
    dprintf( D_ALWAYS, 
			"ReplicatorStateMachine::versionDownloadingTimer started\n" );
    utilCancelTimer(m_versionDownloadingTimerId);
    dprintf( D_FULLDEBUG, "ReplicatorStateMachine::versionDownloadingTimer "
			"cancelling version downloading timer\n" );
	m_versionsList.clear();
    
	checkVersionSynchronization( );	

	m_state = BACKUP;
}

/* Function    : updateCollectors
 * Description : sends the classad update to collectors
 */
void
ReplicatorStateMachine::updateCollectors( int /* timerID */ )
{
	if (m_classAd) {
		daemonCore->sendUpdates (UPDATE_AD_GENERIC, m_classAd, nullptr, true);
	}
}
