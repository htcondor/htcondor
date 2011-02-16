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

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include <fstream.h>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <map.h>
#include <string.h>
#include <sys/time.h>
#include "iso_dates.h"
#include "condor_classad.h"
#include "MyString.h"
#include "user_log.c++.h"
#include "code.h"
#include "modTrial.h"







JobLog::JobLog(void){
}

JobLog::JobLog(char* persistFile,char* iFile,int erotate){
  initStates(persistFile,iFile,erotate);
}

void JobLog::initStates(char* persistFile,char*iFile,int erotate){
  ifstream pRFile;
  //Init the include or exclude event list
  //IMP : ??? chould this be configurable or in the header file
  //getting the include exclude job eventId list

  std::cout << "\n In initStates() fn" <<std::endl;
  seek_pos = 0;
  pos=0;
  fileVersion = 1;
  is_rotated = 0;
  eventNum=0;

max_file_size = 2000000;
file_size_limit = max_file_size;

//global constants defined here

reportEventId[0]=1;
reportEventId[1]=1;
reject_list_size =2;
max_file_size = 2000000;
AttrList_toAvoid[0]="MyType";
AttrList_toAvoid[1]="TargetType";
AttrList_toAvoid[2]="EventTypeNumber";
AttrList_toAvoid[3]="EventTime";
mytype = "MyType";
attr_limit = 4;
max_event_id = 28;
jobid =" job.id=";
guidstr = " guid=";
schedName = "SCHED1";

 ULogEventOutcomeName[0]= "ULOG_OK";
 ULogEventOutcomeName[1]= "ULOG_NO_EVENT ";
 ULogEventOutcomeName[2]= "ULOG_RD_ERROR ";
 ULogEventOutcomeName[3]= "ULOG_MISSED_EVENT ";
 ULogEventOutcomeName[4]= "ULOG_UNK_ERROR";

  //ends here
 /* for(int i=0;i< reject_list_size;i++){
    int in = reportEventId[i];
    reportEvent[in] = 1;
  }*/
  
  // activate all events to be reported
  for (int i = 0; i < 28; i++)
  	reportEvent[i] = 1;
std::cout << "\n ModTrial initStates variables initialized \n" <<std::endl;
  //*** these can be class memebers -defined at the time of init -EventLog reader
  //stuff

  ReadUserLog :: InitFileState(stateBuf);

  //INIT FUNCTION
  //NOTE: chk if PERSISTENT file present else default initialisation
  pRFile.open(persistFile, ios::in); 
  if(pRFile){
    pRFile.seekg(0);
    if(pRFile.read((char*)stateBuf.buf,stateBuf.size)){
      log = new ReadUserLog (stateBuf,true) ; 
      //reading the seek position for the output Job file
      pRFile.seekg(stateBuf.size);
      std::cout << "\n PERSIST FILE OPENED: Initialize using filename and max rotate:: status "<<iFile << erotate<<std::endl;
      int seek_pos1 = 0;
      char * temp = "";
      string seekStr = "";
      pRFile >> seekStr;
      string newSeek = seekStr.substr(0,seekStr.length()-2);
      temp = (char*)newSeek.c_str();

      //class variable initialized
      seek_pos = atoi(temp);
      pos = seek_pos;
      fileVersion = atoi((char*)(seekStr.substr(seekStr.length()-2,1)).c_str());
      is_rotated = atoi((char*)(seekStr.substr(seekStr.length()-1,1)).c_str());

    }
    else{
      //if the persist file read failed default initialize from input Event Log
      log = new ReadUserLog () ; 
      bool status1 = log->initialize(iFile,erotate,true,true);
      std::cout << "\nCANNOT open PERSIST FILE: DEFAULT INITIALIZATION "<<iFile << erotate<<std::endl;

      log->GetFileState(stateBuf);
      fileVersion =1;
      is_rotated = 0;
      pos=0;
      seek_pos =0;
    }
    pRFile.close();
  }
  //Default initialize readUserLog from input Event Log
  else {
    log = new ReadUserLog () ;
    bool status1 = log->initialize(iFile,erotate,true,true);
    std::cout << "\nNO PERSIST FILE: DEFAULT INITIALIZATION "<<iFile << erotate<<std::endl;
    log->GetFileState(stateBuf);
    fileVersion =1;
    is_rotated = 0;
    pos=0;
    seek_pos =0;
  }

  //NOTE:log reader not initialized:this situation can ideally not happen
  if(!log->isInitialized()){
    bool status = log->initialize(stateBuf,erotate,true);
  }
std::cout << "\n Exitting initStates function.... \n" <<std::endl;
}

