
#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_classad_lookup.h"
#include "condor_query.h"
#include "condor_adtypes.h"
#include "condor_debug.h"

#include "HashCache.h"
#include "MyString.h"

static ClassAdLookupFunc lookup_func = 0;
static void *lookup_arg = 0;
static HashCache<MyString,ClassAd *> *cache;

/* Please read condor_classad_lookup.h for documentation. */

void ClassAdLookupRegister( ClassAdLookupFunc func, void *arg )
{
	lookup_func = func;
	lookup_arg = arg;
}

ClassAd * ClassAdLookupByName( const char *name )
{
	char buffer[ATTRLIST_MAX_EXPRESSION];
	CondorQuery query(ANY_AD);
	ClassAdList list;
	ClassAd *ad;
	ClassAd *evicted;

	if(lookup_func) {
		return lookup_func( name, lookup_arg );
	}

	if(!cache) {
		int cache_size = param_integer("CLASSAD_CACHE_SIZE",127);
		cache = new HashCache<MyString,ClassAd *>(cache_size,MyStringHash);
	}

	if(cache->lookup(name,ad)==0) {
		return new ClassAd(*ad);
	}

	dprintf(D_ALWAYS,"ClassAd: Contacting collector to find Name==\"%s\"\n",name);

	sprintf(buffer,"TARGET.%s == \"%s\"",ATTR_NAME,name);
	query.addANDConstraint(buffer);

	if(query.fetchAds(list)!=Q_OK) {
		dprintf(D_FULLDEBUG,"ClassAd Warning: couldn't contact collector!\n");
		return 0;
	}

	list.Open();
	ad=list.Next();
	if(!ad) {
		dprintf(D_FULLDEBUG,"ClassAd Warning: collector has no ad named %s\n",name);
		return 0;
	}

	evicted = 0;
	int lifetime = param_integer("CLASSAD_CACHE_LIFETIME",60);
	cache->insert(name,new ClassAd(*ad),lifetime,evicted);
	if(evicted) delete evicted;

	return new ClassAd(*ad);
}

