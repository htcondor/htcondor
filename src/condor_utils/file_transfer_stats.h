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

#ifndef __FILETRANSFER_STATS_H__
#define __FILETRANSFER_STATS_H__

#include "condor_classad.h"
#include "hashkey.h"
#include "generic_stats.h"
#include "classad/classad.h"

class FileTransferStats
{
    public:
        FileTransferStats();
        ~FileTransferStats();

        void Init();
        void Publish(classad::ClassAd & ad) const;


        bool TransferSuccess;

        double ConnectionTimeSeconds;
        int LibcurlReturnCode;
        time_t TransferEndTime;
        time_t TransferStartTime;

        long long TransferFileBytes;
        long TransferHTTPStatusCode;
        long long TransferTotalBytes;
        long TransferTries;

        std::string HttpCacheHitOrMiss;
        std::string HttpCacheHost;
        std::string TransferError;
        std::string TransferFileName;
        std::string TransferHostName;
        std::string TransferLocalMachineName;
        std::string TransferProtocol;
        std::string TransferType;
        std::string TransferUrl;

        StatisticsPool Pool;

        std::vector<ClassAd> TransferErrorData;
};

#endif
