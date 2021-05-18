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
	if ( ! StrIsProcId(str, rval.cluster, rval.proc, NULL)) {
		rval.cluster = rval.proc = -1;
	}
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

/*
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
*/

// parse a string of the form X.Y as a PROC_ID.
// return true if the input string was a valid proc id and it ended with \0 or comma or whitespace.
// a pointer to the first unparsed character is optionally returned.
// input may be X  or X.  or X.Y  if no Y is specified then proc will be set to -1
bool StrIsProcId(const char *str, int &cluster, int &proc, const char ** pend)
{
	bool valid = false;
	char * pe = const_cast<char*>(str);

	const char * p = str;
	cluster = strtol(p, &pe, 10);
	if ((pe > p) && (*pe == 0 || isspace(*pe) || *pe == ',')) {
		// if ate at least one character and ended on , space or \0, then the procid is a simple number
		// it's valid as long as it's positive.
		proc = -1;
		valid = cluster >= 0;
	} else if (*pe == '.') {
		// if we end on a . then cluster must be followed by a proc
		p = ++pe;
		proc = -1;
		if (*p == 0 || isspace(*p) || *pe == ',') {
			// ok, if the cluster is followed by a dot and nothing more
			// this parsed the same as if the dot were not there.
			valid = cluster >= 0;
		} else {
			// if we get to here, we must have a valid proc
			// but the proc is allowed to be negative - it just can't have any spaces in it.
			bool negative = false;
			if (*p == '-') { negative = true; ++p; }
			if (*p < '0' || *p > '9') {
				// if the next character isn't a digit. then this is NOT a valid procid.
				valid = false;
			} else {
				proc = strtol(p, &pe, 10);
				valid = (pe > p && (*pe == 0 || isspace(*pe)));
				if (negative) proc = -proc;
			}
		}
	}

	if (pend) *pend = pe;
	return valid;
}

void JOB_ID_KEY::sprint(std::string & s) const
{
	formatstr(s,"%d.%d", this->cluster, this->proc);
}

void JOB_ID_KEY::sprint(MyString &s) const
{
	s.formatstr("%d.%d", this->cluster, this->proc);
}

JOB_ID_KEY::operator std::string() const
{
	std::string s;
	if ( proc == -1 ) {
		// cluster ad key
		formatstr(s, "0%d.-1", this->cluster);
	} else {
		// proc ad key
		formatstr(s, "%d.%d", this->cluster, this->proc);
	}
	return s;
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
	unsigned int h2 = u2 * c1;
	h1 = rotl32(h1,15);
	h2 = rotl32(h2,15);
	h1 *= c2;
	h2 *= c2;

	// mix
	h1 = rotl32(h1,13);
	h1 = h1*5+0xe6546b64;
	h1 ^= h2;

	// finalize to force all bits to avalanche
	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;
	return h1;
}

unsigned int inline MurmurHash32x2_nofinal(unsigned int u1, unsigned int u2) {
	const unsigned int c1 = 0xcc9e2d51;
	const unsigned int c2 = 0x1b873593;

	// we interleave operations on h1 and h2 in the hope that they will parallelize
	unsigned int h1 = u1 * c1;
	unsigned int h2 = u2 * c1;
	h1 = rotl32(h1,15);
	h2 = rotl32(h2,15);
	h1 *= c2;
	h2 *= c2;

	// mix
	h1 = rotl32(h1,13);
	h1 = h1*5+0xe6546b64;
	h1 ^= h2;

	// finalize to force all bits to avalanche
	//h1 ^= h1 >> 16;
	//h1 *= 0x85ebca6b;
	//h1 ^= h1 >> 13;
	//h1 *= 0xc2b2ae35;
	//h1 ^= h1 >> 16;
	return h1;
}

#define JOB_HASH_ALGOR 2
int job_hash_algorithm = JOB_HASH_ALGOR;

#if JOB_HASH_ALGOR == 0
inline size_t hashkey_compat_hash(const char * p)
{
	size_t hash = 0;

	while (*p) {
		hash = (hash<<5) + hash + (unsigned char)*p++;
	}

	return hash;
}
#endif

size_t JOB_ID_KEY::hash(const JOB_ID_KEY &key) noexcept
{
#if JOB_HASH_ALGOR == 0
	char buf[PROC_ID_STR_BUFLEN];
	ProcIdToStr(key.cluster, key.proc, buf);
	return hashkey_compat_hash(buf);
#elif JOB_HASH_ALGOR == 1
	return key.cluster + key.proc*19;
#elif JOB_HASH_ALGOR == 2
	return key.cluster*1013 + key.proc;
#elif JOB_HASH_ALGOR == 3
	return MurmurHash32x2(key.cluster, key.proc);
#elif JOB_HASH_ALGOR == 4
	return MurmurHash32x2_nofinal(key.cluster, key.proc);
#endif
}


size_t hashFunction(const JOB_ID_KEY &key)
{
#if JOB_HASH_ALGOR == 0
	char buf[PROC_ID_STR_BUFLEN];
	ProcIdToStr(key.cluster, key.proc, buf);
	return hashkey_compat_hash(buf);
#elif JOB_HASH_ALGOR == 1
	return key.cluster + key.proc*19;
#elif JOB_HASH_ALGOR == 2
	return key.cluster*1013 + key.proc;
#elif JOB_HASH_ALGOR == 3
	return MurmurHash32x2(key.cluster, key.proc);
#elif JOB_HASH_ALGOR == 4
	return MurmurHash32x2_nofinal(key.cluster, key.proc);
#endif
}


bool operator==( const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}

// The str will be like this: "12.0,12.1,12.2,12.3...."
// The caller is responsible for freeing this memory.
std::vector<PROC_ID>*
string_to_procids(const std::string &str)
{
	StringList sl(str.c_str());
	char *s = NULL;
	std::vector<PROC_ID> *jobs = NULL;

	jobs = new std::vector<PROC_ID>;
	ASSERT(jobs);

	sl.rewind();

	while((s = sl.next()) != NULL) {
		jobs->push_back(getProcByString(s));
	}

	return jobs;
}

// convert a std::vector<PROC_ID> to a std::string suitable to construct a StringList
// out of.
void
procids_to_string(const std::vector<PROC_ID> *procids, std::string &str)
{
	str = "";

	// A null procids pretty much means an empty string list.
	if (procids == NULL) {
		return;
	}

	for(size_t i = 0; i < procids->size(); i++) {
		formatstr_cat(str, "%d.%d", (*procids)[i].cluster, (*procids)[i].proc);
		// don't put a comma on the last one.
		if (i < (procids->size() - 1)) {
			str += ",";
		}
	}
}



