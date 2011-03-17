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

#ifndef _TRIGGERD_H
#define _TRIGGERD_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "classad_collection.h"

#include <map>
#include <string>

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include "CondorTriggerService.h"
#include "TriggerConsole.h"
#include "TriggerObject.h"

#include "dc_collector.h"


namespace com {
namespace redhat {
namespace grid {

using namespace qpid::management;
using namespace qmf::com::redhat::grid;

class Triggerd : public Service, public Manageable
{
   public:
      Triggerd();
      ~Triggerd();

      ManagementObject* GetManagementObject(void) const;
      status_t ManagementMethod(uint32_t methodId, Args &args, std::string &text);
      bool PerformQueries();
      void config();
      void init();
      int32_t GetInterval();
      void SetInterval(int32_t interval);
      Manageable::status_t AddTrigger(std::string name, std::string query, std::string triggerText, std::string& text);
      int HandleMgmtSocket(Service*, Stream*);
      void CheckUpdateInterval();

   private:
      CondorTriggerService* mgmtObject;
      ManagementAgent::Singleton* singleton;
      std::map<uint32_t, Trigger*> triggers;
      ClassAdCollection* triggerCollection;
      TriggerConsole* console;
      std::string daemonName;
      ClassAd publicAd;

      int triggerEvalTimerId;
      int triggerEvalPeriod;
      int triggerUpdateTimerId;
      int triggerUpdateInterval;

      CollectorList* collectors;
      DCCollector* query_collector;
      bool initialized;

      Manageable::status_t AddTrigger(ClassAd* ad, std::string& text);
      Manageable::status_t AddTrigger(uint32_t key, ClassAd* ad, std::string& text);
      Manageable::status_t AddTriggerToCollection(uint32_t key, ClassAd* ad, std::string& text);
      Manageable::status_t DelTrigger(uint32_t triggerId, std::string& text);
      Manageable::status_t SetTriggerName(uint32_t key, std::string name, std::string& text);
      Manageable::status_t SetTriggerQuery(uint32_t key, std::string query, std::string& text);
      Manageable::status_t SetTriggerText(uint32_t key, std::string val, std::string& text);
      Manageable::status_t SetInterval(int32_t interval, std::string& text);
      Manageable::status_t GetInterval(int32_t& interval, std::string& text);

      uint32_t GenerateId();
      char* RemoveWS(const char* text);
      void SetEvalTimer();
      void ReplaceAllChars(char* string, char replace, char with);

      void UpdateCollector();
      void InvalidatePublicAd();
      void InitPublicAd();
};

}}} /* com::redhat::grid */

#endif /* _TRIGGERD_H */
