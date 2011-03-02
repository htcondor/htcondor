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
#include "modTrial.h"

// qpid client header files
#include <qpid/client/Connection.h>
#include <qpid/client/Session.h>
#include <qpid/client/AsyncSession.h>
#include <qpid/client/Message.h>
#include <qpid/client/MessageListener.h>
#include <qpid/client/LocalQueue.h>
#include <qpid/client/SubscriptionManager.h>


using namespace qpid::client;
using namespace qpid::framing;

using std::stringstream;
using std::string;



using namespace std;


int getPortNumber(char *hostname){
	FILE *f = popen("daemonReader", "r");
	char buffer[256];
	int count = 0;
	while (fgets(buffer, 256, f)) {
		buffer[strlen(buffer)-1] = '\0';
		if (count > 0)
			return atoi(buffer);
		sprintf(hostname, "%s", buffer);
		++count;
		
	}
	return -1;
}

/* 
CLASS: LISTNER 
DESC:Listener class inheritted to include new listen functions
*/
class Listener : public MessageListener {
  private:
    Session& session;
    SubscriptionManager subscriptions;
  public:
    string newMessage;
    LocalQueue local_queue;
    LocalQueue temp_local_queue;
    Listener(Session& session);
    virtual void prepareQueue(std::string queue, std::string exchange, std::string routing_key);
    virtual void received(Message& message);
    virtual void listen();
    void dequeue();
    void browse();
    void consume(); 
    bool initListen();
    string getNewMessage();
    ~Listener() { };
};


/*
 *  Listener::Listener
 *
 *  Subscribe to the queue, route it to a client destination for the
 *  listener. (The destination name merely identifies the destination
 *  in the listener, you can use any name as long as you use the same
 *  name for the listener).
 */

Listener::Listener(Session& session) : 
  session(session),
  subscriptions(session)
{
}

/*
FUNCTION : getNewMessage()
DESC : used to extract the last listened message from the qpid borker queue
*/
string Listener::getNewMessage(){
  return newMessage;
}

/*
FUNCTION : prepareQueue
DESC: not used!
*/
void Listener::prepareQueue(std::string queue, std::string exchange, std::string routing_key) {

  std::cout << "Subscribing to queue " << queue << std::endl;
  //subscriptions.subscribe(*this, queue);
  // Will not acquire messages but instead browse them.
  //  subscriptions.setAcquireMode(message::ACQUIRE_MODE_NOT_ACQUIRED);
  subscriptions.subscribe(local_queue, string("condor_queue"));
}

/*
FUNCTION : received()
DESC : function to process the new message listened to from the qpid broker queue
*/
void Listener::received(Message& message) {

  char* prevStateFileName = "tempLRM.dat";
  ofstream prevStateFile ;
  prevStateFile.open(prevStateFileName,ios::out);
  cout << "\n STOP : about to write to a file().. " <<endl;
  prevStateFile << message.getData();
  prevStateFile.close();
  rename(prevStateFileName,"LRM.dat");
  string msgTxt = message.getData();
  newMessage = message.getData();

  if (message.getData() == "") {
    std::cout << "Shutting down listener for " << message.getDestination() << std::endl;
    subscriptions.cancel(message.getDestination());
  }
  subscriptions.stop();
  subscriptions.cancel(message.getDestination());

}

/*
FUNCTION:listen()
DESC: function to acquire/dequeue messages from the qpid borker queue by giving control over to run -not used!
*/
void Listener::listen() {
  subscriptions.run();
}

/*
FUNCTION:listen()
DESC: function to simply copy/browse messages(w/o a dequeue) from the qpid borker queue by giving control over to run
*/

void Listener::browse() 
{ 
  subscriptions.subscribe(*this, "condor_queue", SubscriptionSettings(FlowControl::unlimited(), ACCEPT_MODE_NONE, ACQUIRE_MODE_NOT_ACQUIRED));     
  subscriptions.run(); 
}

/*
FUNCTION:consume()
DESC: function to acquire/dequeue messages using get() fn from the qpid borker queue by giving control over to run -not used!
*/

