/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_state.h"
#include "Set.h"
#include "directory.h"
#include "view_server.h"

//-------------------------------------------------------------------

// about self

//-------------------------------------------------------------------

int ViewServer::HistoryInterval;
int ViewServer::MaxFileSize;
DataSetInfo ViewServer::DataSet[DataSetCount][HistoryLevels];
int ViewServer::TimeStamp;
int ViewServer::HistoryTimer;
MyString ViewServer::DataFormat[DataSetCount];
AccHash* ViewServer::GroupHash;
int ViewServer::KeepHistory;

//-----------------------
// Constructor
//------------------------
ViewServer::ViewServer() : MaxGroups(20) {}



//-------------------------------------------------------------------
// Initialization
//-------------------------------------------------------------------

// main initialization code... the real main() is in DaemonCore
void ViewServer::Init()
{
	dprintf(D_ALWAYS, "In ViewServer::Init()\n");

	// Check operation mode

	KeepHistory=FALSE;
	char* tmp=param("KEEP_POOL_HISTORY");
	if( tmp ) {
		if( *tmp == 'T' || *tmp == 't' ) KeepHistory=TRUE;
		free( tmp );
	}

	// Initialize collector daemon

	CollectorDaemon::Init();
	if (!KeepHistory) return;

	// install command handlers for queries

    daemonCore->Register_Command(QUERY_HIST_STARTD,"QUERY_HIST_STARTD",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_STARTD_LIST,"QUERY_HIST_STARTD_LIST",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTOR,"QUERY_HIST_SUBMITTOR",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTOR_LIST,"QUERY_HIST_SUBMITTOR_LIST",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTORGROUPS,"QUERY_HIST_SUBMITTORGROUPS",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTORGROUPS_LIST,"QUERY_HIST_SUBMITTORGROUPS_LIST",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_GROUPS,"QUERY_HIST_GROUPS",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_GROUPS_LIST,"QUERY_HIST_GROUPS_LIST",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_CKPTSRVR,"QUERY_HIST_CKPTSRVR",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);
    daemonCore->Register_Command(QUERY_HIST_CKPTSRVR_LIST,"QUERY_HIST_CKPTSRVR_LIST",
        (CommandHandler)ReceiveHistoryQuery,"ReceiveHistoryQuery",NULL,READ);

	tmp=param("UPDATE_INTERVAL");
	int InitialDelay=300;
	if (tmp) {
		InitialDelay=atoi(tmp);
		free(tmp);
	}

	// Add View Server Flag to class ad
	ad->Insert("ViewServer = True");

	// Register timer for logging information to history files
	HistoryTimer = daemonCore->Register_Timer(InitialDelay,HistoryInterval,
		(Event)WriteHistory,"WriteHistory");

	// Initialize hash tables

	for (int i=0; i<DataSetCount; i++) {
		for (int j=0; j<HistoryLevels; j++) {
			DataSet[i][j].AccData=new AccHash(1000,HashFunc);
		}
	}
	GroupHash=new AccHash(100,HashFunc);

	// File data format

	DataFormat[SubmittorData]="%d\t%s\t:\t%.0f\t%.0f\n";
	DataFormat[SubmittorGroupsData]="%d\t%s\t:\t%.0f\t%.0f\n";
	DataFormat[StartdData]="%d\t%s\t:\t%.0f\t%7.3f\t%.0f\n";
	DataFormat[GroupsData]="%d\t%s\t:\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\n";
	DataFormat[CkptData]="%d\t%s\t:\t%.3f\t%.3f\t%.3f\t%.3f\n";
	
	return;
}

//-------------------------------------------------------------------
// Read configuration
//-------------------------------------------------------------------

