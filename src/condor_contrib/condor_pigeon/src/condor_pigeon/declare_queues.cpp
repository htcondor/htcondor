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

 
#include "getPort.h"

#include <qpid/client/Connection.h>
#include <qpid/client/Session.h>
#include <qpid/client/QueueOptions.h>




using namespace qpid::client;
using namespace qpid::framing;


int main(int argc, char** argv) {
  const char* host = argc>1 ? argv[1] : "127.0.0.1";
  std::string sPort = getPort();
  int port =  argc>2 ? atoi(argv[2]) : atoi(sPort.c_str()); //argc>2 ? atoi(argv[2]) : 5672;
  printf("Port: %i \n", port);
  Connection connection;

  try {
    connection.open(host, port);
    Session session =  connection.newSession();


    //--------- Main body of program --------------------------------------------

    // Create a queue named "message_queue", and route all messages whose
    // routing key is "routing_key" to this newly created queue.
//    QueueOptions qo;
  //  qo.setOrdering(LVQ);



//    session.queueDeclare(arg::queue="message_queue");//, arg::arguments=qo);

      //session.queueDeclare(arg::queue="message_queue", arg::exclusive=false);
      session.queueDeclare(arg::queue="condor_queue", arg::exclusive=false);
      session.queueDeclare(arg::queue="log_msg_queue", arg::exclusive=false);
      session.queueDeclare(arg::queue="log_msg_queue1", arg::exclusive=false);
      //session.queueDeclare(arg::queue="message_queue");
//    string key="test";
  //  qo.getLVQKey(key);

    //session.exchangeBind(arg::exchange="amq.topic", arg::queue="message_queue", arg::bindingKey="routing_key1");
    session.exchangeBind(arg::exchange="amq.topic", arg::queue="condor_queue", arg::bindingKey="routing_key1");
    session.exchangeBind(arg::exchange="amq.topic", arg::queue="log_msg_queue", arg::bindingKey="routing_key1");
    session.exchangeBind(arg::exchange="amq.topic", arg::queue="log_msg_queue1", arg::bindingKey="routing_key1");

//    session.queueDeclare(arg::queue="ack_message_queue");
  //  session.exchangeBind(arg::exchange="amq.direct", arg::queue="message_queue", arg::bindingKey="ack_routing_key");

    //-----------------------------------------------------------------------------

    connection.close();
    return 0;
  } catch(const std::exception& error) {
    std::cout << error.what() << std::endl;
  }
  return 1;

}