void Listener::consume() 
{ 
  Message lMsg;
  lMsg = subscriptions.get(string("condor_queue"));
  newMessage = lMsg.getData();
  cout << "\n consume() => " <<newMessage <<endl; 
} 

/*
FUNCTION:dequeue()
DESC: function to acquire/dequeue messages using LOCAL QUEUE subscribe of the qpid borker queue
*/

void Listener::dequeue() {
  subscriptions.subscribe(local_queue, string("condor_queue"));   
  Message lMsg;
  int size = 10;
  size = local_queue.size();
  cout << "\n STOP : about to dequeue  " <<endl;

  local_queue.get(lMsg,10000);
  newMessage = lMsg.getData();
  cout << "\n STOP : after dequeue " <<endl;
}


/*
FUNCTION:initListen()
DESC: function to chk if the qpid borker queue is empty or not
RETURNS: bool value of TRUE:if we should load using the init file
FALSE:if the qpid broker queue has message unused still
*/

bool Listener::initListen() {
  // Receive messages
  //Added to test for init on recovery ***********************
  subscriptions.subscribe(local_queue, string("condor_queue"),SubscriptionSettings(FlowControl::unlimited(), ACCEPT_MODE_NONE, ACQUIRE_MODE_NOT_ACQUIRED));   
  Message lMsg;
  // sleep so that the local queue can get the message from the main condor
  // queue
  sleep(2);
  int size = 10;
  size = local_queue.size();
  local_queue.get(lMsg,10000);
  newMessage = lMsg.getData();
  subscriptions.stop();
  subscriptions.cancel(lMsg.getDestination());
  cout << "\n initListen(): size of the queue is : " << size <<endl;
  if(size > 0){
    //message queue has unprocessed message
    return false;
  }
  else{
    // message queue is empty:read from the file and send to the main condor_queue
    return true;
  }
}



/*
CLASS: clientPub inherits Joblog to read event logs and send to qpid messaging system
*/

class ClientPub : public JobLog {
  private:

    bool compareMsgId(int p ,int c);
    bool compareFId(int p ,int c,int pe,int ce);
    int getMsgId(string msgTxt);
    int getMsgId(string msgTxt,string id);
    bool validLogMsg(string msg1, string msg2);
    
    bool mustFree;
    
  public:
    ClientPub(char* persistFile,char* iFile,int erotate, char *host = NULL, int port = -1);
    ~ClientPub();
    void publish_messages(string logMsg,char *persistFile);
    void initLoadMsg(char *loadFile);
    void readLog(char* persistFile,  int eventTrack, int erotate, bool excludeFlag);
    bool  initFlag;
    bool skipFlag;
    int curMsgId;
    int prevMsgId;
    int curFId;
    int prevFId;
    string tempMsg;
    int port;
    char *host;
};

ClientPub::ClientPub(char* persistFile,char* iFile,int erotate, char *hostArg, int portArg):JobLog(persistFile,iFile,erotate){
  cout << "Init ...  " << endl;
  initFlag = true;
  skipFlag = false;
  curMsgId = 0;
  prevMsgId = 0;
  curFId = 0;
  prevFId = 0;
  mustFree = true;
  this->host = (char*)malloc(256);
  if (portArg < 0)
  	this->port = getPortNumber(this->host);
  else 
  	this->port = portArg;
  	
  if (hostArg != NULL) {
  	if (this->host != NULL)
  		free(this->host);	
  	this->host = hostArg;
  	mustFree = false;
  }	
  
  tempMsg = "";
  //initStates(persistFile,iFile,erotate);
}

ClientPub::~ClientPub(){
	if (this->host != NULL && mustFree)
		free(this->host);
}

//new message queue part


/*
FUNCTION:compareMsgId
DESC: to compare the event id of the current message to send vs last sent message to the qpid broker queue to avoid duplicates
RETURN: bool value of true or false
*/
bool ClientPub::compareMsgId(int p ,int c){
  bool  flag = true;
  if(p!=c)
    flag = true;
  else
    flag = false;
  //  cout << "\nCompare EventId result for "<<p<<" < " <<c << "is :" <<flag<<endl;
  return flag;
}

