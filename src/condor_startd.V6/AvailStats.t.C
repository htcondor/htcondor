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
#include "startd.h"

bool compute_avail_stats = true;

int
main()
{
	AvailStats as;
	State current_state = owner_state;

	printf("Enter commands: s[witch state]\n");
	printf("                c[ompute statistics]\n");
	printf("> ");

	int c;
	while ((c = getchar()) != EOF) {
		switch (c) {
		case 's': {
			if (current_state == owner_state) {
				current_state = unclaimed_state;
			} else {
				current_state = owner_state;
			}
			as.update(current_state, idle_act);
			break;
		}
		case 'c': {
			as.compute(~(A_SHARED));
			printf("%s=%0.2f\n", ATTR_AVAIL_TIME, as.avail_time() );
			printf("%s=%d\n", ATTR_LAST_AVAIL_INTERVAL,
				   as.last_avail_interval() );
			if (current_state != owner_state) {
				// only display these attributes when in non-owner state
				printf("%s=%d\n", ATTR_AVAIL_SINCE, as.avail_since());
				printf("%s=%d\n", ATTR_AVAIL_TIME_ESTIMATE,
					   as.avail_estimate() );
			}
			break;
		}
		default: {
			fprintf(stderr, "unrecognized command\n");
		}
		}
		while ((c = getchar()) != EOF && c != '\n');
		printf("> ");
	}
	printf("\n");

	return 0;
}
