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
 #include "condor_collector.h"
 #include "hashkey.h"
 #include "extArray.h"
 #include "generic_stats.h"
 
 using namespace std;

 class FileTransferStats
 {
	 public:
		FileTransferStats();
		~FileTransferStats();

		bool TransferSuccess;
		
		double ConnectionTimeSeconds;
		double TransferEndTime;
		double TransferStartTime;
		
		long TransferFileBytes;
		long TransferReturnCode;
		long TransferTotalBytes;
		long TransferTries;
		
		string HttpCacheHitOrMiss;
		string HttpCacheHost;
		string TransferError;
		string TransferFileName;
		string TransferHostName;
		string TransferLocalMachineName;
		string TransferProtocol;
		string TransferType;
		string TransferUrl;
		
		StatisticsPool Pool;

		
		void Init();
		void Publish(classad::ClassAd & ad) const;
		 
	 private:
		
 };

 #endif
