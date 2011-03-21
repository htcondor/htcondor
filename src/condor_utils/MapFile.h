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


class MapFile
{
 public:
	MapFile();
	~MapFile();

	int
	ParseCanonicalizationFile(const MyString filename);

	int
	ParseUsermapFile(const MyString filename);

	int
	GetCanonicalization(const MyString method,
						const MyString principal,
						MyString & canonicalization);

	int
	GetUser(const MyString canonicalization,
			MyString & user);

 private:
	struct CanonicalMapEntry {
		MyString method;
		MyString principal;
		MyString canonicalization;
		Regex regex;
	};

	struct UserMapEntry {
		MyString canonicalization;
		MyString user;
		Regex regex;
	};

	ExtArray<CanonicalMapEntry> canonical_entries;
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

	int
	ParseField(MyString & line, int offset, MyString & field);
};

#endif
