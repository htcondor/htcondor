#ifndef _VIEW_SERVER_H_
#define _VIEW_SERVER_H_

//---------------------------------------------------

#include "collector.h"
#include "Set.h"
#include "HashTable.h"

//---------------------------------------------------

struct GeneralRecord {
	float Data[5];
	GeneralRecord() { Data[0]=Data[1]=Data[2]=Data[3]=Data[4]=0.0; }
};

//---------------------------------------------------

typedef HashTable<MyString, GeneralRecord*> AccHash;

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

	static int HashFunc(const MyString& Key, int TableSize);

private:

	// Configuration variables

	static int HistoryInterval;
	static int MaxFileSize;

	// Constants

	enum { HistoryLevels=3 };
	enum { SubmittorData, StartdData, GroupsData, SubmittorGroupsData, CkptData, DataSetCount };
	const int MaxGroups;

	// State variables - data set information

	static DataSetInfo DataSet[DataSetCount][HistoryLevels];
	static MyString DataFormat[DataSetCount];
	
	// Variables used during iteration

	static int TimeStamp;
	static AccHash* GroupHash;

	static GeneralRecord* ViewServer::GetAccData(AccHash* AccData,const MyString& Key);

	// misc variables

	static int HistoryTimer;
	static int KeepHistory;

	// Utility functions

	static int ReadTime(char* Line);
	static int ReadTimeAndName(char* Line, MyString& Name);
	static int ReadTimeChkName(char* Line, const MyString& Name);
	static int FindFileStartTime(char* Name);
};


#endif
