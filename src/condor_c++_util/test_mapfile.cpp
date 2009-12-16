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
#include "MyString.h"
#include "MapFile.h"

#include <iostream>


/*
 * argv[1] = certificate mapfile
 * argv[2] = user mapfile
 * argv[3] = method
 * argv[4] = test string to map
 */
int
main(int argc, char **argv)
{
	MyString canonical_filename = argv[1];
	MyString user_filename = argv[2];
	MapFile map;
	int line;

		//dprintf_config("TOOL");

	if (0 != (line = map.ParseCanonicalizationFile(canonical_filename))) {
		cout << "Error parsing line " << line << " of " << canonical_filename << "." << endl;
	}

	if (0 != (line = map.ParseUsermapFile(user_filename))) {
		cout << "Error parsing line " << line << " of " << user_filename << "." << endl;
	}

	for (int index = 4; index < argc; index++) {
		MyString canonicalization;
		MyString user;

		cout << argv[index] << " -> ";

		if (map.GetCanonicalization(argv[3], argv[index], canonicalization)) {
			canonicalization = "(NO MATCH)";
		}

		cout << canonicalization << " -> ";

		if (map.GetUser(canonicalization, user)) {
			user = "(NO MATCH)";
		}

		cout << user << endl;
	}

	return 0;
}
