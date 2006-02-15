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
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#ifdef WIN32
#define strcasecmp(s1, s2) stricmp(s1, s2)
#endif

int main() {
	char buff[500];
	char buff2[2];
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

	buff2[1] = '\n';


	do {
		count = 0;
		do {
			int c = getchar();
			sprintf (buff2, "%c", c);
			if (buff2[0] != '\n') {
				buff[count++]=buff2[0];
			}
		} while (buff2[0] != '\n');

		buff [count++] = '\0';

		printf ("\nHello, (%d) %s\n", p, buff);
		
		read (io[0], buff, 7);
		buff[7]='\0';
		printf ("From child: %s\n", buff);
		


	} while (1);

	return 0;
}
