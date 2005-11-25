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
#include "condor_daemon_core.h"
#include "KeyCache.h"
#include "string_list.h"

extern bool operator==(const struct in_addr a, const struct in_addr b);

template class HashTable<pid_t, DaemonCore::PidEntry*>;
template class HashTable<struct in_addr, int>;
template class HashTable<struct in_addr, HashTable<MyString, int> *>;
template class ExtArray<DaemonCore::SockEnt>;
template class ExtArray<DaemonCore::PipeEnt>;
template class Queue<DaemonCore::WaitpidEntry>;
template class Queue<ServiceData*>;
template class HashTable<MyString, StringList *>;
template class List<DaemonCore::TimeSkipWatcher>;
template class Item<DaemonCore::TimeSkipWatcher>;
