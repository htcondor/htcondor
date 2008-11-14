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


