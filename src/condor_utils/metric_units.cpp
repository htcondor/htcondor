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

const char *
metric_units( double bytes )
{
	static char buffer[METRIC_UNITS_BUFFER_SIZE];
	static const char *suffix[METRIC_POWER_COUNT] =
		{ "B ", "KB", "MB", "GB", "TB" };

	double value=0;
	int power=0;

	value = bytes;

	while( (value>1024.0) && (power<(METRIC_POWER_COUNT-1)) ) {
		value = value / 1024.0;
		power++;
	}

	snprintf( buffer, sizeof(buffer), "%.1f %s", value, suffix[power] );

	return buffer;
}

// This function originally lived in submit_utils.cpp and the docstring had
// the following piece of knowledge:
//
// This function exists to regularize the former ad-hoc parsing of integers in the
// submit file, it expands parsing to handle 64 bit ints and multiplier suffixes.
// Note that new classads will interpret the multiplier suffixes without
// regard for the fact that the defined units of many job ad attributes are
// in Kbytes or Mbytes. We need to parse them in submit rather than
// passing the expression on to the classad code to be parsed to preserve the
// assumption that the base units of the output is not bytes.
//
bool parse_int64_bytes(
    const char * input,
    int64_t & value,
    int base,
    char * parsed_unit /*=nullptr*/,
    const char* separators /*=nullptr*/,
    const char ** endp /*=nullptr*/)
{
        const char * tmp = input;
        while (isspace(*tmp)) ++tmp;

        char * p;
#ifdef WIN32
        int64_t val = _strtoi64(tmp, &p, 10);
#else
        int64_t val = strtol(tmp, &p, 10);
#endif

        // allow input to have a fractional part, so "2.2M" would be valid input.
        // this doesn't have to be very accurate, since we round up to base anyway.
        double fract = 0;
        if (*p == '.') {
                ++p;
                if (isdigit(*p)) { fract += (*p - '0') / 10.0; ++p; }
                if (isdigit(*p)) { fract += (*p - '0') / 100.0; ++p; }
                if (isdigit(*p)) { fract += (*p - '0') / 1000.0; ++p; }
                while (isdigit(*p)) ++p;
        }

        // if the first non-space character wasn't a number
        // then this isn't a simple integer, return false.
        if (p == tmp)
                return false;

        while (isspace(*p)) ++p;

        // parse the multiplier postfix
        int64_t mult = 1;
        if (!*p || (separators && strchr(separators, *p))) {
            mult = base;
            if (parsed_unit) { *parsed_unit = 0; }
        } else {
            if (*p == 'k' || *p == 'K') mult = 1024;
            else if (*p == 'm' || *p == 'M') mult = 1024*1024;
            else if (*p == 'g' || *p == 'G') mult = (int64_t)1024*1024*1024;
            else if (*p == 't' || *p == 'T') mult = (int64_t)1024*1024*1024*1024;
            else return false;

            if (parsed_unit) { *parsed_unit = *p; }
            ++p;
            // Tolerate a b (as in Kb) and as part of the unit
            if (*p == 'b' || *p == 'B') { ++p; }
        }

        val = (int64_t)((val + fract) * mult + base-1) / base;

        // tolerate whispace at the end or before the separator
        while (isspace(*p)) ++p;

        // If optional separator and endp are passed, the input is valid if we are on
        // a separator character now.
        if (endp && separators && strchr(separators, *p)) {
            if (endp) *endp = p;
            value = val;
            return true;
        }
        // otherwise the input is valid if we are on a \0 char
        if (!*p) {
            if (endp) *endp = p;
            value = val;
            return true;
        }

        return false;
}
