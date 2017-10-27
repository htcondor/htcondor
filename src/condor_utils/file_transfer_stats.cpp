/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.	You may
 * obtain a copy of the License at
 * 
 *		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "file_transfer_stats.h"

FileTransferStats::FileTransferStats() {
    Init();
}

FileTransferStats::~FileTransferStats() {}

void FileTransferStats::Init() {
    TransferReturnCode = -1;
    TransferSuccess = false;
    TransferTotalBytes = 0;
    TransferTries = 0;
    ConnectionTimeSeconds = 0;
	TransferEndTime = 0;
	TransferStartTime = 0;
	TransferFileBytes = 0;
}

void FileTransferStats::Publish(ClassAd &ad) const {
    
    // The following statistics appear in every ad
    ad.Assign("ConnectionTimeSeconds", ConnectionTimeSeconds);
    ad.Assign("TransferEndTime", TransferEndTime);
    ad.Assign("TransferFileBytes", TransferFileBytes);
    ad.Assign("TransferStartTime", TransferStartTime);
    ad.Assign("TransferSuccess", TransferSuccess);
    ad.Assign("TransferTotalBytes", TransferTotalBytes);

    // The following statistics only appear if they have a value set
    if (!HttpCacheHitOrMiss.empty())
        ad.Assign("HttpCacheHitOrMiss", HttpCacheHitOrMiss);
    if (!HttpCacheHost.empty())
        ad.Assign("HttpCacheHost", HttpCacheHost);
    if (!TransferError.empty())
        ad.Assign("TransferError", TransferError);
    if (!TransferFileName.empty())
        ad.Assign("TransferFileName", TransferFileName);
    if (!TransferHostName.empty())
        ad.Assign("TransferHostName", TransferHostName);
    if (!TransferLocalMachineName.empty())
        ad.Assign("TransferLocalMachineName", TransferLocalMachineName);
    if (!TransferProtocol.empty())
        ad.Assign("TransferProtocol", TransferProtocol);
    if (TransferReturnCode > 0) 
        ad.Assign("TransferReturnCode", TransferReturnCode);
    if (TransferTries > 0) 
        ad.Assign("TransferTries", TransferTries);
    if (!TransferType.empty())
        ad.Assign("TransferType", TransferType);
    if (!TransferUrl.empty())
        ad.Assign("TransferUrl", TransferUrl);     
    
}
