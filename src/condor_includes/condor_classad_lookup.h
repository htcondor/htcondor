
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
