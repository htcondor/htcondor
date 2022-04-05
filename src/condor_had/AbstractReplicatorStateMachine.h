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

#ifndef ABSTRACT_REPLICATOR_STATE_MACHINE_H

#define ABSTRACT_REPLICATOR_STATE_MACHINE_H

// for 'ReplicatorState'
#include "ReplicatorState.h"
#include "Utils.h"
#include "Version.h"
#include "reli_sock.h"
#include "dc_service.h"
#include "list.h"

/* Class      : AbstractReplicatorStateMachine
 * Description: base abstract class for replication service state machine,
 *              contains useful functions for implementation of replication, 
 *              such as broadcasting version, downloading/uploading etc.
 *              Contains handlers for notifications from HAD about its
 *              transitions in its state machine:
 *              1) beforePassiveStateHandler - for HAD_BEFORE_PASSIVE_STATE
 *              2) afterElectionStateHandler - for HAD_AFTER_ELECTION_STATE
 *              3) afterLeaderStateHandler   - for HAD_AFTER_LEADER_STATE
 *              4) inLeaderStateHandler      - for HAD_IN_LEADER_STATE
 *
 *              Besides, it contains handlers for selection of the best version
 *              out of versions list and selection of the gid according to those
 *              versions:
 *              1) replicaSelectionHandler
 *              2) gidSelectionHandler
 */
class AbstractReplicatorStateMachine: public Service
{
public:
	// inner true/false values
    enum { TRANSFERER_TRUE = 0, TRANSFERER_FALSE };
    /* Function: AbstractReplicatorStateMachine constructor
     */
    AbstractReplicatorStateMachine();
	/* Function: AbstractReplicatorStateMachine destructor
	 */
    virtual ~AbstractReplicatorStateMachine() = 0;
	/* Function   : reinitialize
	 * Description: rereads all the configuration parameters and resets all the
	 *              data members
	 */
    void reinitialize();
// Notification handlers
    /* Function   : beforePassiveStateHandler
	 * Description: generic handler before the event, when HAD entered PASSIVE
	 *              state
	 */
    virtual void beforePassiveStateHandler() = 0;
	/* Function   : afterElectionStateHandler
	 * Description: generic handler after the event, when HAD is in transition
	 *              from ELECTION to LEADER state
	 */
    virtual void afterElectionStateHandler() = 0;
	/* Function   : afterLeaderStateHandler 
     * Description: generic handler after the event, when HAD is in transition
	 *              from LEADER to PASSIVE state 
	 */
    virtual void afterLeaderStateHandler() = 0;
	/* Function   : inLeaderStateHandler 
     * Description: generic handler after the event, when HAD is in inner loop
     *              of LEADER state
     */
    virtual void inLeaderStateHandler() = 0;
// End of notification handlers
// Selection handlers
	/* Function    : replicaSelectionHandler
	 * Description : generic handler for selection of the best version out of
     *               versions list
     */
    virtual bool replicaSelectionHandler(Version& newVersion) = 0;
	/* Function   : gidSelectionHandler
     * Description: generic handler for selection of gid for the pool
     */
    virtual void gidSelectionHandler() = 0;
// End of selection handlers
protected:
	// version sending commands between replication daemons
    typedef bool (AbstractReplicatorStateMachine::*CommandFunction)(ReliSock& );
    /* Function    : downloadReplicaTransfererReaper
	 * Arguments   : service    - the daemon, for which the transfer has ended
	 *				 pid        - id of the downloading 'condor_transferer'
	 *							  process
	 *				 exitStatus - return value of the downloading
	 *							 'condor_transferer' process
     * Return value: int - success/failure value
	 * Note        : returns 0 upon success, 1 - upon failure
	 * Description : reaper of downloading 'condor_transferer' process 
     */
	int
    downloadReplicaTransfererReaper(int pid, int exitStatus);
	/* Function    : uploadReplicaTransfererReaper
     * Arguments   : service    - the daemon, for which the transfer has ended
     *               pid        - id of the uploading 'condor_transferer' 
     *                            process
     *               exitStatus - return value of the uploading 
     *                           'condor_transferer' process
	 * Return value: int - success/failure value 
     * Note        : returns 0 upon success, 1 - upon failure
     * Description : reaper of uploading 'condor_transferer' process
     */
    int
    uploadReplicaTransfererReaper(int pid, int exitStatus);
	/* Function   : broadcastVersion
	 * Arguments  : command - id that is sent to other replication daemons
	 *						  along with the local version
	 * Description: broadcasting different commands to other replication daemons
	 *				along with the local version
	 */
    void broadcastVersion( int command );
	/* Function   : requestVersions 
     * Description: sending command to other replication daemons, asking them to
	 * 				send their replica versions to this replication daemon 
     */
    void requestVersions();
	/* Function    : download
	 * Arguments   : daemonSinfulString - address of daemon to download the
	 *									 version from
	 * Return value: bool - success/failure value
	 * Description : starts downloading 'condor_transferer' process to download
	 *				 the version of remote replication daemon.
	 *				 downloadNew() uses a newer protocol.
     */
    bool download(const char* daemonSinfulString);
    bool downloadNew(const char* daemonSinfulString);
	/* Function    : upload
     * Arguments   : daemonSinfulString - address of daemon to upload the
     *                                   version to
	 * Return value: bool - success/failure value
     * Description : starts uploading 'condor_transferer' process to upload
     *               the version to remote replication daemon.
     *               uploadNew() uses a newer protocol, where the transferer
     *               inherits the given connection.
     */
    bool upload(const char* daemonSinfulString);
    bool uploadNew(Stream *stream);
	/* Function   : finalize
	 * Description: clears and resets all inner structures and data members
	 */
    void finalize();
    /* Function   : initializeReplicationList
	 * Arguments  : buffer - the string to initialize the replication daemons'
	 *						 list from
     * Description: initializes replication daemons list from the given string 
     */ 
    void initializeReplicationList(char* buffer);
	/* Function   : updateVersionsList
	 * Arguments  : newVersion - the version to update the versions' list
	 * Description: updates list of versions with new version
	 */
    void updateVersionsList(Version& newVersion);
	/* Function   : cancelVersionsListLeader 
     * Description: sets the state of all versions in the list to BACKUP
     */
    void cancelVersionsListLeader();
	/* Function   : sendCommand
	 * Arguments  : command            - id
	 *				daemonSinfulString - remote replication daemon address
	 *				function           - function that adds specific data that
	 *									 is to be sent
     * Description: generic function to send any command to the remote 
	 *				replication daemon; the given function adds specific
	 *				data structures to send according to the kind of message
	 *				needed. For example, for sending version through this
	 *				function the 'function' must be able to encode the local
	 *				version to the socket
     */
    void sendCommand(int command, char* daemonSinfulString,
                     CommandFunction function);
	/* Function   : sendVersionAndStateCommand
	 * Arguments  : command            - id
	 *				daemonSinfulString - remote replication daemon address
     * Description: this function demonstrates a usage of more general 
	 *				'sendCommand'; it sends to the remote daemon the local
	 *				version and the state of this replication daemon
     */
    void sendVersionAndStateCommand(int command, char* daemonSinfulString) {
        sendCommand( command, daemonSinfulString,
            &AbstractReplicatorStateMachine::versionAndStateCommand );
    };
// Command functions    
	/* Function    : versionAndStateCommand 
     * Arguments   : socket - socket through which the data is send to the 
	 *						  remote replication daemon
	 * Return value: success/failure value
     * Description : specific command function, adding to the socket the local
	 *				 version and the state of this replication daemon
     */
    bool versionAndStateCommand(ReliSock& socket);
	/* Function    : versionCommand
     * Arguments   : socket - socket through which the data is send to the 
     *                        remote replication daemon
     * Return value: success/failure value
     * Description : specific command function, adding to the socket the local
     *               version
     */
    bool versionCommand(ReliSock& );
	/* Function    : noCommand
     * Arguments   : socket - socket through which the data is send to the 
     *                        remote replication daemon
     * Return value: success/failure value
     * Description : specific command function, adding nothing to the socket
     */
    bool noCommand(ReliSock& ) { return true;  };
// End of command functions
	/* Function    : killTransferers
     * Description : kills all the uploading and downloading transferers
     */
    void killTransferers();

protected:
	/* The structure encapsulates process id and the last timestamp of the
     * process creation. The structure is used for downloading/uploading
	 * transferer processes
	 */
	struct ProcessMetadata
	{
		ProcessMetadata()
		{
			set();
		};

