#include "condor_common.h"
#include "condor_distribution.h"

/* Here is the global object everyone will need when they need
   to use the distribution class.  myDistro is just a global pointer to it...
*/

Distribution	myDistribution;
Distribution	*myDistro = &myDistribution;
