#include "condor_common.h"

#ifdef OPEN_MAX
static int open_max = OPEN_MAX;
#else
static int open_max = 0;
#endif

#define OPEN_MAX_GUESS 256

int
max_fd() {
	if( open_max == 0 ) {	/* first time */
		errno = 0;
		if( (open_max = sysconf(_SC_OPEN_MAX)) < 0 ) {
			if( errno == 0 ) {
					/* _SC_OPEN_MAX is indeterminate */
				open_max = OPEN_MAX_GUESS;
			} else {
					/* Error in sysconf */
			}
		}
	}
	return open_max;
}

