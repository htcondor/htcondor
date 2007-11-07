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

#define SENTINAL 42

void foo(void)
{
	throw SENTINAL;
}

int main(void)
{
	try
	{
		foo();
	}
	catch(int m)
	{
		if (m == SENTINAL)
		{
			printf("Got correct sentinal value through c++ exception\n");
			printf("SUCCESS\n");
			exit(EXIT_SUCCESS);
		}
		printf("Got incorrect sentinal value through c++ exception\n");
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}

	printf("Should have exited with an exception, but I did not.\n");
	printf("FAILURE\n");
	exit(EXIT_FAILURE);
}
