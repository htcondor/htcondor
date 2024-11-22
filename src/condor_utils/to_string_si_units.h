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

#ifndef FORMAT_BYTES_H
#define FORMAT_BYTES_H

enum class BYTES_CONVERSION_BASE : long long {
	METRIC = 1000LL,
	IEC_BINARY = 1024LL,
};

std::string to_string_byte_units(filesize_t size, BYTES_CONVERSION_BASE base=BYTES_CONVERSION_BASE::IEC_BINARY);

#endif
