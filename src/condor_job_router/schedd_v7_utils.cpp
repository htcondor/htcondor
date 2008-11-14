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
#include "condor_debug.h"
#include "schedd_v7_utils.h"
#include "HashTable.h"


unsigned int hashFuncStdString( std::string const & key)
{
    return hashFuncChars(key.c_str());
}

#if 0 /*Not currently needed*/

bool encode_classad(Stream *sock,classad::ClassAd *ad) {
	classad::ClassAdUnParser unparser;
	std::string ad_string;
	unparser.Unparse(ad_string, ad);
	return sock->put((char *)ad_string.c_str());
}

bool encode_classad_chain(Stream *sock,classad::ClassAd *ad) {
	classad::ClassAd chain;
	classad::ClassAdUnParser unparser;
	std::string ad_string;
	chain.CopyFromChain(*ad);
	unparser.Unparse(ad_string, &chain);
	return sock->put((char *)ad_string.c_str());
}

bool decode_classad(Stream *sock,classad::ClassAd *ad) {
	classad::ClassAdParser parser;
	char *ad_string=NULL;
	bool success;

	if(!sock->get(ad_string)) {
		dprintf(D_FULLDEBUG,"Failed to read ClassAd string.\n");
		return false;
	}
	ASSERT(ad_string);
	success = parser.ParseClassAd(ad_string, *ad, true);
	if(!success) {
		dprintf(D_FULLDEBUG,"Failed to parse ClassAd string: '%s'\n",ad_string);
	}
	free(ad_string);
	return success;
}

std::string
ClassAdToString(classad::ClassAd *ad) {
	classad::ClassAdUnParser unparser;
	std::string ad_string;
	unparser.Unparse(ad_string,ad);
	return ad_string;
}
#endif
