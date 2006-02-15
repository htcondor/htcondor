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

 

/*
** This program makes sure file opens and closes are stable around
** checkpoints (for example, the cwd doesn't change).  When running
** this test, make sure to do condor_vacates to force checkpoints.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
	FILE *fp;
	int i;
	char pathname[1024];

	for (i=0; i < 30; i++) {
		fp = fopen("output", "w");
		if (fp == NULL) {
			perror("fopen");
			getcwd(pathname, 1024);
			fprintf(stderr, "pathname = %s\n", pathname);
			abort();
		}
		sleep(5);
		fclose(fp);
	}
	fprintf(stdout, "completed successfully\n");
	return 0;
}
