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
#include "store_cred.h"
#include "condor_config.h"
#include "get_daemon_name.h"

#include "Triggerd.h"
#include "broker_utils.h"

#include "EventCondorTriggerNotify.h"
#include "CondorTrigger.h"
#include "ArgsCondorTriggerServiceAddTrigger.h"
#include "ArgsCondorTriggerServiceRemoveTrigger.h"
#include "ArgsCondorTriggerServiceSetTriggerName.h"
#include "ArgsCondorTriggerServiceSetTriggerQuery.h"
#include "ArgsCondorTriggerServiceSetTriggerText.h"
#include "ArgsCondorTriggerServiceSetEvalInterval.h"
#include "ArgsCondorTriggerServiceGetEvalInterval.h"

#include "condor_classad.h"

#include <sstream>

#include <qpid/management/ConnectionSettings.h>

using namespace qpid::management;
using namespace qmf::com::redhat::grid;
using namespace com::redhat::grid;

const char* ATTR_TRIGGER_NAME = "TriggerName";
const char* ATTR_TRIGGER_QUERY = "TriggerQuery";
const char* ATTR_TRIGGER_TEXT = "TriggerText";


Triggerd::Triggerd()
{
   singleton = NULL;
   mgmtObject = NULL;
   triggerCollection = NULL;
   collectors = NULL;
   query_collector = NULL;
   console = NULL;

   triggerEvalTimerId = -1;
   triggerEvalPeriod = 0;
   triggerUpdateTimerId = -1;
   triggerUpdateInterval = 0;

   initialized = false;

   dprintf(D_FULLDEBUG, "Triggerd::Triggerd called\n");
}


Triggerd::~Triggerd()
{
   std::map<uint32_t,Trigger*>::iterator iter;
   Trigger* trig;

   if (0 <= triggerEvalTimerId)
   {
      daemonCore->Cancel_Timer(triggerEvalTimerId);
      triggerEvalTimerId = -1;
   }
   if (0 <= triggerUpdateTimerId)
   {
      daemonCore->Cancel_Timer(triggerUpdateTimerId);
      triggerUpdateTimerId = -1;
   }
   if (NULL != collectors)
   {
      delete collectors;
      collectors = NULL;
   }
   if (NULL != query_collector)
   {
      delete[] query_collector;
      query_collector = NULL;
   }

   // Clean up in memory triggers
   for (iter = triggers.begin(); iter != triggers.end(); iter++)
   {
      trig = iter->second;
      triggers.erase(iter);
      delete trig;
   }
   delete triggerCollection;

   if (NULL != mgmtObject)
   {
      mgmtObject->resourceDestroy();
   }
   if (true == param_boolean("QMF_DELETE_ON_SHUTDOWN", true))
   {
      if (NULL != singleton)
      {
         dprintf(D_FULLDEBUG, "Triggerd: Deleting QMF object\n");
         delete singleton;
         dprintf(D_FULLDEBUG, "Triggerd: Deleted QMF object\n");
         singleton = NULL;
      }
   }
   if (NULL != console)
   {
      delete console;
      console = NULL;
   }

   InvalidatePublicAd();
}


void
Triggerd::InitPublicAd()
{
   publicAd = ClassAd();

   publicAd.SetMyTypeName(GENERIC_ADTYPE);
   publicAd.SetTargetTypeName("Triggerd");

   publicAd.Assign(ATTR_NAME, daemonName.c_str());

   daemonCore->publish(&publicAd);
}


