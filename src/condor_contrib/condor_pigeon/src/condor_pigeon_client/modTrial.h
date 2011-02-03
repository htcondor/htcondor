/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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


#include <iostream.h>
#include <stdlib.h>
#include <fstream.h>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <map.h>
#include <string.h>
#include <sys/time.h>
#include "iso_dates.h"
#include "condor_classad.h"
#include "user_log.c++.h"
using namespace std;




class JobLog {
  public:
    //---Constructor---/
    //JobLog(char* fname);
    JobLog(char* persistFile,char* iFile,int erotate);
    JobLog(void);

    //Destructor
    //    ~JobLog();

    //my functions

    void writeJobLog(ReadUserLog ::FileState stateBuf,fstream& dbWFile,string eStr,ReadUserLog* log, char* persistFile,char*outFile, int& is_rotated, int& fileVersion, int& pos);
    void writeToPPFile(fstream& dbWFile,string eStr,char* persistFile,char*outFile);

    void writeToFile(fstream& dbWFile,string eStr, char* persistFile);

    string classAdFormatLog(ULogEvent *event);
    bool updateState(ReadUserLog ::FileState stateBuf,ReadUserLog* log, char* persistFile,int pos,int fileFlag, int rotate);
    void sendXmppIM(string eStr, char* persistFile);
    void initStates(char* persistFile,char*iFile,int erotate);
    void readLog(char* outFile, char* persistFile,  int eventTrack, int erotate, bool excludeFlag);

    // ************
    //my functions ends


    //info related to the output file details  
    int seek_pos;
    int pos;
    int fileVersion;
    int is_rotated;

    // include/exclude event list
    int reportEvent[28];
    int eventNum;


    //gen global variables
    int reportEventId[2];
    int reject_list_size;
    int max_file_size;
    char* AttrList_toAvoid[30];
    char * mytype;
    int attr_limit;
    int max_event_id;
    char* jobid;
    char* guidstr;
    char* schedName;
     int file_size_limit;

    const char* ULogEventOutcomeName[5]; 
    //ends

    //UserLog for reading the EventLog
    ReadUserLog* log;
    ReadUserLog :: FileState stateBuf;

  private:

    string timestr( struct tm &tm );
    void updatePersist();
    string netFormatLog(ULogEvent *event);
    void initOutputFile(char* outFile,fstream& dbWFile);
};
