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
