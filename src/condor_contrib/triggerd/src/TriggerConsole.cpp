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

#include "TriggerConsole.h"
#include "MgmtConversionMacros.h"

using namespace com::redhat::grid;


TriggerConsole::TriggerConsole()
{
   sm = NULL;
   broker = NULL;
}


TriggerConsole::~TriggerConsole()
{
   if (false == param_boolean("QMF_DELETE_ON_SHUTDOWN", true))
   {
      return;
   }

   if (NULL != broker)
   {
      dprintf(D_FULLDEBUG, "Triggerd: Deleting broker object\n");
      sm->delBroker(broker);
      dprintf(D_FULLDEBUG, "Triggerd: Deleted broker object\n");
      broker = NULL;
   }
   if (NULL != sm)
   {
      dprintf(D_FULLDEBUG, "Triggerd: Deleting session manager object\n");
      delete sm;
      dprintf(D_FULLDEBUG, "Triggerd: Deleted session manager object\n");
      sm = NULL;
   }
}


void
TriggerConsole::config(std::string host, int port, std::string user, std::string passwd, std::string methods)
{
   qpid::client::ConnectionSettings settings;
   SessionManager::Settings sm_settings;

   sm_settings.rcvObjects = false;
   sm_settings.rcvEvents = false;
   sm_settings.rcvHeartbeats = false;
   sm_settings.userBindings = false;

   settings.host = host;
   settings.port = port;
   settings.username = user;
   settings.password = passwd;
   settings.mechanism = methods;

   if (NULL == sm)
   {
      sm = new SessionManager(this, sm_settings);
   }

   if (NULL != sm)
   {
      if (NULL != broker)
      {
         sm->delBroker(broker);
         broker = NULL;
      }
      broker = sm->addBroker(settings);
   }
}


std::list<std::string>
TriggerConsole::findAbsentNodes()
{
   Object::Vector list;
   std::map<std::string, bool> nodes_in_pool;
   std::list<std::string> missing_nodes;

   // Get the list of Master objects, which represent each condor node
   // crrently running
   try
   {
      sm->getObjects(list, "master");
   }
   catch(...)
   {
      dprintf(D_ALWAYS, "Triggerd Error: Failed to contact AMQP broker.  Unable to find nodes in the pool\n");
      dprintf(D_ALWAYS, "Triggerd: Check for absent nodes aborted\n");
      return missing_nodes;
   }

   for (Object::Vector::iterator obj = list.begin(); obj != list.end(); obj++)
   {
      nodes_in_pool.insert(std::pair<std::string, bool>(obj->attrString("Name"), false));
   }
   dprintf(D_FULLDEBUG, "Triggerd: Found %d nodes in the pool\n", (int)nodes_in_pool.size());

   // Get the list of nodes configured at the store
   try
   {
      sm->getObjects(list, "condorconfigstore");
   }
   catch(...)
   {
      dprintf(D_ALWAYS, "Triggerd Error: Failed to contact AMQP broker.  Unable to find nodes expected to be in the pool\n");
      dprintf(D_ALWAYS, "Triggerd: Check for absent nodes aborted\n");
      return missing_nodes;
   }

   if (list.size() > 0)
   {
      Object& store = *list.begin();
      MethodResponse result;
      Object::AttributeMap args;
      std::string name;

      store.invokeMethod("GetNodeList", args, result);
      if (result.code != 0)
      {
         dprintf(D_ALWAYS, "Triggerd Error(%d, %s): Failed to retrieve list of expected nodes\n", result.code, result.text.c_str());
         return missing_nodes;
      }

      dprintf(D_FULLDEBUG, "Triggerd: %d nodes expected to be in the pool\n", (int)result.arguments.size());
      for (Object::AttributeMap::iterator node = result.arguments.begin();
           node != result.arguments.end(); node++)
      {
         name = node->first;
         if (nodes_in_pool.end() == nodes_in_pool.find(name))
         {
            // This is a missing node
            missing_nodes.push_front(name);
         }
         else
         {
            nodes_in_pool[name] = true;
         }
      }
   }

   dprintf(D_FULLDEBUG, "Triggerd: Found %d missing nodes\n", (int)missing_nodes.size());
   return missing_nodes;
}
