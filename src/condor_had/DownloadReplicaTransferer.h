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

#ifndef DOWNLOAD_REPLICA_TRANSFERER_H
#define DOWNLOAD_REPLICA_TRANSFERER_H

#include "BaseReplicaTransferer.h"

/* Class      : ReplicaDown
 * Description: class, encapsulating the downloading 'condor_transferer'
 *              process behaviour
 */
class DownloadReplicaTransferer : public BaseReplicaTransferer
{
public:
   /* Function  : DownloadReplicaTransferer constructor
    * Arguments : command              - exact command given on command line
    *             pDaemonSinfulString  - uploading daemon sinful string
    *             pVersionFilePath     - version string in dot-separated format
    *             pStateFilesPathsList - list of paths to the state files
    */
    DownloadReplicaTransferer(const std::string& command,
                              const std::string& pDaemonSinfulString,
                              const std::string& pVersionFilePath,
                              const std::vector<std::string>& pStateFilePathsList):
        BaseReplicaTransferer( pDaemonSinfulString,
                               pVersionFilePath,
							   pStateFilePathsList ),
        m_command( command ) {};
    /* Function    : initialize
     * Return value: TRANSFERER_TRUE - upon success
     *               TRANSFERER_FALSE - upon failure
     * Description : main function of downloading 'condor_transferer' process,
     *               where the downloading of important files happens
     */
    virtual int initialize();

private:
    int download();
    int downloadFile(const std::string& filePath, const std::string& extension);

    int transferFileCommand();
    int transferFileCommandNew();

	std::string m_command;
};

#endif // DOWNLOAD_REPLICA_TRANSFERER_H