void ViewServer::Config()
{
	MyString history_dir_buf;
	char const *history_dir;
	dprintf(D_ALWAYS, "In ViewServer::Config()\n");
	
	// Configure Collector daemon

	CollectorDaemon::Config();
	if (!KeepHistory) return;

	// Specialized configuration

	char* tmp=param("POOL_HISTORY_SAMPLING_INTERVAL");
	if (!tmp) {
		HistoryInterval=60;
	} else {
		HistoryInterval=atoi(tmp);
		free(tmp);
	}

	int MaxStorage;
	tmp=param("POOL_HISTORY_MAX_STORAGE");
	if (!tmp) {
		MaxStorage=10000000;
	} else {
		MaxStorage=atoi(tmp);
		free(tmp);
	}
	MaxFileSize=MaxStorage/(HistoryLevels*DataSetCount*2);

	tmp=param("POOL_HISTORY_DIR");
	if (!tmp) {
		tmp=param("LOCAL_DIR");
		if (!tmp) {
			EXCEPT("No POOL_HISTORY_DIR or LOCAL_DIR directory specified in config file\n");
		}
		history_dir_buf.sprintf("%s/ViewHist",tmp);
	}
	else {
		history_dir_buf = tmp;
	}
	history_dir = history_dir_buf.Value();
	free(tmp);

	dprintf(D_ALWAYS, "Configuration: SAMPLING_INTERVAL=%d, MAX_STORAGE=%d, MaxFileSize=%d, POOL_HISTORY_DIR=%s\n",HistoryInterval,MaxStorage,MaxFileSize,history_dir);

	if(!IsDirectory(history_dir)) {
		EXCEPT("POOL_HISTORY_DIR (%s) does not exist.\n",history_dir);
	}

	for (int i=0; i<DataSetCount; i++) {
		for (int j=0; j<HistoryLevels; j++) {
			DataSet[i][j].MaxSamples=4*((int) pow((double)4,(double)j));
			DataSet[i][j].NumSamples=0;
			DataSet[i][j].OldFileName.sprintf("%s/viewhist%d.%d.old",history_dir,i,j);
			DataSet[i][j].OldStartTime=FindFileStartTime(DataSet[i][j].OldFileName.Value());
			DataSet[i][j].NewFileName.sprintf("%s/viewhist%d.%d.new",history_dir,i,j);
			DataSet[i][j].NewStartTime=FindFileStartTime(DataSet[i][j].NewFileName.Value());
		}
	}

	return;
}

//-------------------------------------------------------------------
// Fast shutdown
//-------------------------------------------------------------------

void ViewServer::Exit()
{
	CollectorDaemon::Exit();
	return;
}

//-------------------------------------------------------------------
// Gracefull shutdown
//-------------------------------------------------------------------

void ViewServer::Shutdown()
{
	CollectorDaemon::Shutdown();
	return;
}

//-------------------------------------------------------------------

int ViewServer::HashFunc(const MyString& Key, int TableSize) {
	int count=0;
	int length=Key.Length();
	for(int i=0; i<length; i++) count+=Key[i];
	return (count % TableSize);
}

//-------------------------------------------------------------------
// Receive the query parameters from the socket, then call
// HandleQuery to take care of replying to the query
//-------------------------------------------------------------------

int ViewServer::ReceiveHistoryQuery(Service* s, int command, Stream* sock)
{
	dprintf(D_ALWAYS,"Got history query %d\n",command);

	int FromDate, ToDate, Options;
	char Line[200];
	char* LinePtr=Line;

	// Read query parameters

	sock->decode();
	if (!sock->code(FromDate) ||
	    !sock->code(ToDate) ||
	    !sock->code(Options) ||
	    !sock->code(LinePtr) ||
	    !sock->end_of_message()) {
		dprintf(D_ALWAYS,"Can't receive history query!\n");
		return 1;
	} 

	dprintf(D_ALWAYS, "Got history query: FromDate=%d, ToDate=%d, Type=%d, Arg=%s\n",FromDate,ToDate,command,LinePtr);

	// Reply to query

	sock->encode();

	HandleQuery(sock,command,FromDate,ToDate,Options,LinePtr);

	strcpy(Line,""); // Send an empty line to signal end of data
	if (!sock->code(LinePtr) ||
	    !sock->end_of_message()) {
		dprintf(D_ALWAYS,"Can't send information to client!\n");
		return 1;
	} 
	
	return 0;
}

