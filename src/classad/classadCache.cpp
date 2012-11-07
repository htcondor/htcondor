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

#include "classad/classadCache.h"
#include <assert.h>
#include <stdio.h>
#include <list>

#ifdef WIN32
#include <process.h> 
#define _getpid getpid
#endif

using namespace classad;
using namespace std;

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
    
        typedef classad_unordered<std::string, pCacheEntry, StringCaseIgnHash, CaseIgnEqStr> AttrValues;
        typedef classad_unordered<std::string, pCacheEntry, StringCaseIgnHash, CaseIgnEqStr>::iterator value_iterator;
        
	typedef classad_unordered<std::string, AttrValues, StringCaseIgnHash, CaseIgnEqStr> AttrCache;
	typedef classad_unordered<std::string, AttrValues, StringCaseIgnHash, CaseIgnEqStr>::iterator cache_iterator;

	AttrCache m_Cache;		///< Data Store
	unsigned long m_HitCount;	///< Hit Counter
	unsigned long m_MissCount;	///< Miss Counter
	unsigned long m_MissCheck;	///< Miss Counter
	unsigned long m_RemovalCount;	///< Useful to see churn
	
public:
	ClassAdCache()
	{ 
	  m_HitCount = 0; 
	  m_MissCount = 0;
	  m_MissCheck = 0;
	  m_RemovalCount = 0;
	};
	
	virtual ~ClassAdCache(){;};

	///< cache's a local attribute->ExpTree
	pCacheData cache( std::string & szName, const std::string & szValue , ExprTree * pVal)
	{
		pCacheData pRet;
		
		cache_iterator itr = m_Cache.find(szName);
                bool bValidName=false;
		
		if ( itr != m_Cache.end() ) 
		{
                  bValidName = true;
                  value_iterator vtr = itr->second.find(szValue);
                  szName = itr->first;

                  // check the value cache
                  if (vtr != itr->second.end())
                  {
                    pRet = vtr->second.lock();
                    
                    m_HitCount++;
                    if (pVal)
                    {
                        delete pVal;
                    }
                    
                    // don't to any more checks just return.
                    return pRet;
                    
                  }
                  
		} 
		
		// if we got here we missed 
                if (pVal)
                {
                    pRet.reset( new CacheEntry(szName,szValue,pVal) );

                    if (bValidName)
                    {
                        itr->second[szValue] = pRet;
                    }
                    else
                    {
                        AttrValues vCache;
                        vCache[szValue] = pRet;
                        m_Cache[szName] = vCache;
                    }

                    m_MissCount++;
                }
                else
                {
                    m_MissCheck++;
                }

		return pRet;
	}

	///< clears a cache key
	bool flush(const std::string & szName, const std::string & szValue)
	{
	  cache_iterator itr = m_Cache.find(szName);

          // remove all conditional checks because they are always true.
	  if (itr->second.size() == 1)
          {
            m_Cache.erase(itr);
          }
          else
          {
            value_iterator vtr = itr->second.find(szValue);
            itr->second.erase(vtr);
	  }

	  m_RemovalCount++;
	  return (true);
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
	    fprintf( fp, "ClassAdCache data for PID(%d)\n", getpid() ); 
	    fprintf( fp, "Hits [%lu - %f] Misses[%lu - %f] QueryMiss[%lu]\n", m_HitCount,dHitRatio,m_MissCount,dMissRatio,m_MissCheck ); 
	    fprintf( fp, "Entries[%lu] UseCount[%lu] FlushedCount[%lu]\n", lEntries,lTotalUseCount,m_RemovalCount );
	    fprintf( fp, "Pruned[%lu] - SHOULD BE 0\n",lTotalPruned);
	    fprintf( fp, "------------------------------------------------\n");
	    fclose(fp);

	    bRet = true;

	  }

	  return (bRet);
	}
};


static classad_shared_ptr<ClassAdCache> _cache;
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

CacheEntry::CacheEntry()
{
	pData=0;
}

CacheEntry::CacheEntry(const std::string & szNameIn, const std::string & szValueIn, ExprTree * pDataIn)
{
    szName = szNameIn;
    szValue = szValueIn;
    pData = pDataIn;
}

CacheEntry::~CacheEntry()
{
    if (pData)
    {
        delete pData;
        pData=0;
        _cache->flush(szName, szValue);
    }
}

