/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison,
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


/*
  StateMachine.cpp: implementation of the HADStateMachine class.
*/

#include "condor_common.h"
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_query.h"
#include "daemon.h"
#include "daemon_types.h"
#include "internet.h"
// for 'my_username'
#include "my_username.h"
#include "StateMachine.h"
#include "Utils.h"
#include "ipv6_hostname.h"


#define MESSAGES_PER_INTERVAL_FACTOR (2)
#define DEFAULT_HAD_UPDATE_INTERVAL  (5 * MINUTE)


/***********************************************************
  Function :
*/
HADStateMachine::HADStateMachine(void)
{
    init();
}

/***********************************************************
  Function :
*/
void
HADStateMachine::init(void)
{
    m_state = PRE_STATE;
    m_masterDaemon = NULL;
	m_controlleeName = NULL;
    m_isPrimary = false;
    m_usePrimary = false;
    m_standAloneMode = false;
    m_callsCounter = 0;
    m_stateMachineTimerID = -1;
    m_hadInterval = -1;
    // set valid value to send NEGOTIATOR_OFF
    m_connectionTimeout = DEFAULT_SEND_COMMAND_TIMEOUT;
    m_selfId = -1;

	// replication-specific initializations
    m_useReplication = false;
    m_replicationDaemonSinfulString = NULL;
    // classad-specific initializations
    m_updateCollectorTimerId = -1;
    m_updateCollectorInterval = -1;
}

/***********************************************************
  Function :  freeResources() - free all resources
*/
void
HADStateMachine::freeResources(void)
{
    m_state = PRE_STATE;
    m_connectionTimeout = DEFAULT_SEND_COMMAND_TIMEOUT;

    if( m_masterDaemon != NULL ){
        // always kill leader when HAD dies - if I am leader I should
        // kill my "controlled daemon".  If I am not a leader - my
		// controllee should not be on anyway
        sendControlCmdToMaster( CHILD_OFF_FAST );
        delete m_masterDaemon;
        m_masterDaemon = NULL;
    }
    m_isPrimary = false;
    m_usePrimary = false;
    m_standAloneMode = false;
    m_callsCounter = 0;
	m_hadInterval = -1;
    utilCancelTimer( m_stateMachineTimerID );

    m_selfId = -1;

    clearBuffers();

	// replication-specific finalizings
    m_useReplication = false;

	if( m_replicationDaemonSinfulString != NULL ) {
		free(m_replicationDaemonSinfulString);
		m_replicationDaemonSinfulString = NULL;
    }

	// classad finalizings
	utilCancelTimer( m_updateCollectorTimerId );
    m_updateCollectorInterval = -1;
}

/***********************************************************
  Function :
*/
HADStateMachine::~HADStateMachine(void)
{

    ClassAd invalidate_ad;
    std::string line;

    freeResources();

    SetMyTypeName( invalidate_ad, QUERY_ADTYPE );
    invalidate_ad.Assign(ATTR_TARGET_TYPE, HAD_ADTYPE );
    formatstr( line, "TARGET.%s == \"%s\"", ATTR_NAME, m_name.c_str( ) );
    invalidate_ad.AssignExpr( ATTR_REQUIREMENTS, line.c_str( ) );
	invalidate_ad.Assign( ATTR_NAME, m_name );
    daemonCore->sendUpdates( INVALIDATE_HAD_ADS, &invalidate_ad, NULL, false );
}


bool
HADStateMachine::isHardConfigurationNeeded(void)
{
	char		*tmp      = NULL;
	char		 controllee[128];
	int     	 selfId = -1;
	std::vector<std::string> allHadIps;

	tmp = param( "HAD_CONTROLLEE" );
	if ( tmp ) {
		strncpy( controllee, tmp, sizeof(controllee) - 1 );
        free( tmp );
	}
	else {
		strncpy( controllee, daemonString(DT_NEGOTIATOR), sizeof(controllee) - 1 );
	}
	controllee[COUNTOF(controllee)-1] = 0; // make sure it's null terminated.
	if ( strcasecmp(controllee, m_controlleeName) ) {
		return true;
	}

	if ( param_boolean("HAD_USE_PRIMARY", false) != m_usePrimary ) {
		return true;
	}

    tmp = param( "HAD_LIST" );
    if ( tmp ) {
		std::vector<std::string> otherHadIps;
       	// we do not care for the return value here, so there is no importance
		// whether we use the 'usePrimaryCopy' or 'm_usePrimary'
		getHadList( tmp, m_usePrimary, otherHadIps, allHadIps, selfId );
        free( tmp );
    } else {
		utilCrucialError( utilNoParameterError( "HAD_LIST", "HAD" ).c_str() );
    }

	// if either the HAD_LIST length has changed or the index of the local
	// HAD in the list has changed or the rest of remote HAD sinful strings
	// has changed their order or value, we do need the hard reconfiguration
	if( m_selfId != selfId  || m_allHadIps != allHadIps ) {
		return true;
	}

	return false;
}

