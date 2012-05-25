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
#include <boost/weak_ptr.hpp>
#include <assert.h>
#include <stdio.h>
#include <list>

#ifdef WIN32
#include <process.h> 
#define _getpid getpid
#endif

using namespace classad;
using namespace std;

typedef boost::weak_ptr< classad::ExprTree > pWeakEntry;
typedef std::vector< boost::weak_ptr< classad::ExprTree > >  pCacheEntry;
typedef std::vector< boost::weak_ptr< classad::ExprTree > >::iterator entry_iterator;

#define MAX_COLLISIONS	10

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
	
#ifdef WIN32

typedef classad_hash_map<string, pCacheEntry, StringCaseIgnHash> AttrCache;
typedef classad_hash_map<string, pCacheEntry, StringCaseIgnHash>::iterator cache_iterator;
#else

typedef classad_hash_map<string, pCacheEntry, StringCaseIgnHash, CaseIgnEqStr> AttrCache;
typedef classad_hash_map<string, pCacheEntry, StringCaseIgnHash, CaseIgnEqStr>::iterator cache_iterator;

#endif

	AttrCache m_Cache;		///< Data Store
	unsigned long m_HitCount;	///< Hit Counter
	unsigned long m_MissCount;	///< Miss Counter
	unsigned long m_MissCheck;	///< Miss Counter
	unsigned long m_RemovalCount;	///< Useful to see churn
	
	///< inserts a new element
	void insert(pCacheEntry & pEntry, pCacheData& pRet)
	{

	  unsigned int iCtr=0;
	  unsigned int iSize=pEntry.size();
	  unsigned int iRemoveMe=0;
	  unsigned int iMinUseCount;
	  const ExprTree * p1, *p2;

	  p1 = pRet->self();
	  
	  if (iSize)
	  {
	    iMinUseCount = pEntry[iCtr].use_count();
	  }
	  
	  for( ;iCtr< iSize && iCtr<MAX_COLLISIONS; iCtr++)
	  {
	      if ( !(pEntry[iCtr].expired()) )
	      {
		pCacheData pTemp = pEntry[iCtr].lock();
		 
		p2 = pTemp.get();
		
		if ( p1->SameAs(p2) ) 
		{
		  pRet = pTemp;
		  m_HitCount++;
		  return;
		}
		
		if ( iSize==MAX_COLLISIONS && pEntry[iCtr].use_count() <  iMinUseCount)
		{
		  iRemoveMe=iCtr;
		  iMinUseCount=pEntry[iCtr].use_count();
		}
		
	      }
	      else
	      {
		iRemoveMe = iCtr;
		iMinUseCount=0;
	      }

	  }

	  if (iCtr<MAX_COLLISIONS)
	  {
	    pEntry.push_back(pRet);
	  }
	  else
	  {
	    // this may cause shuffling.  
	    pEntry[iRemoveMe] = pRet;
	  }
	  
	  
	  
	  m_MissCount++;
	}
	
	///< finds the 1st entry and returns used to check key_hits()
	bool find_first(pCacheEntry & pEntry, pCacheData& pRet)
	{

	  entry_iterator itr = pEntry.begin();

	  while ( itr != pEntry.end() )
	  {
	    if (!itr->expired())
	    {
	      pRet = itr->lock();
	      m_HitCount++;
	      return true;
	    }
	    else
	    {
	      itr = pEntry.erase(itr);
	    }
	  }

	  m_MissCount++;
	  return false;
	}
	

