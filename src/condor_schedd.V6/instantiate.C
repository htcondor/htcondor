#ifdef __GNUG__
#pragma implementation "HashTable.h"
#endif


#include "HashTable.h"

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "sched.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "proc.h"
#include "exit.h"
#include "condor_collector.h"

bool operator==(const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}

#include "scheduler.h"
typedef HashTable<int, int> ClusterSizeHashTable_t;


