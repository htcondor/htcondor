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