/*
FUNCTION:compareFId
DESC: to compare the file id  of the current message to send vs last sent message to the qpid broker queue to avoid duplicates
RETURN: bool value of true or false
*/
bool ClientPub::compareFId(int p ,int c,int pe,int ce){
  bool  flag = true;
  if(p<c)
    flag = true;
  else if((p==c)&&(pe<ce))
      flag =true;
  else    
    flag = false;
  // cout << "\nCompare FileId result for "<<p<<" < " <<c << "is :" <<flag<<endl;
  return flag;
}

/*
FUNCTION:getMsgId
DESC: function to extract message id embedded in the log message
RETURN : id value as int
*/
int  ClientPub::getMsgId(string msgTxt){
  string estr = "MsgId=";
  int startpos = msgTxt.find(estr,0);
  if(startpos >= 0){
    //step 2: look for first space after MsgId
    int stoppos = msgTxt.find(" ",startpos);
    string idValStr = msgTxt.substr(startpos+6,stoppos);
    int msgId =atoi(idValStr.c_str());
    return(msgId);
  }
  else{
    return(-1);
  }
}

/*
FUNCTION:getMsgId
DESC: function to extract unique id embedded in the log message
RETURN : id value as int
*/
int  ClientPub::getMsgId(string msgTxt,string id){
  string estr = id+"=";
  int startpos = msgTxt.find(estr,0);
  if(startpos >= 0){
    //step 2: look for first space after MsgId
    int stoppos = msgTxt.find(" ",startpos);
    string idValStr = msgTxt.substr(startpos+4,stoppos);
    int msgId =atoi(idValStr.c_str());
    return(msgId);
  }
  else{
    return(-1);
  }
}

/*
FUNCTION:validLogMsg
DESC: function to compare the current message to send vs last sent message to chk for duplicates or msg loss
RETURN : bool true for valid current msg
*/
bool ClientPub::validLogMsg(string msg1, string msg2){
  //NOTE: ideally this condition is not posisble...Added to take care of cases
  //where msg  not delivered in order from the broker to client listeners
  cout << "\n validLogMsg()....." << endl;
  cout << "\t Message1 =>" << msg1 <<endl;
  cout << "\t Message2 =>" << msg2 <<endl;
  bool validFlag = false;
  int msg1Id = getMsgId(msg1,"eId");
  curMsgId = msg1Id;
  int m1Id = getMsgId(msg1,"fId");
  curFId = m1Id;


  int msg2Id = getMsgId(msg2,"eId");
  if(msg2Id > -1){
    int m2Id = getMsgId(msg2,"fId");
    if((m2Id > prevFId)||( msg2Id > prevMsgId && m2Id >= prevFId)){
      prevMsgId = msg2Id;
      prevFId = m2Id;
    }
    validFlag = compareFId(prevFId,curFId,prevMsgId,curMsgId);
  }
  else{
    validFlag = false;
    //TODO :: what if a valid msg existed then u will never turn off the
    //initFlag
    if(initFlag){
      validFlag = true;
      initFlag = false;
    }
  }
  return validFlag;
}

/*
FUNCTION:initLoadMsg
DESC:chks the qpid broker queue on start/restart to decide for initialization of message 
RETURN :void 
*/
void ClientPub::initLoadMsg(char *loadFile)
{
  printf("host: %s , port: %i \n", host, port); 
  if (port < 0) {
  	printf("QPID does not seem to be running - exiting");
  	return;
  }
  bool sendFlag = false;
  string logMsg="";
  fstream ifile;
  int getSize=0;
  Connection connection;
  try {
    connection.open(host, port);
    Session session =  connection.newSession();

    Listener listener(session);
    bool loadFlag = listener.initListen(); 

    //if message already present in condor_queue return 
    if(!loadFlag){
      cout << "\n initLoadMsg() => STATUS: condor_queue has contents " <<endl;
      session.close();
      connection.close();
      return;
    }
    ifile.open(loadFile, ios::out|ios::in);
   	
    ifile.seekg (0, ios::end);
    getSize = ifile.tellg();
    ifile.seekg (0);
    char *log;
    log =(char *)malloc(getSize);
    if(ifile.read(log,getSize)){
      logMsg=log;

      cout <<"\n initLoadMsg() => STATUS: load from file the condor_queue with msg : " << logMsg <<endl;
      Message message;
      message.getDeliveryProperties().setRoutingKey("routing_key1"); 
      message.getDeliveryProperties().setDeliveryMode(PERSISTENT);
      stringstream message_data;

      message_data << logMsg ;

      message.setData(message_data.str());
      async(session).messageTransfer(arg::content=message, arg::destination="amq.topic");
      ifile.close();
    }
    session.close();
    connection.close();
  } catch(const std::exception& error) {
    std::cout << error.what() << std::endl;
  }
}

