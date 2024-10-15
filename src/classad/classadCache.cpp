/***************************************************************
 * 
 * Copyright 2012 Red Hat, Inc. 
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

#include "classad/common.h"
#include "classad/classadCache.h"
#include "classad/sink.h"
#include "classad/source.h"
#include <assert.h>
#include <stdio.h>
#include <list>

using namespace classad;
using std::string;
using std::vector;
using std::pair;


/**
 * ClassAdCache - is meant to be the storage container which is used to cache classads,
 * I've tried some fancy tricks but they don't actually yield much better performance 
 * characteristics so I've decided to keep it simple stupid (KISS)
 * 
 * @author Timothy St. Clair
 */
class ClassAdCache
{
protected:

	typedef classad_unordered<std::string, pCacheEntry> AttrValues;
	typedef classad_unordered<std::string, pCacheEntry>::iterator value_iterator;

	typedef classad_unordered<std::string, AttrValues, ClassadAttrNameHash, CaseIgnEqStr> AttrCache;
	typedef classad_unordered<std::string, AttrValues, ClassadAttrNameHash, CaseIgnEqStr>::iterator cache_iterator;

	AttrCache m_Cache;		///< Data Store
	unsigned long m_HitCount;	///< Hit Counter
	unsigned long m_MissCount;	///< Miss Counter
	unsigned long m_QueryCount;	///< Checks that don't offer an expr-tree
	unsigned long m_HitDelete;	///< Hits that freed the incoming expr tree
	unsigned long m_RemovalCount;	///< Useful to see churn
	unsigned long m_UnparseCount; ///< number of times we had to unparse a tree to populate the cache.
	bool          m_destroyed;
	
public:
	ClassAdCache()
	: m_HitCount(0)
	, m_MissCount(0)
	, m_QueryCount(0)
	, m_HitDelete(0)
	, m_RemovalCount(0)
	, m_UnparseCount(0)
	, m_destroyed(false)
	{ 
	};
	
	virtual ~ClassAdCache(){ m_destroyed = true; };

	///< cache's a local attribute->ExpTree
	pCacheData cache(const std::string & szName, ExprTree * pTree, const std::string & szValue)
	{
		pCacheData pRet;

		cache_iterator itr = m_Cache.find(szName);
		bool bValidName=false; // cache does not have this attribute yet.

		if (itr != m_Cache.end()) {
			bValidName = true;  // cache has the attribute
			value_iterator vtr = itr->second.find(szValue);

			// check the value cache
			if (vtr != itr->second.end()) {
				pRet = vtr->second.lock();

				m_HitCount++;
				if (pTree) { // for lazy insert, pTree can be nullptr
					delete pTree;
					m_HitDelete++;
				}

				// don't to any more checks just return.
				return pRet;
			}
		}

		// if we got here we missed 
		pRet.reset( new CacheEntry(szName,szValue,pTree) );

		if (bValidName) {
			// just add value to the cache
			itr->second[szValue] = pRet;
		} else {
			// add both attribute and value to cache
			(m_Cache[szName])[szValue] = pRet;
		}

		return pRet;
	}

#if 0
	pCacheData insert_lazy(const std::string & szName, const std::string & szValue)
	{
		pCacheData pRet;

		cache_iterator itr = m_Cache.find(szName);
		bool bValidName=false;

		if (itr != m_Cache.end()) {
			bValidName = true;
			value_iterator vtr = itr->second.find(szValue);

			// check the value cache
			if (vtr != itr->second.end()) {
				pRet = vtr->second.lock();
				m_HitCount++;
				// don't to any more checks just return.
				return pRet;
			}
		}

		// if we got here we missed
		m_MissCount++;
		pRet.reset( new CacheEntry(szName,szValue,NULL) );

		if (bValidName) {
			itr->second[szValue] = pRet;
		} else {
			(m_Cache[szName])[szValue] = pRet;
		}

		return pRet;
	}
#endif

	pCacheData lookup(const std::string & szName, const std::string & szValue)
	{
		m_QueryCount++;

		cache_iterator itr = m_Cache.find(szName);
		if (itr != m_Cache.end()) {
			value_iterator vtr = itr->second.find(szValue);

			// check the value cache
			if (vtr != itr->second.end()) {
				m_HitCount++;
				// don't to any more checks just return.
				return vtr->second.lock();
			}
		}

		return nullptr;
	}

	///< clears a cache key
	bool flush(const std::string & szName, const std::string & szValue)
	{
		// this can get called after the cache has been destroyed, and that will cause an abort in MSVC11
		// and possibly other places as well.
		if (m_destroyed) return false;

		cache_iterator itr = m_Cache.find(szName);

		if (itr != m_Cache.end()) {
			if (itr->second.size() == 1) {
				m_Cache.erase(itr);
			} else {
				value_iterator vtr = itr->second.find(szValue);
				itr->second.erase(vtr);
			}

			m_RemovalCount++;
			return (true);
		}

		return false;
	} 
	