//-------------------------------------------------------------------
// Decide which files to get the data from, and call the
// appropriate function to send the data
//-------------------------------------------------------------------

int ViewServer::HandleQuery(Stream* sock, int command, int FromDate, int ToDate, int Options, MyString Arg)
{
	int DataSetIdx=-1;
	int ListFlag=0;

	// Find out which data set type

	switch(command) {
		case QUERY_HIST_STARTD_LIST:			ListFlag=1;
		case QUERY_HIST_STARTD:					DataSetIdx=StartdData;
												break;
		case QUERY_HIST_SUBMITTOR_LIST: 		ListFlag=1;
		case QUERY_HIST_SUBMITTOR:				DataSetIdx=SubmittorData;
												break;
		case QUERY_HIST_SUBMITTORGROUPS_LIST:	ListFlag=1;
		case QUERY_HIST_SUBMITTORGROUPS:		DataSetIdx=SubmittorGroupsData;
												break;
		case QUERY_HIST_GROUPS_LIST:			ListFlag=1;
		case QUERY_HIST_GROUPS:					DataSetIdx=GroupsData;
												break;
		case QUERY_HIST_CKPTSRVR_LIST:			ListFlag=1;
		case QUERY_HIST_CKPTSRVR:				DataSetIdx=CkptData;
												break;
	}

	if (DataSetIdx==-1) return -1; // Illegal command

	// find out which level

	int HistoryLevel=-1;
	int OldFlag=1, NewFlag=1;

	for (int j=0; j<HistoryLevels; j++) {
		if (FromDate>=DataSet[DataSetIdx][j].NewStartTime && DataSet[DataSetIdx][j].NewStartTime!=-1) {
			HistoryLevel=j;
			OldFlag=0;
			break;
		}
		else if (FromDate>=DataSet[DataSetIdx][j].OldStartTime && DataSet[DataSetIdx][j].OldStartTime!=-1) {
			HistoryLevel=j;
			if (ToDate<DataSet[DataSetIdx][j].NewStartTime) NewFlag=0;
			break;
		}
	}

	if (HistoryLevel==-1) {
		int LevelStartTime;
		int MinStartTime=-1;
		for (int j=0; j<HistoryLevels; j++) {
			LevelStartTime=DataSet[DataSetIdx][j].OldStartTime;
			if (LevelStartTime==-1) LevelStartTime=DataSet[DataSetIdx][j].NewStartTime;
			if (LevelStartTime!=-1) {
				if (LevelStartTime<MinStartTime || MinStartTime==-1) {
					MinStartTime=LevelStartTime;
					HistoryLevel=j;
				}
			}
		}
	}

	dprintf(D_ALWAYS,"DataSetIdx=%d, HistoryLevel=%d, OldFlag=%d, NewFlag=%d\n",DataSetIdx,HistoryLevel,OldFlag,NewFlag);
	if (HistoryLevel==-1) return 0;
	dprintf(D_ALWAYS,"OldStartTime=%d , NewStartTime=%d\n",DataSet[DataSetIdx][HistoryLevel].OldStartTime,DataSet[DataSetIdx][HistoryLevel].NewStartTime);

	// Read file and send Data

	if (ListFlag) {
		Set<MyString> Names;
		if (OldFlag) SendListReply(sock, DataSet[DataSetIdx][HistoryLevel].OldFileName,FromDate,ToDate,Names);
		if (NewFlag) SendListReply(sock, DataSet[DataSetIdx][HistoryLevel].NewFileName,FromDate,ToDate,Names);
	} else {
		if (OldFlag) SendDataReply(sock, DataSet[DataSetIdx][HistoryLevel].OldFileName,FromDate,ToDate,Options,Arg);
		if (NewFlag) SendDataReply(sock, DataSet[DataSetIdx][HistoryLevel].NewFileName,FromDate,ToDate,Options,Arg);
	}

	return 0;
}

//---------------------------------------------------------------------
// Send the list of names from the specified file for the
// requested time range
//---------------------------------------------------------------------