void
Triggerd::init()
{
   std::string trigger_log;
   ClassAd* ad;
   HashKey key;
   uint32_t key_value;
   ReliSock* sock;
   int index;
   char* host;
   char* tmp;
   char* dataDir = NULL;
   char* username;
   char* password;
   char* mechanism;
   int port, interval;
   std::string storefile;
   std::string error_text;
   std::stringstream int_str;
   qpid::management::ConnectionSettings settings;
   bool enable_console = true;

   dprintf(D_FULLDEBUG, "Triggerd::init called\n");

   char* name = param("TRIGGERD_NAME");
   if (name)
   {
      char* valid_name = build_valid_daemon_name(name);
      daemonName = valid_name;
      delete[] name;
      delete[] valid_name;
   }
   else
   {
      char* default_name = build_valid_daemon_name("triggerd");
      if(default_name)
      {
         daemonName = default_name;
         delete[] default_name;
      }
   }

   port = param_integer("QMF_BROKER_PORT", 5672);
   if (NULL == (host = param("QMF_BROKER_HOST")))
   {
      host = strdup("localhost");
   }

   if (NULL == (username = param("QMF_BROKER_USERNAME")))
   {
      username = strdup("");
   }

   if (NULL == (mechanism = param("QMF_BROKER_AUTH_MECH")))
   {
      mechanism = strdup("ANONYMOUS");
   }

   tmp = param("QMF_STOREFILE");
   if (NULL == tmp)
   {
      storefile = ".triggerd_storefile";
   }
   else
   {
      storefile = tmp;
      free(tmp);
      tmp = NULL;
   }
   interval = param_integer("QMF_UPDATE_INTERVAL", 10);

   password = getBrokerPassword();

   dataDir = param("DATA");
   ASSERT(dataDir);

   trigger_log = dataDir;
   trigger_log += "/triggers.log";
   triggerCollection = new ClassAdCollection(trigger_log.c_str());
   free(dataDir);

   settings.host = std::string(host);
   settings.port = port;
   settings.username = std::string(username);
   settings.password = std::string(password);
   settings.mechanism = std::string(mechanism);

   // Initialize the QMF agent
   singleton = new ManagementAgent::Singleton();
   ManagementAgent* agent = singleton->getInstance();

   CondorTriggerService::registerSelf(agent);
   CondorTrigger::registerSelf(agent);
   EventCondorTriggerNotify::registerSelf(agent);

   agent->setName("com.redhat.grid","condortriggerservice", daemonName.c_str());
   agent->init(settings, interval, true, storefile);
   mgmtObject = new CondorTriggerService(agent, this);

   // Initialize the QMF console, if desired
   enable_console = param_boolean("ENABLE_ABSENT_NODES_DETECTION", false);
   if (true == enable_console)
   {
      console = new TriggerConsole();
      console->config(host, port, username, password, mechanism);
   }

   free(host);
   free(username);
   free(password);
   free(mechanism);

   // Initialize the triggers if any already exist
   triggerCollection->StartIterateAllClassAds();
   while(true == triggerCollection->IterateAllClassAds(ad, key))
   {
      key_value = atoll(key.value());
      if (triggers.end() == triggers.find(key_value))
      {
         if (STATUS_OK != AddTriggerToCollection(key_value, ad, error_text))
         {
            dprintf(D_ALWAYS, "Triggerd Error: '%s'.  Removing trigger\n", error_text.c_str());
            int_str << key_value;
            triggerCollection->DestroyClassAd(int_str.str().c_str());
         }
      }
   }

   bool lifetime = param_boolean("QMF_IS_PERSISTENT", true);
   agent->addObject(mgmtObject, daemonName.c_str(), lifetime);

   // Create a socket to handle management method calls
   sock = new ReliSock;
   if (NULL == sock)
   {
      EXCEPT("Failed to create Managment socket");
   }
   if (0 == sock->assign(agent->getSignalFd()))
   {
      EXCEPT("Failed to bind Management socket");
   }
   if (-1 == (index = daemonCore->Register_Socket((Stream *) sock, 
                                                  "Management Method Socket",
                                                  (SocketHandlercpp) &Triggerd::HandleMgmtSocket,
                                                  "Handler for Management Methods",
                                                  this)))
   {
      EXCEPT("Failed to register Management socket");
   }

   config();
}


void
Triggerd::config()
{
   dprintf(D_FULLDEBUG, "Triggerd::config called\n");

   if (initialized == false)
   {
      triggerEvalPeriod = param_integer("TRIGGERD_DEFAULT_EVAL_PERIOD", 10);
      SetInterval(triggerEvalPeriod);
      initialized = true;
   }

   InitPublicAd();

   int interval = param_integer("UPDATE_INTERVAL", 60); 
   if (triggerUpdateInterval != interval)
   {
      triggerUpdateInterval = interval;

      if (triggerUpdateTimerId >= 0)
      {
         daemonCore->Cancel_Timer(triggerUpdateTimerId);
         triggerUpdateTimerId = -1;
      }

      dprintf(D_FULLDEBUG, "Updating collector every %d seconds\n", triggerUpdateInterval);
      triggerUpdateTimerId = daemonCore->Register_Timer(0, triggerUpdateInterval,
                                                        (TimerHandlercpp)&Triggerd::UpdateCollector,
                                                        "Triggerd::UpdateCollector",
                                                        this);
   }

   if (NULL != collectors)
   {
      delete collectors;
      collectors = NULL;
   }
   collectors = CollectorList::create();

   if (NULL != query_collector)
   {
      delete query_collector;
      query_collector = NULL;
   }

   char* query_col_name = param("TRIGGERD_QUERY_COLLECTOR");
   if (NULL != query_col_name)
   {
      query_collector = new DCCollector(query_col_name);
      if (NULL == query_collector->addr())
      {
         dprintf(D_ALWAYS, "Triggerd Error:  Unable to find query collector '%s'.  Will query all collectors\n", query_col_name);
         delete query_collector;
         query_collector = NULL;
      }
      else
      {
         dprintf(D_FULLDEBUG, "Triggerd: Will query collector '%s'\n", query_col_name);
      }
   }
   delete[] query_col_name;
}


