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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_state.h"
#include <set>
#include "directory.h"
#include "view_server.h"
#include "util_lib_proto.h" // for rotate_file

//-------------------------------------------------------------------

// about self

//-------------------------------------------------------------------

int ViewServer::HistoryInterval;
int ViewServer::MaxFileSize;
DataSetInfo ViewServer::DataSet[DataSetCount][HistoryLevels];
time_t ViewServer::TimeStamp;
int ViewServer::HistoryTimer;
std::string ViewServer::DataFormat[DataSetCount];
AccHash* ViewServer::GroupHash;
bool ViewServer::KeepHistory;
HashTable< std::string, int >* ViewServer::FileHash;
std::vector<ExtIntArray *> *ViewServer::TimesArray;
std::vector<ExtOffArray *> *ViewServer::OffsetsArray;

//-----------------------
// Constructor
//------------------------
ViewServer::ViewServer() {}



//-------------------------------------------------------------------
// Initialization
//-------------------------------------------------------------------

// main initialization code... the real main() is in DaemonCore
void ViewServer::Init()
{
	dprintf(D_ALWAYS, "In ViewServer::Init()\n");

	// Check operation mode

	KeepHistory = param_boolean_crufty("KEEP_POOL_HISTORY", false);

	// We can't do this check at compile time, but we'll except if
	// the startd states has changed and we haven't been updated
	// to match
	MSC_SUPPRESS_WARNING(6326) // warning comparing a constant to a constant.
	if ( (int)VIEW_STATE_MAX != (int)_machine_max_state ) {
		EXCEPT( "_max_state=%d (from condor_state.h) doesn't match "
				" VIEW_STATE_MAX=%d (from view_server.h)",
				(int)_machine_max_state, (int)VIEW_STATE_MAX );
	}

	// Initialize collector daemon

	CollectorDaemon::Init();
	if (!KeepHistory) return;

	// install command handlers for queries

    daemonCore->Register_Command(QUERY_HIST_STARTD,"QUERY_HIST_STARTD",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_STARTD_LIST,"QUERY_HIST_STARTD_LIST",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTOR,"QUERY_HIST_SUBMITTOR",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTOR_LIST,"QUERY_HIST_SUBMITTOR_LIST",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTORGROUPS,"QUERY_HIST_SUBMITTORGROUPS",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_SUBMITTORGROUPS_LIST,"QUERY_HIST_SUBMITTORGROUPS_LIST",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_GROUPS,"QUERY_HIST_GROUPS",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_GROUPS_LIST,"QUERY_HIST_GROUPS_LIST",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_CKPTSRVR,"QUERY_HIST_CKPTSRVR",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);
    daemonCore->Register_Command(QUERY_HIST_CKPTSRVR_LIST,"QUERY_HIST_CKPTSRVR_LIST",
        ReceiveHistoryQuery,"ReceiveHistoryQuery",READ);

	int InitialDelay=param_integer("UPDATE_INTERVAL",300);

	// Add View Server Flag to class ad
	ad->Assign("ViewServer",  true);

	// Register timer for logging information to history files
	HistoryTimer = daemonCore->Register_Timer(InitialDelay,HistoryInterval,
		WriteHistory,"WriteHistory");

	// Initialize hash tables

	for (int i=0; i<DataSetCount; i++) {
		for (int j=0; j<HistoryLevels; j++) {
			DataSet[i][j].AccData=new AccHash(hashFunction);
		}
	}
	GroupHash = new AccHash(hashFunction);
	FileHash = new HashTable< std::string, int >(hashFunction);
	OffsetsArray = new std::vector< ExtOffArray*>;
	TimesArray = new std::vector<ExtIntArray*>;

	// File data format

	DataFormat[SubmittorData]="%d\t%s\t:\t%.0f\t%.0f\n";
	DataFormat[SubmittorGroupsData]="%d\t%s\t:\t%.0f\t%.0f\n";
	DataFormat[StartdData]="%d\t%s\t:\t%.0f\t%7.3f\t%.0f\n";
	DataFormat[GroupsData]="%d\t%s\t:\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\n";
	DataFormat[CkptData]="%d\t%s\t:\t%.3f\t%.3f\t%.3f\t%.3f\n";

	return;
}

