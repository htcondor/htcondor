#include "condor_common.h"
#include "dag.h"
#include "job.h"
#include "script.h"
template class List<TQI>;
template class Item<TQI>;
template class ListIterator<TQI>;
template class List<Job>;
template class Item<Job>;
template class ListIterator<Job>;
template class SimpleListIterator<JobID_t>;
template class HashTable<int, Script*>;
template class Queue<Script*>;
template class Queue<Job*>;
