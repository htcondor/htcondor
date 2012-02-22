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
	Create a large data file.
	Read a random selection of records from that file.
	Verify each record contains the proper data.
	Write the records in order to an output file.
	Re-read the output file to verify the data.
*/

#define RECORD_SIZE		(127)
#define RECORDS_IN_INPUT	(10000)
#define RECORDS_IN_OUTPUT	(100)

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "x_waste_second.h"


int main( int argc, char *argv[] )
{
	FILE *in, *out;
	int i,j;

	int selection[RECORDS_IN_OUTPUT];
	char buffer[RECORD_SIZE];

	srand(time(0));

	if(argc!=3) {
		fprintf(stderr,"Use: %s <input-name> <output-name>\n",argv[0]);
		exit(-1);
	}

	in = fopen(argv[1],"wb");
	if(!in) {
		perror(argv[1]);
		exit(-1);
	}

	printf("creating input records\n");
	for( i=0; i<(RECORDS_IN_INPUT*RECORD_SIZE); i++ ) {
		putc(i,in);
	}

	fclose(in);

	for( i=0; i<RECORDS_IN_OUTPUT; i++ ) {
		selection[i] = rand()%RECORDS_IN_INPUT;
	}

	in = fopen(argv[1],"rb");
	if(!in) {
		perror(argv[1]);
		exit(-1);
	}

	out = fopen(argv[2],"wb");
	if(!out) {
		perror(argv[2]);
		exit(-1);
	}

	for( i=0; i<RECORDS_IN_OUTPUT; i++ ) {
		printf("record %d:\n",selection[i]);
		printf("\treading\n");
		fseek(in,selection[i]*RECORD_SIZE,SEEK_SET);
		fread(buffer,1,RECORD_SIZE,in);
		x_waste_a_second();

		printf("\tverifying\n");
		for( j=0; j<RECORD_SIZE; j++ ) {
			if(buffer[j]!=(char)(selection[i]*RECORD_SIZE+j)) {
				printf("pos %d should be %x but is %x\n",j,selection[i]*RECORD_SIZE+j,buffer[j]);
			}
		}

		printf("\twriting\n");
		fwrite(buffer,RECORD_SIZE,1,out);
	}

	fclose(in);
	fclose(out);

	printf("verifying output records\n");

	out = fopen(argv[2],"rb");
	if(!out) {
		perror(argv[2]);
		exit(-1);
	}

	for( i=0; i<RECORDS_IN_OUTPUT; i++ ) {
		fread(buffer,1,RECORD_SIZE,out);
		printf("record %d:\n",selection[i]);
		x_waste_a_second();
		for( j=0; j<RECORD_SIZE; j++ ) {
			if(buffer[j]!=(char)(selection[i]*RECORD_SIZE+j)) {
				printf("pos %d should be %x but is %x\n",j,selection[i]*RECORD_SIZE+j,buffer[j]);
			}
		}
	}

	fclose(out);

	printf("SUCCESS\n");
	return 0;
}

