
#ifndef METRIC_UNITS_H
#define METRIC_UNITS_H

#include "condor_common.h"

/*
Converts an integer into a string with appropriate units added.
i.e. 2096 becomes "2.0 KB", 3221225472 becomes "3.0 GB".
Returns a pointer to a static buffer.
*/

BEGIN_C_DECLS

char * metric_units( double bytes );

END_C_DECLS

#endif
