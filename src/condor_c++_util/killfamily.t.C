/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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