string JobLog::timestr( struct tm &tm )
{
  time_t timemk = mktime(&tm);
  struct tm* tempTm = gmtime(&timemk);
  char* timeStr1 =(char*)time_to_iso8601(*tempTm, ISO8601_ExtendedFormat,ISO8601_DateAndTime,true);
  string timeVal = timeStr1;
  return timeVal;
}


bool JobLog::updateState(ReadUserLog ::FileState stateBuf,ReadUserLog* log, char* persistFile,int pos,int fileFlag, int rotate)
{
  string tempName = persistFile;
  ofstream pWFile;
  tempName = "temp_"+tempName;
  char* tempPersistFile =(char*) tempName.c_str();

  log->GetFileState(stateBuf);
  pWFile.open(tempPersistFile, ios::out); 
  if(pWFile){
    pWFile.write((char*)stateBuf.buf,stateBuf.size);

    //condition for output file
//    if(pos > 0) {
      pWFile << pos; //pos in write file
      pWFile << fileFlag;//file version
      pWFile << rotate;//rotated flag*/
  //  }
    pWFile.close();
    //ensuring the write to persist file is maintained atomic
    rename(tempPersistFile,persistFile);
    //std::cout <<"writen to to persist pos = "<<pos<<" FV" <<fileVersion <<" ISR" <<is_rotated<<std::endl ;
  }
  else{
    std::cout <<"cant right to persist\n";
    return false;
  }
  return true;
}


void JobLog::initOutputFile(char* outFile,fstream& dbWFile){
  string fname = outFile;
  string fno ="";
  if(fileVersion ==1){
    fno ="1";
  }
  else{
    fno ="2";
  }
  fname=fname+"."+fno+".log";
  char* oFile = (char*)fname.c_str();
  dbWFile.open(oFile); 
  if (!dbWFile) 
  {
    //eFile << "OUTPUT FILE : FAILED to open in dafault(OUT) mode of  " << oFile;
    dbWFile.open(oFile,ios::out); 
    if(!dbWFile){  
      //eFile <<"OUTPUT FILE:CANT open output file" << dbWFile;
      exit(1);
    }
  }
}




  
  //void writeJobLog(ReadUserLog ::FileState stateBuf,fstream& dbWFile,string eStr,ReadUserLog* log, char* persistFile,char*outFile, int& is_rotated, int& fileVersion, int& pos)
