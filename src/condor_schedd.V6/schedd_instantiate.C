/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "scheduler.h"
#include "proc.h"
#include "dedicated_scheduler.h"
#include "grid_universe.h"

template class HashTable<int, int>;
template class HashBucket<int,int>;
template class HashTable<int, shadow_rec *>;
template class HashBucket<int,shadow_rec *>;
template class HashTable<HashKey, match_rec *>;
template class HashBucket<HashKey,match_rec *>;
template class HashTable<PROC_ID, shadow_rec *>;
template class HashBucket<PROC_ID,shadow_rec *>;
template class Queue<shadow_rec*>;
template class Queue<ContactStartdArgs*>;
template class List<shadow_rec*>;
template class Item<shadow_rec*>;
template class List<PROC_ID>;
template class Item<PROC_ID>;
template class ExtArray<int>;

// for condor-G
template class HashTable<MyString,GridUniverseLogic::gman_node_t *>;

// for MPI use:
template class ExtArray<match_rec*>;
template class ExtArray<MRecArray*>;
template class ExtArray<ClassAd*>;
template class HashTable<int,AllocationNode*>;
template class HashBucket<int,AllocationNode*>;
template class List<ResTimeNode>;
template class Item<ResTimeNode>;
template class List<ClassAd>;
template class Item<ClassAd>;
// You'd think we'd need to instantiate a HashTable and HashBucket for
// <HashKey, ClassAd*> here, but those are already instantiated in
// classad_log.C in the c++_util_lib (not in c++_util_instantiate.C
// where you'd expect to find it *sigh*)


bool operator==(const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}