int ViewServer::SendListReply(Stream* sock,const MyString& FileName, int FromDate, int ToDate, Set<MyString>& Names) 
{
	char InpLine[200];
	char OutLine[200];
	char* OutLinePtr=OutLine;
	MyString Arg;
	int T;

	// dprintf(D_ALWAYS,"Filename=%s\n",(const char*)FileName);
	FILE* fp=fopen(FileName.Value(),"r");
	if (!fp) return -1;

	while(fgets(InpLine,sizeof(InpLine),fp)) {
		// dprintf(D_ALWAYS,"Line read: %s\n",InpLine);
		T=ReadTimeAndName(InpLine,Arg);
		// dprintf(D_ALWAYS,"T=%d\n",T);
		if (T>ToDate) break;
		if (T<FromDate) continue;
		if (!Names.Exist(Arg)) {
			// dprintf(D_ALWAYS,"Adding Name=%s\n",Arg.Value());
			Names.Add(Arg);
			sprintf(OutLinePtr,"%s\n",Arg.Value());
			if (!sock->code(OutLinePtr)) {
				dprintf(D_ALWAYS,"Can't send information to client!\n");
				fclose(fp);
				return -1;
			}
		}
	}

	fclose(fp);
	return 0;
}

//---------------------------------------------------------------------
// Send the actual data from the specified file for the
// requested time range and name
//---------------------------------------------------------------------

int ViewServer::SendDataReply(Stream* sock,const MyString& FileName, int FromDate, int ToDate, int Options, const MyString& Arg) 
{
	char InpLine[200];
	char* InpLinePtr=InpLine;
	char OutLine[200];
	char* OutLinePtr=OutLine;
	char* tmp;
	int Status=0;
	int NewTime, OldTime;
	float OutTime;
	int T;

	OldTime = 0;
	FILE* fp=fopen(FileName.Value(),"r");
	if (!fp) return -1;

	while(fgets(InpLine,sizeof(InpLine),fp)) {
		// dprintf(D_ALWAYS,"Line read: %s\n",InpLine);
		T=ReadTimeChkName(InpLine,Arg);
		// dprintf(D_ALWAYS,"T=%d\n",T);
		if (T>ToDate) break;
		if (T<FromDate) continue;
		if (Options) {
			if (!sock->code(InpLinePtr)) {
				dprintf(D_ALWAYS,"Can't send information to client!\n");
				Status=-1;
				break;
			}
		} 
		else {
			tmp=strchr(InpLine,':');
			OutTime=float(T-FromDate)/float(ToDate-FromDate);
			NewTime=(int)rint(1000*OutTime);
			if (NewTime==OldTime) continue;
            OldTime=NewTime;
			sprintf(OutLinePtr,"%.2f%s",OutTime*100,tmp+1);
			if (!sock->code(OutLinePtr)) {
				dprintf(D_ALWAYS,"Can't send information to client!\n");
				Status=-1;
				break;
			}
		} 
	}

	fclose(fp);
	return Status;
} 

//*******************************************************************
// Utility functions for reading the history files & data
//*******************************************************************

//-------------------------------------------------------------------
// Read the first entry to decide the time of the earliest entry
// in the file
//-------------------------------------------------------------------

int ViewServer::FindFileStartTime(const char *Name)
{
	char Line[200];
	int T=-1;
	FILE* fp=fopen(Name,"r");
	if (fp) {
		fgets(Line,sizeof(Line),fp);
		T=ReadTime(Line);
		fclose(fp);
	}
	dprintf(D_ALWAYS,"FileName=%s , StartTime=%d\n",Name,T);
	return T;
}

//-------------------------------------------------------------------
// Parse the entry time from the line
//-------------------------------------------------------------------

int ViewServer::ReadTime(char* Line)
{
	int t=-1;
	if (sscanf(Line,"%d",&t)!=1) return -1;
	return t;
}

//-------------------------------------------------------------------
// Parse the entry time and name from the line
//-------------------------------------------------------------------