void
Triggerd::SetEvalTimer()
{
   if (0 <= triggerEvalTimerId)
   {
      daemonCore->Cancel_Timer(triggerEvalTimerId);
      triggerEvalTimerId = -1;
   }

   if (0 < triggerEvalPeriod)
   {
      triggerEvalTimerId = daemonCore->Register_Timer(triggerEvalPeriod,
                                                      triggerEvalPeriod,
                                                      (TimerHandlercpp)&Triggerd::PerformQueries,
                                                      "Triggerd::PerformQueries",
                                                      this);
      dprintf(D_FULLDEBUG, "Triggerd: Registered PerformQueries() to evaluate triggers every %d seconds\n", triggerEvalPeriod);
   }
   else
   {      dprintf(D_FULLDEBUG, "Triggerd: No evaluation interval set so no triggers will be evaluated\n");
   }
}


int
Triggerd::HandleMgmtSocket(Service*, Stream*)
{
   singleton->getInstance()->pollCallbacks();
   return KEEP_STREAM;
}


ManagementObject*
Triggerd::GetManagementObject(void) const
{
   return mgmtObject;
}


Manageable::status_t
Triggerd::ManagementMethod(uint32_t methodId, Args &args, std::string &text)
{
   Manageable::status_t ret_val;

   dprintf(D_FULLDEBUG, "Triggerd::ManagementMethod called\n");
   switch (methodId)
   {
      case CondorTriggerService::METHOD_ADDTRIGGER:
         ret_val = AddTrigger(((ArgsCondorTriggerServiceAddTrigger &) args).i_Name,
                              ((ArgsCondorTriggerServiceAddTrigger &) args).i_Query,
                              ((ArgsCondorTriggerServiceAddTrigger &) args).i_Text,
                              text);
         break;
      case CondorTriggerService::METHOD_REMOVETRIGGER:
         ret_val = DelTrigger(((ArgsCondorTriggerServiceRemoveTrigger &) args).i_TriggerID, text);
         break;
      case CondorTriggerService::METHOD_SETTRIGGERNAME:
         ret_val = SetTriggerName(((ArgsCondorTriggerServiceSetTriggerName &) args).i_TriggerID,
                                  ((ArgsCondorTriggerServiceSetTriggerName &) args).i_Name,
                                  text);
         break;
      case CondorTriggerService::METHOD_SETTRIGGERQUERY:
         ret_val = SetTriggerQuery(((ArgsCondorTriggerServiceSetTriggerQuery &) args).i_TriggerID,
                                  ((ArgsCondorTriggerServiceSetTriggerQuery &) args).i_Query,
                                  text);
         break;
      case CondorTriggerService::METHOD_SETTRIGGERTEXT:
         ret_val = SetTriggerText(((ArgsCondorTriggerServiceSetTriggerText &) args).i_TriggerID,
                                  ((ArgsCondorTriggerServiceSetTriggerText &) args).i_Text,
                                  text);
         break;
      case CondorTriggerService::METHOD_SETEVALINTERVAL:
         ret_val = SetInterval(((ArgsCondorTriggerServiceSetEvalInterval &) args).i_Interval, text);
         break;
      case CondorTriggerService::METHOD_GETEVALINTERVAL:
         ret_val = GetInterval(((ArgsCondorTriggerServiceGetEvalInterval &) args).o_Interval, text);
         break;
      default:
         ret_val = STATUS_UNKNOWN_METHOD;
         break;
   }

   return ret_val;
}


