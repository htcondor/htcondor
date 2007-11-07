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


/*
 * This file has been deprecated, but I'm not removing with "cvs
 * remove" for the time being.  It was written by Doug Thain for
 * research relating to his paper titled "Gathering at the Well:
 * Creating Communities for Grid I/O". It hasn't been used since, and
 * the fact that this causes ClassAds to need to talk to Daemons is
 * causing linking problems for libcondorapi.a, so we're just ditching
 * it.
 */

#if 0

#ifndef CONDOR_CLASSAD_LOOKUP_H
#define CONDOR_CLASSAD_LOOKUP_H

/*
This function searches for ClassAds in the global
scope that match the given constraint string.
By default, it calls the collector to find such entries.
The returned classad is freshly allocated, so the caller must
delete it when done.

Entries are cached for a short time to prevent collector-swamping.
The cache is controlled by two config variables:
	CLASSAD_CACHE_SIZE  (default is 127)
	CLASSAD_CACHE_LIFETIME (default is 300 seconds)
*/

ClassAd * ClassAdLookupGlobal( const char *constraint );

/*
The programmer may replace the function called to resolve
global ads by calling ClassAdLookupRegister with a pointer
to a new function that provides the same semantics as the
above.  For example, the collector cannot call itself, so
it registers its own lookup function.
*/

typedef ClassAd * (*ClassAdLookupFunc) ( const char *constraint, void *arg );

void ClassAdLookupRegister( ClassAdLookupFunc func, void *arg );

#endif
#endif
