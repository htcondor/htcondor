/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 *
 * The Condor High Availability Daemon (HAD) software was developed by
 * by the Distributed Systems Laboratory at the Technion Israel
 * Institute of Technology, and contributed to to the Condor Project.
 *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/*
  StateMachine.cpp: implementation of the HADStateMachine class.
*/

#include "condor_common.h"
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_api.h"
#include "condor_classad_lookup.h"
#include "condor_query.h"
#include "daemon.h"
#include "daemon_types.h"
#include "internet.h"
#include "list.h"
// for CollectorList
#include "daemon_list.h"
// for 'my_username'
#include "my_username.h"
#include "StateMachine.h"
#include "Utils.h"


//#undef IS_REPLICATION_USED
#define IS_REPLICATION_USED          (1)
#define MESSAGES_PER_INTERVAL_FACTOR (2)
#define DEFAULT_HAD_UPDATE_INTERVAL  (5 * MINUTE)

extern int main_shutdown_graceful();


/***********************************************************
  Function :
*/
HADStateMachine::HADStateMachine()
{
    init();
}

/***********************************************************
  Function :
*/
void
HADStateMachine::init()
{
    m_state = PRE_STATE;
    m_otherHADIPs = NULL;
    m_masterDaemon = NULL;
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
    replicationDaemonSinfulString = NULL;
    // classad-specific initializations
    m_classAd = NULL;
    m_collectorsList = NULL;
    m_updateCollectorTimerId = -1;
    m_updateCollectorInterval = -1;
}
/***********************************************************
  Function :  finalize() - free all resources
*/	
void
HADStateMachine::finalize()
{
    m_state = PRE_STATE;
    m_connectionTimeout = DEFAULT_SEND_COMMAND_TIMEOUT;
    
    if( m_otherHADIPs != NULL ){
        delete m_otherHADIPs;
		// since m_otherHADIPs is StringList it frees all its elements
        m_otherHADIPs = NULL;
    }
    if( m_masterDaemon != NULL ){
        // always kill leader when HAD dies - if I am leader I should 
        // kill my Negotiator.If I am not a leader - my Negotiator
        // should not be on anyway
        sendNegotiatorCmdToMaster( CHILD_OFF_FAST );
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
	
	if( replicationDaemonSinfulString != NULL ) {
		free(replicationDaemonSinfulString);
    	replicationDaemonSinfulString = NULL;
    }
	// classad finalizings
    if( m_classAd != NULL) {
		delete m_classAd;
    	m_classAd = NULL;
	}
	if( m_collectorsList != NULL ) {
    	delete m_collectorsList;
    	m_collectorsList = NULL;
    }
	utilCancelTimer( m_updateCollectorTimerId );
    m_updateCollectorInterval = -1;
}

/***********************************************************
  Function :
*/
HADStateMachine::~HADStateMachine()
{
    finalize();
}


bool 
HADStateMachine::isHardConfigurationNeeded()
{
	char*       buffer          = NULL;
	bool        usePrimaryCopy  = false;
	StringList* otherHADIPsCopy = NULL;
	int         selfIdCopy;

	buffer = param( "HAD_USE_PRIMARY" );

    if( buffer ) {
        if( strncasecmp( buffer, "true", strlen( "true" ) ) == 0 ) {
          usePrimaryCopy = true;
        }
        free( buffer );
    }

    buffer = param( "HAD_LIST" );

    if ( buffer ) {
		otherHADIPsCopy = new StringList( );
       	// we do not care for the return value here, so there is no importance
		// whether we use the 'usePrimaryCopy' or 'm_usePrimary'
		initializeHADList( buffer, m_usePrimary, otherHADIPsCopy, &selfIdCopy );
        free( buffer );
    } else {
		utilCrucialError( utilNoParameterError( "HAD_LIST", "HAD" ).GetCStr() );
    }

	// if either the HAD_LIST length has changed or the index of the local
	// HAD in the list has changed or the rest of remote HAD sinful strings
	// has changed their order or value, we do need the hard reconfiguration
	if( m_usePrimary             != usePrimaryCopy ||
		m_selfId                 != selfIdCopy     ||
		m_otherHADIPs->number( ) != otherHADIPsCopy->number( ) ) {
		delete otherHADIPsCopy;

		return true;
	}

	char* address     = NULL;
	char* addressCopy = NULL;

	while( ( address     = m_otherHADIPs->next( ) ) && 
		   ( addressCopy = otherHADIPsCopy->next( ) ) ) {
		dprintf( D_ALWAYS, "Addresses to compare %s and %s\n", address,
				 addressCopy );
		if( strcmp( address, addressCopy ) != 0 ) {
			delete otherHADIPsCopy;

			return true;
		}	
	}
	delete otherHADIPsCopy;

	return false;
}

int 
HADStateMachine::softReconfigure()
{
	// initializing necessary data members to prepare for the reconfiguration
	m_standAloneMode          = false;
	m_connectionTimeout       = DEFAULT_SEND_COMMAND_TIMEOUT;
	m_hadInterval             = -1;
	m_useReplication          = false;
	m_updateCollectorInterval = -1;	

	if( replicationDaemonSinfulString != NULL ) {
		free( replicationDaemonSinfulString );
		replicationDaemonSinfulString = NULL;
	}
	if( m_classAd != NULL) {
		delete m_classAd;
    	m_classAd = NULL;
	}
	if( m_collectorsList != NULL ) {
		delete m_collectorsList;
    	m_collectorsList = NULL;
	}
	// reconfiguration
	char* buffer = NULL;

	buffer = param( "HAD_STAND_ALONE_DEBUG" );

    if( buffer ){
		// for now we disable 'standalone' mode, since there is no need to use
		// it in the production code
		m_standAloneMode = false;
        // m_standAloneMode = true;
        free( buffer );
    }

	buffer = param( "HAD_CONNECTION_TIMEOUT" );
    
	if( buffer ) {
        bool res = false;
        
		m_connectionTimeout = utilAtoi(buffer, &res);
        
		if( ! res || m_connectionTimeout <= 0 ) {
            free( buffer );
            utilCrucialError( utilConfigurationError( 
								"HAD_CONNECTION_TIMEOUT", "HAD" ).GetCStr( ) );
        }
    
        free( buffer );
    } else {
        dprintf( D_ALWAYS, "No HAD_CONNECTION_TIMEOUT in config file,"
                		   " use default value\n" );
    }
    // calculate m_hadInterval
    int safetyFactor = 1;
            
    // timeoutNumber
    // connect + startCommand(sock.code() and sock.eom() aren't blocking)
    int timeoutNumber = 2;
    
    int time_to_send_all = (m_connectionTimeout*timeoutNumber);   
    time_to_send_all *= (m_otherHADIPs->number() + 1); //otherHads + master
	
	m_hadInterval = (time_to_send_all + safetyFactor)*
                  (MESSAGES_PER_INTERVAL_FACTOR);
#if IS_REPLICATION_USED  
    // setting the replication usage permissions
    buffer = param( "HAD_USE_REPLICATION" );
    
	if ( buffer && ! strncasecmp( buffer, "true", strlen("true") ) ) {
        m_useReplication = true;
        free( buffer );
    }
    setReplicationDaemonSinfulString( );
#endif   

	dprintf(D_ALWAYS, "HADStateMachine::softReconfigure classad information\n");
    initializeClassAd( );
    m_collectorsList = CollectorList::create();
    m_updateCollectorInterval = param_integer ("HAD_UPDATE_INTERVAL",
                                             DEFAULT_HAD_UPDATE_INTERVAL );
    /*buffer = param( "HAD_UPDATE_INTERVAL" );
    if( buffer ) {
        bool res = false;
        
		m_updateCollectorInterval = utilAtoi(buffer, &res);
        
		if( ! res || m_updateCollectorInterval <= 0 ) {
            free( buffer );
            utilCrucialError( utilConfigurationError(
                                "HAD_UPDATE_INTERVAL", "HAD" ).GetCStr( ) );
        }
        free( buffer );
    } else {
        utilCrucialError( utilNoParameterError(
                            "HAD_UPDATE_INTERVAL", "HAD" ).GetCStr( ) );
    }*/
	printParamsInformation();

	return TRUE;
}

/***********************************************************
  Function :
*/
void
HADStateMachine::initialize()
{
    reinitialize ();
    daemonCore->Register_Command ( HAD_ALIVE_CMD, "ALIVE command",
            (CommandHandlercpp) &HADStateMachine::commandHandler,
            "commandHandler", (Service*) this, DAEMON );

    daemonCore->Register_Command ( HAD_SEND_ID_CMD, "SEND ID command",
            (CommandHandlercpp) &HADStateMachine::commandHandler,
            "commandHandler", (Service*) this, DAEMON );

}

void
HADStateMachine::initializeClassAd()
{
    m_classAd = new ClassAd();

    m_classAd->SetMyTypeName(HAD_ADTYPE);
    m_classAd->SetTargetTypeName("");

    MyString line;
    // 'my_username' allocates dynamic string
    char* userName = my_username();
    MyString name;

    name.sprintf( "%s@%s -p %d", userName, my_full_hostname( ),
				  daemonCore->InfoCommandPort( ) );
    free( userName );
    // two following attributes: ATTR_NAME and ATTR_MACHINE are mandatory
    // in order to be accepted by collector
    line.sprintf( "%s = \"%s\"", ATTR_NAME, name.GetCStr( ) );
    m_classAd->Insert(line.Value());

    line.sprintf( "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() );
    m_classAd->Insert(line.Value());

    line.sprintf( "%s = \"%s\"", ATTR_MY_ADDRESS,
                        daemonCore->InfoCommandSinfulString() );
    m_classAd->Insert(line.Value());

	// declaring boolean attributes this way, no need for \"False\"
    line.sprintf( "%s = False", ATTR_HAD_IS_ACTIVE );
    m_classAd->Insert(line.Value());

	// publishing list of hads in classad
	char*      buffer     = param( "HAD_LIST" );
	char*      hadAddress = NULL;
	StringList hadList; 
	MyString   attrHadList;
	MyString   comma;

	hadList.initializeFromString( buffer );
	hadList.rewind( );

	while( ( hadAddress = hadList.next() ) ) {
		attrHadList += comma;
		attrHadList += hadAddress;
		comma = ",";
	}
	line.sprintf( "%s = \"%s\"", ATTR_HAD_LIST, attrHadList.GetCStr( ) );
	m_classAd->Insert(line.Value());

	free( buffer );

	// publishing had's real index in list of hads
	line.sprintf( "%s = \"%d\"", ATTR_HAD_INDEX, hadList.number() - 1 - m_selfId);
	m_classAd->Insert(line.Value());
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
HADStateMachine::reinitialize()
{
    char* tmp;
    finalize(); // DELETE all and start everything over from the scratch
    m_masterDaemon = new Daemon( DT_MASTER );

	// reconfiguring data members, on which the negotiator location inside the
	// pool depends
    tmp=param( "HAD_USE_PRIMARY" );
    if( tmp ) {
        if(strncasecmp(tmp,"true",strlen("true")) == 0) {
          m_usePrimary = true;
        }
        free( tmp );
    }
    
    m_otherHADIPs = new StringList();
    tmp=param( "HAD_LIST" );
    if ( tmp ) {
        m_isPrimary = initializeHADList( tmp, m_usePrimary, m_otherHADIPs, &m_selfId );
        free( tmp );
    } else {
        utilCrucialError("HAD CONFIGURATION ERROR: no HAD_LIST in config file");
    }
	// reconfiguring the rest of the data members: those, which do not affect
	// the negotiator's location
	softReconfigure( );
   
	// initializing the timers 
    m_stateMachineTimerID = daemonCore->Register_Timer ( 0,
                                    (TimerHandlercpp) &HADStateMachine::cycle,
                                    "Time to check HAD", this);
    dprintf( D_ALWAYS,"** Register on m_stateMachineTimerID , interval = %d\n",
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
        sendNegotiatorCmdToMaster(CHILD_OFF_FAST) == FALSE ) {
         utilCrucialError("HAD ERROR: unable to send NEGOTIATOR_OFF command");
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
HADStateMachine::cycle(){
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
HADStateMachine::step()
{

    my_debug_print_buffers();
    switch( m_state ) {
        case PRE_STATE:
//            sendReplicationCommand( HAD_BEFORE_PASSIVE_STATE );
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
            if( checkList(&receivedIdList) == FALSE ) {
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
            int returnValue = sendNegotiatorCmdToMaster(CHILD_ON);

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
                // may be call sendNegotiatorCmdToMaster(CHILD_ON)in a loop?
                dprintf( D_FULLDEBUG,"id %d , cannot send NEGOTIATOR_ON cmd,"
                    " stay in ELECTION state\n",
                    daemonCore->getpid() );
                utilCrucialError("");
            }

            break;
		}
        case LEADER_STATE:
    		if( ! receivedAliveList.IsEmpty() &&
                  checkList(&receivedAliveList) == FALSE ) {
                // send to master "negotiator_off"
                printStep( "LEADER_STATE","PASSIVE_STATE" );
                m_state = PASSIVE_STATE;
                updateCollectorsClassAd( "False" );

                if( m_useReplication ) {
                    sendReplicationCommand( HAD_AFTER_LEADER_STATE );
                }
                if( ! m_standAloneMode ) {
                    sendNegotiatorCmdToMaster( CHILD_OFF_FAST );
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
int
HADStateMachine::sendMessages()
{
   switch( m_state ) {
        case ELECTION_STATE:
            return sendCommandToOthers( HAD_SEND_ID_CMD );
        case LEADER_STATE:
            return sendCommandToOthers( HAD_ALIVE_CMD ) ;
        default :
            return FALSE;
   }
  
}

/***********************************************************
  Function :  sendCommandToOthers( int comm )
    send command "comm" to all HADs in HAD_LIST
*/
int
HADStateMachine::sendCommandToOthers( int comm )
{

    char* addr;
    m_otherHADIPs->rewind();
    while( (addr = m_otherHADIPs->next()) ) {

        dprintf( D_FULLDEBUG, "send command %s(%d) to %s\n",
                        utilToString(comm),comm,addr);

        Daemon d( DT_ANY, addr );
        ReliSock sock;

        sock.timeout( m_connectionTimeout );
        sock.doNotEnforceMinimalCONNECT_TIMEOUT();

        // blocking with timeout m_connectionTimeout
        if(!sock.connect( addr,0,false )) {
            dprintf( D_ALWAYS,"cannot connect to addr %s\n",addr );
            sock.close();
            continue;
        }

        int cmd = comm;
        // startCommand - max timeout is m_connectionTimeout sec
        if( !d.startCommand(cmd,&sock,m_connectionTimeout ) ) {
            dprintf( D_ALWAYS,"cannot start command %s(%d) to addr %s\n",
                utilToString(comm),cmd,addr);
            sock.close();
            continue;
        }
 
        char stringId[256];
        sprintf( stringId,"%d",m_selfId );

        char* subsys = (char*)stringId;

        if(!sock.code( subsys ) || !sock.eom()) {
            dprintf( D_ALWAYS,"sock.code false \n");
        } else {
            dprintf( D_FULLDEBUG,"sock.code true \n");
        }               
        sock.close();   
    }

    return TRUE;
}

/* Function    : sendReplicationCommand
 * Arguments   : command - id
 * Return value: success/failure value
 * Description : sends specified command to local replication daemon
 */
int
HADStateMachine::sendReplicationCommand( int command )
{
    ReliSock sock;

    sock.timeout( m_connectionTimeout );
    sock.doNotEnforceMinimalCONNECT_TIMEOUT();

    // blocking with timeout m_connectionTimeout
    if(!sock.connect( replicationDaemonSinfulString, 0,false) ) {
        dprintf( D_ALWAYS,"cannot connect to replication daemon , addr %s\n",
                    replicationDaemonSinfulString );
        sock.close();

        return FALSE;
    }

    int cmd = command;
    dprintf( D_FULLDEBUG,"send command %s(%d) to replication daemon %s\n",
                utilToString(cmd), cmd, replicationDaemonSinfulString );

    // startCommand - max timeout is m_connectionTimeout sec
    if(! (m_masterDaemon->startCommand(cmd,&sock,m_connectionTimeout )) ) {
        dprintf( D_ALWAYS,"cannot start command %s, addr %s\n",
                    utilToString(cmd), replicationDaemonSinfulString );
        sock.close();

        return FALSE;
    }

    char* subsys = (char*)daemonCore->InfoCommandSinfulString( );

    if( !sock.code(subsys) || !sock.eom() ) {
        dprintf( D_ALWAYS, "send to replication daemon, !sock.code false \n");
        sock.close();

        return FALSE;
    } else {
        dprintf( D_FULLDEBUG, "send to replication daemon, !sock.code true \n");
    }
    sock.close();

    return TRUE;
}

void
HADStateMachine::setReplicationDaemonSinfulString( )
{
    if( ! m_useReplication ) {
        return ;
    }
    char* tmp = param( "REPLICATION_LIST" );

    if ( ! tmp ) {
        utilCrucialError( utilNoParameterError("REPLICATION_LIST",
                                               "REPLICATION").GetCStr( ) );
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
            sprintf( buffer, "HADStateMachine::setReplicationDaemonSinfulString"
                             " invalid address %s\n", replicationAddress );
            utilCrucialError( buffer );

            //continue;
        }
        char* sinfulAddressHost = getHostFromAddr( sinfulAddress );

        if( replicationDaemonIndex == m_selfId && 
			strcmp( sinfulAddressHost,  host ) == 0 ) {
            replicationDaemonSinfulString = sinfulAddress;
            free( sinfulAddressHost );
			dprintf( D_ALWAYS,
					"HADStateMachine::setReplicationDaemonSinfulString "
					"corresponding replication daemon - %s\n", sinfulAddress );
            // not freeing 'sinfulAddress', since its referent is pointed by
            // 'replicationDaemonSinfulString' too
            break;
        } else if( replicationDaemonIndex == m_selfId ) {
			sprintf( buffer, "HADStateMachine::setReplicationDaemonSinfulString"
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
    if( replicationDaemonSinfulString == 0 )
    {
        dprintf( D_ALWAYS, "HADStateMachine::setReplicationDaemonSinfulString "
                "local replication daemon not found in REPLICATION_LIST, "
                "switching the replication off\n" );
        m_useReplication = false;
    }
}

/***********************************************************
  Function : sendNegotiatorCmdToMaster( int comm )
    send command "comm" to master
*/
int
HADStateMachine::sendNegotiatorCmdToMaster( int comm )
{

    ReliSock sock;

    sock.timeout( m_connectionTimeout );
    sock.doNotEnforceMinimalCONNECT_TIMEOUT();

    // blocking with timeout m_connectionTimeout
    if(!sock.connect( m_masterDaemon->addr(),0,false) ) {
        dprintf( D_ALWAYS,"cannot connect to master , addr %s\n",
                    m_masterDaemon->addr() );
        sock.close();
        return FALSE;
    }

    int cmd = comm;
    dprintf( D_FULLDEBUG,"send command %s(%d) to master %s\n",
                utilToString(cmd), cmd,m_masterDaemon->addr() );

    // startCommand - max timeout is m_connectionTimeout sec
    if(! (m_masterDaemon->startCommand(cmd,&sock,m_connectionTimeout )) ) {
        dprintf( D_ALWAYS,"cannot start command %s, addr %s\n",
                    utilToString(cmd), m_masterDaemon->addr() );
        sock.close();
        return FALSE;
    }

    char* subsys = (char*)daemonString( DT_NEGOTIATOR );

    if( !sock.code(subsys) || !sock.eom() ) {
        dprintf( D_ALWAYS,"send to master , !sock.code false \n");
        sock.close();
        return FALSE;
    } else {
        dprintf( D_FULLDEBUG,"send to master , !sock.code true \n");
    }
                      
    sock.close();     
    return TRUE;      
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
  Function :
  Sets selfId's referent according to my index in the HAD_LIST (in reverse order)
  Initializes otherIps
  Returns m_isPrimary according to m_usePrimary and first index in HAD_LIST
  Returns the dynamically allocated list of remote HAD daemons sinful strings
  in third parameter and index of itself in fourth parameter
*/
bool
HADStateMachine::initializeHADList( char* str, 
									bool usePrimary,
									StringList* otherIps, 
									int* selfId )
{
    StringList had_list;
    int counter        = 0;  // priority counter
    bool isPrimaryCopy = false;

    //   initializeFromString() and rewind() return void
    had_list.initializeFromString( str );
    counter = had_list.number() - 1;
    
    char* try_address;
    had_list.rewind();

    bool iAmPresent = false;
    while( (try_address = had_list.next()) ) {
        char* sinfull_addr = utilToSinful( try_address );

		dprintf(D_ALWAYS, "HADStateMachine::initializeHADList my address %s "
						  "vs. next address in the list%s\n", 
				daemonCore->InfoCommandSinfulString(), sinfull_addr );
        if(sinfull_addr == NULL) {
            dprintf(D_ALWAYS,"HAD CONFIGURATION ERROR: pid %d",
                daemonCore->getpid());
            dprintf(D_ALWAYS,"not valid address %s\n", try_address);
            
            utilCrucialError( "" );
            continue;
        }
        if(strcmp( sinfull_addr,
                daemonCore->InfoCommandSinfulString()) == 0 ){
                  
            iAmPresent = true;
            // HAD id of each HAD is just the index of its <ip:port> 
            // in HAD_LIST in reverse order
            *selfId = counter;
            
            if(usePrimary && counter == (had_list.number() - 1)){
              // I am primary
              // Primary is the first one in the HAD_LIST
              // That way primary gets the highest id of all
              isPrimaryCopy = true;
            }
        } else {
            otherIps->insert( sinfull_addr );
        }
	// put attention to release memory allocated by malloc with free and by 
	// new with delete here utilToSinful returns memory allocated by malloc
        free( sinfull_addr );
        counter-- ;
    } // end while

    if( !iAmPresent ) {
        utilCrucialError( "HAD CONFIGURATION ERROR :  my address is "
        "not present in HAD_LIST" );
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
int
HADStateMachine::checkList( List<int>* list )
{
    int id;

    list->Rewind();
    while(list->Next( id ) ) {
        if(id > m_selfId){
            return FALSE;
        }
    }
    return TRUE;
}

/***********************************************************
  Function :
*/
void HADStateMachine::removeAllFromList( List<int>* list )
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
void HADStateMachine::clearBuffers()
{
    removeAllFromList( &receivedAliveList );
    removeAllFromList( &receivedIdList );
}

/***********************************************************
  Function :   commandHandler(int cmd,Stream *strm)
    cmd can be HAD_ALIVE_CMD or HAD_SEND_ID_CMD
*/
void
HADStateMachine::commandHandler(int cmd,Stream *strm)
{
 
    dprintf( D_FULLDEBUG, "commandHandler command %s(%d) is received\n",
            utilToString(cmd),cmd);
    
    char* subsys = NULL;
    strm->timeout( m_connectionTimeout );
    
    strm->decode();
    if( ! strm->code(subsys) ) {
        dprintf( D_ALWAYS, "commandHandler -  can't read subsystem name\n" );
        return;
    }
    if( ! strm->end_of_message() ) {
        dprintf( D_ALWAYS, "commandHandler -  can't read end_of_message\n" );
        free( subsys );
        return;
    }


    bool res = false;
    int new_id = utilAtoi(subsys,&res);
    if(!res){
          dprintf( D_ALWAYS,"commandHandler received invalid id %s\n",
                 subsys );
        
    }
    free( subsys );
    

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


}

/***********************************************************
  Function :
*/
void
HADStateMachine::printStep( char *curState,char *nextState )
{
      dprintf( D_FULLDEBUG,
                "State mashine step : pid <%d> port <%d> "
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
    dprintf( D_FULLDEBUG, "----> begin print list , id %d\n",
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
HADStateMachine::printParamsInformation()
{
     dprintf( D_ALWAYS,"** HAD_ID   %d\n",m_selfId);
     dprintf( D_ALWAYS,"** HAD_CYCLE_INTERVAL   %d\n",m_hadInterval);
     dprintf( D_ALWAYS,"** HAD_CONNECTION_TIMEOUT   %d\n",m_connectionTimeout);
     dprintf( D_ALWAYS,"** HAD_USE_PRIMARY(true/false)   %d\n",m_usePrimary);
     dprintf( D_ALWAYS,"** AM I PRIMARY ?(true/false)   %d\n",m_isPrimary);
     dprintf( D_ALWAYS,"** HAD_LIST(others only)\n");
	 dprintf( D_ALWAYS,"** HAD_UPDATE_INTERVAL   %d\n",m_updateCollectorInterval);
	 dprintf( D_ALWAYS,"** Is replication used (true/false) %d\n", 
			  			m_useReplication);     

	 char* addr = NULL;
     m_otherHADIPs->rewind();
     while( (addr = m_otherHADIPs->next()) ) {
           dprintf( D_ALWAYS,"**    %s\n",addr);
     }
     dprintf( D_ALWAYS,"** HAD_STAND_ALONE_DEBUG(true/false)    %d\n",
        m_standAloneMode);

}
/* Function    : updateCollectors
 * Description: sends the classad update to collectors and resets the timer
 *                      for later updates
 */
void
HADStateMachine::updateCollectors()
{
    dprintf(D_FULLDEBUG, "HADStateMachine::updateCollectors started\n");

    if (m_classAd) {
        int successfulUpdatesNumber =
            m_collectorsList->sendUpdates (UPDATE_HAD_AD, m_classAd);
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

    line.sprintf( "%s = %s", ATTR_HAD_IS_ACTIVE, isHadActive.GetCStr( ) );
    m_classAd->InsertOrUpdate( line.GetCStr( ) );

    int successfulUpdatesNumber =
        m_collectorsList->sendUpdates ( UPDATE_HAD_AD, m_classAd );
    dprintf( D_ALWAYS, "HADStateMachine::updateCollectorsClassAd %d "
                    "successful updates\n", successfulUpdatesNumber);
}

