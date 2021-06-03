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

#ifndef _MAPFILE_H
#define _MAPFILE_H

#include "condor_common.h"
#include "Regex.h"
#include "extArray.h"
#include "MyString.h"

#define USE_MAPFILE_V2 1
#ifdef USE_MAPFILE_V2
#include "pool_allocator.h"
class CanonicalMapList;
typedef std::map<const YourString, CanonicalMapList*, CaseIgnLTYourString> METHOD_MAP;
#endif

typedef struct _MapFileUsage {
	int cMethods;
	int cRegex;
	int cHash;
	int cEntries;
	int cAllocations;
	int cbStrings;
	int cbStructs;
	int cbWaste;
} MapFileUsage;

class MapFile
{
 public:
	MapFile();
	~MapFile();

	int
	ParseCanonicalizationFile(const MyString filename, bool assume_hash=false, bool allow_include=true);

	int
	ParseCanonicalization(MyStringSource & src, const char* srcname, bool assume_hash=false, bool allow_include=true);

	int
	ParseUsermapFile(const MyString filename, bool assume_hash=true);

	int
	ParseUsermap(MyStringSource & src, const char * srcname, bool assume_hash=true);

	int
	GetCanonicalization(const std::string method, const std::string principal,
						std::string &canonicalization)
	{
		MyString internal_canon;
		auto retval = GetCanonicalization(MyString(method), MyString(principal), internal_canon);
		if (!retval) {
			canonicalization = internal_canon;
		}
		return retval;
	}

	int
	GetCanonicalization(const MyString method,
						const MyString principal,
						MyString & canonicalization);

	int
	GetUser(const MyString canonicalization,
			MyString & user);

#ifdef USE_MAPFILE_V2
	bool empty() { return methods.empty(); }
	void reserve(int cbReserve) { apool.reserve(cbReserve); } // reserve space in the allocation pool
	void dump(FILE* fp);
#else
	bool empty() { return canonical_entries.length() == 0 && user_entries.length() == 0; }
	void reserve(int /*cbReserve*/) { } // reserve space in the allocation pool
#endif
	int  size(MapFileUsage * pusage=NULL); // returns number of items in the map, and also usage information if pusage is non-null
	void reset(); // remove all items, but do not free memory
	void clear(); // clear all items and free memory

 private:
#ifdef USE_MAPFILE_V2
	ALLOCATION_POOL apool;
	METHOD_MAP methods;

	// find or create a CanonicalMapList for the given method.
	// use NULL as the method value for for the usermap file
	CanonicalMapList* GetMapList(const char * method);

	// add CanonicalMapEntry of type regex or hash (if regex_opts==0) to the given list
	void AddEntry(CanonicalMapList* list, int regex_opts, const char * principal, const char * canonicalization);

	bool
	FindMapping(CanonicalMapList* list,       // in: the mapping data set
				const MyString & input,         // in: the input to be matched and mapped.
				ExtArray<MyString> * groups,  // out: match groups from the input
				const char ** pcanon);        // out: canonicalization pattern

	void
	PerformSubstitution(ExtArray<MyString> & groups, // in: match gropus (usually from FindMapping)
						const char * pattern,        // in: canonicalization pattern
						MyString & output);          // out: the input pattern with groups substituted is appended to this
#else
	struct CanonicalMapEntry {
		MyString method;
		MyString principal;
		MyString canonicalization;
		Regex regex;
	};

	ExtArray<CanonicalMapEntry> canonical_entries;
	struct UserMapEntry {
		MyString canonicalization;
		MyString user;
		Regex regex;
	};

	ExtArray<UserMapEntry> user_entries;

	bool
	PerformMapping(Regex & regex,
				   const MyString input,
				   const MyString pattern,
				   MyString & output);

	void
	PerformSubstitution(ExtArray<MyString> & groups,
						const MyString pattern,
						MyString & output);
#endif

	size_t
	ParseField(const std::string & line, size_t offset, std::string & field, int * popts = NULL);
};

#endif
