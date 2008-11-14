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

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_classad_lookup.h"
#include "condor_query.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "ListCache.h"
#include "dc_collector.h"

static ClassAdLookupFunc lookup_func = 0;
static void *lookup_arg = 0;
static ListCache<ClassAd> *cache;

/* Please read condor_classad_lookup.h for documentation. */

void ClassAdLookupRegister( ClassAdLookupFunc func, void *arg )
{
	lookup_func = func;
	lookup_arg = arg;
}

ClassAd * ClassAdLookupGlobal( const char *constraint )
{
	CondorQuery query(ANY_AD);
	ClassAdList list;
	ClassAd *ad;
	ClassAd queryAd;

	if(lookup_func) {
		return lookup_func( constraint, lookup_arg );
	}

	if(!cache) {
		int cache_size = param_integer("CLASSAD_CACHE_SIZE",127);
		cache = new ListCache<ClassAd>(cache_size);
	}

	query.addANDConstraint(constraint);
	query.getQueryAd(queryAd);

	queryAd.SetTargetTypeName(ANY_ADTYPE);

	ad = cache->lookup_lte(&queryAd);
	if(ad) return new ClassAd(*ad);

	dprintf(D_ALWAYS,"ClassAd: Contacting collector to find \"%s\"\n",constraint);

	CollectorList * collectorList = CollectorList::create();
	if (collectorList->query (query, list) != Q_OK) {
		delete collectorList;
		dprintf(D_FULLDEBUG,"ClassAd Warning: couldn't contact collector!\n");
		return 0;
	}
	delete collectorList;

	list.Open();
	ad=list.Next();
	if(!ad) {
		dprintf(D_FULLDEBUG,"ClassAd Warning: collector has no ad matching %s\n",constraint);
		return 0;
	}

	int lifetime = param_integer("CLASSAD_CACHE_LIFETIME",300);
	cache->insert(ad,lifetime);

	return new ClassAd(*ad);
}

