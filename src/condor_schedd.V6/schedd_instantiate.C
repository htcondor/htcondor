/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "scheduler.h"
#include "proc.h"
#include "MyString.h"
#include "dedicated_scheduler.h"
#include "grid_universe.h"
#include "simplelist.h"

class Shadow;

template class HashTable<int, int>;
template class HashBucket<int,int>;
template class HashTable<int, shadow_rec *>;
template class HashBucket<int,shadow_rec *>;
template class HashTable<HashKey, match_rec *>;
template class HashBucket<HashKey,match_rec *>;
template class HashTable<PROC_ID, shadow_rec *>;
template class HashBucket<PROC_ID,shadow_rec *>;
template class HashTable<PROC_ID, ClassAd *>;
template class HashBucket<PROC_ID, ClassAd *>;
template class Queue<shadow_rec*>;
template class Queue<ContactStartdArgs*>;
template class List<shadow_rec*>;
template class Item<shadow_rec*>;
template class SimpleList<PROC_ID>;
template class ExtArray<int>;
template class ExtArray<MyString*>;
template class ExtArray<bool>;
template class ExtArray<PROC_ID>;
template class ExtArray<OwnerData>;
template class SimpleList<Shadow*>;
template class HashTable<int, ExtArray<PROC_ID> *>;

// for condor-G
template class HashTable<MyString,GridUniverseLogic::gman_node_t *>;

// for MPI (or parallel) use:
template class ExtArray<match_rec*>;
template class ExtArray<MRecArray*>;
template class ExtArray<ClassAd*>;
template class HashTable<int,AllocationNode*>;
template class HashBucket<int,AllocationNode*>;
template class List<ResTimeNode>;
template class Item<ResTimeNode>;
template class List<ClassAd>;
template class Item<ClassAd>;
template class Queue<ClassAd*>;
// You'd think we'd need to instantiate a HashTable and HashBucket for
// <HashKey, ClassAd*> here, but those are already instantiated in
// classad_log.C in the c++_util_lib (not in c++_util_instantiate.C
// where you'd expect to find it *sigh*)


bool operator==(const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}
