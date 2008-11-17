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
#include "proc.h"
#include "HashTable.h"
#include "list.h"
#include "simplelist.h"

#include "gridmanager.h"
#include "amazonresource.h"	// added by fangcao
#include "basejob.h"
#include "condorjob.h"
#include "condorresource.h"
#include "gahp-client.h"
#include "globusjob.h"
#include "globusresource.h"
#include "gt4job.h"
#include "gt4resource.h"
#include "infnbatchjob.h"
#include "mirrorjob.h"
#include "mirrorresource.h"
#include "nordugridjob.h"
#include "nordugridresource.h"
#include "unicorejob.h"
#include "creamjob.h"
#include "creamresource.h"


#if defined(ORACLE_UNIVERSE)
#   include "oraclejob.h"
#   include "oraclereosurce.h"

    template class HashTable<HashKey, OracleJob *>;
    template class HashBucket<HashKey, OracleJob *>;
    template class List<OracleJob>;
    template class Item<OracleJob>;
#endif

#include "proxymanager.h"










