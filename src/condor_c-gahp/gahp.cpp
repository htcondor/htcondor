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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


int main() {
	char buff[500];
	int count=0;

	int p = 0;

	int io[2];

	if (pipe (io) != 0) {
		return 1;
	}
	




	pid_t child_pid = 0;

	if ((child_pid = fork()) == 0) {
		printf ("Child\n");


		while (1) {
			sleep (1);
   			write (io[1], "<child>", 7);


		}
	}

	printf ("Child PID %d\n", child_pid);

	do {
		count = 0;
		int c;
		do {
			c = getchar();
			if (c != '\n') {
				buff[count++]=(char)c;
			}
		} while (c != '\n');

		buff [count++] = '\0';

		printf ("\nHello, (%d) %s\n", p, buff);
		
		read (io[0], buff, 7);
		buff[7]='\0';
		printf ("From child: %s\n", buff);
		


	} while (1);

	return 0;
}
