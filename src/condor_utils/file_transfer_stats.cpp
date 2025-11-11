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

    // This first block of attributes are common to all plug-ins.
    ad.InsertAttr("TransferSuccess", TransferSuccess);

    if (!TransferError.empty()) {
        std::string augmented_error_msg = TransferError;

        const char * http_proxy = getenv("http_proxy");
        const char * https_proxy = getenv("https_proxy");
        if( http_proxy || https_proxy ) {
            formatstr_cat ( augmented_error_msg,
                " (with environment: http_proxy='%s', https_proxy='%s')",
                http_proxy ? http_proxy : "",
                https_proxy ? https_proxy : ""
            );
        }

        ad.InsertAttr("TransferError", augmented_error_msg);
    }

    if (!TransferProtocol.empty()) {
        ad.InsertAttr("TransferProtocol", TransferProtocol);
    }
    if (!TransferType.empty()) {
        ad.InsertAttr("TransferType", TransferType);
    }
    if (!TransferFileName.empty()) {
        ad.InsertAttr("TransferFileName", TransferFileName);
    }

    ad.InsertAttr("TransferFileBytes", TransferFileBytes);
    ad.InsertAttr("TransferTotalBytes", TransferTotalBytes);
    ad.InsertAttr("TransferStartTime", TransferStartTime);
    ad.InsertAttr("TransferEndTime", TransferEndTime);
    ad.InsertAttr("ConnectionTimeSeconds", ConnectionTimeSeconds);

    if (!TransferUrl.empty()) {
        ad.InsertAttr("TransferUrl", TransferUrl);
    }


    // The following attributes go into the DeveloperData attribute until
    // such time as we elect to standardize them.
    ClassAd * developerAd = new ClassAd();

    if (!HttpCacheHitOrMiss.empty()) {
        developerAd->InsertAttr("HttpCacheHitOrMiss", HttpCacheHitOrMiss);
    }
    // This should be standardized; the host you end up transferring from
    // may not be in the URL, and could still have a cache.
    if (!HttpCacheHost.empty()) {
        developerAd->InsertAttr("HttpCacheHost", HttpCacheHost);
    }
    // This is standardized in the current TransferErrorData proposal
    // as `IntermediateServer`.  It's currently always set incorrectly.
    if (!TransferHostName.empty()) {
        developerAd->InsertAttr("TransferHostName", TransferHostName);
    }
    if (!TransferLocalMachineName.empty()) {
        developerAd->InsertAttr("TransferLocalMachineName", TransferLocalMachineName);
    }
    if (TransferHTTPStatusCode > 0) {
        developerAd->InsertAttr("TransferHTTPStatusCode", TransferHTTPStatusCode);
    }
    if (LibcurlReturnCode >= 0) {
        developerAd->InsertAttr("LibcurlReturnCode", LibcurlReturnCode);
    }
    // This is implicit in the current TransferErrorData proposal.
    if (TransferTries > 0) {
        developerAd->InsertAttr("TransferTries", TransferTries);
    }

    if(developerAd->size() != 0) {
        ad.Insert( "DeveloperData", developerAd );
    } else {
    	delete developerAd;
    }


    // Generate TransferErrorData.
    if( TransferErrorData.size() > 0 ) {
        classad::ExprList * transferErrorDataList = new classad::ExprList();
        for( const auto & ad : TransferErrorData ) {
            ClassAd * sigh = new ClassAd(ad);
            transferErrorDataList->push_back(sigh);
        }
        ad.Insert( "TransferErrorData", transferErrorDataList );
    }
}