int ViewServer::ReadTimeAndName(char* Line, MyString& Name)
{
	int t=-1;
	char tmp[100];
	if (sscanf(Line,"%d %s",&t,tmp)!=2) return -1;
	Name=tmp;
	return t;
}

//-------------------------------------------------------------------
// Parse the time and check for the specified name
//-------------------------------------------------------------------

int ViewServer::ReadTimeChkName(char* Line, const MyString& Name)
{
	int t=-1;
	char tmp[100];
	if (sscanf(Line,"%d %s",&t,tmp)!=2) return -1;
	if (Name!="*" && Name!=MyString(tmp)) return -1;
	return t;
}

//*********************************************************************
// History file writing functions
//*********************************************************************

//---------------------------------------------------------------------
// Scan function for submittor data
//---------------------------------------------------------------------

void ViewServer::WriteHistory()
{
	MyString Key;
	GeneralRecord* GenRec;
	FILE* DataFile;
	struct stat statbuf;
	char OutLine[200];

	// Accumulate data

	TimeStamp=time(0);

	dprintf(D_ALWAYS,"Accumulating data: Time=%d\n",TimeStamp);

	if (!collector.walkHashTable(SUBMITTOR_AD, SubmittorScanFunc)) {
		dprintf (D_ALWAYS, "Error accumulating data\n");
		return;
	}
	SubmittorTotalFunc();

	if (!collector.walkHashTable(STARTD_AD, StartdScanFunc)) {
		dprintf (D_ALWAYS, "Error accumulating data\n");
		return;
	}
	StartdTotalFunc();
	if (!collector.walkHashTable(CKPT_SRVR_AD, CkptScanFunc)) {
		dprintf (D_ALWAYS, "Error accumulating data\n");
		return;
	}

	// Write to history files

	for (int i=0; i<DataSetCount; i++) {

		for (int j=0; j<HistoryLevels; j++) {

			// Open files for writing

			DataSet[i][j].NumSamples++;
			if (DataSet[i][j].NumSamples<DataSet[i][j].MaxSamples) continue;
			DataSet[i][j].NumSamples=0;
			dprintf(D_FULLDEBUG,"Openning file %s\n",DataSet[i][j].NewFileName.Value());
			DataFile=fopen(DataSet[i][j].NewFileName.Value(),"a");
			if (!DataFile) {
				dprintf(D_ALWAYS,"Could not open data file %s for appending!!! errno=%d\n",DataSet[i][j].NewFileName.Value(),errno);
				EXCEPT("Could not open data file appending!!!");
			}

			// Iterate through accumulated values

			DataSet[i][j].AccData->startIterations();
			while(DataSet[i][j].AccData->iterate(Key,GenRec)) {
				sprintf(OutLine,DataFormat[i].Value(),TimeStamp,Key.Value(),GenRec->Data[0],GenRec->Data[1],GenRec->Data[2],GenRec->Data[3],GenRec->Data[4]);
				delete GenRec;
				fputs(OutLine, DataFile);
			}
	
			// Clear accumulated values

			DataSet[i][j].AccData->clear();

			// close file

			fclose(DataFile);
			dprintf(D_FULLDEBUG,"Closing file %s\n",DataSet[i][j].NewFileName.Value());

			// Update file start time

			if (DataSet[i][j].NewStartTime==-1) DataSet[i][j].NewStartTime=TimeStamp;

			// Check for size limitation and take necessary action

			if (stat(DataSet[i][j].NewFileName.Value(),&statbuf)) {
				dprintf(D_ALWAYS,"Could not check data file %s size!!! errno=%d\n",DataSet[i][j].NewFileName.Value(),errno);
				EXCEPT("Could not check data file size!!!");
			}
			if (statbuf.st_size>MaxFileSize) {
				rename(DataSet[i][j].NewFileName.Value(),DataSet[i][j].OldFileName.Value());
				DataSet[i][j].OldStartTime=DataSet[i][j].NewStartTime;
				DataSet[i][j].NewStartTime=-1;
			}
		}
	}

	return;
}

//---------------------------------------------------------------------
// Scan function for the submittor data
//---------------------------------------------------------------------

