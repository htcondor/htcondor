/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "list.h"
#include "simplelist.h"
#include "extArray.h"
#include "stringSpace.h"
#include "killfamily.h"
#include "HashTable.h"
#include "condor_classad.h"
#include "ad_printmask.h"
#include "Set.h"
#include "condor_distribution.h"
#include "file_transfer.h"
#include "extra_param_info.h"
#include "daemon.h"
#include "condor_cred_base.h"
#include "condor_credential.h"

#include "passwd_cache.h"

template class List<char>; 		template class Item<char>;
template class List<int>; 		template class Item<int>;
template class List<ClassAd>;
template class List<Formatter>;
template class SimpleList<int>; 
template class SimpleList<float>;
template class ExtArray<char *>;
template class ExtArray<ProcFamily::a_pid>;
//template class HashTable<int, BaseCollection*>;
//template class HashBucket<int, BaseCollection*>;
template class Set<MyString>;
template class SetElem<MyString>;
template class Set<int>;
template class SetElem<int>;
//template class Set<RankedClassAd>;
//template class SetElem<RankedClassAd>;
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
template class SimpleList<Daemon*>;
template class HashTable<Credential_t, Condor_Credential_B*>;

int 	CondorErrno;
string	CondorErrMsg;

