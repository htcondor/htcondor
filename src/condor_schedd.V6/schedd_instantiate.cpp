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


#include "HashTable.h"
#include "condor_daemon_core.h"
#include "scheduler.h"
#include "proc.h"
#include "MyString.h"
#include "dedicated_scheduler.h"
#include "grid_universe.h"
#include "simplelist.h"
#include "list.h"
#ifdef HAVE_EXT_GSOAP
#  include "schedd_api.h"
#endif
#include "tdman.h"
#include "condor_crontab.h"
#include "transfer_queue.h"


class Shadow;



// for condor-G

// for MPI (or parallel) use:
// You'd think we'd need to instantiate a HashTable and HashBucket for
// <HashKey, ClassAd*> here, but those are already instantiated in
// classad_log.C in the c++_util_lib (not in c++_util_instantiate.C
// where you'd expect to find it *sigh*)

// schedd_api
#ifdef HAVE_EXT_GSOAP
#endif