int
HADStateMachine::softReconfigure(void)
{
	// initializing necessary data members to prepare for the reconfiguration
	m_standAloneMode          = false;
	m_connectionTimeout       = DEFAULT_SEND_COMMAND_TIMEOUT;
	m_hadInterval             = -1;
	m_useReplication          = false;
	m_updateCollectorInterval = -1;

	if( m_replicationDaemonSinfulString != NULL ) {
		free( m_replicationDaemonSinfulString );
		m_replicationDaemonSinfulString = NULL;
	}

	// reconfiguration
	char		*buffer = NULL;
	const char	*tmp;

        // 'my_username' allocates dynamic string
        buffer = my_username();
        tmp = buffer ? buffer : "UNKNOWN";
        formatstr( m_name, "%s@%s", tmp, get_local_fqdn().c_str() );
	if ( buffer ) {
		free( buffer );
	}

	buffer = param( "HAD_STAND_ALONE_DEBUG" );

    if( buffer ){
		// for now we disable 'standalone' mode, since there is no need to use
		// it in the production code
		m_standAloneMode = false;
        // m_standAloneMode = true;
        free( buffer );
    }

	m_connectionTimeout = param_integer("HAD_CONNECTION_TIMEOUT",
										DEFAULT_SEND_COMMAND_TIMEOUT,
										0); // min value

    // calculate m_hadInterval
    int safetyFactor = 1;

    // timeoutNumber
    // connect + startCommand(sock.code() and sock.end_of_message() aren't 
	//blocking)
    int timeoutNumber = 2;

    int time_to_send_all = (m_connectionTimeout*timeoutNumber);
    time_to_send_all *= (m_otherHadIps.size() + 1); //otherHads + master

	m_hadInterval = (time_to_send_all + safetyFactor)*
                  (MESSAGES_PER_INTERVAL_FACTOR);

    // setting the replication usage permissions
	m_useReplication = param_boolean("HAD_USE_REPLICATION", m_useReplication);
    setReplicationDaemonSinfulString( );

	dprintf(D_ALWAYS,
			"HADStateMachine::softReconfigure classad information\n");
    initializeClassAd( );
    m_updateCollectorInterval = param_integer ("HAD_UPDATE_INTERVAL",
                                             DEFAULT_HAD_UPDATE_INTERVAL );
	printParamsInformation();

	return TRUE;
}

/***********************************************************
  Function :
*/
void
HADStateMachine::initialize(void)
{
    reinitialize ();
    daemonCore->Register_Command (
		HAD_ALIVE_CMD, "ALIVE command",
		(CommandHandlercpp) &HADStateMachine::commandHandlerHad,
		"commandHandler", (Service*) this, DAEMON );

    daemonCore->Register_Command (
		HAD_SEND_ID_CMD, "SEND ID command",
		(CommandHandlercpp) &HADStateMachine::commandHandlerHad,
		"commandHandler", (Service*) this, DAEMON );
}

void
HADStateMachine::initializeClassAd(void)
{
	m_classAd.Clear();

    SetMyTypeName(m_classAd, HAD_ADTYPE);

    // ATTR_NAME is mandatory in order to be accepted by collector
    m_classAd.Assign( ATTR_NAME, m_name );
    m_classAd.Assign( ATTR_MY_ADDRESS,
					  daemonCore->InfoCommandSinfulString() );

	// declaring boolean attributes this way, no need for \"False\"
    m_classAd.Assign( ATTR_HAD_IS_ACTIVE, false );

	// publishing list of hads in classad
	if ( m_allHadIps.empty() ) {
		m_classAd.Assign( ATTR_HAD_LIST, "UNKNOWN" );
	}
	else {
		m_classAd.Assign( ATTR_HAD_LIST, join(m_allHadIps, ",") );
	}

	// Publish the "controllee" name
	m_classAd.Assign( ATTR_HAD_CONTROLLEE_NAME, m_controlleeName );

	// publishing had's real index in list of hads
	m_classAd.Assign( ATTR_HAD_INDEX, m_allHadIps.size() - 1 - m_selfId );

	// publish my "self ID" 
	m_classAd.Assign( ATTR_HAD_SELF_ID, m_selfId );

	// publish all of the DC-specific attributes, SUBSYS_ATTRS, etc.
	daemonCore->publish( &m_classAd );
}

