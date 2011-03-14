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

#include <qpid/client/Connection.h>
#include <qpid/client/Session.h>
#include <qpid/client/Message.h>
#include <qpid/client/MessageListener.h>
#include <qpid/client/SubscriptionManager.h>

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


using namespace qpid::client;
using namespace qpid::framing;


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

class Listener : public MessageListener {
  private:
    Session& session;
    SubscriptionManager subscriptions;
    string sendTo;
  public:
    Listener(Session& session);
    virtual void prepareQueue(std::string queue, std::string exchange, std::string routing_key);
    virtual void received(Message& message);
    virtual void listen();
    virtual void initSendTo(string receiver);
    virtual void sendXmppIM(string eStr);
    virtual void sendSkypeIM(string eStr);
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


//function: sets the recepient of the skype event log IM's
void Listener::initSendTo(string receiver){
  sendTo=receiver;
  std::cout << "in sendTo=>" << sendTo <<std::endl;
}

void Listener::prepareQueue(std::string queue, std::string exchange, std::string routing_key) {

  //    session.exchangeBind(arg::exchange=exchange, arg::queue=queue, arg::bindingKey=routing_key);
  std::cout << "Subscribing to queue " << queue << std::endl;
  subscriptions.subscribe(*this, queue);
}


//Function: for the recieved message from the queue susbscribed to
//optional: the received message from the broker queue can be then sent to
//external agents like skype IM's /write to file/send emails etc
void Listener::received(Message& message) {

  std::cout << "Message: " << message.getData() << " from " << message.getDestination() << std::endl;
  //obtaining the message as received
  string msg = message.getData();
  int spos = msg.find("MSG=");
  string msgTxt="";
  if (spos > 0){
    //    std::cout << "chk here 2" <<std::endl;
    msgTxt = msg.substr(spos+4);
  }
  else{
    msgTxt = msg;
  }  
  std::cout << "stripped content " << msgTxt  << std::endl;
  //logic: sending the receieved message as a skype IM
  //sendXmppIM(message.getData());
  sendSkypeIM(msgTxt);
  if (message.getData() == "That's all, folks!") {
    std::cout << "Shutting down listener for " << message.getDestination() << std::endl;
    subscriptions.cancel(message.getDestination());
  }
}

void Listener::listen() {
  // Receive messages
  subscriptions.run();
}

//Function: send message via xmpp
void Listener::sendXmppIM(string eStr){

  string cmd1 = "echo \"";
  //  string cmd2 = "\" | sendxmpp -s test cLogTest@jabber.org";
  string cmd2 = "\" | sendxmpp -s test condorTest1@jabber.org";
  string cmd = cmd1 + eStr + cmd2;
  std::cout << "Calling :: "<< cmd <<std::endl;
  system(cmd.c_str());

}

//Function: send message via skype calling the sendAll.py python script 
void Listener::sendSkypeIM(string eStr){

  string cmd1 = "./sendAll.py "+sendTo+" ";
  string cmd = cmd1 + "\"" + eStr + "\"" ;
  std::cout << "in sendSkypeIM=>" << sendTo<< " ;cmd="<<cmd <<std::endl;
  // std::cout << "Calling :: "<< cmd <<std::endl;
  system(cmd.c_str());
}

int main(int argc, char** argv) {

  char* fileName = argv[1];
  ifstream fin(fileName); 
  string param;
  char host[20];
  fin.getline(host,20);
  std::cout << "host=>" << host <<std::endl;
  string portStr,queueStr,keyStr,receiver;
  char hostProx[20];
  //portStr = string(portNum);
  //getline(fin,portStr);
  getline(fin,queueStr);
  getline(fin,keyStr);
  getline(fin,receiver);
  fin.close();
  int port = getPortNumber(hostProx); //atoi(portStr.c_str());//  5672;
  std::cout << "port=>" << port <<std::endl;
  std::cout << "sendTo=>" << receiver <<std::endl;
  std::string exchange = "amq.topic";
  Connection connection;
  try {
    connection.open(host, port);
    Session session =  connection.newSession();

    //--------- Main body of program --------------------------------------------

    // Create a listener for the session

    Listener listener(session);
    listener.initSendTo(receiver);

    // Subscribe to messages on the queues we are interested in

    listener.prepareQueue(queueStr, exchange, keyStr);

    std::cout << "Listening for messages ..." << std::endl;

    // Give up control and receive messages
    listener.listen();


    //-----------------------------------------------------------------------------

    connection.close();
    return 0;
  } catch(const std::exception& error) {
    std::cout << error.what() << std::endl;
  }
  return 1;   
}


