/*
  StateMachine.h: interface for the HADStateMachine class.
*/

#if !defined(HAD_StateMachine_H__)
#define HAD_StateMachine_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"

// TODO : to enter the commands to command types
// file   condor_includes/condor_commands.h
#define HAD_COMMANDS_BASE       700
#define HAD_ALIVE_CMD           (HAD_COMMANDS_BASE + 0)
#define HAD_SEND_ID_CMD         (HAD_COMMANDS_BASE + 1)

// command names
#define HAD_ALIVE_CMD_STR           "HAD_ALIVE_CMD"
#define HAD_SEND_ID_CMD_STR         "HAD_SEND_ID_CMD"
#define DAEMON_ON_STR           "DAEMON_ON"
#define DAEMON_OFF_FAST_STR           "DAEMON_OFF_FAST"
// end TODO



#define MESSAGES_PER_INTERVAL_FACTOR (2)
#define SEND_COMMAND_TIMEOUT (5) // 5 seconds

typedef enum {
    PRE_STATE = 1,
    PASSIVE_STATE = 2,
    ELECTION_STATE = 3,
    LEADER_STATE = 4
}STATES;

/*
  class HADStateMachine
*/
class HADStateMachine  :public Service
{
public:

    /*
      Const'rs
    */

    HADStateMachine();

    /*
      Destructor
    */
    virtual ~HADStateMachine();

    virtual void initialize();

    virtual int reinitialize();

protected:
    /*
      step() - called each hadInterval, implements one state of the
      state machine.
    */
    void  step();
    
    /*
      cycle() - called MESSAGES_PER_INTERVAL_FACTOR times per hadInterval
    */
    void  cycle();


    /*
      sendCommandToOthers(int command) - send "ALIVE command" or
      "SEND ID command" to all HADs from HAD list.
    */
    int sendCommandToOthers( int );

    /*
      send "ALIVE command" or "SEND ID command" to all HADs from HAD list
      acconding to state
    */
    int sendMessages();
    
    /*
      sendNegotiatorCmdToMaster(int) - snd "NEGOTIATOR ON" or "NEGOTIATOR OFF"
      to master.
      return TRUE in case of success or FALSE otherwise
    */
    int sendNegotiatorCmdToMaster( int );

    /*
      pushReceivedAlive(int id) - push to buffer id of HAD that
      sent "ALIVE command".
    */
    int pushReceivedAlive( int );

    /*
      pushReceivedId(int id) -  push to buffer id of HAD that sent
      "SEND ID command".
    */
    int pushReceivedId( int );

    void commandHandler(int cmd,Stream *strm) ;

    void onError(char*);
        
    int state;   
    int stateMachineTimerID;
        
    int hadInterval;
    int connectionTimeout;
    
    // if callsCounter equals to 0 ,
    // enter state machine , otherwise send messages
    char callsCounter;
    
    int selfId;
    bool isPrimary;
    bool usePrimary;
    StringList* otherHADIPs;
    Daemon* masterDaemon;

    List<int> receivedAliveList;
    List<int> receivedIdList;

    void initializeHADList(char*);
    int  checkList(List<int>*);
    void removeAllFromList(List<int>*);
    void clearBuffers();
    void printStep(char *curState,char *nextState);
    char* commandToString(int command);

    void finilize();
    void init();
    
    /*
        convertToSinfull(char* addr)
        addr - address in one of the formats :
            <IP:port>,IP:port,<name:port>,name:port
        return address in the format <IP:port>
    */
    char* convertToSinfull(char* addr);
    void printParamsInformation();

    int myatoi(const char* str, bool* res);

    // debug information
    bool standAloneMode;
    void my_debug_print_list(StringList* str);
    void my_debug_print_buffers();

};

#endif // !HAD_StateMachine_H__
