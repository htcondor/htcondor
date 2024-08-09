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
#include "condor_regex.h"
#include "MyString.h"

#include "pool_allocator.h"
class CanonicalMapList;
#ifdef    DARWIN
	// I don't pretend to understand why, but on Mac, adding `const`
	// to the YourString here causes the build to fail because clang
	// can't figure out how to write the default assignment operator.
	typedef std::map<YourString, CanonicalMapList*, CaseIgnLTYourString> METHOD_MAP;
#else
	typedef std::map<const YourString, CanonicalMapList*, CaseIgnLTYourString> METHOD_MAP;
#endif /* DARWIN */

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
	ParseCanonicalizationFile(const std::string& filename, bool assume_hash=false, bool allow_include=true, bool is_prefix=false);

	int
	ParseCanonicalization(MyStringSource & src, const char* srcname, bool assume_hash=false, bool allow_include=true, bool is_prefix=false);

	int
	GetCanonicalization(const std::string& method,
	                    const std::string& principal,
	                    std::string & canonicalization);

	int
	GetUser(const std::string canonicalization,
	        std::string & user);

	bool empty() { return methods.empty(); }
	void reserve(int cbReserve) { apool.reserve(cbReserve); } // reserve space in the allocation pool
	void dump(FILE* fp);
	int  size(MapFileUsage * pusage=NULL); // returns number of items in the map, and also usage information if pusage is non-null
	void reset(); // remove all items, but do not free memory
	void clear(); // clear all items and free memory

 private:
	ALLOCATION_POOL apool;
	METHOD_MAP methods;

	// find or create a CanonicalMapList for the given method.
	// use NULL as the method value for for the usermap file
	CanonicalMapList* GetMapList(const char * method);

	// add CanonicalMapEntry of type regex or hash (if regex_opts==0) to the given list
	void AddEntry(CanonicalMapList* list, uint32_t regex_opts, const char * principal, const char * canonicalization, bool is_prefix=false);

	bool
	FindMapping(CanonicalMapList* list,       // in: the mapping data set
	            const std::string & input,         // in: the input to be matched and mapped.
	            std::vector<std::string> * groups,  // out: match groups from the input
	            const char ** pcanon);        // out: canonicalization pattern

	void
	PerformSubstitution(std::vector<std::string> & groups, // in: match gropus (usually from FindMapping)
						const char * pattern,        // in: canonicalization pattern
						std::string & output);          // out: the input pattern with groups substituted is appended to this

	size_t
	ParseField(const std::string & line, size_t offset, std::string & field, uint32_t * popts = NULL);
};

#endif
