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
#include "proc.h"
#include "HashTable.h"
#include "list.h"
#include "simplelist.h"

#include "gridmanager.h"
#include "basejob.h"
#include "condorjob.h"
#include "condorresource.h"
#include "gahp-client.h"
#include "globusjob.h"
#include "globusresource.h"
#include "gt3job.h"
#include "gt3resource.h"
#include "gt4job.h"
#include "gt4resource.h"
#include "infnbatchjob.h"
#include "mirrorjob.h"
#include "mirrorresource.h"
/*
#include "nordugridjob.h"
#include "nordugridresource.h"

template class List<NordugridJob>;
template class Item<NordugridJob>;
template class HashTable<HashKey, NordugridResource *>;
template class HashBucket<HashKey, NordugridResource *>;
*/

#if defined(ORACLE_UNIVERSE)
#   include "oraclejob.h"
#   include "oraclereosurce.h"

    template class HashTable<HashKey, OracleJob *>;
    template class HashBucket<HashKey, OracleJob *>;
    template class List<OracleJob>;
    template class Item<OracleJob>;
#endif

#include "proxymanager.h"

template class HashTable<PROC_ID, BaseJob *>;
template class HashBucket<PROC_ID, BaseJob *>;
template class List<BaseJob>;
template class Item<BaseJob>;
template class HashTable<HashKey, BaseJob *>;
template class HashBucket<HashKey, BaseJob *>;

template class HashTable<HashKey, CondorResource *>;
template class HashBucket<HashKey, CondorResource *>;
template class List<CondorJob>;
template class Item<CondorJob>;

template class HashTable<int,GahpClient*>;
template class ExtArray<Gahp_Args*>;
template class Queue<int>;
template class HashTable<HashKey, GahpServer *>;
template class HashBucket<HashKey, GahpServer *>;
template class HashTable<HashKey, GahpProxyInfo *>;
template class HashBucket<HashKey, GahpProxyInfo *>;
template class SimpleList<PROC_ID>;
template class SimpleListIterator<PROC_ID>;
template class SimpleListIterator<int>;

template class HashTable<HashKey, GlobusJob *>;
template class HashBucket<HashKey, GlobusJob *>;
template class HashTable<HashKey, GlobusResource *>;
template class HashBucket<HashKey, GlobusResource *>;

template class HashTable<HashKey, GT3Resource *>;
template class HashBucket<HashKey, GT3Resource *>;

template class HashTable<HashKey, GT4Resource *>;
template class HashBucket<HashKey, GT4Resource *>;

template class HashTable<HashKey, MirrorJob *>;
template class HashBucket<HashKey, MirrorJob *>;
template class HashTable<HashKey, MirrorResource *>;
template class HashBucket<HashKey, MirrorResource *>;

template class HashTable<HashKey, Proxy *>;
template class HashBucket<HashKey, Proxy *>;
template class HashTable<HashKey, ProxySubject *>;
template class HashBucket<HashKey, ProxySubject *>;
template class List<Proxy>;
template class Item<Proxy>;
template class SimpleList<MyProxyEntry*>;
