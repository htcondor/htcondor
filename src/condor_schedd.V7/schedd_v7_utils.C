/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "schedd_v7_utils.h"
#include "HashTable.h"


int hashFuncStdString( std::string const & key, int numBuckets)
{
    return hashFuncChars(key.c_str(),numBuckets);
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