/***********************************************************
  Function : reinitialize -
    delete all previuosly alocated memory, read all config params from
    config_file again and init all relevant parameters

    Checking configurations parameters:
    -----------------------------------

    HAD_ID is given.
    HAD_LIST is given and all addresses in it are in the formats :
         (<IP:port>),(IP:port),(<name:port>),(name:port)
    HAD_CYCLE_INTERVAL is given

    Daemon command port and address matches exactly one port and address
    in HAD_LIST.

    In case of any of this errors we should exit with error.
*/
int
HADStateMachine::reinitialize(void)
{
    char* tmp;
    freeResources(); // DELETE all and start everything over from the scratch
    m_masterDaemon = new Daemon( DT_MASTER );

	// Find the name of the controllee, default to negotiator
	tmp = param( "HAD_CONTROLLEE" );
	if ( tmp ) {
		m_controlleeName = tmp;
	}
	else {
		m_controlleeName = strdup( daemonString(DT_NEGOTIATOR) );
	}

	// reconfiguring data members, on which the negotiator location inside the
	// pool depends
	m_usePrimary = param_boolean("HAD_USE_PRIMARY", false);

    tmp = param( "HAD_LIST" );
    if ( tmp ) {
        m_isPrimary = getHadList( tmp, m_usePrimary,
								  m_otherHadIps, m_allHadIps,
								  m_selfId );
        free( tmp );
    } else {
        utilCrucialError("HAD CONFIGURATION ERROR:"
						 " no HAD_LIST in config file");
    }
	// reconfiguring the rest of the data members: those, which do not affect
	// the negotiator's location
	softReconfigure( );

	// initializing the timers
    m_stateMachineTimerID = daemonCore->Register_Timer (
		0,
		(TimerHandlercpp) &HADStateMachine::cycle,
		"Time to check HAD", this);
    dprintf( D_ALWAYS,"** Register on m_stateMachineTimerID, interval = %d\n",
            m_hadInterval/(MESSAGES_PER_INTERVAL_FACTOR));
	m_updateCollectorTimerId = daemonCore->Register_Timer (
        0, m_updateCollectorInterval,
        (TimerHandlercpp) &HADStateMachine::updateCollectors,
        "Update collector", this );

    if( m_standAloneMode ) {
         //printParamsInformation();
         return TRUE;
    }

    if( m_masterDaemon == NULL ||
        sendControlCmdToMaster(CHILD_OFF_FAST) == false ) {
         utilCrucialError("HAD ERROR: unable to send CHILD_OFF_FAST command");
    }

    //printParamsInformation();
    return TRUE;
}

/***********************************************************
  Function :   cycle()
    called MESSAGES_PER_INTERVAL_FACTOR times per m_hadInterval
    send messages according to a state,
    once in m_hadInterval call step() functions - step of HAD state machine
*/
void
HADStateMachine::cycle( int /* timerID */ )
{
    dprintf( D_FULLDEBUG, "-------------- > Timer m_stateMachineTimerID"
        " is called\n");

    utilCancelTimer( m_stateMachineTimerID );
	m_stateMachineTimerID = daemonCore->Register_Timer (
		m_hadInterval/(MESSAGES_PER_INTERVAL_FACTOR),
		(TimerHandlercpp) &HADStateMachine::cycle,
		"Time to check HAD",
		this);

    if(m_callsCounter == 0){ //  once in m_hadInterval
        // step of HAD state machine
        step();
    }
    sendMessages();
    m_callsCounter ++;
    m_callsCounter = m_callsCounter % MESSAGES_PER_INTERVAL_FACTOR;

}

