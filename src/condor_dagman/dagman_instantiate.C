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


#include "condor_common.h"
#include "dag.h"
#include "job.h"
#include "script.h"
#include "MyString.h"
#include "read_multiple_logs.h"
#include "prioritysimplelist.h"
#include "binary_search.h"
#include "extArray.h"

template class List<Job>;
template class List<MyString>;
template class Item<Job>;
template class Item<MyString>;
template class ListIterator<MyString>;
template class ListIterator<Job>;
template class SimpleListIterator<JobID_t>;
template class HashTable<int, Script*>;
template class Queue<Script*>;
template class Queue<Job*>;
template class SimpleList<Job*>;
template class PrioritySimpleList<Job*>;
template class BinarySearch<int>;
CHECK_EVENTS_HASH_INSTANCE;
template class HashTable<MyString, Job *>;
template class HashTable<JobID_t, Job *>;
template class HashTable<MyString, ThrottleByCategory::ThrottleInfo *>;
template class HashTable<MyString, Dag*>;
template class ExtArray<Job*>;
