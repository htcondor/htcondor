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
#include "metric_units.h"

#define METRIC_UNITS_BUFFER_SIZE 80
#define METRIC_POWER_COUNT 5

extern "C" char *metric_units( double bytes )
{
	static char buffer[METRIC_UNITS_BUFFER_SIZE];
	static char *suffix[METRIC_POWER_COUNT] =
		{ "B ", "KB", "MB", "GB", "TB" };

	double value=0;
	int power=0;

	value = bytes;

	while( (value>1024.0) && (power<(METRIC_POWER_COUNT-1)) ) {
		value = value / 1024.0;
		power++;
	}

	sprintf( buffer, "%.1f %s", value, suffix[power] );

	return buffer;
}