CachedExprEnvelope::~CachedExprEnvelope()
{
  // nothing to do shifted to the cache entry. 
}

ExprTree * CachedExprEnvelope::cache (std::string & pName, ExprTree * pTree)
{
	ExprTree * pRet=pTree;
	string szValue;
	NodeKind nk = pTree->GetKind();
	
	switch (nk)
	{
	  case EXPR_ENVELOPE:
	     pRet = pTree;
	  break;
	  
	  case EXPR_LIST_NODE:
	  case CLASSAD_NODE:
	    // for classads the values are already cached but we still should string space the name
	    check_hit (pName, szValue);
	  break;
	    
	  default:
	  {
	    CachedExprEnvelope * pNewEnv = new CachedExprEnvelope();
	
            
	    classad::val_str(szValue, pTree); 
	
	    pNewEnv->nodeKind = EXPR_ENVELOPE;
	    if (!_cache)
	    {
	      _cache.reset( new ClassAdCache() );
	    }

	    pNewEnv->m_pLetter = _cache->cache( pName, szValue, pTree);
	    pRet = pNewEnv;
	  }
	}
	
	return ( pRet );
}

bool CachedExprEnvelope::_debug_dump_keys(const string & szFile)
{
  return _cache->dump_keys(szFile);
}

CachedExprEnvelope * CachedExprEnvelope::check_hit (string & szName, const string& szValue)
{
   CachedExprEnvelope * pRet = 0; 

   if (!_cache)
   {
	_cache.reset(new ClassAdCache());
   }

   pCacheData cache_check = _cache->cache( szName, szValue, 0);

   if (cache_check)
   {
     pRet = new CachedExprEnvelope();
     pRet->nodeKind = EXPR_ENVELOPE;
     pRet->m_pLetter = cache_check;
   }

   return pRet;
}

void CachedExprEnvelope::getAttributeName(std::string & szFillMe)
{
    if (m_pLetter)
    {
        szFillMe = m_pLetter->szName;
    }
}

ExprTree * CachedExprEnvelope::get()
{
	ExprTree * pRet = 0;
	
	if (m_pLetter)
	{
		pRet = m_pLetter->pData;
	}
	
	return ( pRet );
}

ExprTree * CachedExprEnvelope::Copy( ) const
{
	CachedExprEnvelope * pRet = new CachedExprEnvelope();
	
	// duplicate as little data as possible.
	pRet->nodeKind = EXPR_ENVELOPE;
	pRet->m_pLetter = this->m_pLetter;
	pRet->parentScope = this->parentScope;
	
	return ( pRet );
}

const ExprTree* CachedExprEnvelope::self()
{
	return m_pLetter->pData;
}

void CachedExprEnvelope::_SetParentScope( const ClassAd* )
{
	// nothing to do here already set @ base
}

bool CachedExprEnvelope::SameAs(const ExprTree* tree) const
{
	bool bRet = false;
	
	if (tree && m_pLetter && m_pLetter->pData)
	{
		bRet = m_pLetter->pData->SameAs(((ExprTree*)tree)->self()) ;
	}

	return bRet;
}

bool CachedExprEnvelope::isClassad( ClassAd ** ptr )
{
	bool bRet = false;
	
	if (m_pLetter && m_pLetter->pData && CLASSAD_NODE == m_pLetter->pData->GetKind() )
	{
		if (ptr)
		{
			*ptr = (ClassAd *) m_pLetter->pData;
		}
		bRet = true;
	}
	
	return bRet;
}


bool CachedExprEnvelope::_Evaluate( EvalState& st, Value& v ) const
{
	bool bRet = false;
	
	if (m_pLetter && m_pLetter->pData)
	{
		bRet = m_pLetter->pData->Evaluate(st,v);
	}

	return bRet;
}

bool CachedExprEnvelope::_Evaluate( EvalState& st, Value& v, ExprTree*& t) const
{
	bool bRet = false;
	
	if (m_pLetter && m_pLetter->pData)
	{
		bRet = m_pLetter->pData->Evaluate(st,v,t);
	}

	return bRet;
	
}

bool CachedExprEnvelope::_Flatten( EvalState& st, Value& v, ExprTree*& t, int* i)const
{
	bool bRet = false;
	
	if (m_pLetter && m_pLetter->pData)
	{
		bRet = m_pLetter->pData->Flatten(st,v,t,i);
	}

	return bRet;
	
}





