/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
