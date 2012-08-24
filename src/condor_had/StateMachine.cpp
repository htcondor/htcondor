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
#include "condor_api.h"
#include "condor_query.h"
#include "daemon.h"
#include "daemon_types.h"
#include "internet.h"
#include "list.h"
// for 'my_username'
#include "my_username.h"
#include "StateMachine.h"
#include "Utils.h"


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
    MyString line;

    freeResources();

    invalidate_ad.SetMyTypeName( QUERY_ADTYPE );
    invalidate_ad.SetTargetTypeName( HAD_ADTYPE );
    line.formatstr( "TARGET.%s == \"%s\"", ATTR_NAME, m_name.Value( ) );
    invalidate_ad.AssignExpr( ATTR_REQUIREMENTS, line.Value( ) );
	invalidate_ad.Assign( ATTR_NAME, m_name.Value() );
    daemonCore->sendUpdates( INVALIDATE_HAD_ADS, &invalidate_ad, NULL, false );
}


bool
HADStateMachine::isHardConfigurationNeeded(void)
{
	char		*tmp      = NULL;
	char		 controllee[128];
	int     	 selfId = -1;
	StringList	 allHadIps;

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
		StringList	otherHadIps;
       	// we do not care for the return value here, so there is no importance
		// whether we use the 'usePrimaryCopy' or 'm_usePrimary'
		getHadList( tmp, m_usePrimary, otherHadIps, allHadIps, selfId );
        free( tmp );
    } else {
		utilCrucialError( utilNoParameterError( "HAD_LIST", "HAD" ).Value() );
    }

	// if either the HAD_LIST length has changed or the index of the local
	// HAD in the list has changed or the rest of remote HAD sinful strings
	// has changed their order or value, we do need the hard reconfiguration
	if(  ( m_selfId     != selfId              )  ||
		 ( !m_allHadIps.identical(m_allHadIps) )   ) {
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
        m_name.formatstr( "%s@%s", tmp, my_full_hostname( ) );
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
    time_to_send_all *= (m_otherHadIps.number() + 1); //otherHads + master

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
	char	*buffer;

	m_classAd.Clear();

    m_classAd.SetMyTypeName(HAD_ADTYPE);
    m_classAd.SetTargetTypeName("");

    MyString line;

    // ATTR_NAME is mandatory in order to be accepted by collector
    m_classAd.Assign( ATTR_NAME, m_name );
    m_classAd.Assign( ATTR_MY_ADDRESS,
					  daemonCore->InfoCommandSinfulString() );

	// declaring boolean attributes this way, no need for \"False\"
    m_classAd.Assign( ATTR_HAD_IS_ACTIVE, false );

	// publishing list of hads in classad
	buffer = m_allHadIps.print_to_string( );
	if ( !buffer ) {
		m_classAd.Assign( ATTR_HAD_LIST, "UNKNOWN" );
	}
	else {
		m_classAd.Assign( ATTR_HAD_LIST, buffer );
		free( buffer );
	}

	// Publish the "controllee" name
	m_classAd.Assign( ATTR_HAD_CONTROLLEE_NAME, m_controlleeName );

	// publishing had's real index in list of hads
	m_classAd.Assign( ATTR_HAD_INDEX, m_allHadIps.number() - 1 - m_selfId );

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
	m_usePrimary = param_boolean("HAD_USE_PRIMARY", m_usePrimary);

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
HADStateMachine::cycle(void)
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
            if( receivedAliveList.IsEmpty() || m_isPrimary ) {
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
			if( !receivedAliveList.IsEmpty() && !m_isPrimary ) {
                m_state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // command ALIVE isn't received
            if( checkList(&receivedIdList) == false ) {
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
    		if( ! receivedAliveList.IsEmpty() &&
                  checkList(&receivedAliveList) == false ) {
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

    char* addr;
    m_otherHadIps.rewind();
    while( (addr = m_otherHadIps.next()) ) {

        dprintf( D_FULLDEBUG, "send command %s(%d) to %s\n",
				 utilToString(comm),comm,addr);

        Daemon d( DT_ANY, addr );
        ReliSock sock;

        sock.timeout( m_connectionTimeout );
        sock.doNotEnforceMinimalCONNECT_TIMEOUT();

        // blocking with timeout m_connectionTimeout
        if(!sock.connect( addr, 0, false )) {
            dprintf( D_ALWAYS,"cannot connect to addr %s\n", addr );
            sock.close();
            continue;
        }

        int cmd = comm;
        // startCommand - max timeout is m_connectionTimeout sec
        if( !d.startCommand( cmd, &sock, m_connectionTimeout ) ) {
            dprintf( D_ALWAYS,"cannot start command %s(%d) to addr %s\n",
					 utilToString(comm),cmd,addr);
            sock.close();
            continue;
        }

        if(! m_classAd.put(sock) || !sock.end_of_message()) {
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
			 utilToString(cmd), cmd, m_replicationDaemonSinfulString );

    // startCommand - max timeout is m_connectionTimeout sec
    if(! (m_masterDaemon->startCommand(cmd,&sock,m_connectionTimeout )) ) {
        dprintf( D_ALWAYS,
				 "cannot start command %s, addr %s\n",
				 utilToString(cmd), m_replicationDaemonSinfulString );
        sock.close();

        return false;
    }

    char* subsys = const_cast<char*>( daemonCore->InfoCommandSinfulString( ) );

    if( !sock.code(subsys) || !sock.end_of_message() ) {
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
                                               "REPLICATION").Value( ) );
    }

    StringList replicationAddressList;
    char*      replicationAddress    = NULL;
	char       buffer[BUFSIZ];

    replicationAddressList.initializeFromString( tmp );
    replicationAddressList.rewind( );

    free( tmp );
    char* host = getHostFromAddr( daemonCore->InfoCommandSinfulString( ) );

	int replicationDaemonIndex = replicationAddressList.number() - 1;

    while( ( replicationAddress = replicationAddressList.next( ) ) ) {
        char* sinfulAddress     = utilToSinful( replicationAddress );

        if( sinfulAddress == 0 ) {
            sprintf( buffer,
					 "HADStateMachine::setReplicationDaemonSinfulString"
					 " invalid address %s\n",
					 replicationAddress );
            utilCrucialError( buffer );

            //continue;
        }
        char* sinfulAddressHost = getHostFromAddr( sinfulAddress );

        if(  (replicationDaemonIndex == m_selfId)     &&
			 (strcmp( sinfulAddressHost, host ) == 0)  ) {
            m_replicationDaemonSinfulString = sinfulAddress;
            free( sinfulAddressHost );
			dprintf( D_ALWAYS,
					"HADStateMachine::setReplicationDaemonSinfulString "
					"corresponding replication daemon - %s\n",
					 sinfulAddress );
            // not freeing 'sinfulAddress', since its referent is pointed by
            // 'replicationDaemonSinfulString' too
            break;
        } else if( replicationDaemonIndex == m_selfId ) {
			sprintf( buffer,
					 "HADStateMachine::setReplicationDaemonSinfulString"
					 "host names of machine and replication daemon do "
					 "not match: %s vs. %s\n", host, sinfulAddressHost);
			utilCrucialError( buffer );
		}

		replicationDaemonIndex --;
        free( sinfulAddressHost );
        free( sinfulAddress );
    }
    free( host );

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
			 utilToString(cmd), cmd, m_controlleeName,
			 m_masterDaemon->addr() );

    // startCommand - max timeout is m_connectionTimeout sec
    if(! m_masterDaemon->startCommand( cmd, &sock, m_connectionTimeout )  ) {
        dprintf( D_ALWAYS,"cannot start command %s, addr %s\n",
				 utilToString(cmd), m_masterDaemon->addr() );
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
  Function :
*/
int
HADStateMachine::pushReceivedAlive( int id )
{
    int* alloc_id = new int[1];
    *alloc_id = id;
    return (receivedAliveList.Append( alloc_id ));
}

/***********************************************************
  Function :
*/
int
HADStateMachine::pushReceivedId( int id )
{
	int* alloc_id = new int[1];
	*alloc_id = id;
	return (receivedIdList.Append( alloc_id ));
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
							 StringList &otherIps,
							 StringList &allIps,
							 int &selfId )
{
    StringList had_list;
    int counter        = 0;  // priority counter
    bool isPrimaryCopy = false;

    //   initializeFromString() and rewind() return void
    had_list.initializeFromString( str );
    counter = had_list.number() - 1;

    char* try_address;
    had_list.rewind();

	Sinful my_addr( daemonCore->InfoCommandSinfulString() );
	ASSERT( daemonCore->InfoCommandSinfulString() && my_addr.valid() );

    bool iAmPresent = false;
    while( (try_address = had_list.next()) ) {
        char *sinful_addr = utilToSinful( try_address );
		dprintf(D_ALWAYS,
				"HADStateMachine::initializeHADList my address %s "
				"vs. next address in the list%s\n",
				my_addr.getSinful(), sinful_addr );
        if( sinful_addr == NULL ) {
            dprintf( D_ALWAYS,
					 "HAD CONFIGURATION ERROR: pid %d", daemonCore->getpid() );
            dprintf( D_ALWAYS,"not valid address %s\n", try_address );

            utilCrucialError( "" );
            continue;
        }
		allIps.insert( sinful_addr );
        if( my_addr.addressPointsToMe( Sinful(sinful_addr) ) ) {
            iAmPresent = true;
            // HAD id of each HAD is just the index of its <ip:port>
            // in HAD_LIST in reverse order
            selfId = counter;

            if(usePrimary && counter == (had_list.number() - 1)){
				// I am primary
				// Primary is the first one in the HAD_LIST
				// That way primary gets the highest id of all
				isPrimaryCopy = true;
            }
        } else {
            otherIps.insert( sinful_addr );
        }

		// put attention to release memory allocated by malloc with
		// free and by new with delete here utilToSinful returns
		// memory allocated by malloc
        free( sinful_addr );
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
  Function :  checkList( List<int>* list )
    check if "list" contains ID bigger than mine.
    Note that in this implementation Primary always has
    the highest id of all and therefore will always
    win in checkList election process
*/
bool
HADStateMachine::checkList( List<int>* list )
{
    int id;

    list->Rewind();
    while(list->Next( id ) ) {
        if(id > m_selfId){
            return false;
        }
    }
    return true;
}

/***********************************************************
  Function :
*/
void
HADStateMachine::removeAllFromList( List<int>* list )
{
    int* elem;
    list->Rewind();
    while((elem = list->Next()) ) {
        delete elem;
        list->DeleteCurrent();
    }
    //assert(list->IsEmpty());

}

/***********************************************************
    Function :
*/
void
HADStateMachine::clearBuffers(void)
{
    removeAllFromList( &receivedAliveList );
    removeAllFromList( &receivedIdList );
}

/***********************************************************
  Function :   commandHandler(int cmd,Stream *strm)
    cmd can be HAD_ALIVE_CMD or HAD_SEND_ID_CMD
*/
int
HADStateMachine::commandHandlerHad(int cmd, Stream *strm)
{
	char	buf[1024];

    dprintf( D_FULLDEBUG, "commandHandler command %s(%d) is received\n",
			 utilToString(cmd), cmd);

    strm->timeout( m_connectionTimeout );

    ClassAd	ad;
    strm->decode();
    if( ! ad.initFromStream(*strm) ) {
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

	if ( !ad.LookupString( ATTR_HAD_CONTROLLEE_NAME, buf, sizeof(buf) ) ) {
		dprintf( D_ALWAYS, "commandHandler ERROR:"
				 " controllee not in ad received\n" );
		return FALSE;
	}
	if ( strcasecmp(buf, m_controlleeName) ) {
		dprintf( D_ALWAYS,
				 "ERROR: controllee different me='%s' other='%s'\n",
				 m_controlleeName, buf);
		return FALSE;
	}

	if ( !ad.LookupString( ATTR_HAD_LIST, buf, sizeof(buf) ) ) {
		dprintf( D_ALWAYS,
				 "commandHandler ERROR: HADLlist not in ad received\n" );
		return FALSE;
	}
	StringList	had_ips( buf );
	if ( ! m_allHadIps.identical( had_ips ) ) {
		char	*cur_str = m_allHadIps.print_to_string( );
		dprintf( D_ALWAYS,
				 "commandHandler: WARNING: HAD IP list different!\n"
				 "\tme='%s'\n"
				 "\tother='%s'\n",
				 cur_str ? cur_str : "NULL", buf );
		if ( cur_str ) {
			free( cur_str );
		}
	}

    switch(cmd){
	case HAD_ALIVE_CMD:
		dprintf( D_FULLDEBUG,
				 "commandHandler received HAD_ALIVE_CMD with id %d\n",
				 new_id);
		pushReceivedAlive( new_id );
		break;

	case HAD_SEND_ID_CMD:
		dprintf( D_FULLDEBUG,
				 "commandHandler received HAD_SEND_ID_CMD with id %d\n",
				 new_id);
		pushReceivedId( new_id );
		break;
    }
	return TRUE;

}

/***********************************************************
  Function :
*/
void
HADStateMachine::printStep( const char *curState, const char *nextState )
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
HADStateMachine::my_debug_print_list( StringList* str )
{
    str->rewind();
    char* elem;
    dprintf( D_FULLDEBUG, "----> begin print list, id %d\n",
                daemonCore->getpid() );
    while( (elem = str->next()) ) {
        dprintf( D_FULLDEBUG, "----> %s\n",elem );
    }
    dprintf( D_FULLDEBUG, "----> end print list \n" );
}

/***********************************************************
  Function :
*/
void
HADStateMachine::my_debug_print_buffers()
{
    int id;
    dprintf( D_FULLDEBUG, "ALIVE IDs list : \n" );
    receivedAliveList.Rewind();
    while( receivedAliveList.Next( id ) ) {
        dprintf( D_FULLDEBUG, "<%d>\n",id );
    }

    int id2;
    dprintf( D_FULLDEBUG, "ELECTION IDs list : \n" );
    receivedIdList.Rewind();
    while( receivedIdList.Next( id2 ) ) {
        dprintf( D_FULLDEBUG, "<%d>\n",id2 );
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

	 char* addr = NULL;
     m_otherHadIps.rewind();
     while( (addr = m_otherHadIps.next()) ) {
		 dprintf( D_ALWAYS,"**    %s\n",addr);
     }
     dprintf( D_ALWAYS,"** HAD_STAND_ALONE_DEBUG   %s\n",
			  m_standAloneMode ? "True":"False" );

}
/* Function    : updateCollectors
 * Description: sends the classad update to collectors and resets the timer
 *                      for later updates
 */
void
HADStateMachine::updateCollectors(void)
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
HADStateMachine::updateCollectorsClassAd(const MyString& isHadActive)
{
    MyString line;

    m_classAd.Assign( ATTR_HAD_IS_ACTIVE, isHadActive );

    int successfulUpdatesNumber =
        daemonCore->sendUpdates ( UPDATE_HAD_AD, &m_classAd, NULL, true );
    dprintf( D_ALWAYS, "HADStateMachine::updateCollectorsClassAd %d "
                    "successful updates\n", successfulUpdatesNumber);
}
