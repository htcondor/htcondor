/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "list.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include <map>

#include "basejob.h"

// Special value for a daemon-core timer id which indicates that there
// is no timer currently registered (for variables holding a timer id) or
// that no timer should be signalled when one normally would be (for
// functions that take as an argument a timer id to be signalled when
// something happens).
#define TIMER_UNSET -1

#define GM_RESOURCE_UNLIMITED	1000000000

extern char *ScheddAddr;
extern char *ScheddName;
extern DCSchedd *ScheddObj;
extern char *ScheddJobConstraint;
extern char *GridmanagerScratchDir;
extern char *myUserName;
extern char *SelectionValue;

// initialization
void Init();
void Register();

// maintainence
void Reconfig();

void requestScheddUpdate( BaseJob *job, bool notify );
void requestScheddUpdateNotification( int timer_id );

extern std::map<std::string, BaseJob*> FetchProxyList;

#endif