Manageable::status_t
Triggerd::AddTrigger(std::string name, std::string query, std::string triggerText, std::string& text)
{
   std::string attr;
   ClassAd* ad = new ClassAd();
   Manageable::status_t ret_val;
   char* tmp;

   ad->SetMyTypeName("EventTrigger");
   ad->SetTargetTypeName("Trigger");
   tmp = strdup(name.c_str());
   ReplaceAllChars(tmp, '"', '\'');
   ad->Assign(ATTR_TRIGGER_NAME, tmp);
   free(tmp);

   tmp = strdup(query.c_str());
   ReplaceAllChars(tmp, '"', '\'');
   ad->Assign(ATTR_TRIGGER_QUERY, tmp);
   free(tmp);

   tmp = strdup(triggerText.c_str());
   ReplaceAllChars(tmp, '"', '\'');
   ad->Assign(ATTR_TRIGGER_TEXT, tmp);
   free(tmp);

   ret_val = AddTrigger(ad, text);
   delete ad;

   return ret_val;
}


Manageable::status_t
Triggerd::AddTrigger(ClassAd* ad, std::string& text)
{
   return AddTrigger(GenerateId(), ad, text);
}


Manageable::status_t
Triggerd::AddTrigger(uint32_t key, ClassAd* ad, std::string& text)
{
   Manageable::status_t ret_val;
   std::stringstream int_str;

   dprintf(D_FULLDEBUG, "Triggerd::AddTrigger called\n");
   ret_val = AddTriggerToCollection(key, ad, text);
   if (STATUS_OK == ret_val)
   {
      int_str << key;
      triggerCollection->BeginTransaction();
      triggerCollection->NewClassAd(int_str.str().c_str(), ad);
      triggerCollection->CommitTransaction();
   }
   dprintf(D_FULLDEBUG, "Triggerd::AddTrigger exited with return value %d\n", ret_val);

   return ret_val;
}


Manageable::status_t
Triggerd::AddTriggerToCollection(uint32_t key, ClassAd* ad, std::string& text)
{
   Trigger* trig;
   char* value;
   Manageable::status_t ret_val;

   dprintf(D_FULLDEBUG, "Triggerd::AddTriggerToCollection called\n");
   if (triggers.end() != triggers.find(key))
   {
      text = "Failed to create new trigger.  A trigger already exists with id ";
      text += key;
      ret_val = STATUS_USER + 1;
   }
   else
   {
      trig = new Trigger(singleton->getInstance(), key);
      if (NULL == trig)
      {
         text = "Failed to create new trigger.  Out of memory";
         ret_val = STATUS_USER + 2;
      }
      else
      {
         if (0 != ad->LookupString(ATTR_TRIGGER_NAME, &value))
         {
            ret_val = trig->SetName(value, text);
         }
         else
         {
            dprintf(D_ALWAYS, "Triggerd Error: '%s' not found in classad.  Failed to create new trigger.\n", ATTR_TRIGGER_NAME);
            text = "Failed to create new trigger.  Unable to find ";
            text += ATTR_TRIGGER_NAME;
            text += " in classad";
            ret_val = STATUS_USER + 3;
            delete trig;
         }
         if (STATUS_OK == ret_val)
         {
            if (0 != ad->LookupString(ATTR_TRIGGER_QUERY, &value))
            {
               ret_val = trig->SetQuery(value, text);
            }
            else
            {
               dprintf(D_ALWAYS, "Triggerd Error: '%s' not found in classad.  Failed to create new trigger.\n", ATTR_TRIGGER_QUERY);
               text = "Failed to create new trigger.  Unable to find ";
               text += ATTR_TRIGGER_QUERY;
               text += " in classad";
               ret_val = STATUS_USER + 3;
               delete trig;
            }
         }
         if (STATUS_OK == ret_val)
         {
            if (0 != ad->LookupString(ATTR_TRIGGER_TEXT, &value))
            {
               ret_val = trig->SetText(value, text);
            }
            else
            {
               dprintf(D_ALWAYS, "Triggerd Error: '%s' not found in classad.  Failed to create new trigger.\n", ATTR_TRIGGER_TEXT);
               text = "Failed to create new trigger.  Unable to find ";
               text += ATTR_TRIGGER_TEXT;
               text += " in classad";
               ret_val = STATUS_USER + 3;
               delete trig;
            }
         }

         if (STATUS_OK == ret_val)
         {
            triggers.insert(std::pair<uint32_t, Trigger*>(key, trig));
         }
      }
   }
   dprintf(D_FULLDEBUG, "Triggerd::AddTriggerToCollection exited with return value %d\n", ret_val);
   return ret_val;
}

