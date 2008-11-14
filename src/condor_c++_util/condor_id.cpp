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
#include "condor_id.h"
#include "condor_string.h"


int compare(int a, int b) {
  if (a == b) return 0;
  return (a > b ? 1 : -1);
}


int CondorID::Compare (const CondorID condorID) const {
  int result = compare (_cluster, condorID._cluster);
  if (result == 0) result = compare (_proc, condorID._proc);
  if (result == 0) result = compare (_subproc, condorID._subproc);
  return result;
}


int
CondorID::SetFromString( const char* s )
{
    if( !s ) {
        return 0;
    }
    return sscanf( s, "%d.%d.%d", &_cluster, &_proc, &_subproc );
}


int
CondorID::ServiceDataCompare( ServiceData* lhs, ServiceData* rhs )
{
	CondorID* id_lhs = (CondorID*)lhs;
	CondorID* id_rhs = (CondorID*)rhs;
	if( id_lhs && ! id_rhs ) {
		return -1;
	}
	if( ! id_lhs && ! id_rhs ) {
		return 0;
	}
	if( ! id_lhs && id_rhs ) {
		return -1;
	}
	return id_lhs->Compare( *id_rhs );
}
