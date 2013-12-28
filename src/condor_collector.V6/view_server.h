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
#include "Set.h"
#include "HashTable.h"
#include "extArray.h"

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

typedef HashTable<MyString, GeneralRecord*> AccHash;
typedef ExtArray< int > ExtIntArray;
typedef ExtArray< fpos_t* > ExtOffArray;

//---------------------------------------------------

struct DataSetInfo {
	MyString OldFileName, NewFileName;
	int OldStartTime, NewStartTime;
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

	static int ReceiveHistoryQuery(Service*, int, Stream*);
	static int HandleQuery(Stream*, int cmd, int FromDate, int ToDate, int Options, MyString Arg);
	static int SendListReply(Stream*,const MyString& FileName, int FromDate, int ToDatei, Set<MyString>& Names);
	static int SendDataReply(Stream*,const MyString& FileName, int FromDate, int ToDate, int Options, const MyString& Arg);

	static void WriteHistory();
	static int SubmittorScanFunc(ClassAd* ad);
	static int SubmittorTotalFunc(void);
	static int StartdScanFunc(ClassAd* ad);
	static int StartdTotalFunc(void);
	static int CkptScanFunc(ClassAd* ad);

private:

	// Configuration variables

	static int HistoryInterval;
	static int MaxFileSize;

	// Constants

	enum { HistoryLevels=3 };
	enum { SubmittorData, StartdData, GroupsData, SubmittorGroupsData, CkptData, DataSetCount };

	// State variables - data set information

	static DataSetInfo DataSet[DataSetCount][HistoryLevels];
	static MyString DataFormat[DataSetCount];
	
	// Variables used during iteration

	static int TimeStamp;
	static AccHash* GroupHash;

	static GeneralRecord* GetAccData(AccHash* AccData,const MyString& Key);

	// Variables used for quick searches by condor_stats

	static HashTable< MyString, int >* FileHash;
	static ExtArray< ExtIntArray* >* TimesArray;
	static ExtArray< ExtOffArray* >* OffsetsArray;

	// misc variables

	static int HistoryTimer;
	static bool KeepHistory;

	// Utility functions

	static void addNewOffset(FILE*&, int &offset_ctr, int read_time,
						ExtIntArray* times_array, ExtOffArray* offsets);
	static fpos_t* findOffset(FILE*&, int FromDate, int ToDate,
						ExtIntArray* times_array, ExtOffArray* offsets);

	static int ReadTime(char* Line);
	static int ReadTimeAndName(char* Line, MyString& Name);
	static int ReadTimeChkName(char* Line, const MyString& Name);
	static int FindFileStartTime(const char *Name);
};


#endif
