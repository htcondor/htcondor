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

#include "condor_common.h"

#include "killfamily.h"

int
main(int argc, char *argv[])
{
	pid_t daddy_pid;
	char choice,enter;

	if ( argc != 2 ) {
		printf("Usage: %s <pid>\n",argv[0]);
		return 1;
	}

	daddy_pid = atoi(argv[1]);

	printf("Daddy Pid is %d\n",daddy_pid);

	ProcFamily family(daddy_pid,PRIV_ROOT,1);

	choice = 0;
	while ( choice != 'q' ) {

		printf("\nEnter Choice ([s]usp [r]esume [t]erm [k]ill [q]uit): ");
		scanf("%c%c",&choice,&enter);
		switch (choice) {
			case 'q':
				// fall through the outer while loop
				break;
			case 's':
				printf("Calling suspend\n");
				family.suspend();
				break;
			case 'r':
				printf("Calling resume\n");
				family.resume();
				break;
			case 't':
				printf("Calling softkill\n");
				family.softkill(SIGTERM);
				break;
			case 'k':
				printf("Calling hardkill\n");
				family.hardkill();
				break;
			default:
				printf("Unrecognized choice.\n");
				break;
		}	// end of switch
	}  // end of while

	return 0;
}


