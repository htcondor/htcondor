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
#include "MyString.h"
#include "HashTable.h"
#include "extArray.h"
#include "view_server.h"

#if HAVE_DLOPEN
#include "CollectorPlugin.h"
#endif

template class HashTable<MyString,ClassAd*>;
template class HashTable<MyString, GeneralRecord*>;
template class HashBucket<MyString, GeneralRecord*>;
template class ExtArray<fpos_t*>;
template class ExtArray<ExtArray<fpos_t*>*>;
template class ExtArray<ExtArray<int>*>;
template class List<ClassAd>;
template class Item<ClassAd>; 

#if HAVE_DLOPEN
template class PluginManager<CollectorPlugin>;
template class SimpleList<CollectorPlugin *>;
#endif
