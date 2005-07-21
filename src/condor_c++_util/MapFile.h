#ifndef _MAPFILE_H
#define _MAPFILE_H

#include "condor_common.h"
#include "Regex.h"
#include "ExtArray.h"
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
