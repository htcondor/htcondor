/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
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

#ifndef __MUTTERINTERFACE_H__
#define __MUTTERINTERFACE_H__

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "stat_info.h"

struct MutterSession {
  uint64_t idle_time;
  uid_t owner;
  gid_t group;
  std::string sesion_bus;
};

class MutterInterface
{
  public:
    MutterInterface();

    bool CheckActivity();
  private:
    bool GetSessions();

    uint64_t last_min_idle;
    std::vector<struct MutterSession> sessions;
};
#endif //__MUTTERINTERFACE_H__