/*
FUNCTION:publish_messages
DESC:fn to send message to qpid broker queue 
RETURN :void 
*/
void ClientPub::publish_messages(string logMsg,char *persistFile)
{
  //publisher connection establishing
  
  //int port = getPortNumber();
  bool sendFlag = false;

  Connection connection;
  try {
    connection.open(host, port);
    Session session =  connection.newSession();


    Message message;
    Message lMsg;

    // Set the routing key once, we'll use the same routing key for all
    // messages.
    message.getDeliveryProperties().setRoutingKey("routing_key1"); 
    message.getDeliveryProperties().setDeliveryMode(PERSISTENT);

    stringstream message_data;
    message_data << logMsg ;

    //Maintain global class variable to avoid waiting on listen when last Msg had to be skipped.
    if(!skipFlag){
      Listener listener(session);

      // STEP 3: browsing the main qpid broker queue to get a copy of the last sent message
      cout << "\n \n ******************************************************************************** \n \n" ;
//      cout << "\npublish_messages(): STATUS => STEP 3 :about to browse() in 2 sec.... " << endl;
      //sleep(2);
      listener.browse();
      tempMsg = listener.getNewMessage();

      // STEP 4: dequeuing the main qpid broker queue to remove the last sent message
  //    cout << "\npublish_messages(): STATUS => STEP 4: about to dequeue() in 2 sec.... " << endl;
      //sleep(2);
      listener.dequeue();
    }
    string estr = "fId=";
    int origstartpos = logMsg.find(estr,0);
    if(origstartpos >= 0){

      sendFlag = validLogMsg(logMsg,tempMsg);
    }
    else{
      sendFlag = false;
      cout << "Error: Current message is not a valid condor Log Message!" <<endl;
    }
    if(sendFlag){
      skipFlag = false;
      cout << "\npublish_messages(): RESULT => send msg TRUE.... " << endl;
      message.setData(message_data.str());

      //STEP 5: Asynchronous transfer sends messages as quickly as
      // possible without waiting for confirmation.
    //  cout << "\npublish_messages(): STATUS => STEP 5: about to sendMsg() in 2 sec.... " << endl;
      //sleep(2);
      async(session).messageTransfer(arg::content=message, arg::destination="amq.topic");

      //STEP 6: updating the  persist file with the state details of the system
     // cout << "\npublish_messages(): STATUS => STEP 6: about to updatePersistFile() in 2 sec.... " << endl;
      //sleep(2);
      bool stateUpFlag = updateState(stateBuf,log,persistFile,0,1,0);
      if(stateUpFlag){
        pos = 0;//TODO :: think to see if u can update the count of messages sent successfully
      }

    }
    else{
      skipFlag = true;
      cout << "\npublish_messages(): RESULT => send msg FALSE.... " << endl;
      //STEP 6: updating the  persist file with the state details of the system
      // cout << "\npublish_messages(): STATUS => STEP 6: about to updatePersistFile() in 2 sec.... " << endl;
      bool stateUpFlag = updateState(stateBuf,log,persistFile,0,1,0);
      if(stateUpFlag){
        pos = 0;//TODO :: think to see if u can update the count of messages sent successfully
      }

    }
    session.close();
    connection.close();
  } catch(const std::exception& error) {
    std::cout << error.what() << std::endl;
  }
}

/*
FUNCTION:readLog()
DESC:overridding fn to read event logs from the ELog file and send as message to qpid broker queue 
RETURN :void 
*/

