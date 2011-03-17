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

//#include <cstdlib>
//#include <iostream>

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
  public:
    Listener(Session& session);
    virtual void prepareQueue(std::string queue, std::string exchange, std::string routing_key);
    virtual void received(Message& message);
    virtual void listen();
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


void Listener::prepareQueue(std::string queue, std::string exchange, std::string routing_key) {

//Binding the queue to the exchange on the specified routing key 
    session.exchangeBind(arg::exchange=exchange, arg::queue=queue, arg::bindingKey=routing_key);

    /*
     * subscribe to the queue using the subscription manager.
     */

    std::cout << "Subscribing to queue " << queue << std::endl;
    subscriptions.subscribe(*this, queue);
}

void Listener::received(Message& message) {
    std::cout << "Message: " << message.getData() << " from " << message.getDestination() << std::endl;

    if (message.getData() == "") {
        std::cout << "Shutting down listener for " << message.getDestination() << std::endl;
        subscriptions.cancel(message.getDestination());
    }
}

void Listener::listen() {
    // Receive messages
    subscriptions.run();
}

int main(int argc, char** argv) {
    char* host = (char*)malloc(256);
    int port =  getPortNumber(host);
    if (port < 0) {
    	printf("Apparently no qpid daemon is running. Exiting.\n");
    } 
    
    std::string exchange = "amq.topic";
    Connection connection;
    try {
        connection.open(host, port);
        Session session =  connection.newSession();

        //--------- Main body of program --------------------------------------------

	// Create a listener for the session

        Listener listener(session);

        // Subscribe to messages on the queues we are interested in

        listener.prepareQueue("log_msg_queue", exchange, "routing_key1");

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