		ProcessMetadata(int pPid, time_t pLastTimeCreated)
		{
			set(pPid, pLastTimeCreated);
		};

		/* Function   : set
		 * Arguments  : pPid - new pid
		 * 			    pLastTimeCreated - new last creation timestamp
		 * Description: assigns specified arguments to the structure members
		 */
		void set(int pPid = -1, time_t pLastTimeCreated = -1)
		{
			m_pid             = pPid;
			m_lastTimeCreated = pLastTimeCreated;
		};		
		/* Function    : isValid
         * Return value: checks whether the structure values belong to a valid
		 *				 process, i.e that the structure was initialized
		 * Description : checks the structure validity by comparing its data
		 *				 members to the initialization value
         */
		bool isValid() const
		{
			return m_pid != -1 && m_lastTimeCreated != -1;
		};

		int    m_pid;
    	time_t m_lastTimeCreated;
	};
	// local version
    Version                  m_myVersion;
    // list of versions sent to the daemon during JOINING state
	List<Version>            m_versionsList;
	// configuration variables	
    std::string              m_stateFilePath;
    std::string              m_versionFilePath;
    std::string                 m_transfererPath;

	// the replication daemon state
    ReplicatorState          m_state;

	// list of remote replication daemons
    StringList               m_replicationDaemonsList;
	// socket connection timeout
    int                      m_connectionTimeout;

	// uploading/downloading 'condor_transferer' reapers' ids
    int                      m_downloadReaperId;
    int                      m_uploadReaperId;

	void printDataMembers()
	{
		dprintf( D_ALWAYS, "\n"	
						   "State file path        - %s\n"
						   "Version file path      - %s\n"
						   "State                  - %d\n"
						   "Transferer executeable - %s\n"
						   "Connection timeout     - %d\n"
						   "Downloading reaper id  - %d\n"
						   "Uploading reaper id    - %d\n",
				 m_stateFilePath.c_str(), m_versionFilePath.c_str(), 
				 m_state, m_transfererPath.c_str(), m_connectionTimeout,
				 m_downloadReaperId, m_uploadReaperId );    
	};
	// process ids of uploading/downloading 'condor_transferer' processes for
	// monitoring and handling the problem of stuck transferer processes and
    // starting times of uploading/downloading 'condor_transferer' processes
	// for handling the problem of stuck transferer processes
	ProcessMetadata          m_downloadTransfererMetadata;
	List<ProcessMetadata>    m_uploadTransfererMetadataList;
};

#endif // ABSTRACT_REPLICATOR_STATE_MACHINE_H
