#include "condor_common.h"
#include "condor_distribution.h"

/* Here is the global singleton object everyone will need when they need
	to use the distribution class. Treat myDistro like a pointer in your code.

	It is also separated into its own .o file so that it can be included both
	into daemons and distributed libraries.
*/

s_Distribution myDistro;
