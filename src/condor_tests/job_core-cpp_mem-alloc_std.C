/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

/* size of the memory footprint test */
#define BIGNESS (1024*1024*8)

class foo
{
	public:
		foo();
		void bar(void){printf("SUCCESS\n");};
};

foo::foo()
{
	char *space = NULL;

	space = (char*)malloc(sizeof(char*)*BIGNESS);
	if (space == NULL)
	{
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}
	memset(space, BIGNESS, 42);
	free(space);

	space = new char[BIGNESS];
	if (space == NULL)
	{
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}
	memset(space, BIGNESS, 42);
	delete [] space;
}

foo f;

int main(void)
{
	f.bar();
	return(0);
}

