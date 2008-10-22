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


 


#include <sys/file.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void ckpt_and_exit();
void loop(){};

int main( int argc, char **argv )
{
	int num_tests = 0;
	int i, j;
	register int k;
	char buf[ 1024 ];
	FILE	*fp;
	void *curbrk, *oldbrk;

	if( argc > 1 ) {
		num_tests = atoi( argv[1] );
	}
	if( ! num_tests ) {
		num_tests = 500;
	}


	/*
	DebugFlags = D_CKPT;
	*/

	if( (fp=fopen( "job_ckpt_combo-sanity_std.results", "w" )) == NULL ) {
		perror( "job_ckpt_combo-sanity_std.results" );
		exit( 1 );
	}

	oldbrk = sbrk(0);

	for( i = 0; i < num_tests; i++ ) {
		sprintf(buf, "set i = %d\n", i);
		if (write(2, buf, strlen(buf)) == -1)
		{
			printf("WHAT?!? Write failed on fd[%d]\n", fileno(fp));
			exit(EXIT_FAILURE);
		}

		rewind( fp );
		if (fprintf( fp, "set i = %d\n", i ) == -1)
		{
				printf("WHAT?!? fprintf failed on fd[%d]\n", fileno(fp));
				exit(EXIT_FAILURE);
		}
		fflush( fp );

		curbrk = sbrk(0);
		if( oldbrk != curbrk ) {
			printf("Current break is now 0x%x, old was 0x%x (set i = %d)\n",
				curbrk, oldbrk, i);
		}

		oldbrk = curbrk;

		for(k=j=0; j < 50000; k++,j++ );
		if( j != k ) {
			sprintf(buf, "j = %d, k (register) = %d\n", j, k);
			if (write(2, buf, strlen(buf)) == -1)
			{
				printf("WHAT?!? Write failed on fd[%d]\n", fileno(fp));
				exit(EXIT_FAILURE);
			}
		}

		ckpt_and_exit();
	}
	fclose(fp);
	exit( 0 );
}

#ifdef __cplusplus
}
#endif
