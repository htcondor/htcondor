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
