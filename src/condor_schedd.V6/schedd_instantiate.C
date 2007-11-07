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


#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "scheduler.h"
#include "proc.h"
#include "MyString.h"
#include "dedicated_scheduler.h"
#include "grid_universe.h"
#include "simplelist.h"
#include "list.h"
#ifdef HAVE_EXT_GSOAP
#  include "schedd_api.h"
#endif
#include "tdman.h"
#include "condor_crontab.h"
#include "transfer_queue.h"

template class SimpleList<TransferQueueRequest *>;
template class SimpleList<TransferRequest*>;
template class HashTable<MyString, TransferRequest*>;

class Shadow;

template class HashTable<int, int>;
template class HashBucket<int,int>;
template class HashTable<int, shadow_rec *>;
template class HashBucket<int,shadow_rec *>;
template class HashTable<HashKey, match_rec *>;
template class HashTable<PROC_ID, match_rec *>;
template class HashBucket<HashKey,match_rec *>;
template class HashTable<PROC_ID, shadow_rec *>;
template class HashBucket<PROC_ID,shadow_rec *>;
template class HashTable<PROC_ID, ClassAd *>;
template class HashBucket<PROC_ID, ClassAd *>;
template class HashTable<UserIdentity, GridJobCounts>;
template class HashBucket<UserIdentity, GridJobCounts>;
template class HashTable<PROC_ID, CronTab *>;
template class HashBucket<PROC_ID, CronTab *>;
template class Queue<shadow_rec*>;
template class Queue<ContactStartdArgs*>;
template class Queue<int>;
template class List<shadow_rec*>;
template class Item<shadow_rec*>;
template class SimpleList<PROC_ID>;
template class ExtArray<MyString*>;
template class ExtArray<bool>;
template class ExtArray<OwnerData>;
template class SimpleList<Shadow*>;
template class HashTable<int, ExtArray<PROC_ID> *>;

template class HashTable<MyString, TransferDaemon*>;
template class HashBucket<MyString, TransferDaemon*>;
template class HashTable<long, TransferDaemon*>;
template class HashBucket<long, TransferDaemon*>;

// for condor-G
template class HashTable<MyString,GridUniverseLogic::gman_node_t *>;

// for MPI (or parallel) use:
template class ExtArray<match_rec*>;
template class ExtArray<MRecArray*>;
template class ExtArray<ClassAd*>;
template class HashTable<int,AllocationNode*>;
template class HashBucket<int,AllocationNode*>;
template class List<ClassAd>;
template class Item<ClassAd>;
template class Queue<PROC_ID>;
// You'd think we'd need to instantiate a HashTable and HashBucket for
// <HashKey, ClassAd*> here, but those are already instantiated in
// classad_log.C in the c++_util_lib (not in c++_util_instantiate.C
// where you'd expect to find it *sigh*)

// schedd_api
#ifdef HAVE_EXT_GSOAP
template class HashTable<MyString, JobFile>;
template class List<FileInfo>;
template class Item<FileInfo>;
template class HashTable<int, ScheddTransaction*>;
template class HashTable<PROC_ID, Job*>;
#endif
