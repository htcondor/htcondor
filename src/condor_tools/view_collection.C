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
	char cmd[_POSIX_PATH_MAX];
	sprintf(cmd, "cp %s %s", argv[1], tmpfile);
	system(cmd);
	
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