Manageable::status_t
Triggerd::DelTrigger(uint32_t triggerId, std::string& text)
{
   Trigger* trig;
   std::stringstream int_str;
   Manageable::status_t ret_val;

   dprintf(D_FULLDEBUG, "Triggerd::DelTrigger called\n");
   if (triggers.end() == triggers.find(triggerId))
   {
      text = "Unable to find trigger with id ";
      text += triggerId;
      text += ".  Failed to delete trigger";
      ret_val = STATUS_UNKNOWN_OBJECT;
   }
   else
   {
      int_str << triggerId;
      triggerCollection->BeginTransaction();
      if (false == triggerCollection->DestroyClassAd(int_str.str().c_str()))
      {
         dprintf(D_FULLDEBUG, "Error: Failed to remove trigger from internal memory\n");
         ret_val = STATUS_USER + 4;
      }
      else
      {
         trig = triggers[triggerId];
         triggers.erase(triggerId);
         delete trig;
         ret_val = STATUS_OK;
      }
      triggerCollection->CommitTransaction();
   }
   dprintf(D_FULLDEBUG, "Triggerd::DelTrigger exited with return value %d\n", ret_val);

   return ret_val;
}


Manageable::status_t
Triggerd::SetTriggerName(uint32_t key, std::string name, std::string& text)
{
   Manageable::status_t ret_val;
   std::stringstream int_str;
   std::map<uint32_t, Trigger*>::iterator iter;

   dprintf(D_FULLDEBUG, "Triggerd::SetTriggerName called\n");
   int_str << key;
   triggerCollection->BeginTransaction();
   if (0 >= name.size())
   {
      text = "Invalid trigger name.  Failed to set trigger name for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else if (triggers.end() == (iter = triggers.find(key)))
   {
      text = "Trigger not found.  Failed to set trigger name for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else if (false == triggerCollection->SetAttribute(int_str.str().c_str(), ATTR_TRIGGER_NAME, name.c_str()))
   {
      text = "Failed to set trigger name for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else
   {
      ret_val = iter->second->SetName(name, text);
   }
   triggerCollection->CommitTransaction();

   return ret_val;
}


Manageable::status_t
Triggerd::SetTriggerQuery(uint32_t key, std::string query, std::string& text)
{
   Manageable::status_t ret_val;
   std::stringstream int_str;
   std::map<uint32_t, Trigger*>::iterator iter;

   dprintf(D_FULLDEBUG, "Triggerd::SetTriggerQuery called\n");
   int_str << key;
   triggerCollection->BeginTransaction();
   if (0 >= query.size())
   {
      text = "Invalide trigger query.  Failed to set trigger query for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else if (triggers.end() == (iter = triggers.find(key)))
   {
      text = "Trigger not found.  Failed to set trigger query for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else if (false == triggerCollection->SetAttribute(int_str.str().c_str(), ATTR_TRIGGER_QUERY, query.c_str()))
   {
      text = "Failed to set trigger query for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else
   {
      ret_val = iter->second->SetQuery(query, text);
   }
   triggerCollection->CommitTransaction();

   return ret_val;
}


Manageable::status_t
Triggerd::SetTriggerText(uint32_t key, std::string val, std::string& text)
{
   Manageable::status_t ret_val;
   std::stringstream int_str;
   std::map<uint32_t, Trigger*>::iterator iter;

   dprintf(D_FULLDEBUG, "Triggerd::SetTriggerText called\n");
   int_str << key;
   triggerCollection->BeginTransaction();
   if (0 >= val.size())
   {
      text = "Invalid trigger text.  Failed to set trigger text for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else if (triggers.end() == (iter = triggers.find(key)))
   {
      text = "Trigger not found.  Failed to set trigger text for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else if (false == triggerCollection->SetAttribute(int_str.str().c_str(), ATTR_TRIGGER_TEXT, val.c_str()))
   {
      text = "Failed to set trigger text for id ";
      text += key;
      ret_val = STATUS_USER + 4;
   }
   else
   {
      ret_val = iter->second->SetText(val, text);
   }
   triggerCollection->CommitTransaction();

   return ret_val;
}


Manageable::status_t
Triggerd::SetInterval(int32_t interval, std::string& /*text*/)
{
   SetInterval(interval);
   return STATUS_OK;
}


void
Triggerd::SetInterval(int32_t interval)
{
   dprintf(D_FULLDEBUG, "Triggerd::SetInterval called\n");
   triggerEvalPeriod = interval;
   SetEvalTimer();
   mgmtObject->set_EvalInterval(interval);
}


Manageable::status_t
Triggerd::GetInterval(int32_t& interval, std::string& /*text*/)
{
   interval = GetInterval();
   return STATUS_OK;
}


int32_t
Triggerd::GetInterval()
{
   dprintf(D_FULLDEBUG, "Triggerd::GetInterval called\n");
   return triggerEvalPeriod;
}


uint32_t
Triggerd::GenerateId()
{
   return ((uint32_t)time(NULL) + triggers.size());
}


bool
Triggerd::PerformQueries()
{
   ClassAdList result;
   CondorError errstack;
   QueryResult status;
   Trigger* trig = NULL;
   CondorQuery* query;
   bool ret_val = true;
   std::map<uint32_t,Trigger*>::iterator iter;
   ClassAd* ad = NULL;
   std::string eventText;
   char* token = NULL;
   std::string triggerText;
   char* queryString = NULL;
   ExprTree* attr = NULL;
   std::list<std::string> missing_nodes;
   size_t pos;
   size_t prev_pos;
   bool bad_trigger = false;
   const char* token_str = NULL;

   if (0 < triggers.size())
   {
      dprintf(D_FULLDEBUG, "Triggerd: Evaluating %d triggers\n", (int)triggers.size());
      query = new CondorQuery(ANY_AD);
      for (iter = triggers.begin(); iter != triggers.end(); iter++)
      {
         // Clear any pre-exhisting custom contraints and add the constraint
         // for this trigger
         trig = iter->second;
         query->clearORCustomConstraints();
         query->clearANDCustomConstraints();
         queryString = strdup(trig->GetQuery().c_str());
         ReplaceAllChars(queryString, '\'', '"');
         query->addANDConstraint(queryString);
         free(queryString);

         // Perform the query and check the result
         if (NULL != query_collector)
         {
            status = query->fetchAds(result, query_collector->addr(), &errstack);
         }
         else
         {
            status = collectors->query(*query, result, &errstack);
         }
         if (Q_OK != status)
         {
            // Problem with the query
            if (Q_COMMUNICATION_ERROR == status)
            {
               dprintf(D_ALWAYS, "Triggerd Error: Error contacting the collecter - %s\n", errstack.getFullText(true).c_str());
               if (CEDAR_ERR_CONNECT_FAILED == errstack.code(0))
               {
                  dprintf(D_ALWAYS, "Triggerd Error: Couldn't contact the collector on the central manager\n");
               }
            }
            else
            {
               dprintf(D_ALWAYS, "Triggerd Error: Could not retrieve ads - %s\n", getStrQueryResult(status));
            }

            ret_val = false;
            break;
         }
         else
         {
            dprintf(D_FULLDEBUG, "Query successful.  Parsing results\n");

            // Query was successful, so parse the results
            result.Open();
            while ((ad = result.Next()))
            {
               if (true == bad_trigger)
               {
                  // Avoid processing a bad trigger multiple times.  Remove
                  // all result ads and reset the flag
                  dprintf(D_FULLDEBUG, "Cleaning up after a bad trigger\n");
                  result.Delete(ad);
                  while ((ad = result.Next()))
                  {
                     result.Delete(ad);
                  }
                  bad_trigger = false;
                  break;
               }
               eventText = "";
               triggerText = trig->GetText();
               dprintf(D_FULLDEBUG, "Parsing trigger text '%s'\n", triggerText.c_str());
               prev_pos = pos = 0;
               while (prev_pos < triggerText.length())
               {
                  pos = triggerText.find("$(", prev_pos, 2);
                  if (std::string::npos == pos)
                  {
                     // Didn't find the start of a varible, so append the
                     // remaining string
                     dprintf(D_FULLDEBUG, "Adding text string to event text\n");
                     eventText += triggerText.substr(prev_pos, std::string::npos);
                     prev_pos = triggerText.length();
                  }
                  else
                  {
                     // Found a variable for substitution.  Need to add
                     // text before it to the string, grab the variable
                     // to substitute for, and put its value in the text
                     eventText += triggerText.substr(prev_pos, pos - prev_pos);
                     dprintf(D_FULLDEBUG, "Adding text string prior to variable substitution to event text\n");

                     // Increment the position by 2 to skip the $(
                     prev_pos = pos + 2;
                     pos = triggerText.find(")", prev_pos, 1);

                     if (std::string::npos == pos)
                     {
                        // Uh-oh.  We have a start of a variable substitution
                        // but no closing marker.
                        dprintf(D_FULLDEBUG, "Error: Failed to find closing varable substitution marker ')'.  Aborting processing of the trigger\n");
                        bad_trigger = true;
                        break;
                     }
                     else
                     {
                        token_str = triggerText.substr(prev_pos, pos-prev_pos).c_str();
                        token = RemoveWS(token_str);
                        dprintf(D_FULLDEBUG, "token: '%s'\n", token);
                        if (NULL == token)
                        {
                           dprintf(D_ALWAYS, "Removing whitespace from %s produced unusable name.  Aborting processing of the trigger\n", token_str);
                           bad_trigger = true;
                           break;
                        }

                        attr = ad->LookupExpr(token);
                        if (NULL == attr)
                        {
                           // The token isn't found in the classad, so treat it
                           // like a string
                           dprintf(D_FULLDEBUG, "Adding text string to event text\n");
                           eventText += token;
                        }
                        else
                        {
                           dprintf(D_FULLDEBUG, "Adding classad value to event text\n");
                           eventText += ExprTreeToString(attr);
                        }
                        if (NULL != token)
                        {
                           free(token);
                           token = NULL;
                        }
                        ++pos;
                     }
                     prev_pos = pos;
                  }
               }

               // Remove the trailing space
               std::string::size_type notwhite = eventText.find_last_not_of(" ");
               eventText.erase(notwhite+1);

               // Send the event
               if (false == bad_trigger)
               {
                  EventCondorTriggerNotify event(eventText, time(NULL));
                  singleton->getInstance()->raiseEvent(event);
                  dprintf(D_FULLDEBUG, "Triggerd: Raised event with text '%s'\n", eventText.c_str());
               }
               result.Delete(ad);
            }
            bad_trigger = false;
            result.Close();
         }
      }
      delete query;
   }
   else
   {
      dprintf(D_FULLDEBUG, "Triggerd: No triggers to evaluate\n");
   }

   // Look for absent nodes (nodes expected to be in the pool but aren't)
   if (NULL != console)
   {
      missing_nodes = console->findAbsentNodes();
      if (0 < missing_nodes.size())
      {
         for (std::list<std::string>::iterator node = missing_nodes.begin();
              node != missing_nodes.end(); ++ node)
         {
            eventText = node->c_str();
            eventText += " is missing from the pool";
            EventCondorTriggerNotify event(eventText, time(NULL));
            singleton->getInstance()->raiseEvent(event);
            dprintf(D_FULLDEBUG, "Triggerd: Raised event with text '%s'\n", eventText.c_str());
         }
      }
   }

   return ret_val;
}

char*
Triggerd::RemoveWS(const char* text)
{
   int pos = 0;
   char* result = NULL;

   if (NULL != text)
   {
      // Remove preceeding whitespace first
      result = strdup(text);
      while (result[0] && isspace(result[0]))
      {
         ++result;
      }

      // Now remove trailing whitespace
      pos = strlen(result) - 1;
      while ((pos >= 0) && isspace(result[pos]))
      {
         result[pos--] = '\0';
      }
   }
   return result;
}


void
Triggerd::ReplaceAllChars(char* string, char replace, char with)
{
   char* place;
   bool ret_val = false;

   while (NULL != (place = strchr(string, replace)))
   {
      *place = with;
      ret_val = true;
   }
}


void
Triggerd::UpdateCollector()
{
   dprintf(D_FULLDEBUG, "Triggerd::UpdateCollector called\n");
   daemonCore->sendUpdates(UPDATE_AD_GENERIC, &publicAd);
}


void
Triggerd::InvalidatePublicAd()
{
   ClassAd invalidate_ad;
   MyString line;

   invalidate_ad.SetMyTypeName(QUERY_ADTYPE);
   invalidate_ad.SetTargetTypeName(GENERIC_ADTYPE);

   line.formatstr("%s == \"%s\"", ATTR_NAME, daemonName.c_str()); 
   invalidate_ad.AssignExpr(ATTR_REQUIREMENTS, line.Value());

   daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &invalidate_ad, NULL, false);
}
