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
 */
#include "condor_common.h"
#include "condor_config.h"

#include "qpid/console/ConsoleListener.h"
#include "qpid/console/SessionManager.h"

#include <getopt.h>

using namespace std;
using namespace qpid::console;

struct Trigger
{
   string name;
   string query;
   string text;

   Trigger(string const& n, string const& q, string const& t) : name(n), query(q), text(t) {}
};

class TriggerConfig : ConsoleListener
{
   public:
      TriggerConfig();
      ~TriggerConfig();
      void init();
      int AddDefaultTriggers();
      void ListTriggers();
      int AddTrigger(string name, string query, string text);
      int DelTrigger(uint32_t id);

   private:
      SessionManager* sm;
      Broker* broker;
};

TriggerConfig::TriggerConfig()
{
   sm = NULL;
   broker = NULL;
}

TriggerConfig::~TriggerConfig()
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
TriggerConfig::init()
{
   SessionManager::Settings sm_settings;
   qpid::client::ConnectionSettings settings;
   char* host;
   int port;

   sm_settings.rcvObjects = false;
   sm_settings.rcvEvents = false;
   sm_settings.rcvHeartbeats = false;
   sm_settings.userBindings = false;

   port = param_integer("QMF_BROKER_PORT", 5672);
   host = param("QMF_BROKER_HOST");
   if (NULL == host)
   {
      host = strdup("localhost");
   }

   settings.host = host;
   settings.port = port;
   settings.username = "guest";
   settings.password = "guest";

   sm = new SessionManager(this, sm_settings);
   broker = sm->addBroker(settings);
   free(host);
}

void
TriggerConfig::ListTriggers()
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;

   sm->getObjects(list, "condortrigger");
   cout << "Currenly installed triggers:" << endl;
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
}

int
TriggerConfig::AddTrigger(string name, string query, string text)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;
   int ret_val = 0;

   // Iterate over the defaults and add them
   sm->getObjects(list, "agent");

   sm->getObjects(list, "condortriggerservice");
   Object& triggerService = *list.begin();

   args.clear();
   args.addString("Name", name);
   args.addString("Query", query);
   args.addString("Text", text);
   triggerService.invokeMethod("AddTrigger", args, result);
   if (0 != result.code)
   {
      cout << "Error: Failed to add trigger named: " << name << endl;
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
      ret_val = 1;
   }

   return ret_val;
}

int
TriggerConfig::DelTrigger(uint32_t id)
{
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;
   int ret_val = 0;

   // Iterate over the defaults and add them
   sm->getObjects(list, "agent");

   sm->getObjects(list, "condortriggerservice");
   Object& triggerService = *list.begin();

   args.clear();
   args.addUint("TriggerID", id);
   triggerService.invokeMethod("RemoveTrigger", args, result);
   if (0 != result.code)
   {
      cout << "Error: Failed to remove trigger id: " << id << endl;
      cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
      ret_val = 1;
   }

   return ret_val;
}

