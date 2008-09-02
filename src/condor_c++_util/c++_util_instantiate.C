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
#include "list.h"
#include "simplelist.h"
#include "extArray.h"
#include "stringSpace.h"
#include "HashTable.h"
#include "condor_classad.h"
#include "classad_collection_types.h"
#include "MyString.h"
#include "Set.h"
#include "condor_distribution.h"
#include "file_transfer.h"
#include "extra_param_info.h"
#include "daemon.h"
#include "condor_cred_base.h"
#include "condor_credential.h"
#include "condor_config.h"
#include "condor_transfer_request.h"
#include "log_transaction.h"
#include "killfamily.h"
#include "passwd_cache.h"
#include "proc_family_direct.h"

#if HAVE_EXT_CLASSADS
#include "credential.h"
#endif

template class ExtArray<ParamValue>;
template class SimpleList<ClassAd*>;
template class List<char>; 		template class Item<char>;
template class List<int>; 		template class Item<int>;
template class SimpleList<int>; 
template class SimpleList<float>;
template class ExtArray<PROC_ID>;
template class ExtArray<char *>;
template class ExtArray<int>;
template class ExtArray<MyString>;
template class ExtArray<StringSpace::SSStringEnt>;
template class ExtArray<StringSpace*>;
template class ExtArray<KillFamily::a_pid>;
template class HashTable<int, BaseCollection*>;
template class HashBucket<int, BaseCollection*>;
template class Set<MyString>;
template class SetElem<MyString>;
template class Set<int>;
template class SetElem<int>;
template class Set<RankedClassAd>;
template class SetElem<RankedClassAd>;
template class HashTable<MyString, int>;
template class HashBucket<MyString,int>;
template class HashTable<MyString, MyString>;
template class HashBucket<MyString, MyString>;
template class HashTable<MyString, KeyCacheEntry*>;
template class Queue<char *>;
template class HashTable<int, FileTransfer *>;
template class HashTable<MyString, FileTransfer *>;
template class HashTable<MyString, ExtraParamInfo *>;
template class HashTable<MyString, group_entry*>;
template class HashTable<MyString, uid_entry*>;
template class HashTable<MyString, CatalogEntry*>;
template class SimpleList<Daemon*>;
template class HashTable<Credential_t, Condor_Credential_B*>;
template class SimpleList<MyString>;
template class SimpleListIterator<MyString>;
template class List<LogRecord>;
template class Item<LogRecord>;
template class HashTable<YourSensitiveString,LogRecordList *>;
template class HashTable<pid_t, ProcFamilyDirectContainer*>;

#if HAVE_EXT_CLASSADS
template class SimpleList <Credential*>;
#endif

#if defined(Solaris)
template class ExtArray<long>;
#endif
