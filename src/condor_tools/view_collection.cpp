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
#include "classad_collection.h"
#include "condor_distribution.h"

/*template class Set<RankedClassAd>;*/
/*template class Set<int>;*/
/*template class HashTable<int, BaseCollection *>;*/
/*template class SetElem<RankedClassAd>;*/
/*template class Set<MyString>;*/
/*template class SetElem<MyString>;*/

int
main(int argc, char *argv[])
{
	myDistro->Init( argc, argv );
	if (argc != 2) {
		fprintf(stderr, "usage: %s collection-file\n", argv[0]);
	}

	// rename the collection file to some temporary file, so we don't
	// mess up the actual collection file
	char tmpfile[L_tmpnam];
	tmpnam(tmpfile);
	MyString cmd;
	cmd.formatstr( "cp %s %s", argv[1], tmpfile);
	system(cmd.Value());
	
	ClassAdCollection c(tmpfile);

	c.StartIterateAllCollections();

	ClassAd *ad = NULL; 
	while (c.IterateAllClassAds(ad)) {
		ad->fPrint(stdout);
		printf("\n");
	}

	unlink(tmpfile);

	return 0;
}
