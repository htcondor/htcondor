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

#ifndef REPLICATOR_STATE_MACHINE_H
#define REPLICATOR_STATE_MACHINE_H

#include "AbstractReplicatorStateMachine.h"

/* Class      : ReplicatorStateMachine
 * Description: concrete class for replication service state machine,
 *              Contains implementation of handlers for notifications from 
 *              HAD about its transitions in its state machine:
 *              1) beforePassiveStateHandler - for HAD_BEFORE_PASSIVE_STATE
 *              2) afterElectionStateHandler - for HAD_AFTER_ELECTION_STATE
 *              3) afterLeaderStateHandler   - for HAD_AFTER_LEADER_STATE
 *              4) inLeaderStateHandler      - for HAD_IN_LEADER_STATE
 *
 *              Besides, it contains implementation of handlers for selection 
 *              of the best version out of versions list and selection of the 
 *              gid according to those versions:
 *              1) replicaSelectionHandler
 *              2) gidSelectionHandler
 */
class ReplicatorStateMachine: public AbstractReplicatorStateMachine
{
public:
	/* Function: ReplicatorStateMachine constructor
     */
    ReplicatorStateMachine();
	/* Function: ReplicatorStateMachine destructor
     */
    virtual ~ReplicatorStateMachine();
// Notification handlers
    /* Function   : beforePassiveStateHandler
     * Description: concrete handler before the event, when HAD entered PASSIVE
     * 				state; broadcasts old local version, solicits versions from
	 *				other replication daemons in the pool
     */
    virtual void beforePassiveStateHandler();
	/* Function   : afterElectionStateHandler
     * Description: concrete handler after the event, when HAD is in transition
     *              from ELECTION to LEADER state; sets the last time, when HAD
	 *              sent a HAD_IN_STATE_STATE (which is kind of "I am alive" 
	 *              message for replication daemon) and selects the new gid for
	 *              the pool 
     */
    virtual void afterElectionStateHandler();
	/* Function   : afterLeaderStateHandler
	 * Description: concrete handler after the event, when HAD is in transition
     *              from ELECTION to PASSIVE state
	 * Remarks    : void by now
	 */
    virtual void afterLeaderStateHandler();
	/* Function   : inLeaderStateHandler
     * Description: concrete handler after the event, when HAD is in inner loop
     *              of LEADER state; sets the last time, when HAD sent a 
	 * 				HAD_IN_STATE_STATE
     */
    virtual void inLeaderStateHandler();
// End of notification handlers
// Selection handlers
	/* Function    : replicaSelectionHandler
     * Arguments   : newVersion -
     *                  in JOINING state: the version selected for downloading
     *                  in BACKUP state : the version compared against the local
     *                                    one in order to check, whether to
     *                                    download the leader's version or not
     * Return value: bool - whether it is worth to download the remote version
	 *						or not
	 * Description : concrete handler for selection of the best version out of
     *               versions list
     */
    virtual bool replicaSelectionHandler(Version& newVersion);
	/* Function   : gidSelectionHandler
     * Description: concrete handler for selection of gid for the pool
	 * Remarks    : void by now
     */
    virtual void gidSelectionHandler();
// End of selection handlers
	/* Function   : initialize
     * Description: initializes all inner structures, such as
	 *				commands, timers, reapers and data members
     */
    void initialize();
	/* Function   : reinitialize
     * Description: reinitializes all inner structures, such as
     *              commands, timers, reapers and data members
     */
    void reinitialize();
protected:
	/* Function   : downloadReplicaTransfererReaper
     * Arguments  : service    - the daemon, for which the transfer has ended
     *              pid        - id of the downloading 'condor_transferer'
     *                           process
     *              exitStatus - return value of the downloading
     *                           'condor_transferer' process
     * Description: reaper of downloading 'condor_transferer' process
     */
    int 
	downloadReplicaTransfererReaper(int pid, int exitStatus);
private:
    std::string    m_name;
    ClassAd*       m_classAd;
    int            m_updateCollectorTimerId;
    int            m_updateInterval;

    void initializeClassAd();
    void updateCollectors();
// Managing stuck transferers
	void killStuckDownloadingTransferer(time_t currentTime);
	void killStuckUploadingTransferers (time_t currentTime);
// End of managing stuck transferers
    int commandHandler(int command, Stream* stream);
    void registerCommand(int command);

    void finalize();
    void finalizeDelta();
// Timers handlers
    void replicationTimer();
    void versionRequestingTimer();
    void versionDownloadingTimer();
// End of timers handlers
// Command handlers
    void onLeaderVersion(Stream* stream);
    void onTransferFile(char* daemonSinfulString);
    void onTransferFileNew(Stream *stream);
    void onSolicitVersion( char* daemonSinfulString );
    void onSolicitVersionReply(Stream* stream);
    void onNewlyJoinedVersion(Stream* stream);
    void onGivingUpVersion(Stream* stream);
// End of command handlers

    static Version* decodeVersionAndState( Stream* stream );
	void            becomeLeader( );

	/* Function   : downloadTransferersNumber
	 * Description: returns number of running downloading 'condor_transferers'
	 */
    int  downloadTransferersNumber() const { 
		return int( m_downloadTransfererMetadata.m_pid != -1 ); 
	};

// Configuration parameters
    int      m_replicationInterval;
    int      m_hadAliveTolerance;
    int      m_maxTransfererLifeTime;
    int      m_newlyJoinedWaitingVersionInterval;
// End of configuration parameters
// Timers
    int     m_versionRequestingTimerId;
    int     m_versionDownloadingTimerId;
    int     m_replicationTimerId;
// End of timers
	// last time HAD sent HAD_IN_LEADER_STATE
    time_t  m_lastHadAliveTime;

// Debugging utilities
	void printDataMembers() const
    {
        dprintf( D_ALWAYS, "\n"
						   "Replication interval                  - %d\n"
                           "HAD alive tolerance                   - %d\n"
                           "Max transferer life time              - %d\n"
                           "Newly joined waiting version interval - %d\n"
                           "Version requesting timer id           - %d\n"
                           "Version downloading timer id          - %d\n"
						   "Replication timer id                  - %d\n"
                           "Last HAD alive time                   - %ld\n",
                 m_replicationInterval, m_hadAliveTolerance, 
				 m_maxTransfererLifeTime, m_newlyJoinedWaitingVersionInterval, 
				 m_versionRequestingTimerId, m_versionDownloadingTimerId, 
				 m_replicationTimerId, m_lastHadAliveTime );
    };

	void checkVersionSynchronization()
	{
		int temporaryGid = -1, temporaryLogicalClock = -1;
        
        m_myVersion.load( temporaryGid, temporaryLogicalClock);
        REPLICATION_ASSERT(
            temporaryGid == m_myVersion.getGid( ) && 
			temporaryLogicalClock == m_myVersion.getLogicalClock( ));        
	};
// End of debugging utilities
};

#endif // REPLICATOR_STATE_MACHINE_H
