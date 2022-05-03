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


#ifndef CONDOR_URL_H
#define CONDOR_URL_H

#include <string>

#include "condor_common.h"

// returns pointer to : after URL type if input is a url, returns NULL if it is not.
const char* IsUrl( const char *url );
std::string getURLType( const char *url, bool scheme_suffix );

// Return a copy of the given URL where the query component
// (the part after '?') is replaced with '...'.
// Useful when printing URLs that may contain sensitive information
// in the query component.
// The pointer returned by the first variant is valid until 2 subsequent
// calls are made. This allows inline calls in a dprintf() that prints two
// URLs that we want to redact.
// The second variant writes to 'out' and returns out.c_str().
const char* UrlSafePrint(const std::string& in);
const char* UrlSafePrint(const std::string& in, std::string& out);

#endif
