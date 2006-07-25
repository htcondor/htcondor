/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
						const MyString input,
						const MyString pattern,
						MyString & output);

	int
	ParseField(MyString & line, int offset, MyString & field);
};

#endif
