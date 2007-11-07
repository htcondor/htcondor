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
 * winmpichrun
 * This is a wrapper to run mpich2 jobs on Windows under
 * the condor parallel universe.  It simply pulls the 
 * machines out of the job classad using chirp, and 
 * calls mpirun to run the job.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int
main(int argc, char **argv) {

	char *procno_str = getenv("_CONDOR_PROCNO");

	if (procno_str == 0) {
		printf("Environment variable _CONDOR_PROCNO not set, exitting\n");
		exit(1);
	}

	int procno = -1;
	sscanf(procno_str, "%d", &procno);

	if (procno == -1) {
		printf("Can't parse_CONDOR_PROCNO as integer\n");
		exit(1);
	
	}

	// For all non-zero ranks, just sleep it out
	if (procno != 0) {
		while (1) {
			_sleep(3600 * 100);
		}
	}

	// Here we know we are rank zero
	int r = system("condor_chirp get_job_attr RemoteHosts > nodes");

	if (r != 0) {
		printf("Error running condor_chirp, exiting\n");
	}	

	// Nodes file is comma-separated, make it newline separated
	FILE *n = fopen("nodes", "r");
	FILE *m = fopen("machines", "w");

	char c;
	int count = 1;

	while (1) {
		c = fgetc(n);
		if (feof(n)) break;
		if (c == ' ') continue;
		if (c == '"') continue;
		if (c == ',') {
			c = '\n';
			count++;
		}
		fputc(c, m);
	}
	fclose(n);
	fclose(m);

	remove("nodes");

	// Now build up the mpirun command
	int command_size = 128;
	int index;
	for (index = 0; index < argc; index++) {
		command_size += strlen(argv[index]);
	}

	char *command = (char *)malloc(command_size);

	sprintf(command, "mpirun -np %d -machines machines ", count);

	int i;
	for (i = 1; i < argc; i++) {	
		strcat(command, argv[i]);
		strcat(command, " ");
	}

	r = system(command);
	remove("machines");

	exit(r);
	return 0; // keep compiler happy
}
