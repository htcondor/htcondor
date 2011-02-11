/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Build with g++ test_triggers.cpp -l qmfconsole
 */
#include "qpid/console/ConsoleListener.h"
#include "qpid/console/SessionManager.h"

using namespace std;
using namespace qpid::console;


class Tester : ConsoleListener
{
   public:
      Tester();
      ~Tester();
      void init (const char* host, int port);
      void AddTrigger(const char* name, const char* query, const char* text);
      void RemoveTrigger(uint32_t id);
      void event(Event& event);
      SessionManager* GetSessionManager() { return sm; };
      bool GetEventReceived() { return event_received; };
      void ResetEventReceived() { event_received = false; };
      void ChangeTriggerName(uint32_t id, const char* name);
      void ChangeTriggerQuery(uint32_t id, const char* query);
      void ChangeTriggerText(uint32_t id, const char* text);
      int32_t GetInterval();
      void SetInterval(int32_t interval);

   private:
      SessionManager* sm;
      Broker* broker;
      bool event_received;
};


Tester::Tester()
{
   sm = NULL;
   broker = NULL;
   event_received = false;
}

void
Tester::init(const char* host, int port)
{
   SessionManager::Settings sm_settings;
   qpid::client::ConnectionSettings settings;

   sm_settings.rcvObjects = false;
   sm_settings.rcvEvents = true;
   sm_settings.rcvHeartbeats = false;
   sm_settings.userBindings = false;

   settings.host = host;
   settings.port = port;
   settings.username = "guest";
   settings.password = "guest";

   sm = new SessionManager(this, sm_settings);
   broker = sm->addBroker(settings);
}

Tester::~Tester()
{
   if (NULL != sm)
   {
      if (NULL != broker)
      {
         sm->delBroker(broker);
         broker = NULL;
      }
      delete sm;
      sm = NULL;
   }
}

void
Tester::event(Event& event)
{
   ClassKey key = event.getClassKey();
   if (key.getClassName() == "CondorTriggerNotify")
   {
      cout << "Received CondorTriggerNotify Event:" << endl;
      Object::AttributeMap attrs = event.getAttributes();
      for (Object::AttributeMap::iterator aIter = attrs.begin(); aIter != attrs.end(); aIter++)
      {
         cout << aIter->first << " = " << aIter->second->str() << endl;
      }
      cout << endl;
      event_received = true;
   }
}

void
Tester::AddTrigger(const char* name, const char* query, const char* text)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

   cout << "AddTrigger called" << endl;

   sm->getObjects(list, "condortriggerservice");
   cout << "# of objects: " << list.size() << endl;
   if (0 < list.size())
   {
      cout << "Adding Trigger: " << name << endl;
      Object& triggerService = *list.begin();
      args.addString("Name", std::string(name));
      args.addString("Query", std::string(query));
      args.addString("Text", std::string(text));

      triggerService.invokeMethod("AddTrigger", args, result);
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
   }
}

void
Tester::RemoveTrigger(uint32_t id)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

   sm->getObjects(list, "condortriggerservice");
   if (0 < list.size())
   {
      cout << "Removing Trigger ID: " << id << endl;
      Object& triggerService = *list.begin();
      args.addUint("TriggerID", id);
      triggerService.invokeMethod("RemoveTrigger", args, result);
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
   }
}

void
Tester::ChangeTriggerName(uint32_t id, const char* name)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

   sm->getObjects(list, "condortriggerservice");
   if (0 < list.size())
   {
      cout << "Changing Trigger Name to : " << name << endl;
      Object& triggerService = *list.begin();
      args.addUint("TriggerID", id);
      args.addString("Name", std::string(name));
      triggerService.invokeMethod("SetTriggerName", args, result);
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
   }
}

void
Tester::ChangeTriggerQuery(uint32_t id, const char* query)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

   sm->getObjects(list, "condortriggerservice");
   if (0 < list.size())
   {
      cout << "Changing Trigger Query to : " << query << endl;
      Object& triggerService = *list.begin();
      args.addUint("TriggerID", id);
      args.addString("Query", std::string(query));
      triggerService.invokeMethod("SetTriggerQuery", args, result);
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
   }
}

void
Tester::ChangeTriggerText(uint32_t id, const char* text)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

   sm->getObjects(list, "condortriggerservice");
   if (0 < list.size())
   {
      cout << "Changing Trigger Text to : " << text << endl;
      Object& triggerService = *list.begin();
      args.addUint("TriggerID", id);
      args.addString("Text", std::string(text));
      triggerService.invokeMethod("SetTriggerText", args, result);
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
   }
}

