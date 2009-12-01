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
#include "condor_open.h"
#include "condor_debug.h"
#include "MapFile.h"



MapFile::MapFile()
{
}


MapFile::~MapFile()
{
}


int
MapFile::ParseField(MyString & line, int offset, MyString & field)
{
	ASSERT(offset >= 0 && offset <= line.Length());

		// We consume the leading white space
	while (offset < line.Length() &&
		   (' ' == line[offset] ||
			'\t' == line[offset] ||
			'\n' == line[offset])) {
		offset++;
	}

	bool multiword = '"' == line[offset];

		// Consume initial " (quote)
	if (multiword) {
		offset++;
	}

	while (offset < line.Length()) {
		if (multiword) {
				// If we hit a " (quote) we are done, quotes in the
				// field are prefixed with a \ [don't end comments
				// with a backslash]
			if ('"' == line[offset]) {
					// We consume the trailing "
				offset++;
				break;

					// If we see a \ we either write it out or if it
					// is followed by a " we strip it and output the "
					// alone
			} else if ('\\' == line[offset] && ++offset < line.Length()) {
				if ('"' == (line[offset])) {
					field += line[offset];
				} else {
					field += '\\';
					field += line[offset];
				}
			} else {
				field += line[offset];
			}

			offset++;
		} else {
				// This field is not multiple words, so we're done
				// when we see a space
			if (' ' == line[offset] ||
				'\t' == line[offset] ||
				'\n' == line[offset]) {
					// We don't consume the tailing white space. We
					// consume leading white space
				break;
			} else {
				field += line[offset];
			}

			offset++;
		}
	}

		// NOTE: If the field has multiple words, EOL will mark the end
		// of it, even if there is no matching " (quote)
	return offset;
}

int
MapFile::ParseCanonicalizationFile(const MyString filename)
{
	int line = 0;

	FILE *file = safe_fopen_wrapper(filename.Value(), "r");
	if (NULL == file) {
		dprintf(D_ALWAYS,
				"ERROR: Could not open canonicalization file '%s' (%s)\n",
				filename.Value(),
				strerror(errno));
		return -1;
	}

    while (!feof(file)) {
		MyString input_line;
		int offset;
		MyString method;
		MyString principal;
		MyString canonicalization;

		line++;

		input_line.readLine(file); // Result ignored, we already monitor EOF

		if (input_line.IsEmpty()) {
			continue;
		}

		offset = 0;
		offset = ParseField(input_line, offset, method);
		offset = ParseField(input_line, offset, principal);
		offset = ParseField(input_line, offset, canonicalization);

		method.lower_case();

		if (method.IsEmpty() ||
			principal.IsEmpty() ||
			canonicalization.IsEmpty()) {
				dprintf(D_ALWAYS, "ERROR: Error parsing line %d of %s.  Skipping to next line.\n",
						line, filename.Value());

				continue;
		}

		dprintf(D_FULLDEBUG,
				"MapFile: Canonicalization File: method='%s' principal='%s' canonicalization='%s'\n",
				method.Value(),
				principal.Value(),
				canonicalization.Value());

/*
		Regex *re = new Regex;
		if (NULL == re) {
			dprintf(D_ALWAYS, "ERROR: Failed to allocate Regex!\n");
		}
*/
		int last = canonical_entries.getlast() + 1;
		canonical_entries[last].method = method;
		canonical_entries[last].principal = principal;
		canonical_entries[last].canonicalization = canonicalization;

		const char *errptr;
		int erroffset;
		if (!canonical_entries[last].regex.compile(principal,
												   &errptr,
												   &erroffset)) {
			dprintf(D_ALWAYS, "ERROR: Error compiling expression '%s' -- %s.  Skipping to next line.\n",
					principal.Value(),
					errptr);

			continue;
		}
	}

	fclose(file);

	return 0;
}


int
MapFile::ParseUsermapFile(const MyString filename)
{
	int line = 0;

	FILE *file = safe_fopen_wrapper(filename.Value(), "r");
	if (NULL == file) {
		dprintf(D_ALWAYS,
				"ERROR: Could not open usermap file '%s' (%s)\n",
				filename.Value(),
				strerror(errno));
		return -1;
	}

    while (!feof(file)) {
		MyString input_line;
		int offset;
		MyString canonicalization;
		MyString user;

		line++;

		input_line.readLine(file); // Result ignored, we already monitor EOF

		if (input_line.IsEmpty()) {
			continue;
		}

		offset = 0;
		offset = ParseField(input_line, offset, canonicalization);
		offset = ParseField(input_line, offset, user);

		dprintf(D_FULLDEBUG,
				"MapFile: Usermap File: canonicalization='%s' user='%s'\n",
				canonicalization.Value(),
				user.Value());

		if (canonicalization.IsEmpty() ||
			user.IsEmpty()) {
				dprintf(D_ALWAYS, "ERROR: Error parsing line %d of %s.\n",
						line, filename.Value());
				
				return line;
		}
	
		int last = user_entries.getlast() + 1;
		user_entries[last].canonicalization = canonicalization;
		user_entries[last].user = user;

		const char *errptr;
		int erroffset;
		if (!user_entries[last].regex.compile(canonicalization,
											  &errptr,
											  &erroffset)) {
			dprintf(D_ALWAYS, "ERROR: Error compiling expression '%s' -- %s\n",
					canonicalization.Value(),
					errptr);

			return line;
		}
	}

	fclose(file);

	return 0;
}


int
MapFile::GetCanonicalization(const MyString method,
							 const MyString principal,
							 MyString & canonicalization)
{
	bool match_found = false;

	for (int entry = 0;
		 !match_found && entry < canonical_entries.getlast() + 1;
		 entry++) {

//		printf("comparing: %s == %s => %d\n",
//			   method.Value(),
//			   canonical_entries[entry].method.Value(),
//			   method == canonical_entries[entry].method);
		MyString lowerMethod = method;
		lowerMethod.lower_case();
		if (lowerMethod == canonical_entries[entry].method) {
			match_found = PerformMapping(canonical_entries[entry].regex,
										 principal,
										 canonical_entries[entry].canonicalization,
										 canonicalization);

			if (match_found) break;
		}
	}

	return match_found ? 0 : -1;
}


int
MapFile::GetUser(const MyString canonicalization,
				 MyString & user)
{
	bool match_found = false;

	for (int entry = 0;
		 !match_found && entry < user_entries.getlast() + 1;
		 entry++) {
		match_found = PerformMapping(user_entries[entry].regex,
									 canonicalization,
									 user_entries[entry].user,
									 user);
	}

	return match_found ? 0 : -1;
}


bool
MapFile::PerformMapping(Regex & regex,
						const MyString input,
						const MyString pattern,
						MyString & output)
{
	ExtArray<MyString> groups;

	if (!regex.match(input, &groups)) {
		return false;
	}

	PerformSubstitution(groups, pattern, output);

	return true;
}


void
MapFile::PerformSubstitution(ExtArray<MyString> & groups,
							 const MyString pattern,
							 MyString & output)
{
	for (int index = 0; index < pattern.Length(); index++) {
		if ('\\' == pattern[index]) {
			index++;
			if (index < pattern.Length()) {
				if ('1' <= pattern[index] &&
					'9' >= pattern[index]) {
					int match = pattern[index] - '0';
					if (groups.getlast() >= match) {
						output += groups[match];
						continue;
					}
				}

				output += '\\';
			}
		}

		output += pattern[index];
	}
}
