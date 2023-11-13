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

#ifndef BASE_REPLICA_TRANSFERER_H
#define BASE_REPLICA_TRANSFERER_H

#include "dc_service.h"
#include "reli_sock.h"

/* Class      : BaseReplicaTransfererer
 * Description: class, encapsulating the downloading/uploading
 *              'condor_transferer' process behaviour
 *              Generally, the file transfer works in the following way. After
 *	            the backup replication daemon decides to download a version from
 *              the replication leader, the downloading transferer is started.
 *				It opens a listening socket, the port of which it sends to the
 *              replication leader by means of REPLICATION_TRANSFER_FILE command
 *              . The replication leader creates an uploading transferer process
 *              upon receiving the command. The uploading process sends the file
 *              to the downloading transferer to the port, which has been sent
 *              along with the REPLICATION_TRANSFER_FILE command.
 *              In uploading process we do not transfer the actual state and 
 *				version files, we copy them to temporary files and send these
 *				temporary ones to the downloading process. The downloading 
 *				process in its turn receives the files into temporary copies.
 *				After the transfer is finished, the uploading process gets rid
 *				of the temporary copies, while the downloading one replaces the
 *				actual state and version files with their just received
 *				temporary copies.
 */
class BaseReplicaTransferer: public Service
{
public:
    enum { TRANSFERER_TRUE = 0, TRANSFERER_FALSE };
    /* Function  : BaseReplicaTransferer constructor
     * Arguments : pDaemonSinfulString  - downloading/uploading daemon
     *                                    sinfull string
     *             pVersionFilePath     - version string in dot-separated format
     *             pStateFilesPathsList - list of paths to the state files
     */
    BaseReplicaTransferer(const std::string&  pDaemonSinfulString,
                          const std::string&  pVersionFilePath,
                          const std::vector<std::string>& pStateFilePathsList);
    ~BaseReplicaTransferer();                        
    /* Function    : initialize
     * Return value: TRANSFERER_TRUE   - upon success
     *               TRANSFERER_FALSE - upon failure
     * Description : the main function of 'condor_transferer' process,
     *               in which all the communication between the
     *               downloading and the uploading 'condor_transferer'
     *               happens
     */  
    int reinitialize();
	/* Function   : initialize
	 * Description: abstract function, initializing all the data members of
	 *				the 'condor_transferer'
	 */
    virtual int initialize() = 0;
// Inspectors
	std::string getVersionFilePath() { return m_versionFilePath; };
	std::vector<std::string>& getStateFilePathsList() { return m_stateFilePathsList; };
// End of inspectors
protected:
	
	/* Function    : safeUnlinkStateAndVersionFiles
	 * Arguments   : stateFilePathsList - list of paths to the state files
	 *				 versionFilePath    - version string in dot-separated format
	 *				 extension          - extension of temporary file
	 * Description : unlinks temporary copies of version and state files,
	 *				 according to the specified extension
	 */
	static void
	safeUnlinkStateAndVersionFiles(const std::vector<std::string>& stateFilePathsList,
	                               const std::string& versionFilePath,
	                               const std::string& extension);
	// address of the downloading/uploading counterpart	
	std::string  m_daemonSinfulString;
	// path to the file where local version is stored
	std::string  m_versionFilePath;
	// path to the list of files where the state, protected by the replication 
	// is stored
	std::vector<std::string> m_stateFilePathsList;

    // TCP socket, over which all the communication is done
    ReliSock* m_socket;
	// socket connection timeout
    int       m_connectionTimeout;
	int       m_maxTransferLifetime;
};

#endif // BASE_REPLICA_TRANSFERER_H
