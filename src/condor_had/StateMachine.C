/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
 * The Condor High Availablility Daemon (HAD) softare was developed by
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


#include "StateMachine.h"


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
    state = PRE_STATE;
    otherHADIPs = NULL;
    masterDaemon = NULL;
    isPrimary = false;
    usePrimary = false;
    standAloneMode = false;
    callsCounter = 0;
    stateMachineTimerID = -1;
    hadInterval = -1;
    // set valid value to send NEGOTIATOR_OFF
    connectionTimeout = SEND_COMMAND_TIMEOUT;
    selfId = -1;
}
/***********************************************************
  Function :  finilize() - free all resources
*/	
void
HADStateMachine::finilize()
{
    state = PRE_STATE;
    connectionTimeout = SEND_COMMAND_TIMEOUT;
    
    if( otherHADIPs != NULL ){
        delete otherHADIPs;
		// since otherHADIPs is StringList it frees all its elements
        otherHADIPs = NULL;
    }
    if( masterDaemon != NULL ){
        // always kill leader when HAD dies - if I am leader I should 
        // kill my Negotiator.If I am not a leader - my Negotiator
        // should not be on anyway
        sendNegotiatorCmdToMaster( DAEMON_OFF_FAST );
        delete masterDaemon;
        masterDaemon = NULL;
    }
    isPrimary = false;
    usePrimary = false;
    standAloneMode = false;
    callsCounter = 0;
	hadInterval = -1;
    if ( stateMachineTimerID >= 0 ) {
        daemonCore->Cancel_Timer( stateMachineTimerID );
        stateMachineTimerID = -1;
    }
    
    selfId = -1;

    clearBuffers();
}