	///< dumps the contents of the cache to the file
	bool dump_keys(const std::string & szFile)
	{
	  FILE * fp = fopen ( szFile.c_str(), "a+" );
	  bool bRet = false;

	  if (fp)
	  {
	    double dHitRatio = (m_HitCount/ ((double)m_HitCount+m_MissCount))*100;
	    double dMissRatio = (m_MissCount/ ((double)m_HitCount+m_MissCount))*100;
	    unsigned long lTotalUseCount=0;
	    unsigned long lTotalPruned=0;
	    unsigned long lEntries=0;

	    cache_iterator itr = m_Cache.begin();

	    while ( itr != m_Cache.end() )
	    {
              value_iterator vtr = itr->second.begin();
              
              while (vtr != itr->second.end())
              {
                if (vtr->second.expired())
                {
                    // this should never happen.
                    fprintf( fp, "EXPIRED ** %s = %s\n", itr->first.c_str(), vtr->first.c_str() );
                    vtr = itr->second.erase(vtr);
                    lTotalPruned++;
                }
                else
                {
                    lTotalUseCount += vtr->second.use_count();
                    lEntries++;
                    
                    // it's written directly to a file b/c it has the potential to be very large 
                    fprintf( fp, "[%s = %s] - %lu\n", itr->first.c_str(), vtr->first.c_str(), vtr->second.use_count() );
                    vtr++;
                }
              }
              
              itr++;
	    }

	    // written at the end so you can tail the file.
	    fprintf( fp, "------------------------------------------------\n");
	    fprintf( fp, "ClassAdCache data for PID(%d)\n",
#ifdef WIN32
	    GetCurrentProcessId()
#else
	    getpid()
#endif
	    );
	    fprintf( fp, "Hits [%lu - %f] Misses[%lu - %f] Querys[%lu]\n", m_HitCount,dHitRatio,m_MissCount,dMissRatio,m_QueryCount ); 
	    fprintf( fp, "Entries[%lu] UseCount[%lu] FlushedCount[%lu]\n", lEntries,lTotalUseCount,m_RemovalCount );
	    fprintf( fp, "Pruned[%lu] - SHOULD BE 0\n",lTotalPruned);
	    fprintf( fp, "------------------------------------------------\n");
	    fclose(fp);

	    bRet = true;

	  }

	  return (bRet);
	}

	///< dumps the contents of the cache to the file
	void print_stats(FILE* fp)
	{
		double dHitRatio = 0.0;
		double dMissRatio = 0.0;
		unsigned long cTotalUseCount = 0;
		unsigned long cMaxUseCount = 0;
		unsigned long cTotalValues = 0;
		unsigned long cAttribs = 0;
		unsigned long cSingletonValues = 0;
		unsigned long cAttribsWithOnlySingletonValues = 0;
		unsigned long cSingletonAttribs = 0;

		if (m_HitCount+m_MissCount) {
			double dTot = m_HitCount + m_MissCount;
			dHitRatio = (100.0 * m_HitCount) / dTot;
			dMissRatio = (100.0 * m_MissCount) / dTot;
		}

		cache_iterator itr = m_Cache.begin();
		while (itr != m_Cache.end())
		{
			value_iterator vtr = itr->second.begin();

			unsigned long cValues = 0;
			unsigned long cMaxUse = 0;
			while (vtr != itr->second.end())
			{
				unsigned long cUseCount = vtr->second.use_count();
				if (cUseCount == 1) { ++cSingletonValues; }
				cTotalUseCount += cUseCount;
				if (cMaxUse < cUseCount) { cMaxUse = cUseCount; }

				++cTotalValues;
				++cValues;
				vtr++;
			}

			if (cMaxUseCount < cMaxUse) { cMaxUseCount = cMaxUse; }
			if (cMaxUse <= 1) { ++cAttribsWithOnlySingletonValues; }
			if (cValues <= 1) { ++cSingletonAttribs; }

			++cAttribs;
			itr++;
		}

		fprintf( fp, "Attribs: %lu SingleUseAttribs: %lu AttribsWithOnlySingletons: %lu\n",  cAttribs, cSingletonAttribs, cAttribsWithOnlySingletonValues);
		fprintf( fp, "Values: %lu SingleUseValues: %lu UseCountTot:%lu UseCountMax: %lu\n", cTotalValues, cSingletonValues, cTotalUseCount, cMaxUseCount);
		fprintf( fp, "Hits:%lu (%.2f%%) Misses: %lu (%.2f%%) Querys: %lu\n", m_HitCount,dHitRatio,m_MissCount,dMissRatio,m_QueryCount ); 
	};

