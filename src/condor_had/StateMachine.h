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


/*
  StateMachine.h: interface for the HADStateMachine class.
*/

#if !defined(HAD_StateMachine_H__)
#define HAD_StateMachine_H__

#include "condor_daemon_core.h"

#define MESSAGES_PER_INTERVAL_FACTOR (2)
#define SEND_COMMAND_TIMEOUT (5) // 5 seconds

typedef enum {
    PRE_STATE = 1,
    PASSIVE_STATE = 2,
    ELECTION_STATE = 3,
    LEADER_STATE = 4
}STATES;

class Daemon;
/*
  class HADStateMachine
*/
class HADStateMachine  :public Service
{
public:

    /*
      Const'rs
    */

    HADStateMachine(void);

    /*
      Destructor
    */
    virtual ~HADStateMachine(void);

    virtual void initialize(void);

    virtual int reinitialize(void);
	
	bool isHardConfigurationNeeded(void);
	int softReconfigure(void);

protected:
    /*
      step() - called each m_hadInterval, implements one state of the
      state machine.
    */
    void  step(void);
    
    /*
      cycle() - called MESSAGES_PER_INTERVAL_FACTOR times per m_hadInterval
    */
    void  cycle( int timerID = -1 );


    /*
      sendCommandToOthers(int command) - send "ALIVE command" or
      "SEND ID command" to all HADs from HAD list.
    */
    bool sendCommandToOthers( int );

    /*
      send "ALIVE command" or "SEND ID command" to all HADs from HAD list
      acconding to state
    */
    bool sendMessages(void);
    
    /*
      sendControlCmdToMaster(int) - snd "<subsys> ON" or "<subsys> OFF"
      to master.
      return TRUE in case of success or FALSE otherwise
    */
    bool sendControlCmdToMaster( int );

    int commandHandlerHad(int cmd,Stream *strm) ;

    int m_state;   
    int m_stateMachineTimerID;
        
    int m_hadInterval;
    int m_connectionTimeout;
    
    // if m_callsCounter equals to 0 ,
    // enter state machine , otherwise send messages
    char m_callsCounter;
    
    int m_selfId;
    bool m_isPrimary;
    bool m_usePrimary;
    std::vector<std::string> m_otherHadIps;
	std::vector<std::string> m_allHadIps;
    Daemon* m_masterDaemon;
	char *m_controlleeName;

    std::set<int> receivedAliveList;
    std::set<int> receivedIdList;

    static bool getHadList(const char* s,
						   bool use_primary,
						   std::vector<std::string> &other,
						   std::vector<std::string> &all,
						   int &selfId );
    bool checkList(const std::set<int>&) const;
    void clearBuffers(void);
    void printStep(const char *curState,const char *nextState) const;
    //char* commandToString(int command);

    void init(void);
    void freeResources(void);
    
    /*
        convertToSinful(char* addr)
        addr - address in one of the formats :
            <IP:port>,IP:port,<name:port>,name:port
        return address in the format <IP:port>
    */
    //char* convertToSinful(char* addr);
    void printParamsInformation(void);

    //int myatoi(const char* str, bool* res);

    // debug information
    bool m_standAloneMode;
    void my_debug_print_buffers(void);

	// usage of replication, controlled by configuration parameter 
	// HAD_USE_REPLICATION
	bool m_useReplication;

	bool sendReplicationCommand( int );
	void setReplicationDaemonSinfulString( void );

	char* m_replicationDaemonSinfulString;
	// classad-specific data members and functions
    void initializeClassAd(void);
    // timer handler
    void updateCollectors( int timerID = -1 );
    // updates collectors upon changing from/to leader state
    void updateCollectorsClassAd( const std::string& isHadActive );

	std::string    m_name;
	ClassAd		m_classAd;

    // info about our central manager
    int         m_updateCollectorTimerId;
    int         m_updateCollectorInterval;
};

#endif // !HAD_StateMachine_H__
