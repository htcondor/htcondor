/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
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
#include "stl_string_utils.h"
#include "format_bytes.h"
#include <string>
#include <array>

std::string
format_bytes(filesize_t size, BYTES_CONVERSION_BASE base) {
	static const std::array<char, 4> specifiers = {'K', 'M', 'G', 'T'};

	std::string ret;

	if (size < (long long)base) {
		formatstr(ret, "%lld B", (long long)size);
	} else {
		double val = size;
		char unit = '?';
		for (const auto& specifier : specifiers) {
			unit = specifier;
			val /= (long long)base;
			if (val < (long long)base) { break; }
		}
		formatstr(ret, "%.2lf %cB", val, unit);
	}

	return ret;
}
