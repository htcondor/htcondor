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

#ifndef _TRIGGER_H
#define _TRIGGER_H

#include <string>

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include "CondorTrigger.h"

namespace com {
namespace redhat {
namespace grid {

using namespace qpid::management;
using namespace qmf::com::redhat::grid;

class Trigger : public Manageable
{
   public:
      Trigger(ManagementAgent* agent, uint32_t id);
      ~Trigger();

      ManagementObject* GetManagementObject(void) const;
      status_t ManagementMethod(uint32_t methodId, Args &args, std::string &text);

      Manageable::status_t SetName(std::string name, std::string& /*text*/);
      Manageable::status_t SetQuery(std::string query, std::string& /*text*/);
      Manageable::status_t SetText(std::string triggerText, std::string& /*text*/);
      Manageable::status_t GetName(std::string& name, std::string& text);
      std::string GetName();
      Manageable::status_t GetQuery(std::string& query, std::string& text);
      std::string GetQuery();
      Manageable::status_t GetText(std::string& Triggertext, std::string& text);
      std::string GetText();

   private:
      CondorTrigger* mgmtObject;
};

}}} /* com::redhat::grid */

#endif /* _TRIGGER_H */
