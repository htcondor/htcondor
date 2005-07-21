#include "condor_common.h"
#include "MyString.h"
#include "MapFile.h"

#include <iostream>


int
main(int argc, char **argv)
{
	MyString canonical_filename = "ctest.map";
	MyString user_filename = "utest.map";
	MapFile map;
	int line;

		//dprintf_config("TOOL", 2);

	if (0 != (line = map.ParseCanonicalizationFile(canonical_filename))) {
		cout << "Error parsing line " << line << " of " << canonical_filename << "." << endl;
	}

	if (0 != (line = map.ParseUsermapFile(user_filename))) {
		cout << "Error parsing line " << line << " of " << user_filename << "." << endl;
	}

	for (int index = 1; index < argc; index++) {
		MyString canonicalization;
		MyString user;

		cout << argv[index] << " -> ";

		if (map.GetCanonicalization("Z", argv[index], canonicalization)) {
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