/***********************************************************
  Function :
*/
HADStateMachine::~HADStateMachine()
{
    finilize();
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
HADStateMachine::onError(char* error_msg)
{
    dprintf( D_ALWAYS,"%s\n",error_msg );
    main_shutdown_graceful();
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
    finilize(); // DELETE all and start everything over from the scratch
    masterDaemon = new Daemon( DT_MASTER );
    tmp = param( "HAD_STAND_ALONE_DEBUG" );
    if( tmp ){
        standAloneMode = true;
        free( tmp );
    }

    tmp=param( "HAD_USE_PRIMARY" );
    if( tmp ) {
        if(strncasecmp(tmp,"true",strlen("true")) == 0) {
          usePrimary = true;
        }
        free( tmp );
    }
    
    otherHADIPs = new StringList();
    tmp=param( "HAD_LIST" );
    if ( tmp ) {
        initializeHADList( tmp );
        free( tmp );
    } else {
        onError( "HAD CONFIGURATION ERROR: no HAD_LIST in config file" );
    }
    
    tmp=param( "HAD_CONNECTION_TIMEOUT" );
    if( tmp ) {
        bool res = false;
        connectionTimeout = myatoi(tmp,&res);
        if(!res){
            free( tmp );
            onError( "HAD CONFIGURATION ERROR: HAD_CONNECTION_TIMEOUT is "
                "not valid in config file" );
        }
        
        if(connectionTimeout <= 0){
               free( tmp );
               onError( "HAD CONFIGURATION ERROR: HAD_CONNECTION_TIMEOUT is "
                    "not valid in config file" );
        }
        free( tmp );
    } else {
        dprintf( D_ALWAYS,"No HAD_CONNECTION_TIMEOUT in config file,"
                " use default value\n" );
    }
    // calculate hadInterval
    int safetyFactor = 1;

    // timeoutNumber
    // connect + startCommand(sock.code() and sock.eom() aren't blocking)
    int timeoutNumber = 2;

    int time_to_send_all = (connectionTimeout*timeoutNumber);
    time_to_send_all *= (otherHADIPs->number() + 1); //otherHads + master

    hadInterval =  (time_to_send_all + safetyFactor)*
                        (MESSAGES_PER_INTERVAL_FACTOR);
                        

    stateMachineTimerID = daemonCore->Register_Timer ( 0,
                                    (TimerHandlercpp) &HADStateMachine::cycle,
                                    "Time to check HAD", this);

    dprintf( D_ALWAYS,"** Register on stateMachineTimerID , interval = %d\n",
            hadInterval/(MESSAGES_PER_INTERVAL_FACTOR));
                                       

    if( standAloneMode ) {
         printParamsInformation();
         return TRUE;
    }
 
    if( masterDaemon == NULL ||
        sendNegotiatorCmdToMaster(DAEMON_OFF_FAST) == FALSE ) {
         onError("HAD ERROR: unable to send NEGOTIATOR_OFF command");
    }
 
    printParamsInformation();
    return TRUE;
}

/***********************************************************
  Function :   cycle()
    called MESSAGES_PER_INTERVAL_FACTOR times per hadInterval
    send messages according to a state,
    once in hadInterval call step() functions - step of HAD state machine
*/
void
HADStateMachine::cycle(){
    dprintf( D_FULLDEBUG, "-------------- > Timer stateMachineTimerID"
        " is called\n");
    
    if(stateMachineTimerID >= 0){
      daemonCore->Cancel_Timer( stateMachineTimerID );
    }
    stateMachineTimerID = daemonCore->Register_Timer (
                                hadInterval/(MESSAGES_PER_INTERVAL_FACTOR),
                                (TimerHandlercpp) &HADStateMachine::cycle,
                                "Time to check HAD",
                                 this);
                                
    if(callsCounter == 0){ //  once in hadInterval
        // step of HAD state machine
        step();
    }
    sendMessages();
    callsCounter ++;
    callsCounter = callsCounter % MESSAGES_PER_INTERVAL_FACTOR;

}

/***********************************************************
  Function :  step()
    called each hadInterval, implements one state of the
    state machine.
  
*/
void
HADStateMachine::step()
{

    my_debug_print_buffers();
    switch( state ) {
        case PRE_STATE:
            state = PASSIVE_STATE;
            printStep("PRE_STATE","PASSIVE_STATE");
            break;

        case PASSIVE_STATE:
            if( receivedAliveList.IsEmpty() || isPrimary ) {
                state = ELECTION_STATE;
                printStep( "PASSIVE_STATE","ELECTION_STATE" );
                // we don't want to delete elections buffers
                return;
            } else {
                printStep( "PASSIVE_STATE","PASSIVE_STATE" );
            }

            break;
        case ELECTION_STATE:
            if( !receivedAliveList.IsEmpty() && !isPrimary ) {
                state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // command ALIVE isn't received
            if( checkList(&receivedIdList) == FALSE ) {
                // id bigger than selfId is received
                state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // if stand alone mode
            if( standAloneMode ) {
                state = LEADER_STATE;
                printStep("ELECTION_STATE","LEADER_STATE");
                break;
            }

            // no leader in the system and this HAD has biggest id
            if( sendNegotiatorCmdToMaster(DAEMON_ON) == TRUE ) {
                state = LEADER_STATE;
                printStep( "ELECTION_STATE","LEADER_STATE") ;
            } else {
                // TO DO : what with this case ? stay in election case ?
                // return to passive ?
                // may be call sendNegotiatorCmdToMaster(DAEMON_ON)in a loop?
                dprintf( D_FULLDEBUG,"id %d , cannot send NEGOTIATOR_ON cmd,"
                    " stay in ELECTION state\n",
                    daemonCore->getpid() );
                onError("");
            }

            break;

        case LEADER_STATE:
            if( !receivedAliveList.IsEmpty() ) {
                if( checkList(&receivedAliveList) == FALSE ) {
                    // send to master "negotiator_off"
                    printStep( "LEADER_STATE","PASSIVE_STATE" );
                    state = PASSIVE_STATE;
                    if( !standAloneMode ) {
                        sendNegotiatorCmdToMaster( DAEMON_OFF_FAST );
                    }
                    break;
                }
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
   switch( state ) {
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
    otherHADIPs->rewind();
    while( (addr = otherHADIPs->next()) ) {

        dprintf( D_FULLDEBUG, "send command %s(%d) to %s\n",
                        commandToString(comm),comm,addr);

        Daemon d( DT_ANY, addr );
        ReliSock sock;

        sock.timeout( connectionTimeout );
        sock.doNotEnforceMinimalCONNECT_TIMEOUT();

        // blocking with timeout connectionTimeout
        if(!sock.connect( addr,0,false )) {
            dprintf( D_ALWAYS,"cannot connect to addr %s\n",addr );
            sock.close();
            continue;
        }

        int cmd = comm;
        // startCommand - max timeout is connectionTimeout sec
        if( !d.startCommand(cmd,&sock,connectionTimeout ) ) {
            dprintf( D_ALWAYS,"cannot start command %s(%d) to addr %s\n",
                commandToString(comm),cmd,addr);
            sock.close();
            continue;
        }
 
        char stringId[256];
        sprintf( stringId,"%d",selfId );

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

/***********************************************************
  Function : sendNegotiatorCmdToMaster( int comm )
    send command "comm" to master
*/
int
HADStateMachine::sendNegotiatorCmdToMaster( int comm )
{

    ReliSock sock;

    sock.timeout( connectionTimeout );
    sock.doNotEnforceMinimalCONNECT_TIMEOUT();

    // blocking with timeout connectionTimeout
    if(!sock.connect( masterDaemon->addr(),0,false) ) {
        dprintf( D_ALWAYS,"cannot connect to master , addr %s\n",
                    masterDaemon->addr() );
        sock.close();
        return FALSE;
    }

    int cmd = comm;
    dprintf( D_FULLDEBUG,"send command %s(%d) to master %s\n",
                commandToString(cmd), cmd,masterDaemon->addr() );

    // startCommand - max timeout is connectionTimeout sec
    if(! (masterDaemon->startCommand(cmd,&sock,connectionTimeout )) ) {
        dprintf( D_ALWAYS,"cannot start command %s, addr %s\n",
                    commandToString(cmd), masterDaemon->addr() );
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
  Set selfId according to my index in the HAD_LIST (in reverse order)
  Initilaize otherHADIPs
  Set isPrimary according to usePrimary and first index in HAD_LIST
*/
void
HADStateMachine::initializeHADList( char* str )
{
    StringList had_list;
    int counter = 0;  // priority counter
    
    //   initializeFromString() and rewind() return void
    had_list.initializeFromString( str );
    counter = had_list.number() - 1;
    
    char* try_address;
    had_list.rewind();

    bool iAmPresent = false;
    while( (try_address = had_list.next()) ) {
        char* sinfull_addr = convertToSinfull( try_address );
        if(sinfull_addr == NULL) {
            dprintf(D_ALWAYS,"HAD CONFIGURATION ERROR: pid %d",
                daemonCore->getpid());
            dprintf(D_ALWAYS,"not valid address %s\n", try_address);
            
            onError( "" );
            continue;
        }
        if(strcmp( sinfull_addr,
                daemonCore->InfoCommandSinfulString()) == 0 ){
                  
            iAmPresent = true;
            // HAD id of each HAD is just the index of its <ip:port> 
            // in HAD_LIST in reverse order
            selfId = counter;
            
            if(usePrimary && counter == (had_list.number() - 1)){
              // I am primary
              // Primary is the first one in the HAD_LIST
              // That way primary gets the highest id of all
              isPrimary = true;
            }
        } else {
            otherHADIPs->insert( sinfull_addr );
        }
	// put attention to release memory allocated by malloc with free and by new with delete
	// here convertToSinfull returns memory allocated by malloc
        free( sinfull_addr );
        counter-- ;
    } // end while

    if( !iAmPresent ) {
        onError( "HAD CONFIGURATION ERROR :  my address is "
        "not present in HAD_LIST" );
    }
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
        if(id > selfId){
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
  Function :   convertToSinfull(char* addr)
        addr - address in one of the formats :
            <IP:port>,IP:port,<name:port>,name:port
        return address in the format <IP:port>
	[returns dynamically allocated (by malloc) string
	or NULL in case of failure]
*/
char*
HADStateMachine::convertToSinfull( char* addr )
{

    int port = getPortFromAddr(addr);
    if(port == 0) {
        return NULL;
    }

    //getHostFromAddr returns dinamically allocated buf
    char* address = getHostFromAddr( addr ); 
    if(address == 0) {
      return NULL;
    }

    struct in_addr sin;
    if(!is_ipaddr( address, &sin )) {
        struct hostent *ent = gethostbyname( address );
        if( ent == NULL ) {
            free( address );
            return NULL;
        }
        char* ip_addr = inet_ntoa(*((struct in_addr *)ent->h_addr));
        free( address );
        address = strdup( ip_addr );
    }

    char port_str[256];
    sprintf( port_str,"%d",port );
    int len = strlen(address)+strlen(port_str)+2*strlen("<")+strlen(":")+1;
    char* result = (char*)malloc( len );
    sprintf( result,"<%s:%d>",address,port );

    free( address );
    return result;
}

/***********************************************************
  Function :   commandHandler(int cmd,Stream *strm)
    cmd can be HAD_ALIVE_CMD or HAD_SEND_ID_CMD
*/
void
HADStateMachine::commandHandler(int cmd,Stream *strm)
{
 
    dprintf( D_FULLDEBUG, "commandHandler command %s(%d) is received\n",
            commandToString(cmd),cmd);
    
    char* subsys = NULL;
    strm->timeout( connectionTimeout );
    
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
    int new_id = myatoi(subsys,&res);
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
                selfId,
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
     dprintf( D_ALWAYS,"** HAD_ID   %d\n",selfId);
     dprintf( D_ALWAYS,"** HAD_CYCLE_INTERVAL   %d\n",hadInterval);
     dprintf( D_ALWAYS,"** HAD_CONNECTION_TIMEOUT   %d\n",connectionTimeout);
     dprintf( D_ALWAYS,"** HAD_USE_PRIMARY(true/false)   %d\n",usePrimary);
     dprintf( D_ALWAYS,"** AM I PRIMARY ?(true/false)   %d\n",isPrimary);
     dprintf( D_ALWAYS,"** HAD_LIST(others only)\n");
     char* addr = NULL;
     otherHADIPs->rewind();
     while( (addr = otherHADIPs->next()) ) {
           dprintf( D_ALWAYS,"**    %s\n",addr);
     }
     dprintf( D_ALWAYS,"** HAD_STAND_ALONE_DEBUG(true/false)    %d\n",
        standAloneMode);

}

/***********************************************************
  Function :
*/
char*
HADStateMachine::commandToString(int command)
{
    switch(command){
        case HAD_ALIVE_CMD:
            return HAD_ALIVE_CMD_STR ;
        case HAD_SEND_ID_CMD:
            return HAD_SEND_ID_CMD_STR ;
        case DAEMON_ON:
            return DAEMON_ON_STR ;
        case DAEMON_OFF_FAST:
            return DAEMON_OFF_FAST_STR ;
        default :
            return "unknown command";

    }
}

/***********************************************************
  Function :  myatoi(const char* str, bool* res)
    "str" - string to convert
    if "str" can be converted successfully to integer,
        return the integer and set "res" to true
    else return 0 and set "res" to false
*/
int
HADStateMachine::myatoi(const char* str, bool* res)
{
    if ( *str=='\0' ){
        *res = false;
        return 0;
    } else{
        char* endp;
        int ret_val = (int)strtol( str, &endp, 10 );
        if (endp!=NULL && *endp != '\0'){
            // Any reminder is considered as an error
            // Entire string wasn't valid
            *res = false;
            return 0;
        }
        *res = true;
        return ret_val;
    }
}

