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

#ifndef UPLOAD_REPLICA_TRANSFERER_H
#define UPLOAD_REPLICA_TRANSFERER_H

#include "BaseReplicaTransferer.h"

/* Class      : UploadReplicaTransferer
 * Description: class, encapsulating the uploading 'condor_transferer'
 *              process behaviour
 */
class UploadReplicaTransferer : public BaseReplicaTransferer
{
public:
    /* Function  : UploadReplicaTransferer constructor
     * Arguments : pDaemonSinfulString  - downloading daemon sinfull string
     *             pVersionFilePath     - version string in dot-separated format
     *             pStateFilesPathsList - list of paths to the state files
     */
    UploadReplicaTransferer(const MyString&  pDaemonSinfulString,
                            const MyString&  pVersionFilePath,
                            //const MyString&  pStateFilePath):
							const StringList& pStateFilePathsList):
         BaseReplicaTransferer( pDaemonSinfulString,
                                pVersionFilePath,
                                //pStateFilePath ) {};
								pStateFilePathsList ) {}; 
	/* Function    : initialize
     * Return value: TRANSFERER_TRUE  - upon success
     *               TRANSFERER_FALSE - upon failure
     * Description : main function of uploading 'condor_transferer' process,
     *               where the uploading of important files happens
     */
     virtual int initialize();

private:
    int upload();
    int uploadFile(MyString& filePath, MyString& extension);
};

#endif // UPLOAD_REPLICA_TRANSFERER_H
