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
    TransferHTTPStatusCode = -1;
    TransferSuccess = false;
    TransferTotalBytes = 0;
    TransferTries = 0;
    ConnectionTimeSeconds = 0;
	TransferEndTime = 0;
	TransferStartTime = 0;
	TransferFileBytes = 0;
    LibcurlReturnCode = -1;
}

void FileTransferStats::Publish(classad::ClassAd &ad) const {
    
    // The following statistics appear in every ad
    ad.InsertAttr("ConnectionTimeSeconds", ConnectionTimeSeconds);
    ad.InsertAttr("TransferEndTime", TransferEndTime);
    ad.InsertAttr("TransferFileBytes", TransferFileBytes);
    ad.InsertAttr("TransferStartTime", TransferStartTime);
    ad.InsertAttr("TransferSuccess", TransferSuccess);
    ad.InsertAttr("TransferTotalBytes", TransferTotalBytes);

    // The following statistics only appear if they have a value set
    if (!HttpCacheHitOrMiss.empty())
        ad.InsertAttr("HttpCacheHitOrMiss", HttpCacheHitOrMiss);
    if (!HttpCacheHost.empty())
        ad.InsertAttr("HttpCacheHost", HttpCacheHost);
    if (!TransferError.empty()) {
        std::string augmented_error_msg = TransferError;
        const char *proxy = getenv("http_proxy");
        if (proxy) {
            augmented_error_msg += " using http_proxy=";
            augmented_error_msg += proxy;
        }
        ad.InsertAttr("TransferError", augmented_error_msg);
    }
    if (!TransferFileName.empty())
        ad.InsertAttr("TransferFileName", TransferFileName);
    if (!TransferHostName.empty())
        ad.InsertAttr("TransferHostName", TransferHostName);
    if (!TransferLocalMachineName.empty())
        ad.InsertAttr("TransferLocalMachineName", TransferLocalMachineName);
    if (!TransferProtocol.empty())
        ad.InsertAttr("TransferProtocol", TransferProtocol);
    if (TransferHTTPStatusCode > 0) 
        ad.InsertAttr("TransferHTTPStatusCode", TransferHTTPStatusCode);
    if (LibcurlReturnCode >= 0)
        ad.InsertAttr("LibcurlReturnCode", LibcurlReturnCode);
    if (TransferTries > 0) 
        ad.InsertAttr("TransferTries", TransferTries);
    if (!TransferType.empty())
        ad.InsertAttr("TransferType", TransferType);
    if (!TransferUrl.empty())
        ad.InsertAttr("TransferUrl", TransferUrl);     
    
}