int32_t
Tester::GetInterval()
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

   sm->getObjects(list, "condortriggerservice");
   if (0 < list.size())
   {
      cout << "Retrieving Interval" << endl;
      Object& triggerService = *list.begin();
      triggerService.invokeMethod("GetEvalInterval", args, result);
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
   }
   return atoll(result.arguments.begin()->second->str().c_str());
}

void
Tester::SetInterval(int32_t interval)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

// Hack.  Can safely be removed once qmf is fixed
   sm->getObjects(list, "agent");
   sm->getObjects(list, "condortriggerservice");
   if (0 < list.size())
   {
      cout << "Setting Interval to " << interval << endl;
      Object& triggerService = *list.begin();
      args.addInt("Interval", interval);
      triggerService.invokeMethod("SetEvalInterval", args, result);
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
   }
}

int main_int(int argc, char** argv)
{
   SessionManager* smgr;
   Tester engine;
   Object::Vector list;
   Object::AttributeMap args;
//   ClassKey ckey;
   MethodResponse result;
   std::string id;
   const char* host = argc>1 ? argv[1] : "127.0.0.1";
   int port = argc>2 ? atoi(argv[2]) : 5672;
   uint32_t trigger_id;
   int32_t interval;
   time_t first_event, second_event;

   engine.init(host, port);
   smgr = engine.GetSessionManager();

   smgr->getObjects(list, "agent");
   smgr->getObjects(list, "condortrigger");
   cout << "Currently installed triggers:" << endl;
   for (Object::Vector::iterator iter = list.begin(); iter != list.end(); iter++)
   {
      cout << "Trigger ID: " << atoll(iter->getIndex().c_str()) << endl;
      iter->invokeMethod("GetTriggerName", args, result);
      cout << "Trigger Name: " << result.arguments.begin()->second->str() << endl;
      iter->invokeMethod("GetTriggerQuery", args, result);
      cout << "Trigger Query: " << result.arguments.begin()->second->str() << endl;
      iter->invokeMethod("GetTriggerText", args, result);
      cout << "Trigger Text: " << result.arguments.begin()->second->str() << endl << endl;
   }

   // Set the evaluation interval to 5 seconds
   engine.SetInterval(5);
   interval = engine.GetInterval();

   if (interval != 5)
   {
      cout << "Error: Interval was not to 5 seconds" << endl;
   }

   // Add a trigger and verify an event is received
   engine.AddTrigger("TestTrigger", "(SlotID == 1)", "$(Machine) has a slot 1");

   while (false == engine.GetEventReceived())
   {
      sleep(1);
   }
   first_event = time(NULL);
   engine.ResetEventReceived();

   while (false == engine.GetEventReceived())
   {
      sleep(1);
   }
   second_event = time(NULL);
   engine.ResetEventReceived();

   if ((second_event - first_event) > 6)
   {
      cout << "Error: Trigger evaluations occurring greater than evey 5 seconds" << endl;
   }
   else
   {
      cout << "Trigger evaluations occurring every 5 seconds" << endl;
   }

   // Retrieve the ID of the trigger added
   smgr->getObjects(list, "agent");
   smgr->getObjects(list, "condortrigger");
   for (Object::Vector::iterator iter = list.begin(); iter != list.end(); iter++)
   {
      iter->invokeMethod("GetTriggerName", args, result);
      if (result.arguments.begin()->second->str() == "TestTrigger")
      {
         cout << "Getting trigger id" << endl;
         trigger_id = atoll(iter->getIndex().c_str());
         break;
      }
   }

   // Change the trigger Name and verify an event is received
   engine.ChangeTriggerName(trigger_id, "Changed Test Trigger");
   while (false == engine.GetEventReceived())
   {
      sleep(1);
   }
   engine.ResetEventReceived();

   // Change the trigger query and text and verify an event is received
   engine.ChangeTriggerQuery(trigger_id, "(SlotID > 0)");
   engine.ChangeTriggerText(trigger_id, "$(Machine) has a slot $(SlotID)");
   while (false == engine.GetEventReceived())
   {
      sleep(1);
   }
   engine.ResetEventReceived();

   // Remove the trigger
   engine.RemoveTrigger(trigger_id);

   return 0;
}

int main(int argc, char** argv)
{
    try {
        return main_int(argc, argv);
    } catch(std::exception& e) {
        cout << "Top Level Exception: " << e.what() << endl;
    }
}
