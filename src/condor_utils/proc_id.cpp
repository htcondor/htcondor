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
#include "string_list.h"
#include "extArray.h"
#include "MyString.h"

PROC_ID
getProcByString( const char* str )
{
	PROC_ID rval;
	StrToProcId(str,rval);
	return rval;
}

void
ProcIdToStr( const PROC_ID a, char *buf )
{
	ProcIdToStr( a.cluster, a.proc, buf );
}

void
ProcIdToStr( int cluster, int proc, char *buf ) {
	if ( proc == -1 ) {
		// cluster ad key
		sprintf(buf,"0%d.-1",cluster);
	} else {
		// proc ad key
		sprintf(buf,"%d.%d",cluster,proc);
	}
}

bool StrToProcId(char const *str, PROC_ID &id) {
	return StrToProcId(str,id.cluster,id.proc);
}

bool StrToProcId(char const *str, int &cluster, int &proc) {
	char const *tmp;

	// skip leading zero, if any
	if ( *str == '0' ) 
		str++;

	if ( !(tmp = strchr(str,'.')) ) {
		cluster = -1;
		proc = -1;
		return false;
	}
	tmp++;

	cluster = atoi(str);
	proc = atoi(tmp);
	return true;
}

void JOB_ID_KEY::sprint(MyString &s) const 
{
	s.formatstr("%d.%d", this->cluster, this->proc);
}

#ifdef _MSC_VER
#define rotl32(x,y) _rotl(x,y)
#define rotl64(x,y) _rotl64(x,y)
#else
// gcc will optimize this code into a single rot instruction.
inline uint32_t rotl32 ( uint32_t x, int8_t r ) { return (x << r) | (x >> (32 - r)); }
inline uint64_t rotl64 ( uint64_t x, int8_t r ) { return (x << r) | (x >> (64 - r)); }
#endif

// This is extrapolated from the 32bit Murmur3 hash from
// at http://code.google.com/p/smhasher/wiki/MurmurHash3
// 
unsigned int inline MurmurHash32x2(unsigned int u1, unsigned int u2) {
	const unsigned int c1 = 0xcc9e2d51;
	const unsigned int c2 = 0x1b873593;

	// we interleave operations on h1 and h2 in the hope that they will parallelize
	unsigned int h1 = u1 * c1;
	unsigned int h2 = u2 * c2;
	h1 = rotl32(h1,15);
	h2 = rotl32(h2,15);
	h1 *= c2;
	h2 *= c2;

	// mix 
	h1 ^= h2;

	// finalize to force all bits to avalanche
	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;
	return h1;
}

unsigned int JOB_ID_KEY::hash(const JOB_ID_KEY &key)
{
	//return key.cluster*1013 + key.proc;
	return MurmurHash32x2(key.cluster, key.proc);
}

unsigned int hashFunction(const JOB_ID_KEY &key)
{
	//return key.cluster*1013 + key.proc;
	return MurmurHash32x2(key.cluster, key.proc);
}


bool operator==( const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}

// The str will be like this: "12.0,12.1,12.2,12.3...."
// The caller is responsible for freeing this memory.
ExtArray<PROC_ID>*
mystring_to_procids(MyString &str)
{
	StringList sl(str.Value());
	char *s = NULL;
	char *t = NULL;
	ExtArray<PROC_ID> *jobs = NULL;
	int i;

	jobs = new ExtArray<PROC_ID>;
	ASSERT(jobs);

	sl.rewind();

	i = 0;
	while((s = sl.next()) != NULL) {
		// getProcByString modifies the argument in place with strtok, since
		// s is actually held in the string list, I don't want to corrupt
		// that memory, so make a copy and do my task on that instead.
		t = strdup(s);
		ASSERT(t);
		(*jobs)[i++] = getProcByString(t);
		free(t);
	}

	return jobs;
}

// convert an ExtArray<PROC_ID> to a MyString suitable to construct a StringList
// out of.
void
procids_to_mystring(ExtArray<PROC_ID> *procids, MyString &str)
{
	MyString tmp;
	int i;

	str = "";

	// A null procids pretty much means an empty string list.
	if (procids == NULL) {
		return;
	}

	for(i = 0; i < procids->length(); i++) {
		tmp.formatstr("%d.%d", (*procids)[i].cluster, (*procids)[i].proc);
		str += tmp;
		// don't put a comma on the last one.
		if (i < (procids->length() - 1)) {
			str += ",";
		}
	}
}




