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
#include "condor_config.h"
#include "condor_transfer_request.h"
#include "log_transaction.h"
#include "killfamily.h"
#include "passwd_cache.unix.h"
#include "proc_family_direct.h"
