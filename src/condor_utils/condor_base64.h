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

#ifndef CONDOR_BASE64_H
#define CONDOR_BASE64_H

// Caller needs to free the returned pointer
char* condor_base64_encode(const unsigned char *input, int length, bool include_newline=true);

// Caller needs to free *output if non-NULL
void condor_base64_decode(const char *input,unsigned char **output, int *output_length, bool require_newline=true);

#endif
