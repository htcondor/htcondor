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

#ifndef _VIEW_SERVER_H_
#define _VIEW_SERVER_H_

//---------------------------------------------------

#include "collector.h"
#include <set>
#include "HashTable.h"

//---------------------------------------------------

// Note: This should be kept in sync with condor_state.h,  and with
// StartdScanFunc() of view_server.C, and with
// ResConvStr() of condor_tools/stats.C .
// Also, make sure that you keep VIEW_STATE_MAX pointing at
// the last "normal" VIEW_STATE
typedef enum {
	VIEW_STATE_UNDEFINED = 0,
	VIEW_STATE_UNCLAIMED = 1,
	VIEW_STATE_MATCHED = 2,
	VIEW_STATE_CLAIMED = 3,
	VIEW_STATE_PREEMPTING = 4,
	VIEW_STATE_OWNER = 5,
	VIEW_STATE_SHUTDOWN = 6,
	VIEW_STATE_DELETE = 7,
	VIEW_STATE_BACKFILL = 8,
	VIEW_STATE_DRAINED = 9,

	VIEW_STATE_MAX = VIEW_STATE_DRAINED,
	VIEW_STATE_MAX_OFFSET = VIEW_STATE_MAX - 1,
} ViewStates;
struct GeneralRecord {
	float Data[VIEW_STATE_MAX];
	GeneralRecord() {
		for( int i=0; i<(int)VIEW_STATE_MAX; i++ ) {
			Data[i] = 0.0;
		}
	};
};

//---------------------------------------------------

typedef HashTable<std::string, GeneralRecord*> AccHash;
typedef std::vector< time_t > ExtIntArray;
typedef std::vector< fpos_t* > ExtOffArray;

//---------------------------------------------------

struct DataSetInfo {
	std::string OldFileName, NewFileName;
	time_t OldStartTime, NewStartTime;
	int NumSamples, MaxSamples;
	AccHash* AccData;
};

//---------------------------------------------------

class ViewServer : public CollectorDaemon {

public:

	ViewServer();			 // constructor
	virtual ~ViewServer() {};

	void Init();             // main_init
	void Config();           // main_config
	void Exit();             // main__shutdown_fast
	void Shutdown();         // main_shutdown_graceful

	static int ReceiveHistoryQuery(int, Stream*);
	static int HandleQuery(Stream*, int cmd, time_t FromDate, time_t ToDate, int Options, std::string Arg);
	static int SendListReply(Stream*,const std::string& FileName, time_t FromDate, time_t ToDatei, std::set<std::string>& Names);
	static int SendDataReply(Stream*,const std::string& FileName, time_t FromDate, time_t ToDate, int Options, const std::string& Arg);

	static void WriteHistory(int tid);
	static int SubmittorScanFunc(CollectorRecord*);
	static int SubmittorTotalFunc(void);
	static int StartdScanFunc(CollectorRecord*);
	static int StartdTotalFunc(void);
	static int CkptScanFunc(CollectorRecord*);

private:

	// Configuration variables

	static int HistoryInterval;
	static int MaxFileSize;

	// Constants

	static const int HistoryLevels=3;
	enum { SubmittorData, StartdData, GroupsData, SubmittorGroupsData, CkptData, DataSetCount };

	// State variables - data set information

	static DataSetInfo DataSet[DataSetCount][HistoryLevels];
	static std::string DataFormat[DataSetCount];
	
	// Variables used during iteration

	static time_t TimeStamp;
	static AccHash* GroupHash;

	static GeneralRecord* GetAccData(AccHash* AccData,const std::string& Key);

	// Variables used for quick searches by condor_stats

	static HashTable< std::string, int >* FileHash;
	static std::vector< ExtIntArray* >* TimesArray;
	static std::vector< ExtOffArray* >* OffsetsArray;

	// misc variables

	static int HistoryTimer;
	static bool KeepHistory;

	// Utility functions

	static void addNewOffset(FILE*&, int &offset_ctr, int read_time,
						ExtIntArray* times_array, ExtOffArray* offsets);
	static fpos_t* findOffset(FILE*&, int FromDate, int ToDate,
						ExtIntArray* times_array, ExtOffArray* offsets);

	static time_t ReadTime(const char* Line);
	static time_t ReadTimeAndName(const std::string &line, std::string& Name);
	static time_t ReadTimeChkName(const std::string &line, const std::string& Name);
	static time_t FindFileStartTime(const char *Name);
};


#endif
