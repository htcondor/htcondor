#include "condor_common.h"
#include "dag.h"
template class List<TQI>;
template class Item<TQI>;
template class ListIterator<TQI>;
template class List<Job>;
template class Item<Job>;
template class ListIterator<Job>;
template class SimpleListIterator<JobID_t>;
template class HashTable<int,Job*>;