/***********************************************************
  Function :  step()
    called each m_hadInterval, implements one state of the
    state machine.
*/
void
HADStateMachine::step(void)
{

    my_debug_print_buffers();
    switch( m_state ) {
        case PRE_STATE:
			//sendReplicationCommand( HAD_BEFORE_PASSIVE_STATE );
            m_state = PASSIVE_STATE;
            printStep("PRE_STATE","PASSIVE_STATE");
            break;

        case PASSIVE_STATE:
            if( receivedAliveList.empty() || m_isPrimary ) {
                m_state = ELECTION_STATE;
                printStep( "PASSIVE_STATE","ELECTION_STATE" );
                // we don't want to delete elections buffers
                return;
            } else {
                printStep( "PASSIVE_STATE","PASSIVE_STATE" );
            }

            break;
        case ELECTION_STATE:
        {
            if( !receivedAliveList.empty() && !m_isPrimary ) {
                m_state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // command ALIVE isn't received
            if( checkList(receivedIdList) == false ) {
                // id bigger than m_selfId is received
                m_state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

			if( m_standAloneMode && m_useReplication ) {
                sendReplicationCommand( HAD_AFTER_ELECTION_STATE );
            }
            // if stand alone mode
            if( m_standAloneMode ) {
                m_state = LEADER_STATE;
                updateCollectorsClassAd( "True" );
				printStep("ELECTION_STATE","LEADER_STATE");
                break;
            }

			// no leader in the system and this HAD has biggest id
            int returnValue = sendControlCmdToMaster(CHILD_ON);

            if( returnValue == TRUE && m_useReplication) {
                sendReplicationCommand( HAD_AFTER_ELECTION_STATE );
            }
            if( returnValue == TRUE ) {
                m_state = LEADER_STATE;
                updateCollectorsClassAd( "True" );
                printStep( "ELECTION_STATE", "LEADER_STATE" );
            } else {
                // TO DO : what with this case ? stay in election case ?
                // return to passive ?
                // may be call sendControlCmdToMaster(CHILD_ON)in a loop?
                dprintf( D_FULLDEBUG,
						 "id %d, cannot send CHILD_ON cmd,"
						 " stay in ELECTION state\n",
                    daemonCore->getpid() );
                utilCrucialError("");
            }

            break;
		}
        case LEADER_STATE:
            if( ! receivedAliveList.empty() &&
                  checkList(receivedAliveList) == false ) {
                // send to master "child_off"
                printStep( "LEADER_STATE","PASSIVE_STATE" );
                m_state = PASSIVE_STATE;
                updateCollectorsClassAd( "False" );

                if( m_useReplication ) {
                    sendReplicationCommand( HAD_AFTER_LEADER_STATE );
                }
                if( ! m_standAloneMode ) {
                    sendControlCmdToMaster( CHILD_OFF_FAST );
                }

                break;
            }
            if( m_useReplication ) {
                sendReplicationCommand( HAD_IN_LEADER_STATE );
            }
            printStep( "LEADER_STATE","LEADER_STATE" );

            break;
	} // end switch
    clearBuffers();
}

/***********************************************************
  Function :
*/
bool
HADStateMachine::sendMessages(void)
{
   switch( m_state ) {
   case ELECTION_STATE:
	   return sendCommandToOthers( HAD_SEND_ID_CMD );
   case LEADER_STATE:
	   return sendCommandToOthers( HAD_ALIVE_CMD ) ;
   default :
	   return false;
   }

}

/***********************************************************
  Function :  sendCommandToOthers( int comm )
    send command "comm" to all HADs in HAD_LIST
*/
bool
HADStateMachine::sendCommandToOthers( int comm )
{
    for (const auto& addr : m_otherHadIps) {

        dprintf( D_FULLDEBUG, "send command %s(%d) to %s\n",
				 getCommandStringSafe(comm),comm,addr.c_str());

        Daemon d( DT_ANY, addr.c_str() );
        ReliSock sock;

        sock.timeout( m_connectionTimeout );
        sock.doNotEnforceMinimalCONNECT_TIMEOUT();

        // blocking with timeout m_connectionTimeout
        if(!sock.connect( addr.c_str(), 0, false )) {
            dprintf( D_ALWAYS,"cannot connect to addr %s\n", addr.c_str() );
            sock.close();
            continue;
        }

        int cmd = comm;
        // startCommand - max timeout is m_connectionTimeout sec
        if( !d.startCommand( cmd, &sock, m_connectionTimeout ) ) {
            dprintf( D_ALWAYS,"cannot start command %s(%d) to addr %s\n",
					 getCommandStringSafe(comm),cmd,addr.c_str());
            sock.close();
            continue;
        }

        if(! putClassAd(&sock, m_classAd) || !sock.end_of_message()) {
            dprintf( D_ALWAYS, "Failed to send classad to peer\n");
        } else {
            dprintf( D_FULLDEBUG, "Sent classad to peer\n");
        }
        sock.close();
    }

    return true;
}

/* Function    : sendReplicationCommand
 * Arguments   : command - id
 * Return value: success/failure value
 * Description : sends specified command to local replication daemon
 */
bool
HADStateMachine::sendReplicationCommand( int command )
{
    ReliSock sock;

    sock.timeout( m_connectionTimeout );
    sock.doNotEnforceMinimalCONNECT_TIMEOUT();

    // blocking with timeout m_connectionTimeout
	if ( !m_replicationDaemonSinfulString ) {
		EXCEPT( "HAD: Replication sinful string invalid!" );
	}
    if(!sock.connect( m_replicationDaemonSinfulString, 0, false) ) {
        dprintf( D_ALWAYS,
				 "cannot connect to replication daemon, addr %s\n",
				 m_replicationDaemonSinfulString );
        sock.close();

        return false;
    }

    int cmd = command;
    dprintf( D_FULLDEBUG,
			 "send command %s(%d) to replication daemon %s\n",
			 getCommandStringSafe(cmd), cmd, m_replicationDaemonSinfulString );

    // startCommand - max timeout is m_connectionTimeout sec
    if(! (m_masterDaemon->startCommand(cmd,&sock,m_connectionTimeout )) ) {
        dprintf( D_ALWAYS,
				 "cannot start command %s, addr %s\n",
				 getCommandStringSafe(cmd), m_replicationDaemonSinfulString );
        sock.close();

        return false;
    }

    if( !sock.put(daemonCore->InfoCommandSinfulString( )) ||
        !sock.end_of_message() ) {

        dprintf( D_ALWAYS, "send to replication daemon, !sock.code false \n");
        sock.close();

        return false;
    } else {
        dprintf( D_FULLDEBUG,
				 "send to replication daemon, !sock.code true \n");
    }
    sock.close();

    return true;
}

void
HADStateMachine::setReplicationDaemonSinfulString( void )
{
    if( ! m_useReplication ) {
        return ;
    }
    char* tmp = param( "REPLICATION_LIST" );

    if ( ! tmp ) {
        utilCrucialError( utilNoParameterError("REPLICATION_LIST",
                                               "REPLICATION").c_str( ) );
    }

    std::vector<std::string> replicationAddressList = split(tmp);
	char       buffer[BUFSIZ];

    free( tmp );

	int replicationDaemonIndex = (int)replicationAddressList.size() - 1;

	const char * mySinfulString = daemonCore->InfoCommandSinfulString();
	Sinful mySinful( mySinfulString );
    for (const auto& replicationAddress : replicationAddressList) {
        char* sinfulAddress     = utilToSinful( replicationAddress.c_str() );

        if( sinfulAddress == 0 ) {
            snprintf( buffer, sizeof(buffer),
					 "HADStateMachine::setReplicationDaemonSinfulString"
					 " invalid address %s\n",
					 replicationAddress.c_str() );
            utilCrucialError( buffer );

            //continue;
        }

		// We're checking if the replication daemon listed in the position
		// corresponding to ours in the HAD list refers to the same machine
		// or not.  We want to end up with 's' having the replication daemon's
		// shared port ID and port number, while ignoring the shared port
		// ID and port number in our comparison.  We temporarily set the
		// replication daemon's shared port ID to our own (to make sure
		// they match) and set (this sinful's copy of ) our own port number
		// to the replication daemon's port number (to make sure they match).
		// If the sinfuls do point to the same address, we change the
		// replication daemon's socket name to REPLICATION_SOCKET_NAME
		// (for backwards compatability, we can't just set the replication
		// daemon's socket name in the replication daemon list).
        if( replicationDaemonIndex == m_selfId ) {
            Sinful s( sinfulAddress );
            mySinful.setPort( s.getPort(), true );
            s.setSharedPortID( mySinful.getSharedPortID() );
            if( s.valid() && mySinful.addressPointsToMe( s ) ) {
            	if( s.getSharedPortID() ) {
            		s.setSharedPortID( param( "REPLICATION_SOCKET_NAME" ) );
            	}
                m_replicationDaemonSinfulString = strdup( s.getSinful() );
                dprintf( D_ALWAYS,
                    "HADStateMachine::setReplicationDaemonSinfulString "
                    "corresponding replication daemon - %s\n",
                    s.getSinful() );
            } else {
                snprintf( buffer, sizeof(buffer),
                         "HADStateMachine::setReplicationDaemonSinfulString "
                         "host names of machine and replication daemon do "
                         "not match: %s vs. %s\n", mySinful.getSinful(), s.getSinful() );
                utilCrucialError( buffer );
            }
        }

		replicationDaemonIndex --;
        free( sinfulAddress );
    }

    // if failed to found the replication daemon in REPLICATION_LIST, just
    // switch off the replication feature
    if( NULL == m_replicationDaemonSinfulString )
    {
        dprintf( D_ALWAYS,
				 "HADStateMachine::setReplicationDaemonSinfulString "
				 "local replication daemon not found in REPLICATION_LIST, "
				 "switching the replication off\n" );
        m_useReplication = false;
    }
}

/***********************************************************
  Function : sendControlCmdToMaster( int comm )
    send command "comm" to master
*/
bool
HADStateMachine::sendControlCmdToMaster( int comm )
{

    ReliSock sock;

    sock.timeout( m_connectionTimeout );
    sock.doNotEnforceMinimalCONNECT_TIMEOUT();

    // blocking with timeout m_connectionTimeout
    if(!sock.connect( m_masterDaemon->addr(), 0, false ) ) {
        dprintf( D_ALWAYS,
				 "cannot connect to master, addr %s\n",
				 m_masterDaemon->addr() );
        sock.close();
        return false;
    }

    int cmd = comm;
    dprintf( D_FULLDEBUG,
			 "send command %s(%d) [%s] to master %s\n",
			 getCommandStringSafe(cmd), cmd, m_controlleeName,
			 m_masterDaemon->addr() );

    // startCommand - max timeout is m_connectionTimeout sec
    if(! m_masterDaemon->startCommand( cmd, &sock, m_connectionTimeout )  ) {
        dprintf( D_ALWAYS,"cannot start command %s, addr %s\n",
				 getCommandStringSafe(cmd), m_masterDaemon->addr() );
        sock.close();
        return false;
    }

    if( !sock.code(m_controlleeName) || !sock.end_of_message() ) {
        dprintf( D_ALWAYS, "Failed to send controllee name to master\n");
        sock.close();
        return false;
    } else {
        dprintf( D_FULLDEBUG, "Controllee name sent to master\n");
    }
    sock.close();
    return true;
}

/***********************************************************
 *  Function :
 * Sets selfId's referent according to my index in the HAD_LIST
 *  ( in reverse order )
 * Initializes otherIps
 *  ( Returns m_isPrimary according to m_usePrimary and first index
 *    in HAD_LIST )
 * Returns the dynamically allocated list of remote HAD daemons sinful
 *  strings in third parameter and index of itself in fourth parameter
*/
bool
HADStateMachine::getHadList( const char *str,
							 bool usePrimary,
							 std::vector<std::string> &otherIps,
							 std::vector<std::string> &allIps,
							 int &selfId )
{
    std::vector<std::string> had_list = split(str);
    int counter        = 0;  // priority counter
    bool isPrimaryCopy = false;

    counter = (int)had_list.size() - 1;

	Sinful my_addr( daemonCore->InfoCommandSinfulString() );
	ASSERT( daemonCore->InfoCommandSinfulString() && my_addr.valid() );

       // Don't add to the HAD list on each reconfig.
       otherIps.clear();
       allIps.clear();

    bool iAmPresent = false;
    for (const auto& try_address : had_list) {
        char * sinful_addr = utilToSinful( try_address.c_str() );
		dprintf(D_ALWAYS,
				"HADStateMachine::initializeHADList my address '%s' "
				"vs. address in the list '%s'\n",
				my_addr.getSinful(), sinful_addr );
        if( sinful_addr == NULL ) {
            dprintf( D_ALWAYS,
					 "HAD CONFIGURATION ERROR: pid %d "
					 "address '%s' not valid\n",
					 daemonCore->getpid(), try_address.c_str() );
            utilCrucialError( "" );
            continue;
        }
		// The list doesn't include shared port IDs, so if we've got one
		// give it to each other address in the list before we check to
		// see if they're the same.  We know that sinful_addr can't
		// already have a shared port ID because we called utilToSinful,
		// and not just the Sinful constructor.  (The Sinful object and/or
		// constructor should probably resolve hostnames, but HAD does it
		// itself.)
		Sinful s( sinful_addr );
		free( sinful_addr );
		s.setSharedPortID( my_addr.getSharedPortID() );
		allIps.emplace_back( s.getSinful() );
dprintf( D_ALWAYS, "Checking address with shared port ID '%s'...\n", s.getSinful() );
        if( my_addr.addressPointsToMe( s ) ) {
dprintf( D_ALWAYS, "... found myself in list: %s\n", s.getSinful() );
            iAmPresent = true;
            // HAD id of each HAD is just the index of its <ip:port>
            // in HAD_LIST in reverse order
            selfId = counter;

            if(usePrimary && counter == ((int)had_list.size() - 1)){
				// I am primary
				// Primary is the first one in the HAD_LIST
				// That way primary gets the highest id of all
				isPrimaryCopy = true;
            }
        } else {
            otherIps.emplace_back( s.getSinful() );
        }
        counter-- ;
    } // end while

    if( !iAmPresent ) {
		char	buf[1024];
		snprintf( buf, sizeof(buf),
				  "HAD CONFIGURATION ERROR:  my address '%s'"
				  "is not present in HAD_LIST '%s'",
				  daemonCore->InfoCommandSinfulString(), str );
        utilCrucialError( buf );
    }

	return isPrimaryCopy;
}


/***********************************************************
  Function :  checkList( std::set<int> list )
    check if "list" contains ID bigger than mine.
    Note that in this implementation Primary always has
    the highest id of all and therefore will always
    win in checkList election process
*/
bool
HADStateMachine::checkList( const std::set<int>& list ) const
{
	for (auto id : list) {
		if (id > m_selfId) {
			return false;
		}
	}
    return true;
}

/***********************************************************
    Function :
*/
void
HADStateMachine::clearBuffers(void)
{
    receivedAliveList.clear();
    receivedIdList.clear();
}

/***********************************************************
  Function :   commandHandler(int cmd,Stream *strm)
    cmd can be HAD_ALIVE_CMD or HAD_SEND_ID_CMD
*/
int
HADStateMachine::commandHandlerHad(int cmd, Stream *strm)
{
	std::string buf;

    dprintf( D_FULLDEBUG, "commandHandler command %s(%d) is received\n",
			 getCommandStringSafe(cmd), cmd);

    strm->timeout( m_connectionTimeout );

    ClassAd	ad;
    strm->decode();
    if( ! getClassAd(strm, ad) ) {
        dprintf( D_ALWAYS, "commandHandler ERROR -  can't read classad\n" );
        return FALSE;
    }
    if( ! strm->end_of_message() ) {
        dprintf( D_ALWAYS, "commandHandlerHad ERROR -  can't read EOM\n" );
        return FALSE;
    }

    int new_id;
	if ( !ad.LookupInteger( ATTR_HAD_SELF_ID, new_id ) ) {
		dprintf( D_ALWAYS, "commandHandler ERROR: ID not in ad received\n" );
		return FALSE;
    }

	if ( !ad.LookupString( ATTR_HAD_CONTROLLEE_NAME, buf ) ) {
		dprintf( D_ALWAYS, "commandHandler ERROR:"
				 " controllee not in ad received\n" );
		return FALSE;
	}
	if ( strcasecmp(buf.c_str(), m_controlleeName) ) {
		dprintf( D_ALWAYS,
				 "ERROR: controllee different me='%s' other='%s'\n",
				 m_controlleeName, buf.c_str());
		return FALSE;
	}

	if ( !ad.LookupString( ATTR_HAD_LIST, buf ) ) {
		dprintf( D_ALWAYS,
				 "commandHandler ERROR: HADLlist not in ad received\n" );
		return FALSE;
	}
	std::vector<std::string> had_ips = split(buf);
	if ( m_allHadIps != had_ips ) {
		std::string cur_str = join(m_allHadIps, ",");
		dprintf( D_ALWAYS,
				 "commandHandler: WARNING: HAD IP list different!\n"
				 "\tme='%s'\n"
				 "\tother='%s'\n",
				 cur_str.c_str(), buf.c_str() );
	}

    switch(cmd){
	case HAD_ALIVE_CMD:
		dprintf( D_FULLDEBUG,
				 "commandHandler received HAD_ALIVE_CMD with id %d\n",
				 new_id);
		receivedAliveList.emplace(new_id);
		break;

	case HAD_SEND_ID_CMD:
		dprintf( D_FULLDEBUG,
				 "commandHandler received HAD_SEND_ID_CMD with id %d\n",
				 new_id);
		receivedIdList.emplace(new_id);
		break;
    }
	return TRUE;

}

/***********************************************************
  Function :
*/
void
HADStateMachine::printStep( const char *curState, const char *nextState ) const
{
      dprintf( D_FULLDEBUG,
                "State machine step : pid <%d> port <%d> "
                "priority <%d> was <%s> go to <%s>\n",
                daemonCore->getpid(),
                daemonCore->InfoCommandPort(),
                m_selfId,
                curState,nextState );
}

/***********************************************************
  Function :
*/
void
HADStateMachine::my_debug_print_buffers()
{
    dprintf( D_FULLDEBUG, "ALIVE IDs list : \n" );
    for (auto id : receivedAliveList) {
        dprintf( D_FULLDEBUG, "<%d>\n", id );
    }

    dprintf( D_FULLDEBUG, "ELECTION IDs list : \n" );
    for (auto id : receivedIdList) {
        dprintf( D_FULLDEBUG, "<%d>\n", id );
    }
}

/***********************************************************
  Function :
*/
void
HADStateMachine::printParamsInformation(void)
{
     dprintf( D_ALWAYS,"** HAD_ID:                 %d\n",
			  m_selfId );
     dprintf( D_ALWAYS,"** HAD_CONTROLLEE:         %s\n",
			  m_controlleeName );
     dprintf( D_ALWAYS,"** HAD_CYCLE_INTERVAL:     %d\n",
			  m_hadInterval );
     dprintf( D_ALWAYS,"** HAD_CONNECTION_TIMEOUT: %d\n",
			  m_connectionTimeout );
     dprintf( D_ALWAYS,"** HAD_USE_PRIMARY:        %s\n",
			  m_usePrimary ? "True":"False" );
     dprintf( D_ALWAYS,"** AM I PRIMARY:           %s\n",
			  m_isPrimary ? "True":"False" );
     dprintf( D_ALWAYS,"** HAD_LIST(others only)\n");
	 dprintf( D_ALWAYS,"** HAD_UPDATE_INTERVAL:    %d\n",
			  m_updateCollectorInterval );
	 dprintf( D_ALWAYS,"** Is replication used:    %s\n",
			  m_useReplication ? "True":"False" );
	 if ( m_useReplication ) {
		 dprintf( D_ALWAYS,"** Replication sinful:     %s\n",
				  m_replicationDaemonSinfulString );
	 }

	 for (const auto& addr : m_otherHadIps) {
		 dprintf( D_ALWAYS, "**    %s\n", addr.c_str());
	 }
	 dprintf( D_ALWAYS,"** HAD_STAND_ALONE_DEBUG   %s\n",
			  m_standAloneMode ? "True":"False" );

}
/* Function    : updateCollectors
 * Description: sends the classad update to collectors and resets the timer
 *                      for later updates
 */
void
HADStateMachine::updateCollectors( int /* timerID */ )
{
    dprintf(D_FULLDEBUG, "HADStateMachine::updateCollectors started\n");

	// If the ad is valid, publish it
	int		index;
    if ( m_classAd.LookupInteger(ATTR_HAD_INDEX, index) ) {
        int successfulUpdatesNumber =
            daemonCore->sendUpdates (UPDATE_HAD_AD, &m_classAd, NULL, true);
        dprintf( D_ALWAYS, "HADStateMachine::updateCollectors %d "
                    "successful updates\n", successfulUpdatesNumber);
    }

    // Reset the timer for sending classads updates to collectors
    utilCancelTimer( m_updateCollectorTimerId );
    m_updateCollectorTimerId = daemonCore->Register_Timer (
		m_updateCollectorInterval,
		(TimerHandlercpp) &HADStateMachine::updateCollectors,
		"Update collector", this);
}
/* Function  : updateCollectorsClassAd
 * Arguments : isHadActive - designates whether the current HAD is active
 * 							 or not, may be equal to either "FALSE" or "TRUE"
 * Description: updates the local classad with information about whether the
 *              HAD is active or not and sends the update to collectors
 */
void
HADStateMachine::updateCollectorsClassAd(const std::string& isHadActive)
{
    m_classAd.Assign( ATTR_HAD_IS_ACTIVE, isHadActive );

    int successfulUpdatesNumber =
        daemonCore->sendUpdates ( UPDATE_HAD_AD, &m_classAd, NULL, true );
    dprintf( D_ALWAYS, "HADStateMachine::updateCollectorsClassAd %d "
                    "successful updates\n", successfulUpdatesNumber);
}
