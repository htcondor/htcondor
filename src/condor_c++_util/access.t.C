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
