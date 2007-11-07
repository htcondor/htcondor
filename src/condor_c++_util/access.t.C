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
#include "access.h"

int main(int argc, char **argv)
{
	int result;
	char *filename;
	int uid = getuid();
	int gid = getgid();

	if(argc != 2)
	{
		fprintf(stderr, "usage: access.t filename\n");
		exit(EXIT_FAILURE);
	}
	filename = argv[1];

	config();

	printf("Attempting to ask schedd about read access to %s\n", filename);	
	result = attempt_access(argv[1], ACCESS_READ, getuid(), getgid());
	
	if( result == 0 )
	{
		printf("Schedd says it can't read that file.\n");
		exit(0);
	}
	else
	{
		printf("Schedd says it can read that file.\n");
	}
	
	printf("Attempting to ask schedd about write access to %s\n", filename);
	result = attempt_access(argv[1], ACCESS_WRITE, uid, gid);
	
	if( result == 0 )
	{
		printf("Schedd says it can't write to that file.\n");
	}
	else
	{
		printf("Schedd says it can write to that file.\n");
	}
}
