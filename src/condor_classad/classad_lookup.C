/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_classad_lookup.h"
#include "condor_query.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "ListCache.h"

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

	if(query.fetchAds(list)!=Q_OK) {
		dprintf(D_FULLDEBUG,"ClassAd Warning: couldn't contact collector!\n");
		return 0;
	}

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

