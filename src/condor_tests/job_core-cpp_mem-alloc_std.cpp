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