//-------------------------------------------------------------------
// Read configuration
//-------------------------------------------------------------------

void ViewServer::Config()
{
	std::string history_dir_buf;
	char const *history_dir;
	char* tmp;
	dprintf(D_ALWAYS, "In ViewServer::Config()\n");

	// Configure Collector daemon

	CollectorDaemon::Config();
	if (!KeepHistory) return;

	// Specialized configuration

	HistoryInterval=param_integer("POOL_HISTORY_SAMPLING_INTERVAL",60);

	int MaxStorage=param_integer("POOL_HISTORY_MAX_STORAGE",10'000'000);

	MaxFileSize=MaxStorage/(HistoryLevels*DataSetCount*2);

	tmp=param("POOL_HISTORY_DIR");
	if (!tmp) {
		tmp=param("LOCAL_DIR");
		if (!tmp) {
			EXCEPT("No POOL_HISTORY_DIR or LOCAL_DIR directory specified in config file");
		}
		formatstr(history_dir_buf, "%s/ViewHist", tmp);
	}
	else {
		history_dir_buf = tmp;
	}
	history_dir = history_dir_buf.c_str();
	free(tmp);

	dprintf(D_ALWAYS, "Configuration: SAMPLING_INTERVAL=%d, MAX_STORAGE=%d, MaxFileSize=%d, POOL_HISTORY_DIR=%s\n",HistoryInterval,MaxStorage,MaxFileSize,history_dir);

	if(!IsDirectory(history_dir)) {
		EXCEPT("POOL_HISTORY_DIR (%s) does not exist.",history_dir);
	}

	for (int i=0; i<DataSetCount; i++) {
		for (int j=0; j<HistoryLevels; j++) {
			DataSet[i][j].MaxSamples=4*((int) pow((double)4,(double)j));
			DataSet[i][j].NumSamples=0;
			formatstr(DataSet[i][j].OldFileName, "%s/viewhist%d.%d.old",history_dir,i,j);
			DataSet[i][j].OldStartTime=FindFileStartTime(DataSet[i][j].OldFileName.c_str());
			formatstr(DataSet[i][j].NewFileName, "%s/viewhist%d.%d.new",history_dir,i,j);
			DataSet[i][j].NewStartTime=FindFileStartTime(DataSet[i][j].NewFileName.c_str());
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
// Receive the query parameters from the socket, then call
// HandleQuery to take care of replying to the query
//-------------------------------------------------------------------

int ViewServer::ReceiveHistoryQuery(int command, Stream* sock)
{
	dprintf(D_ALWAYS,"Got history query %d\n",command);

	time_t FromDate;
	time_t ToDate;
	int Options;
	char *Line = NULL;

	// Read query parameters

	sock->decode();
	if (!sock->code(FromDate) ||
	    !sock->code(ToDate) ||
	    !sock->code(Options) ||
	    !sock->get(Line) ||
	    !sock->end_of_message()) {
		dprintf(D_ALWAYS,"Can't receive history query!\n");
		free( Line );
		return 1;
	}

	dprintf(D_ALWAYS, "Got history query: FromDate=%lld, ToDate=%lld, Type=%d, Arg=%s\n",(long long) FromDate,(long long) ToDate,command,Line);

	// Reply to query

	sock->encode();

	HandleQuery(sock,command,FromDate,ToDate,Options,Line);

	free( Line );

	if (!sock->put("") ||
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

int ViewServer::HandleQuery(Stream* sock, int command, time_t FromDate, time_t ToDate, int Options, std::string Arg)
{
	int DataSetIdx=-1;
	int ListFlag=0;

	// Find out which data set type

	switch(command) {
		case QUERY_HIST_STARTD_LIST:			ListFlag=1; //@fallthrough@
		case QUERY_HIST_STARTD:					DataSetIdx=StartdData;
												break;
		case QUERY_HIST_SUBMITTOR_LIST: 		ListFlag=1; //@fallthrough@
		case QUERY_HIST_SUBMITTOR:				DataSetIdx=SubmittorData;
												break;
		case QUERY_HIST_SUBMITTORGROUPS_LIST:	ListFlag=1; //@fallthrough@
		case QUERY_HIST_SUBMITTORGROUPS:		DataSetIdx=SubmittorGroupsData;
												break;
		case QUERY_HIST_GROUPS_LIST:			ListFlag=1; //@fallthrough@
		case QUERY_HIST_GROUPS:					DataSetIdx=GroupsData;
												break;
		case QUERY_HIST_CKPTSRVR_LIST:			ListFlag=1; //@fallthrough@
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
		time_t LevelStartTime;
		time_t MinStartTime=-1;
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
	dprintf(D_ALWAYS,"OldStartTime=%lld , NewStartTime=%lld\n",(long long) DataSet[DataSetIdx][HistoryLevel].OldStartTime,(long long) DataSet[DataSetIdx][HistoryLevel].NewStartTime);

	// Read file and send Data

	if (ListFlag) {
		std::set<std::string> Names;
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

int ViewServer::SendListReply(Stream* sock,const std::string& FileName, time_t FromDate, time_t ToDate, std::set<std::string>& Names)
{
	time_t T = 0;
	int file_array_index;
	ExtIntArray* times_array = NULL;
	ExtOffArray* offsets = NULL;

		// first find out which vector to use, by checking the hash
	if( FileHash->lookup( FileName, file_array_index ) == -1 ){

			// FileName was not found in the FileHash
			// Create the necessary arrays and set the index appropriately
		// dprintf(D_ALWAYS, "No cache found for this file, generating new one...\n");
		times_array = new ExtIntArray(100);
		offsets = new ExtOffArray(100);
		file_array_index = OffsetsArray->size();
		FileHash->insert( FileName, file_array_index );
		TimesArray->push_back(times_array);
		OffsetsArray->push_back(offsets);
	} else {
			// otherwise just get the appropriate array
		times_array = (TimesArray->at( file_array_index ));
		offsets = (OffsetsArray->at( file_array_index ));
	}

	FILE* fp=safe_fopen_wrapper_follow(FileName.c_str(),"r");
	if (!fp) return -1;

		//find the offset to search at
	fpos_t* search_offset = findOffset(fp, FromDate, ToDate, times_array, offsets);
	if(search_offset != NULL) {
			// set the seek to the correct spot and begin searching the file
		if(fsetpos(fp, search_offset) < 0) {
				// something failed, so get out of here
			fclose(fp);
			return -1;
		}
	}

	std::string Arg;
	std::string line;
	int new_offset_counter = 1;		// every fifty loops, mark an offset
	while(readLine(line,fp)) {

		T = ReadTimeAndName(line, Arg);
		if( times_array->size() < offsets->size() ) {
				// a file offset was recorded before this line was read; now
				// store the time that was on the marked line
			times_array->push_back( T );
		}

		if (T > ToDate) break;
		if (T < FromDate) {
				// continue to the next loop; but first, if 50 times have been
				// checked, mark the position in the file and the time
			addNewOffset(fp, new_offset_counter, T, times_array, offsets);
			continue;
		}

		if (Names.count(Arg) == 0) {
			Names.insert(Arg);
				// Send the name...
			if (!sock->code(Arg)) {
				dprintf(D_ALWAYS,"Can't send name to client!\n");
				fclose(fp);
				return -1;
			}

			// with a newline
			char newline = '\n';
			if (!sock->code(newline)) {
				dprintf(D_ALWAYS,"Can't send trailing newline to client!\n");
				fclose(fp);
				return -1;
			}
		}
		addNewOffset(fp, new_offset_counter, T, times_array, offsets);
	}

	fclose(fp);
	return 0;
}

int ViewServer::SendDataReply(Stream* sock,const std::string& FileName, time_t FromDate, time_t ToDate, int Options, const std::string& Arg)
{
	std::string InpLine;
	int Status=0;
	time_t NewTime, OldTime;
	float OutTime;
	time_t T = 0;
	int file_array_index;
	ExtIntArray* times_array = NULL;
	ExtOffArray* offsets = NULL;
	// dprintf(D_ALWAYS, "Caches found=%d, looking for correct one...\n", TimesArray->length());

		// first find out which vector to use, by checking the hash
	if( FileHash->lookup( FileName, file_array_index ) == -1 ){

			// FileName was not found in the FileHash
			// Create the necessary arrays and set the index appropriately
		// dprintf(D_ALWAYS, "No cache found for this file, generating new one...\n");
		times_array = new ExtIntArray(100);
		offsets = new ExtOffArray(100);
		file_array_index = OffsetsArray->size();
		FileHash->insert( FileName, file_array_index );
		TimesArray->push_back( times_array );
		OffsetsArray->push_back( offsets );
	} else {

			// otherwise just get the appropriate array
		times_array = (TimesArray->at( file_array_index ));
		offsets = (OffsetsArray->at( file_array_index ));
		// dprintf(D_ALWAYS, "Cache found for this file, %d indices\n", times_array->length());
	}

	OldTime = 0;
	FILE* fp=safe_fopen_wrapper_follow(FileName.c_str(),"r");
	if (!fp) return -1;

		//find the offset to search at
	fpos_t* search_offset = findOffset(fp, FromDate, ToDate, times_array, offsets);
	if(search_offset != NULL) {
			// set the seek to the correct spot and begin searching the file
		if(fsetpos(fp, search_offset) < 0) {
				// something failed, so get out of here
			fclose(fp);
			return -1;
		}
	}

	int new_offset_counter = 1;		// every fifty loops, mark an offset
	while(readLine(InpLine,fp)) {

		T=ReadTimeChkName(InpLine,Arg);
		if( times_array->size() < offsets->size() ) {
				// a file offset was recorded before this line was read; now
				// store the time that was on the marked line
			times_array->push_back( T );
		}
		// dprintf(D_ALWAYS,"T=%d\n",T);

		if (T>ToDate) break;
		if (T<FromDate) {
				// continue to the next loop; but first, if 50 times have been
				// checked, mark the position in the file and the time
			addNewOffset(fp, new_offset_counter, T, times_array, offsets);
			continue;
		}

		if (Options) {
			if (!sock->code(InpLine)) {
				dprintf(D_ALWAYS,"Can't send information to client!\n");
				Status=-1;
				break;
			}
		}
		else {
			char OutLine[200];

			const char *tmp = strchr(InpLine.c_str(),':');
			OutTime=float(T-FromDate)/float(ToDate-FromDate);
			NewTime=(int)rint(1000*OutTime);
			if (NewTime==OldTime) continue;
            OldTime=NewTime;
			snprintf(OutLine,sizeof(OutLine),"%.2f%s",OutTime*100,tmp+1);
			if (!sock->put(OutLine)) {
				dprintf(D_ALWAYS,"Can't send information to client!\n");
				Status=-1;
				break;
			}
		}
		addNewOffset(fp, new_offset_counter, T, times_array, offsets);
	}

	fclose(fp);
	return Status;
}

//*******************************************************************
// Utility files for recording offsets in the condor_stats files.
//*******************************************************************

//-------------------------------------------------------------------
// Increments the counter, and then if it's reached 50, adds the
// offset to the list.
//-------------------------------------------------------------------

void
ViewServer::addNewOffset(FILE* &fp, int &offset_ctr, int read_time, ExtIntArray* times_array, ExtOffArray* offsets) {
	if( ++offset_ctr == 50) {
		offset_ctr = 0;
		if(times_array->size() == 0 || read_time > times_array->back()) {
				// mark the position in the file now, but wait to mark the time
				// until after the line is read
			// dprintf(D_ALWAYS, "Adding new offset to the cache\n");
			fpos_t *tmp_offset_ptr = new fpos_t;
			fgetpos(fp, tmp_offset_ptr);
			offsets->push_back(tmp_offset_ptr);
		} else {
			// dprintf(D_ALWAYS, "I would mark an offset, but it would most likely be a duplicate.\n");
		}
	}
}

//-------------------------------------------------------------------
// Sets the offset to the appropriate spot in the file to minimize
// linear searching of the file.
//-------------------------------------------------------------------

fpos_t*
ViewServer::findOffset(FILE* & /*fp*/, int FromDate, int ToDate, ExtIntArray* times_array, ExtOffArray* offsets) {
	fpos_t* search_offset_ptr = NULL;
	if( times_array->size() == 0 ) {

			// linear progression, return null to inform the rest of the code
			// to start at the beginning
		// dprintf(D_ALWAYS, "No cache, starting at the beginning of the file...\n");
		return NULL;

	} else if( times_array->at( 0 ) > FromDate ) {

		if( times_array->front() > ToDate ) {
				// if this is the case then the record won't be found, so end
			// dprintf(D_ALWAYS, "Impossible request\n");
			return NULL;
		} else {
				// this file begins in the middle of the requested interval, so
				// start at the beginning of the file.
			// dprintf(D_ALWAYS, "Carrying over from previous file at the beginning\n");
			return NULL;
		}

	} else if( times_array->back() < FromDate ) {

			// linear progression, but start with the latest known offset
		// dprintf(D_ALWAYS, "Requesting time at the end of where the current cache knows\n");
		search_offset_ptr = offsets->back();

	} else {

			// binary search through times_array[] for the latest time not
			// greater than FromDate
		// dprintf(D_ALWAYS, "Binary searching the cache table, request should be quick\n");
		int low = 0;
		int high = times_array->size()-1;
		int mid;
		while( high - low > 1 ) {
			mid = (high - low) / 2 + low;
			if( times_array->at( mid ) < FromDate ) {
				low = mid;
			} else {
				high = mid;
			}
		}
			// now we've found the approximate spot in the array; time to find
			// the exact location and set the appropriate offset
		if( times_array->at( low + 1 ) < FromDate ) {
			search_offset_ptr = offsets->at( low + 1 );
		} else if( times_array->at( low ) < FromDate ) {
			search_offset_ptr = offsets->at( low );
		} else {
			return NULL;
		}
	}
	return search_offset_ptr;
}

//*******************************************************************
// Utility functions for reading the history files & data
//*******************************************************************

//-------------------------------------------------------------------
// Read the first entry to decide the time of the earliest entry
// in the file
//-------------------------------------------------------------------

time_t ViewServer::FindFileStartTime(const char *Name)
{
	std::string line;
	time_t T=-1;
	FILE* fp=safe_fopen_wrapper_follow(Name,"r");
	if (fp) {
		if (readLine(line,fp)) {
			T=ReadTime(line.c_str());
		} else {
			T=-1; // fgets failed, return -1 instead of parsing uninit memory
			dprintf(D_ALWAYS, "Failed to parse first line of %s\n", Name);
		}
		fclose(fp);
	}
	dprintf(D_ALWAYS,"FileName=%s , StartTime=%lld\n",Name,(long long)T);
	return T;
}

//-------------------------------------------------------------------
// Parse the entry time from the line
//-------------------------------------------------------------------

time_t ViewServer::ReadTime(const char* Line)
{
	time_t t=-1;
	if (sscanf(Line,"%ld",&t)!=1) return -1;
	return t;
}

//-------------------------------------------------------------------
// Parse the entry time and name from the line
//-------------------------------------------------------------------

time_t ViewServer::ReadTimeAndName(const std::string &line, std::string& Name)
{
	time_t t=-1;

	// Line contains int, tab, a string, tab then junk
	char *endOfInt = nullptr;
	t = strtol(line.c_str(), &endOfInt, 10);
	if (endOfInt) {
		endOfInt++; // skip past tabl
		char *endOfString = endOfInt;
		while (*endOfString != '\t') endOfString++;
		Name = std::string(endOfInt, endOfString);
		return t;
	} else {
		// Some kind of error
		return -1;
	}
}

//-------------------------------------------------------------------
// Parse the time and check for the specified name
//-------------------------------------------------------------------

time_t ViewServer::ReadTimeChkName(const std::string &line, const std::string& Name)
{
	time_t t=-1;
	//if (sscanf(Line,"%d %s",&t,tmp)!=2) return -1;
	char *p = nullptr;
	t = strtol(line.c_str(), &p, 10);
	if (p) {
		p++; // skip the tab separator
		char *endOfString = p;
		while (*endOfString != '\t') endOfString++;

		if (Name != "*" && Name != std::string(p, endOfString)) return -1;
		return t;
	}
	return t;
}

//*********************************************************************
// History file writing functions
//*********************************************************************

//---------------------------------------------------------------------
// Scan function for submittor data
//---------------------------------------------------------------------

void ViewServer::WriteHistory(int /* tid */)
{
	std::string Key;
	GeneralRecord* GenRec;
	FILE* DataFile;
	struct stat statbuf;
	std::string outline;

	// Accumulate data

	TimeStamp=time(0);

	dprintf(D_ALWAYS,"Accumulating data: Time=%lld\n",(long long) TimeStamp);

	if (!collector.walkHashTable(*collector.getHashTable(SUBMITTOR_AD), SubmittorScanFunc)) {
		dprintf (D_ALWAYS, "Error accumulating data\n");
		return;
	}
	SubmittorTotalFunc();

	if (!collector.walkHashTable(*collector.getHashTable(STARTD_AD), StartdScanFunc)) {
		dprintf (D_ALWAYS, "Error accumulating data\n");
		return;
	}
	StartdTotalFunc();
	if (!collector.walkHashTable(*collector.getHashTable(CKPT_SRVR_AD), CkptScanFunc)) {
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
			dprintf(D_FULLDEBUG,"Opening file %s\n",DataSet[i][j].NewFileName.c_str());
			DataFile=safe_fopen_wrapper_follow(DataSet[i][j].NewFileName.c_str(),"a");
			if (!DataFile) {
				dprintf(D_ALWAYS,"Could not open data file %s for appending!!! errno=%d\n",DataSet[i][j].NewFileName.c_str(),errno);
				EXCEPT("Could not open data file appending!!!");
			}

			// Iterate through accumulated values

			DataSet[i][j].AccData->startIterations();
			while(DataSet[i][j].AccData->iterate(Key,GenRec)) {
				formatstr(outline,DataFormat[i].c_str(),TimeStamp,Key.c_str(),GenRec->Data[0],GenRec->Data[1],GenRec->Data[2],GenRec->Data[3],GenRec->Data[4],GenRec->Data[5],GenRec->Data[6], GenRec->Data[7], GenRec->Data[8]);
				delete GenRec;
				fputs(outline.c_str(), DataFile);
			}

			// Clear accumulated values

			DataSet[i][j].AccData->clear();

			// close file

			fclose(DataFile);
			dprintf(D_FULLDEBUG,"Closing file %s\n",DataSet[i][j].NewFileName.c_str());

			// Update file start time

			if (DataSet[i][j].NewStartTime==-1) DataSet[i][j].NewStartTime=TimeStamp;

			// Check for size limitation and take necessary action

			if (stat(DataSet[i][j].NewFileName.c_str(),&statbuf)) {
				dprintf(D_ALWAYS,"Could not check data file %s size, that we just wrote to. Errno=%d\n",DataSet[i][j].NewFileName.c_str(),errno);
				continue;
			}
			if (statbuf.st_size>MaxFileSize) {
				int r = rotate_file(DataSet[i][j].NewFileName.c_str(), DataSet[i][j].OldFileName.c_str());
				if (r < 0) {
					dprintf(D_ALWAYS,"Could not rename %s to %s (%d)\n", DataSet[i][j].NewFileName.c_str(), DataSet[i][j].OldFileName.c_str(), errno);
					EXCEPT("Could not rename data file");
				}
				int newFileIndex = -1;
				int oldFileIndex = -1;
				if(FileHash->lookup(DataSet[i][j].OldFileName,
														oldFileIndex) != -1) {
						// get rid of the old arrays and make new ones
					delete (*TimesArray)[oldFileIndex];
					for (auto p: *(*OffsetsArray)[oldFileIndex]) {
						delete p;
					}
					delete (*OffsetsArray)[oldFileIndex];
					(*TimesArray)[oldFileIndex] = new ExtIntArray;
					(*OffsetsArray)[oldFileIndex] = new ExtOffArray;
					if(FileHash->lookup(DataSet[i][j].NewFileName,
														newFileIndex) != -1) {
							// switch the indices to avoid copying data
						FileHash->remove(DataSet[i][j].OldFileName);
						FileHash->remove(DataSet[i][j].NewFileName);
						FileHash->insert(DataSet[i][j].OldFileName,
															newFileIndex);
						FileHash->insert(DataSet[i][j].NewFileName,
															oldFileIndex);
					}
				}
				else if(FileHash->lookup(DataSet[i][j].NewFileName,
													newFileIndex) != -1) {
						// if no file got overwritten, then just add to the
						// hash and arrays
					FileHash->remove(DataSet[i][j].NewFileName);
					FileHash->insert(DataSet[i][j].OldFileName,
															newFileIndex);
				}
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

int ViewServer::SubmittorScanFunc(CollectorRecord* record)
{
	ClassAd* cad = record->m_publicAd;
	std::string machine;
	std::string submittorName;
	int JobsRunning, JobsIdle;
	std::string GroupName;

	// Get Data From Class Ad

	if (cad->LookupString(ATTR_NAME,submittorName)==false) return 1;
	if (cad->LookupString(ATTR_MACHINE,machine)==false) return 1;
	GroupName = submittorName;

	submittorName += '/';
	submittorName += machine; 	
	
	if (cad->LookupInteger(ATTR_RUNNING_JOBS,JobsRunning)==false) JobsRunning=0;
	if (cad->LookupInteger(ATTR_IDLE_JOBS,JobsIdle)==false) JobsIdle=0;

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
		GenRec=GetAccData(DataSet[SubmittorData][j].AccData, submittorName);
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

	std::string GroupName;
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

int ViewServer::StartdScanFunc(CollectorRecord* record)
{
	ClassAd* cad = record->m_publicAd;
	std::string Name;
	std::string StateDesc;
	double LoadAvg;
	int KbdIdle;

	// Get Data From Class Ad

	if ( !cad->LookupString(ATTR_NAME,Name)) return 1;
	if ( !cad->LookupInteger(ATTR_KEYBOARD_IDLE,KbdIdle) ) KbdIdle=0;
	if ( !cad->LookupFloat(ATTR_LOAD_AVG,LoadAvg) ) LoadAvg=0;
	cad->LookupString(ATTR_STATE,StateDesc);
	State StateEnum=string_to_state( StateDesc.c_str());

	// This block should be kept in sync with view_server.h and
	// condor_state.h.
	ViewStates st = VIEW_STATE_UNDEFINED;
	switch(StateEnum) {
	case owner_state:
		st=VIEW_STATE_OWNER;
		break;
	case preempting_state:
		st=VIEW_STATE_PREEMPTING;
		break;
	case claimed_state:
		st=VIEW_STATE_CLAIMED;
		break;
	case matched_state:
		st=VIEW_STATE_MATCHED;
		break;
	case unclaimed_state:
		st=VIEW_STATE_UNCLAIMED;
		break;
	case shutdown_state:
		st=VIEW_STATE_SHUTDOWN;
		break;
	case delete_state:
		st=VIEW_STATE_DELETE;
		break;
	case backfill_state:
		st=VIEW_STATE_BACKFILL;
		break;
	case drained_state:
		st=VIEW_STATE_DRAINED;
		break;
	default:
		dprintf( D_ALWAYS,
				 "WARNING: Unknown machine state %d from '%s' (ignoring)",
				 (int)StateEnum, Name.c_str());
		return 1;
	}

	// Get Group Name

	std::string tmp;
	if (cad->LookupString(ATTR_ARCH,tmp)==false) tmp = "Unknown";
	std::string GroupName = tmp + '/';
	if (cad->LookupString(ATTR_OPSYS,tmp)==false) tmp = "Unknown";
	GroupName += tmp;

	// Add to group Totals
	// NRL: I'm not sure exactly what this block of code does, but
	// now it at least does it safely.  It's obviously updating
	// the GroupHash <group name> and "Total" chunks.

	int group_index = (int)st - 1;
	if( group_index > (int)VIEW_STATE_MAX_OFFSET ) {
		EXCEPT( "Invalid group_index = %d (max %d)",
				group_index, (int)VIEW_STATE_MAX_OFFSET );
	}
	GeneralRecord* GenRec=GetAccData(GroupHash,"Total");
	ASSERT( GenRec );
	GenRec->Data[group_index] += 1.0;

	GenRec=GetAccData(GroupHash,GroupName);
	ASSERT( GenRec );
	GenRec->Data[group_index] += 1.0;

	// Add to accumulated data

	int NumSamples;
	for (int j=0; j<HistoryLevels; j++) {
		GenRec=GetAccData(DataSet[StartdData][j].AccData, Name);
		ASSERT( GenRec );

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

	std::string GroupName;
	GeneralRecord* CurValue;
	GeneralRecord* AccValue;
	int NumSamples;

	GroupHash->startIterations();
	while(GroupHash->iterate(GroupName,CurValue)) {
		for (int j=0; j<HistoryLevels; j++) {
			AccValue=GetAccData(DataSet[GroupsData][j].AccData, GroupName);
			NumSamples=DataSet[GroupsData][j].NumSamples;
			for (int k=0; k<(int)VIEW_STATE_MAX; k++) {
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

int ViewServer::CkptScanFunc(CollectorRecord* record)
{
	ClassAd* cad = record->m_publicAd;
	std::string Name;
	int Bytes;
	double BytesReceived,BytesSent;
	double AvgReceiveBandwidth,AvgSendBandwidth;

	// Get Data From Class Ad

	if (cad->LookupString(ATTR_NAME,Name)==false) return 1;
	if (cad->LookupInteger("BytesReceived",Bytes)==false) Bytes=0;
	BytesReceived=float(Bytes)/(1024*1024);
	if (cad->LookupInteger("BytesSent",Bytes)==false) Bytes=0;
	BytesSent=float(Bytes)/(1024*1024);
	if (cad->LookupFloat("AvgReceiveBandwidth",AvgReceiveBandwidth)==false) AvgReceiveBandwidth=0;
	AvgReceiveBandwidth=AvgReceiveBandwidth/1024;
	if (cad->LookupFloat("AvgSendBandwidth",AvgSendBandwidth)==false) AvgSendBandwidth=0;
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

GeneralRecord* ViewServer::GetAccData(AccHash* AccData,const std::string& Key)
{
	GeneralRecord* GenRec = NULL;
	int rval;

	rval = AccData->lookup( Key, GenRec );
	if ( ( rval < 0 ) || ( GenRec == NULL ) ) {
		if ( rval >= 0 ) {
			dprintf( D_ALWAYS,
					 "Hash %p lookup returned %d, but NULL record!\n",
					 AccData, rval );
			EXCEPT( "Bad lookup" );
		}
		GenRec=new GeneralRecord;
		if ( GenRec == NULL ) {
			EXCEPT( "Failed to allocate a GeneralRecord" );
		}
		if ( AccData->insert(Key,GenRec) < 0 ) {
			EXCEPT( "Insert failed: Key=%s", Key.c_str() );
		}
	}
	return GenRec;
}