//void JobLog::writeToPPFile(fstream& dbWFile,string eStr, char* persistFile,char*outFile, int& is_rotated, int& fileVersion, int& pos)
void JobLog::writeToPPFile(fstream& dbWFile,string eStr, char* persistFile,char*outFile)
{
  string tempName = persistFile;
  ofstream pWFile;
  tempName = "temp_"+tempName;
  char* tempPersistFile =(char*) tempName.c_str();
  int seek_pos =0;
  string fno ="";
  string fname = "";
  char* oFile = outFile;
  bool flag = true;
  dbWFile.seekp(pos);
  int logLen = eStr.length();
  // CONDITION ADDED FOR FILE ROTATION---STARTS
  if((pos+logLen)> file_size_limit){
    flag =true;
    if(is_rotated ==0 && fileVersion == 1){

      fileVersion=2;
      seek_pos=pos=0;
      //TODO:should be converted to module
      dbWFile.close();
      if(fileVersion ==1){
        fno ="1";
      }
      else{
        fno ="2";
      }
      fname = outFile;
      fname=fname+"."+fno+".log";
      oFile = (char*)fname.c_str();
      unlink(oFile);
      dbWFile.open(oFile); 
      if (!dbWFile) 
      {
        dbWFile.open(oFile,ios::out); 
        if(!dbWFile){  
          exit(1);
        }
      }
      dbWFile.seekp(seek_pos);
      pos=seek_pos;
      flag = false;
    }
    else if(flag && (fileVersion ==2 || is_rotated==1)){
      if(fileVersion == 2){
        fileVersion = 1;
        fno="1";
      }
      else {
        fileVersion = 2;
        fno="2";
      }
      seek_pos=pos=0;
      flag = false;
      is_rotated=1;
      dbWFile.close();
      fname = outFile;
      fname=fname+"."+fno+".log";
      //previously exisiting file that is going to be written into again
      //is first unlinked!
      oFile = (char*)fname.c_str();
      unlink(oFile);
      dbWFile.open(oFile,ios::trunc); 
      if (!dbWFile) 
      {
        dbWFile.open(oFile,ios::out); 
        if(!dbWFile){  
          exit(1);
        }
      }
      dbWFile.seekp(seek_pos);
      pos = seek_pos;
    }
  }
  //rotation code ends

  //write to ouput file
  //std::cout << "should write to "<< oFile<< "and persist at "<< tempPersistFile <<std::endl;
  dbWFile <<eStr<<std::endl;

  //call the stateUpdate function

  //log->GetFileState(stateBuf);
  int tempPos= dbWFile.tellp();
  bool stateUpFlag = updateState(stateBuf,log,persistFile,tempPos,fileVersion,is_rotated);
  if(stateUpFlag){
    pos = tempPos;
  }
}

//NOTE:pass is_rotated and fileversion as standard values
void JobLog::writeToFile(fstream& dbWFile,string eStr, char* persistFile)
{
  dbWFile.seekp(pos);
  //write to ouput file
  dbWFile <<eStr<<std::endl;

  //  log->GetFileState(stateBuf);
  int tempPos= dbWFile.tellp();

  //could remove the fv,ir part
  //bool stateUpFlag = updateState(stateBuf,log,persistFile,tempPos,fileVersion,is_rotated);
  bool stateUpFlag = updateState(stateBuf,log,persistFile,tempPos,1,0);
  if(stateUpFlag){
    std::cout << "\n Updating prev pos=" << pos << " ; new pos=" << tempPos << std::endl;
    //sleep(1);
    pos = tempPos;

  }
}

//NOTE:pass is_rotated and fileversion as standard values
void JobLog::sendXmppIM(string eStr, char* persistFile)
{
  string tempName = persistFile;
  int seek_pos =0;
  string fno ="";
  string fname = "";

  //write to ouput file
  string cmd1 = "echo \"";
  string cmd2 = "\" | sendxmpp -s test cLogTest@jabber.org";
  string cmd = cmd1 + eStr + cmd2;
  std::cout << "Calling :: "<< cmd <<std::endl;
  system(cmd.c_str());

  //update persistent state file
//  log->GetFileState(stateBuf);
 
  bool stateUpFlag = updateState(stateBuf,log,persistFile,0,1,0);
  if(stateUpFlag){
    pos = 0;
  }
}

