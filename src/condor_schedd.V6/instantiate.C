#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "scheduler.h"
#include "proc.h"

template class HashTable<int, int>;
template class HashBucket<int,int>;
template class HashTable<int, shadow_rec *>;
template class HashBucket<int,shadow_rec *>;
template class HashTable<HashKey, match_rec *>;
template class HashBucket<HashKey,match_rec *>;
template class HashTable<PROC_ID, shadow_rec *>;
template class HashBucket<PROC_ID,shadow_rec *>;
template class Queue<shadow_rec*>;
template class List<PROC_ID>;
template class Item<PROC_ID>;

bool operator==(const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}
