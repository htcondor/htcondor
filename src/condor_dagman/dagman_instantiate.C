#include "condor_common.h"
#include "dag.h"
#include "job.h"
#include "script.h"
template class List<Job>;
template class Item<Job>;
template class ListIterator<Job>;
template class SimpleListIterator<JobID_t>;
template class HashTable<int, Script*>;
template class Queue<Script*>;
template class Queue<Job*>;
template class SimpleList<Job*>;
