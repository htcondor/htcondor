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
#include "condor_url.h"
#include "condor_debug.h"
#include "condor_regex.h"

const char* IsUrl( const char *url )
{
	if ( !url ) {
		return NULL;
	}

	if ( !isalpha( url[0] ) ) {
		return NULL;
	}

	const char *ptr = & url[1];
	while ( isalnum( *ptr ) || *ptr == '+' || *ptr == '-' || *ptr == '.' ) {
		++ptr;
	}
	// This is more restrictive than is necessary for URIs, which are not
	// required to have authority ([user@]host[:port]) sections.  It's not
	// clear from the RFC if an authority section is required for a URL,
	// but until somebody complains, it is for HTCondor.
	if ( ptr[0] == ':' && ptr[1] == '/' && ptr[2] == '/' && ptr[3] != '\0' ) {
		return ptr;
	}
	return NULL;
}

/**
 * Return the scheme from the URL string provided.
 *
 * If `scheme_suffix` is set to true, then it returns the last portion
 * of the scheme after any special character (RFC 3986 allows +, -, and .
 * in the scheme).
 *
 * With `scheme_suffix=true`, for scheme `chtc+https` would return `https`.
 */
std::string getURLType( const char *url, bool scheme_suffix ) {
	const char * endp = IsUrl(url);
	std::string scheme;
	if (endp) { // if non-null, this is a URL
		const char * startp = endp;
		if (scheme_suffix) {
			while (startp > url) {
				if (*startp == '+' || *startp == '-' || *startp == '.') {
					startp++;
					break;
				}
				startp--;
			}
		} else {
			startp = url;
		}
		scheme = std::string(startp, (int)(endp - startp));
	}
	return scheme;
}

bool ParseURL(const std::string & url,
              std::string * protocol,
              std::string * host,
              std::string * path)
{
	Regex r;
	int errCode = 0;
	int errOffset = 0;
	bool patternOK = r.compile("([^:]+)://(([^/]+)(/.*)?)", &errCode, &errOffset);
	ASSERT(patternOK);
	std::vector<std::string> groups;
	if(! r.match(url, & groups)) {
		return false;
	}

	if (protocol) {
		*protocol = groups[1];
	}
	if (host) {
		*host = groups[3];
	}
	if (path && groups.size() > 4) {
		*path = groups[4];
	}
	return true;
}

const char* UrlSafePrint(const std::string& in)
{
	const size_t buffer_sz = 2;
	static size_t buffer_idx = 0;
	static std::string buffer[buffer_sz];

	buffer_idx = (buffer_idx + 1) % buffer_sz;
	return UrlSafePrint(in, buffer[buffer_idx]);
}

const char* UrlSafePrint(const std::string& in, std::string& out)
{
	out = in;
	if (IsUrl(in.c_str())) {
		size_t question = out.find('?');
		if (question != out.npos) {
			out.replace(question, out.npos, "?...");
		}
	}
	return out.c_str();
}
