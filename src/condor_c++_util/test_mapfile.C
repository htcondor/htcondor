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

		//dprintf_config("TOOL", 2);

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