int ViewServer::SubmittorScanFunc(ClassAd* ad)
{
	char Machine[200];
	char Name[200];
	int JobsRunning, JobsIdle;
	MyString GroupName;

	// Get Data From Class Ad

	if (ad->LookupString(ATTR_NAME,Name)<0) return 1;
	if (ad->LookupString(ATTR_MACHINE,Machine)<0) return 1;
	GroupName=Name;
	strcat(Name,"/");
	strcat(Name,Machine);
	if (ad->LookupInteger(ATTR_RUNNING_JOBS,JobsRunning)<0) JobsRunning=0;
	if (ad->LookupInteger(ATTR_IDLE_JOBS,JobsIdle)<0) JobsIdle=0;

	// Add to group Totals

	GeneralRecord* GenRec=GetAccData(GroupHash,"Total");
	GenRec->Data[0]+=JobsRunning;
	GenRec->Data[1]+=JobsIdle;
	GenRec=GetAccData(GroupHash,GroupName);
	GenRec->Data[0]+=JobsRunning;
	GenRec->Data[1]+=JobsIdle;
	
	// Add to accumulated data

	int NumSamples;
	for (int j=0; j<HistoryLevels; j++) {
		GenRec=GetAccData(DataSet[SubmittorData][j].AccData, Name);
		NumSamples=DataSet[SubmittorData][j].NumSamples;
		GenRec->Data[0]=(GenRec->Data[0]*NumSamples+JobsRunning)/(NumSamples+1);
		GenRec->Data[1]=(GenRec->Data[1]*NumSamples+JobsIdle)/(NumSamples+1);
	}
	return 1;
}

//---------------------------------------------------------------------
// Accumulates the submittor data totals
//---------------------------------------------------------------------

int ViewServer::SubmittorTotalFunc(void)
{
	// Add to Accumulated Data

	MyString GroupName;
	GeneralRecord* CurValue;
	GeneralRecord* AccValue;
	int NumSamples;

	GroupHash->startIterations();
	while(GroupHash->iterate(GroupName,CurValue)) {
		for (int j=0; j<HistoryLevels; j++) {
			AccValue=GetAccData(DataSet[SubmittorGroupsData][j].AccData, GroupName);
			NumSamples=DataSet[SubmittorGroupsData][j].NumSamples;
			for (int k=0; k<2; k++) {
				AccValue->Data[k]=(AccValue->Data[k]*NumSamples+CurValue->Data[k])/(NumSamples+1);
			}
		}
		delete CurValue;
	}
	GroupHash->clear();

	return 1;
}

//---------------------------------------------------------------------
// Scan function for the startd data
//---------------------------------------------------------------------

int ViewServer::StartdScanFunc(ClassAd* ad)
{
	char Name[200];
	char StateDesc[50];
	float LoadAvg;
	int KbdIdle, st;
	
	// Get Data From Class Ad

	if (ad->LookupString(ATTR_NAME,Name)<0) return 1;
	if (ad->LookupInteger(ATTR_KEYBOARD_IDLE,KbdIdle)<0) KbdIdle=0;
	if (ad->LookupFloat(ATTR_LOAD_AVG,LoadAvg)<0) LoadAvg=0;
	if (ad->LookupString(ATTR_STATE,StateDesc)<0) strcpy(StateDesc,"");
	State StateEnum=string_to_state(StateDesc);
	switch(StateEnum) {
		case owner_state:		st=5; break;
		case preempting_state:	st=4; break;
		case claimed_state:		st=3; break;
		case matched_state:		st=2; break;
		case unclaimed_state:	st=1; break;
	}

	// Get Group Name

	char tmp[200];
	if (ad->LookupString(ATTR_ARCH,tmp)<0) strcpy(tmp,"Unknown");
	MyString GroupName=MyString(tmp)+"/";
	if (ad->LookupString(ATTR_OPSYS,tmp)<0) strcpy(tmp,"Unknown");
	GroupName+=tmp;

	// Add to group Totals

	GeneralRecord* GenRec=GetAccData(GroupHash,"Total");
	GenRec->Data[st-1]++;
	GenRec=GetAccData(GroupHash,GroupName);
	GenRec->Data[st-1]++;
	
	// Add to accumulated data

	int NumSamples;
	for (int j=0; j<HistoryLevels; j++) {
		GenRec=GetAccData(DataSet[StartdData][j].AccData, Name);
		NumSamples=DataSet[StartdData][j].NumSamples;
		GenRec->Data[0]=(GenRec->Data[0]*NumSamples+KbdIdle)/(NumSamples+1);
		GenRec->Data[1]=(GenRec->Data[1]*NumSamples+LoadAvg)/(NumSamples+1);
		GenRec->Data[2]=st;
	}

	return 1;
}

