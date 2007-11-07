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

/* This program calls calloc() for a random amount of memory less than or
	equal to MAX_MEM_SIZE (adjustable below) and checks to see if the
	memory is actually zeroed out. Upon analysis of the memory it then
	memset()s the memory to 42, free()s it and then starts the cycle
	over again. The reason for the memset is to pollute the memory
	space with filled memory so incase calloc happens to choose
	something and not zero it, we'll catch it a lot faster. This
	test is in response to our handling of linux malloc() which
	ends up calling mmap(that we trap and force the libc to use
	sbrk() in a recursive call to malloc() to get the new memory)
	and we hadn't been memsetting the sbrk()'ed memory to zero.
	Remember that mmaping /dev/zero means the memory should have
	been zeroed. :) -psilord 5/22/2001 */

	/* How big the largest allocable chuck can be. */
#define	MAX_MEM_SIZE  20*1024*1024
	/* Keep this a little low because the performance really sucks */
#define	MAX_TRIALS  200

int main(void)
{
	int elems;
	char *mem;
	int i;
	int j;

	/* I want the same sequence of random numbers all the time, at least
		on whatever architecture you ran this on */
	srand(0); 

	for (i = 0; i < MAX_TRIALS; i++)
	{
		/* Get me some pie! */
		elems = (rand()%(MAX_MEM_SIZE)) + 1;
		mem = (char*)calloc(elems, 1);
		if (mem == NULL)
		{
			printf("Out of memory!\n");
			printf("FAILURE\n");
			return 1;
		}
		
		/* check to make sure everything is zero */
		for (j = 0; j < elems; j++)
		{
			/* I calloc'ed it, so everything better be zero! */
			if (mem[j] != 0)
			{
				printf("Element %d of size %d is %d, but should be zero!\n", 
					j, elems, mem[j]);
				printf("FAILURE\n");
				return 1;
			}
		}
		/* set it to something nonzero to pollute the memory space during
			free()s */
		memset(mem, 42, elems);

		free(mem);
	}
	printf("SUCCESS\n");
	return 0;
}
