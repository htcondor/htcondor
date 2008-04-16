#include "condor_common.h"
#include "condor_daemon_core.h"
#include "list.h"
#include "extArray.h"
#include "HashTable.h"
#include "condor_partition.h"
#include "condor_workload.h"

template class ExtArray<Partition>;
template class ExtArray<Workload>;
template class HashTable<MyString, bool>;