	void get_counts(unsigned long &hits, unsigned long &misses, unsigned long &querys, unsigned long & hitdels, unsigned long &removals, unsigned long &unparse) const {
		hits = m_HitCount;
		misses = m_MissCount;
		querys = m_QueryCount;
		hitdels = m_HitDelete;
		removals = m_RemovalCount;
		unparse = m_UnparseCount;
	}
};


static classad_shared_ptr<ClassAdCache> _cache;
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

CacheEntry::~CacheEntry()
{
	if (_cache && _cache.use_count()) {
		_cache->flush(szName, szValue);
	}
	delete pData;
	pData = NULL;
}


ExprTree * CachedExprEnvelope::cache (const std::string & pName, ExprTree * pTree, const std::string & szValue)
{
	ExprTree * pRet = pTree;
	if ( ! pTree || cacheable(pTree)) {
		if ( ! _cache) { _cache.reset( new ClassAdCache() ); }
		CachedExprEnvelope * pEnv = new CachedExprEnvelope();
		pEnv->m_pLetter = _cache->cache(pName, pTree, szValue);
		pRet = pEnv;
	}
	return pRet;
}

#if 0
ExprTree * CachedExprEnvelope::cache_lazy (const std::string & pName, const std::string & szValue)
{
	if ( ! _cache) { _cache.reset( new ClassAdCache() ); }
	CachedExprEnvelope *pEnv = new CachedExprEnvelope();
	pEnv->m_pLetter = _cache->insert_lazy(pName, szValue);
	return pEnv;
}
#endif

bool CachedExprEnvelope::_debug_dump_keys(const string & szFile)
{
  if ( ! _cache) return false;
  return _cache->dump_keys(szFile);
}

bool CachedExprEnvelope::_debug_get_counts(unsigned long &hits, unsigned long &misses, unsigned long &querys, unsigned long & hitdels, unsigned long &removals, unsigned long &unparse)
{
	if ( ! _cache) return false;
	_cache->get_counts(hits, misses, querys, hitdels, removals, unparse);
	return true;
}

void CachedExprEnvelope::_debug_print_stats(FILE* fp)
{
  if (_cache) _cache->print_stats(fp);
}

CachedExprEnvelope * CachedExprEnvelope::check_hit(const string & szName, const string& szValue)
{
	if ( ! _cache) {
		// no cache make one, but don't bother with the lookup since we know it will miss.
		_cache.reset(new ClassAdCache());
	}

	pCacheData cache_check = _cache->lookup(szName, szValue);
	if (cache_check) {
		auto * pRet = new CachedExprEnvelope();
		pRet->m_pLetter = cache_check;
		return pRet;
	}

	return nullptr;
}


ExprTree * CachedExprEnvelope::get() const
{
	ExprTree * expr = NULL;
	
	if (m_pLetter) {
		CacheEntry * ptr = m_pLetter.get();
		expr = ptr->pData;
		if ( ! expr) {
			ClassAdParser parser;
			parser.SetOldClassAd(true);
			expr = parser.ParseExpression(ptr->szValue);
			ptr->pData = expr;
		}
	}
	
	return expr;
}

const std::string & CachedExprEnvelope::get_unparsed_str() const
{
	if (m_pLetter) {
		return m_pLetter->szValue;
	} else {
		static std::string errbuf("error");
		return errbuf;
	}
}


ExprTree * CachedExprEnvelope::Copy( ) const
{
	CachedExprEnvelope * pRet = new CachedExprEnvelope();
	
	// duplicate as little data as possible.
	pRet->m_pLetter = this->m_pLetter;
	
	return ( pRet );
}


// because of the lazy option, we want to try and avoid parsing
// if we can.
bool CachedExprEnvelope::SameAs(const ExprTree* tree) const
{
	if (this == tree) {
		return true;
	}

	if (tree->GetKind() != EXPR_ENVELOPE) {
		if (m_pLetter && m_pLetter->pData) {
			return m_pLetter->pData->SameAs(tree);
		}
		return false;
	}

	const CachedExprEnvelope* that = (const CachedExprEnvelope*)tree;
	if ( ! m_pLetter || ! that->m_pLetter) return false;
	if (m_pLetter == that->m_pLetter) return true;

	return get()->SameAs(tree);
}


bool CachedExprEnvelope::_Evaluate( EvalState& st, Value& v ) const
{
	ExprTree * tree = get();
	if (tree) { return tree->Evaluate(st,v); }
	return false;
}

bool CachedExprEnvelope::_Evaluate( EvalState& st, Value& v, ExprTree*& t) const
{
	ExprTree * tree = get();
	if (tree) { return tree->Evaluate(st,v,t); }
	return false;
}

bool CachedExprEnvelope::_Flatten( EvalState& st, Value& v, ExprTree*& t, int* i) const
{
	ExprTree * tree = get();
	if (tree) { return tree->Flatten(st,v,t,i); }
	return false;
}