//new version of ReadLog
void ClientPub::readLog(char* persistFile,  int eventTrack = ULOG_EXECUTE, int erotate = 10, bool excludeFlag=false){
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


  //setting the ouput file's seek position 

  /**********************************************   EVENT LOG READER  ***************************/

  ULogEvent *event ; 
  int msgId = 1;
  while(true) {
    ULogEventOutcome outCome = log->readEvent(event);
    std::ostringstream missedEStr;
    std::ostringstream mid;
    std::ostringstream fid;
    if(outCome == ULOG_OK){
      if(reportEvent[event->eventNumber] == 1 && excludeFlag){
        //cout << "\n EXCLUDING " << event->eventNumber <<endl;
        continue;
      }
      else if((reportEvent[event->eventNumber] == 1 && !excludeFlag) || excludeFlag){
        //cout << "\n INCLUDING " << event->eventNumber <<endl;

        string logStr = classAdFormatLog(event);

        //acquiring the unique id for each log ; combo of file id and event id within
        //the event log file
        const ReadUserLogStateAccess lsa = ReadUserLogStateAccess(stateBuf);
        unsigned long testId =0;
        unsigned long testId1 =0;
        int tlen = 128;
        char tbuf[128] ;//="";
        bool tf = false;
        tf = lsa.getFileEventNum(testId);
        mid << testId;
        logStr = "eId="+mid.str()+" "+logStr;

        tf = lsa.getSequenceNumber(tlen);
        fid << tlen;
        logStr = "fId="+fid.str()+" "+logStr;

        //send log as message to qpid messaging system broker 
        publish_messages(logStr,persistFile);
      }//else ends here
    } 
    else{
      if(outCome == ULOG_NO_EVENT)
      { 
        
        cout << "sleep for 5 sec" <<endl; 
        sleep(5);
        break;
      }
      else if(outCome == ULOG_MISSED_EVENT){
        error = "";
        otherStateBuf = stateBuf;
        log->GetFileState(tempStateBuf);
        const ReadUserLogStateAccess otherLSA = ReadUserLogStateAccess(otherStateBuf);
        const ReadUserLogStateAccess thisLSA = ReadUserLogStateAccess(tempStateBuf);
        evnDiffFlag = thisLSA.getEventNumberDiff(otherLSA, missedEventCnt);
        error = ULogEventOutcomeName[outCome];
        cout << "ERROR MISSED EVENT " <<missedEventCnt <<endl;
        missedEStr <<missedEventCnt;
        string mcnt = missedEStr.str();
        int cint=0;

        string eStr = "error.type="+error+" missedEventCnt="+mcnt+"\n";
        writeToFile(dbWFile,eStr,persistFile);
        eFile << "Following error was encountered : " << outCome << "\n";
      }
      else
      {
        //write to error log file
        error = "";
        error = ULogEventOutcomeName[outCome];
        string eStr = "error.type="+error+"\n";
        writeToFile(dbWFile,eStr,persistFile);
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

/* MAIN fn: instance of class clientPub created to send logs read as messages
** to qpid broker (server)
*/
int main(int argc, char*argv[]) {
  if(argc < 3) {
    cout << "\n Insufficient input"<<endl;
    exit(1);
  }
  char* inputFName = argv[1];
  char* persistFName = argv[2];
  int eventTrack = 1;
  int erotate = 10;
  if (argc > 3)
  	erotate = atoi(argv[4]);
  int excludeOn = 0;
  if (argc > 4)
  	excludeOn = atoi(argv[5]);
  char *host = NULL;
  if (argc > 5)
  	host = argv[6];
  bool excludeFlag = false;
  if (excludeOn==1)
    excludeFlag = true;
  ClientPub *cpObj = new ClientPub(persistFName,inputFName,erotate, host);
  if (cpObj->port < 0) {
  	printf("It seems as if no qpid daemon is running ... Exiting.\n");
  	delete cpObj;
  	return 0;
  }
  cpObj-> initLoadMsg("LRM.dat");
  cpObj->readLog(persistFName,eventTrack,erotate,excludeFlag);
  delete cpObj;
  return 0;
}