//---------------------------------------------------------------------
// Accumulates the startd data totals
//---------------------------------------------------------------------

int ViewServer::StartdTotalFunc(void)
{
	// Add to Accumulated Data

	MyString GroupName;
	GeneralRecord* CurValue;
	GeneralRecord* AccValue;
	int NumSamples;

	GroupHash->startIterations();
	while(GroupHash->iterate(GroupName,CurValue)) {
		for (int j=0; j<HistoryLevels; j++) {
			AccValue=GetAccData(DataSet[GroupsData][j].AccData, GroupName);
			NumSamples=DataSet[GroupsData][j].NumSamples;
			for (int k=0; k<5; k++) {
				AccValue->Data[k]=(AccValue->Data[k]*NumSamples+CurValue->Data[k])/(NumSamples+1);
			}
		}
		delete CurValue;
	}
	GroupHash->clear();

	return 1;
}

//---------------------------------------------------------------------
// Scan function for the checkpoint server
//---------------------------------------------------------------------

int ViewServer::CkptScanFunc(ClassAd* ad)
{
	char Name[200];
	int Bytes;
	float BytesReceived,BytesSent;
	float AvgReceiveBandwidth,AvgSendBandwidth;
	
	// Get Data From Class Ad

	if (ad->LookupString(ATTR_NAME,Name)<0) return 1;
	if (ad->LookupInteger("BytesReceived",Bytes)<0) Bytes=0;
	BytesReceived=float(Bytes)/(1024*1024);
	if (ad->LookupInteger("BytesSent",Bytes)<0) Bytes=0;
	BytesSent=float(Bytes)/(1024*1024);
	if (ad->LookupFloat("AvgReceiveBandwidth",AvgReceiveBandwidth)<0) AvgReceiveBandwidth=0;
	AvgReceiveBandwidth=AvgReceiveBandwidth/1024;
	if (ad->LookupFloat("AvgSendBandwidth",AvgSendBandwidth)<0) AvgSendBandwidth=0;
	AvgSendBandwidth=AvgSendBandwidth/1024;

	// Add to accumulated data

	int NumSamples;
	GeneralRecord* GenRec;
	for (int j=0; j<HistoryLevels; j++) {
		GenRec=GetAccData(DataSet[CkptData][j].AccData, Name);
		NumSamples=DataSet[CkptData][j].NumSamples;
		GenRec->Data[0]=(GenRec->Data[0]*NumSamples+BytesReceived)/(NumSamples+1);
		GenRec->Data[1]=(GenRec->Data[1]*NumSamples+BytesSent)/(NumSamples+1);
		GenRec->Data[2]=(GenRec->Data[2]*NumSamples+AvgReceiveBandwidth)/(NumSamples+1);
		GenRec->Data[3]=(GenRec->Data[3]*NumSamples+AvgSendBandwidth)/(NumSamples+1);
	}

	return 1;
}

//---------------------------------------------------------------------
// Insert or update data accumulated entry
//---------------------------------------------------------------------

GeneralRecord* ViewServer::GetAccData(AccHash* AccData,const MyString& Key)
{
	GeneralRecord* GenRec;
	if (AccData->lookup(Key,GenRec)==-1) {
		GenRec=new GeneralRecord;
		AccData->insert(Key,GenRec);
	}
	return GenRec;
}
