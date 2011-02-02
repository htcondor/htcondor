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

#include "TriggerObject.h"

#include "ArgsCondorTriggerGetTriggerName.h"
#include "ArgsCondorTriggerGetTriggerQuery.h"
#include "ArgsCondorTriggerGetTriggerText.h"


using namespace com::redhat::grid;

using namespace qpid::management;
using namespace qmf::com::redhat::grid;
using namespace com::redhat::grid;


Trigger::Trigger(ManagementAgent* agent, uint32_t id)
{
   mgmtObject = new CondorTrigger(agent, this, id);
   agent->addObject(mgmtObject, id);
}


Trigger::~Trigger()
{
   if (NULL != mgmtObject)
   {
      mgmtObject->resourceDestroy();
   }
}


ManagementObject*
Trigger::GetManagementObject() const
{
   return mgmtObject;
}


Manageable::status_t
Trigger::ManagementMethod(uint32_t methodId, Args &args, std::string &text)
{
   Manageable::status_t ret_val = STATUS_USER;

   switch (methodId)
   {
      case CondorTrigger::METHOD_GETTRIGGERNAME:
         ret_val = GetName(((ArgsCondorTriggerGetTriggerName &) args).o_Name, text);
         break;
      case CondorTrigger::METHOD_GETTRIGGERQUERY:
         ret_val = GetQuery(((ArgsCondorTriggerGetTriggerQuery &) args).o_Query, text);
         break;
      case CondorTrigger::METHOD_GETTRIGGERTEXT:
         ret_val = GetText(((ArgsCondorTriggerGetTriggerText &) args).o_Text, text);
         break;
      default:
         ret_val = STATUS_UNKNOWN_METHOD;
         break;
   }

   return ret_val;
}



Manageable::status_t
Trigger::SetName(std::string name, std::string& /*text*/)
{
   mgmtObject->set_TriggerName(name);
   return STATUS_OK;
}


Manageable::status_t
Trigger::SetQuery(std::string query, std::string& /*text*/)
{
   mgmtObject->set_TriggerQuery(query);
   return STATUS_OK;
}


Manageable::status_t
Trigger::SetText(std::string triggerText, std::string& /*text*/)
{
   mgmtObject->set_TriggerText(triggerText);
   return STATUS_OK;
}


Manageable::status_t
Trigger::GetName(std::string& name, std::string& /*text*/)
{
   name = mgmtObject->get_TriggerName();
   return STATUS_OK;
}


std::string
Trigger::GetName()
{
   return mgmtObject->get_TriggerName();
}


Manageable::status_t
Trigger::GetQuery(std::string& query, std::string& /*text*/)
{
   query =  mgmtObject->get_TriggerQuery();
   return STATUS_OK;
}


std::string
Trigger::GetQuery()
{
   return  mgmtObject->get_TriggerQuery();
}


Manageable::status_t
Trigger::GetText(std::string& triggerText, std::string& /*text*/)
{
   triggerText = mgmtObject->get_TriggerText();
   return STATUS_OK;
}


std::string
Trigger::GetText()
{
   return mgmtObject->get_TriggerText();
}