public:
	ClassAdCache()
	{ m_HitCount = 0; 
	  m_MissCount = 0;
	  m_MissCheck = 0;
	  m_RemovalCount = 0;
	};
	
	virtual ~ClassAdCache(){;};
	
	///< cache's a local attribute->ExpTree
	pCacheData cache( string & szKey, ExprTree * pVal)
	{
		pCacheData pRet(pVal);
		
		cache_iterator itr = m_Cache.find(szKey);
		
		if ( itr == m_Cache.end() ) 
		{
		  addnew (szKey, pRet);
		} 
		else 
		{
		  // b/c of copy on write semantics ensure on matches we only have the single value
		  szKey = itr->first;
		  insert( itr->second, pRet );
		}
		
		return pRet;
	}
	
	pCacheData check_hit( string & szKey, bool bReturnFirstFound )
	{
	  pCacheData pRet;

	  cache_iterator itr = m_Cache.find(szKey);
	  if ( itr != m_Cache.end() )
	  {
	    szKey = itr->first;
	    if (bReturnFirstFound &&  !find_first(itr->second, pRet) ) 
	    {
	      m_Cache.erase(itr);
	    }
	  }

	  return pRet;
	}
	
	void remove( const std::string & szKey, pCacheData pLetter )
	{
	  cache_iterator itr = m_Cache.find(szKey);
	  if ( itr != m_Cache.end() )
	  {
	    entry_iterator e_itr= itr->second.begin();
	    while ( e_itr != itr->second.end())
	    {
	      if (e_itr->expired())
	      {
		e_itr = itr->second.erase(e_itr);
		m_RemovalCount++;
	      }
	      else if (e_itr->lock() == pLetter)
	      {
		e_itr = itr->second.erase(e_itr);
		m_RemovalCount++;
		break;
	      }
	      else
	      {
		e_itr++;
	      }
	    }

	    if (!itr->second.size())
	    {
	      m_Cache.erase(itr);
	    }
	  }

	}
	
	void addnew (const std::string & szKey, pCacheData pLetter)
	{
	  pCacheEntry newEntry;
	  newEntry.push_back(pLetter);
	  m_Cache[szKey] = newEntry;
	  m_MissCount++;
	}
 
	///< dumps the contents of the cache to the file
	bool dump_keys(const string & szFile)
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
	      
	      entry_iterator e_itr= itr->second.begin();
	      while ( e_itr != itr->second.end())
	      {
		if (e_itr->expired())
		{
		  fprintf( fp, "EXPIRED ** %s\n", itr->first.c_str() );
		  e_itr = itr->second.erase(e_itr);
		  lTotalPruned++;
		}
		else
		{
		  lTotalUseCount += e_itr->use_count();
		  lEntries++;
		  std::stringstream foo;
		  foo << "["<<itr->first <<" = "<< e_itr->lock().get()<<"] - "<< e_itr->use_count();
		  // it's written directly to a file b/c it has the potential to be very large 
		  fprintf( fp, "%s\n", (foo.str()).c_str() );
		  e_itr++;
		}
	      }
	      
	      itr++;
	    }

	    // written at the end so you can tail the file.
	    fprintf( fp, "------------------------------------------------\n");
	    fprintf( fp, "ClassAdCache data for PID(%d) sizeof(%d)\n", getpid(), sizeof(CachedExprEnvelope) ); 
	    fprintf( fp, "Hits [%lu - %f] Misses[%lu - %f]\n", m_HitCount,dHitRatio,m_MissCount,dMissRatio,m_MissCheck ); 
	    fprintf( fp, "Keys[%lu] Entries[%lu] UseCount[%lu] FlushedCount[%lu]\n",m_Cache.size(),lEntries,lTotalUseCount,m_RemovalCount );
	    fprintf( fp, "Pruned[%lu] - SHOULD BE 0\n",lTotalPruned);
	    fprintf( fp, "------------------------------------------------\n");
	    fclose(fp);
	    bRet = true;

	  }

	  return (bRet);
 
	return true;
	}
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static ClassAdCache _cache;

CachedExprEnvelope::~CachedExprEnvelope()
{
  // cleans key's out of cache when ref count 
  // will hit zero.
  if ( m_pLetter.use_count() <= 1 )
  {
    _cache.remove(m_szKey, m_pLetter);
  }
}


ExprTree * CachedExprEnvelope::cache (std::string & key, ExprTree * pTree)
{
	CachedExprEnvelope * pRet=0;
	
	if (pTree->GetKind() == EXPR_ENVELOPE)
	{
	  // This exist because there are so many wonky code paths that are hard to protect against.
	  pRet = (CachedExprEnvelope *)pTree->Copy(); 
	}
	else
	{
	  pRet = new CachedExprEnvelope();
	  pRet->nodeKind = EXPR_ENVELOPE;
	  pRet->m_pLetter = _cache.cache( key, pTree);
	  pRet->m_szKey = key;
	}
	
	return ( pRet );
}

bool CachedExprEnvelope::_debug_dump_keys(const string & szFile)
{
  return _cache.dump_keys(szFile);
}

ExprTree * CachedExprEnvelope::get()
{
	ExprTree * pRet = 0;
	
	if (m_pLetter)
	{
		pRet = m_pLetter.get();
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
	pRet->m_szKey = this->m_szKey;
	
	return ( pRet );
}

const ExprTree* CachedExprEnvelope::self()
{
	return m_pLetter.get();
}

void CachedExprEnvelope::_SetParentScope( const ClassAd* )
{
	// nothing to do here already set @ base
}

bool CachedExprEnvelope::SameAs(const ExprTree* tree) const
{
	bool bRet = false;
	
	if (tree && m_pLetter)
	{
		bRet = m_pLetter->SameAs(((ExprTree*)tree)->self()) ;
	}

	return bRet;
}

bool CachedExprEnvelope::isClassad( ClassAd ** ptr )
{
	bool bRet = false;
	
	if (m_pLetter && CLASSAD_NODE == m_pLetter->nodeKind )
	{
		if (ptr)
		{
			*ptr = (ClassAd *) m_pLetter.get();
		}
		bRet = true;
	}
	
	return bRet;
}


bool CachedExprEnvelope::_Evaluate( EvalState& st, Value& v ) const
{
	bool bRet = false;
	
	if (m_pLetter)
	{
		bRet = m_pLetter->Evaluate(st,v);
	}

	return bRet;
}

bool CachedExprEnvelope::_Evaluate( EvalState& st, Value& v, ExprTree*& t) const
{
	bool bRet = false;
	
	if (m_pLetter)
	{
		bRet = m_pLetter->Evaluate(st,v,t);
	}

	return bRet;
	
}

bool CachedExprEnvelope::_Flatten( EvalState& st, Value& v, ExprTree*& t, int* i)const
{
	bool bRet = false;
	
	if (m_pLetter)
	{
		bRet = m_pLetter->Flatten(st,v,t,i);
	}

	return bRet;
	
}