string JobLog::netFormatLog(ULogEvent *event){
  // map hash to maintain list of classad attributes whose key value to be
  // replaced to Netlogger format
  typedef map<string,string> StringIntMap;
  StringIntMap ahash;
  ahash["MyType"] = " event=condor.";
  ahash["EventTime"]= "ts=";
  ahash["EventTypeNumber"] = " event.id=";
  ahash["TargetType"] = "";


  std::ostringstream o,p,q,e,t1,t2,t3,t4,t5;
  string firstKey = "";
  string guid = "";
  string clust = "";
  string proc = "";
  string subproc = "";
  int clustNo =0;
  MyString inBuf="";
  string eStr ="";

  inBuf ="";

  ClassAd *eventAd=NULL;
  eventAd = event->toClassAd();
  eventAd->sPrint(inBuf);
  string logval ="";
  string chkmystr = inBuf.Value();
  char* input ="";
  input = (char*)chkmystr.c_str();

  char* name =(char*)eventAd->GetMyTypeName();
  string sename = name;
  string estr = "Event";
  int epos = sename.find(estr,0);
  string ename = sename.substr(0,epos);
  std::transform(ename.begin(), ename.end(), ename.begin(), (int(*)(int)) tolower);

  //for netlogger formatting starts
  char* arr = strtok(input,"\n");
  int chkpos = 0;
  int k=0;
  bool flag = false;
  string srcStr ="";
  logval = "";
  string temparr="";

  while(arr != NULL){
    k=0;
    flag =false;
    for(k=0;k<attr_limit;k++){
      srcStr = AttrList_toAvoid[k];
      temparr = arr;
      chkpos = temparr.find(srcStr,0);
      if(chkpos >= 0){
        flag = true;
        break;
      }
    }
    if(flag){
      arr = strtok(NULL,"\n");
      continue;
    }
    int position = temparr.find( " " ); // find first space
    while ( position != string::npos ) 
    {
      temparr.replace( position, 1, "" );
      position = temparr.find( " ", position + 1 );
    } 
    logval=logval+" "+temparr;
    arr = strtok(NULL,"\n");
  }
  string newval = logval;
  std::transform(newval.begin(), newval.end(), newval.begin(), (int(*)(int)) tolower);

  o << event->cluster ;
  clust = o.str();
  p << event->proc ;
  proc = p.str();
  q << event->subproc ;
  subproc = q.str();
  //framing the job id from this
  firstKey = clust + "." + proc + "." + subproc;
  guid = schedName;
  guid=guid+"-"+clust+"-"+proc+"-"+subproc;
  e << event->eventNumber ;
  eStr =ahash["EventTime"]+ timestr(event->eventTime)+ jobid+firstKey + ahash["MyType"]+ename+ ahash["EventTypeNumber"]+e.str()+guidstr+guid;
  eStr=eStr+newval;
  return eStr;
  //netlogger formatting ends
}

string JobLog::classAdFormatLog(ULogEvent *event){

  std::ostringstream o,p,q,e,t1,t2,t3,t4,t5;
  string firstKey = "";
  string guid = "";
  string clust = "";
  string proc = "";
  string subproc = "";
  int clustNo =0;
  MyString inBuf="";
  string eStr ="";
  inBuf ="";

  ClassAd *eventAd=NULL;
  eventAd = event->toClassAd();
  eventAd->sPrint(inBuf);
  string logval ="";
  string chkmystr = inBuf.Value();
  char* input ="";
  string input1 ="";
  input = (char*)chkmystr.c_str();
  input1 = input;

  //normal \n replaced by space and removing "

  int position = input1.find( "\n" ); // find first space
  while ( position != string::npos ) 
  {
    input1.replace( position, 1, " " );
    position = input1.find( "\n", position + 1 );
  }

  //normal \n replaced by space and removing "
  position = input1.find( "\"" ); // find first space
  while ( position != string::npos ) 
  {
    input1.replace( position, 1, "" );
    position = input1.find( "\"", position);
  }


  //std::cout << "\n CLASS AD be4 : "<<input<<std::endl; 
  //std::cout << "\n CLASS AD after : "<<input1<<std::endl; 

  clust = o.str();
  p << event->proc ;
  proc = p.str();
  q << event->subproc ;
  subproc = q.str();
  //framing the job id from this
  guid = schedName;
  guid=guid+"-"+clust+"-"+proc+"-"+subproc;
  eStr =guid+" "+input1;
  return eStr;
}




