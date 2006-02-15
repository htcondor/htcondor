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
