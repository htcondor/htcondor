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
CondorID::ServiceDataCompare( const ServiceData* rhs ) const
{
	CondorID const* id_lhs = (CondorID const*)this;
	CondorID const* id_rhs = (CondorID const*)rhs;
	if( !id_rhs ) {
		return -1;
	}
	
	return id_lhs->Compare( *id_rhs );
}

static unsigned int reverse_bits(unsigned int x) {
	//http://www-graphics.stanford.edu/~seander/bithacks.html#BitReverseObvious
	unsigned int y = x;
	unsigned int high_zeros=sizeof(x)*8-1;
	for(x >>= 1; x; x >>= 1) {
		y <<= 1;
		y |= x&1;
		high_zeros--;
	}
	y <<= high_zeros;
	return y;
}

size_t
CondorID::HashFn() const
{
		// Put the most variable (low) bits of _cluster and _proc
		// at opposite ends of the hash integer so they are unlikely
		// to overlap.  Put the low bits of _subproc near the center.
	size_t a = _cluster;
	size_t b = _proc;
	size_t c = _subproc;
	c = (c<<16) + (c>>(sizeof(unsigned int)*8-16));
	return a + reverse_bits(b) + c;
}
