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