int
TriggerConfig::AddDefaultTriggers()
{
   int ret_val = 0;
   Object::AttributeMap args;
   MethodResponse result;
   Object::Vector list;
   vector<Trigger>::iterator iter;
   vector<Trigger> defaults;

   // Setup the default triggers to iterate over
   defaults.push_back(Trigger("High CPU Usage", "(TriggerdLoadAvg1Min > 5)", "$(Machine) has a high load average ($(TriggerdLoadAvg1Min))"));
   defaults.push_back(Trigger("Low Free Mem", "(TriggerdMemFree <= 10240)", "$(Machine) has low free memory available ($(TriggerdMemFree )k)"));
   defaults.push_back(Trigger("Low Free Disk Space (/)", "(TriggerdFilesystem_slash_Free < 10240)", "$(Machine) has low disk space on the / partition ($(TriggerdFilesystem_slash_Free))"));
   defaults.push_back(Trigger("Busy and Swapping", "(State == \"Claimed\" && Activity == \"Busy\" && TriggerdSwapInKBSec > 1000 && TriggerdActivityTime > 300)", "$(Machine) has been Claimed/Busy for $(TriggerdActivityTime) seconds and is swapping in at $(TriggerdSwapInKBSec) K/sec"));
   defaults.push_back(Trigger("Busy but Idle", "(State == \"Claimed\" && Activity == \"Busy\" && CondorLoadAvg < 0.3 && TriggerdActivityTime > 300)", "$(Machine) has been Claimed/Busy for $(TriggerdActivityTime) seconds but only has a load of $(CondorLoadAvg)"));
   defaults.push_back(Trigger("Idle for long time", "(State == \"Claimed\" && Activity == \"Idle\" && TriggerdActivityTime > 300)", "$(Machine) has been Claimed/Idle for $(TriggerdActivityTime) seconds"));
   defaults.push_back(Trigger("Logs with ERROR entriess", "(TriggerdCondorLogCapitalError != \"\")", "$(Machine) has $(TriggerdCondorLogCapitalErrorCount) ERROR messages in the following log files: $(TriggerdCondorLogCapitalError)"));
   defaults.push_back(Trigger("Logs with error entriess", "(TriggerdCondorLogLowerError != \"\")", "$(Machine) has $(TriggerdCondorLogLowerErrorCount) error messages in the following log files: $(TriggerdCondorLogLowerError)"));
   defaults.push_back(Trigger("Logs with DENIED entries", "(TriggerdCondorLogCapitalDenied != \"\")", "$(Machine) has $(TriggerdCondorLogCapitalDeniedCount) DENIED Error messages in the following log files: $(TriggerdCondorLogCapitalDenied)"));
   defaults.push_back(Trigger("Logs with denied entries", "(TriggerdCondorLogLowerDenied != \"\")", "$(Machine) has $(TriggerdCondorLogLowerDeniedCount) denied Error messages in the following log files: $(TriggerdCondorLogLowerDenied)"));
   defaults.push_back(Trigger("Logs with WARNING entries", "(TriggerdCondorLogCapitalWarning != \"\")", "$(Machine) has $(TriggerdCondorLogCapitalWarningCount) WARNING messages in the following log files: $(TriggerdCondorLogCapitalWarning)"));
   defaults.push_back(Trigger("Logs with warning entries", "(TriggerdCondorLogLowerWarning != \"\")", "$(Machine) has $(TriggerdCondorLogLowerWarningCount) warning messages in the following log files: $(TriggerdCondorLogLowerWarning)"));
   defaults.push_back(Trigger("dprintf Logs", "(TriggerdCondorLogDPrintfs != \"\")", "$(Machine) has the following dprintf_failure log files: $(TriggerdCondorLogDPrintfs)"));
   defaults.push_back(Trigger("Logs with stack dumps", "(TriggerdCondorLogStackDump != \"\")", "$(Machine) has $(TriggerdCondorLogStackDumpCount) stack dumps in the following log files: $(TriggerdCondorLogStackDump)"));
   defaults.push_back(Trigger("Core Files", "(TriggerdCondorCoreFiles != \"\")", "$(Machine) has $(TriggerdCondorLogStackDumpCount) core files in the following locations: $(TriggerdCondorCoreFiles)"));

   // Iterate over the defaults and add them
   sm->getObjects(list, "condortriggerservice");
   if (0 < list.size())
   {
      Object& triggerService = *list.begin();
      for (iter = defaults.begin(); iter != defaults.end(); iter++)
      {
         args.clear();
         args.addString("Name", iter->name);
         args.addString("Query", iter->query);
         args.addString("Text", iter->text);
         triggerService.invokeMethod("AddTrigger", args, result);
         if (0 != result.code)
         {
            cout << "Error: Failed to add query named: " << iter->name << endl;
            cout << "Result: code=" << result.code << " text=" << result.text << endl << endl;
            ret_val = 1;
         }
      }
   }

   return ret_val;
}

int main_int(int argc, char** argv)
{
   TriggerConfig config;
   int index = 0;
   int opt;
   bool init = false;
   static struct option long_opts[] = {
      {"add", 0, 0, 'a'},
      {"delete", 0, 0, 'd'},
      {"init", 0, 0, 'i'},
      {"list", 0, 0, 'l'},
      {0, 0, 0, 0}
   };
   string trname;
   string trquery;
   string trtext;
   string trid;

   config.init();
   while (-1 != (opt = getopt_long(argc, argv, "adil", long_opts, &index)))
   {
      switch (opt)
      {
         case 'a':
            cout << "Enter the Trigger Name: ";
            getline (cin, trname);
            cout << "Enter the Trigger Query: ";
            getline (cin, trquery);
            cout << "Enter the Trigger Text: ";
            getline (cin, trtext);
            if (0 != config.AddTrigger(trname, trquery, trtext))
            {
               cout << "Error: Failed to add trigger" << endl;
            }
            break;
         case 'd':
            cout << "Enter the Trigger ID to delete: ";
            getline (cin, trid);
            if (0 != config.DelTrigger(atoll(trid.c_str())))
            {
               cout << "Error: Failed to delete trigger" << endl;
            }
            break;
         case 'i':

            if (0 != config.AddDefaultTriggers())
            {
               cout << "Error: Failed to add all initial triggers" << endl;
            }
            break;
         case 'l':
               config.ListTriggers();
            break;
         default:
            exit(1);
      }
   }

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