//new version of ReadLog
void JobLog::readLog(char* outFile, char* persistFile,  int eventTrack = ULOG_EXECUTE, int erotate = 10, bool excludeFlag=false){
  //std::cout <<"test flag " <<exdcludeFlag<<std::endl;
  //filestream declarations
  ifstream dbRFile;
  ofstream dbWFileTemp;
  fstream dbWFile;
  ifstream dbSRFile;
  ifstream pRFile;
  ifstream pTempFile;
  ifstream metaRFile;
  ofstream dbSWFile;
  ofstream metaWFile;
  ofstream stateBufFile;
  ofstream eFile;
  ofstream adFile;



  //setting the max output file -for error deduction bufstates
  string error = "";
  ReadUserLog :: FileState otherStateBuf;
  ReadUserLog :: InitFileState(otherStateBuf);
  ReadUserLog :: FileState tempStateBuf;
  ReadUserLog :: InitFileState(tempStateBuf);
  long missedEventCnt = 0;
  bool evnDiffFlag = false;


  //persistance of read and write file held here by renaming files
  char* errorFile = "error.dat";


  int eventNum = 0;

  //opening program's error log file
  eFile.open(errorFile, ios::app); 
  if (!eFile) 
  {
    cerr << "Unable to open " << errorFile;
    exit(1);
  }
  //INIT FUNCTION
  //opening output file if need be
  initOutputFile(outFile,dbWFile);

  //setting the ouput file's seek position 

  /**********************************************   EVENT LOG READER  ***************************/

  ULogEvent *event ; 

  while(true) {
    ULogEventOutcome outCome = log->readEvent(event);
    std::ostringstream missedEStr;
    if(outCome == ULOG_OK){
      if(reportEvent[event->eventNumber] == 1 && excludeFlag){
        continue;
      }
      else if((reportEvent[event->eventNumber] == 1 && !excludeFlag) || excludeFlag){


        string logStr = classAdFormatLog(event);

        const ReadUserLogStateAccess lsa = ReadUserLogStateAccess(stateBuf);
        unsigned long testId =0;
        int tlen = 128;
        char tbuf[128];
        lsa.getUniqId(tbuf,tlen);
        std::cout << "\n Id as obtained => " << tbuf <<std::endl;

        writeToFile(dbWFile,logStr,persistFile);
      }//else ends here
    } 
    else{
      if(outCome == ULOG_NO_EVENT)
      { 
        std::cout << "sleep for 5 sec" <<std::endl; 
        sleep(5);
      }
      else if(outCome == ULOG_MISSED_EVENT){
        error = "";
        otherStateBuf = stateBuf;
        log->GetFileState(tempStateBuf);
        const ReadUserLogStateAccess otherLSA = ReadUserLogStateAccess(otherStateBuf);
        const ReadUserLogStateAccess thisLSA = ReadUserLogStateAccess(tempStateBuf);
        evnDiffFlag = thisLSA.getEventNumberDiff(otherLSA, missedEventCnt);
        error = ULogEventOutcomeName[outCome];
        std::cout << "ERROR MISSED EVENT " <<missedEventCnt <<std::endl;
        missedEStr <<missedEventCnt;
        string mcnt = missedEStr.str();
        int cint=0;

        string eStr = "error.type="+error+" missedEventCnt="+mcnt+"\n";
        //      writeJobLog(stateBuf,dbWFile,eStr,log,persistFile,outFile,is_rotated,fileVersion,pos);
        writeToFile(dbWFile,eStr,persistFile);
        //writeJobLog(dbWFile,eStr,log,persistFile,outFile,is_rotated,fileVersion,pos);
        eFile << "Following error was encountered : " << outCome << "\n";
      }
      else
      {
        //write to error log file
        error = "";
        error = ULogEventOutcomeName[outCome];
        string eStr = "error.type="+error+"\n";
        //        writeJobLog(stateBuf,dbWFile,eStr,log,persistFile,outFile,is_rotated,fileVersion,pos);
        writeToFile(dbWFile,eStr,persistFile);
        //writeJobLog(dbWFile,eStr,log,persistFile,outFile,is_rotated,fileVersion,pos);
        eFile << "Following error was encountered : " << outCome << "\n";
      }
    }
    //cond for log event errors ends

  }
  //looping to extract all event log msges ends

  ReadUserLog :: UninitFileState(stateBuf);
  dbSWFile.close();
  dbWFile.close();
  eFile.close();
  stateBufFile.close();
}



