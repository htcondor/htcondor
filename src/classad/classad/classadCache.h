/***************************************************************
 * 
 * Copyright 2011 Red Hat, Inc. 
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

#ifndef __CLASSAD_CACHE_H__
#define __CLASSAD_CACHE_H__

#include "classad/exprTree.h"
#include <string>

namespace classad {

class CacheEntry
{
public: 
	CacheEntry() : pData(NULL) {}
	CacheEntry(const std::string & szNameIn, const std::string & szValueIn, ExprTree * pDataIn)
		: szName(szNameIn)
		, szValue(szValueIn)
		, pData(pDataIn)
	{}

	virtual ~CacheEntry();

	std::string szName;    // string space the names.
	std::string szValue;   // reference back for cleanup
	ExprTree * pData;
};

typedef classad_weak_ptr< CacheEntry > pCacheEntry;
typedef classad_shared_ptr< CacheEntry > pCacheData;

/**
 * In order to cache elements the could not have pointers
 * to their parents as they would not match.  Hence the idea of 
 * leveraging an envelope-letter-idiom where the envelope contains
 * the pointers and through indirection operates on the letter.
 */
class CachedExprEnvelope : public ExprTree
{
public:
	virtual ~CachedExprEnvelope() {} // nothing to do counted pointer and cache entry do all of the work.

	/// node type
	virtual NodeKind GetKind (void) const { return EXPR_ENVELOPE; }

	/**
	 * cache () - will cache a copy of the pTree and return 
	 * an indirect envelope
	 */
#ifdef HAVE_COW_STRING
	static ExprTree * cache ( std::string & pName, ExprTree * pTree, const std::string & szValue );
	static ExprTree * cache_lazy ( std::string & pName, const std::string & szValue );
#else
	static ExprTree * cache (const std::string & pName, ExprTree * pTree, const std::string & szValue );
	static ExprTree * cache_lazy (const std::string & pName, const std::string & szValue );
#endif

	/**
	 * will check to see if we hit or not and return the value.
	 */ 
	static CachedExprEnvelope * check_hit (std::string & szName, const std::string & szValue);

	/**
	 * will dump the cache contents to a file.
	 */
	static bool _debug_dump_keys(const std::string & szFile);
	static void _debug_print_stats(FILE* fp);
	static bool _debug_get_counts(unsigned long &hits, unsigned long &misses, unsigned long &querys, unsigned long &hitdels, unsigned long &removals, unsigned long &unparse);
	
	ExprTree * get() const;
	const std::string & get_unparsed_str() const;

	virtual const ClassAd *GetParentScope( ) const { return( parentScope ); }

protected:
	
	virtual void _SetParentScope( const ClassAd* parent) { parentScope = parent; }
	CachedExprEnvelope() : parentScope(NULL) {;};
	
	/**
	 * SameAs() - determines if two elements are the same.
	 */
	virtual bool SameAs(const ExprTree* tree) const;
	
	/** Makes a deep copy of the expression tree
	 * 	@return A deep copy of the expression, or NULL on failure.
	 */
	virtual ExprTree *Copy( ) const;

	virtual bool _Evaluate( EvalState& st, Value& v ) const;
	virtual bool _Evaluate( EvalState& st, Value& v , ExprTree*& t) const;
	virtual bool _Flatten( EvalState& st, Value& v, ExprTree*& t, int* i )const;
	
	const ClassAd *parentScope;

	pCacheData  m_pLetter; ///< Pointer to the actual element refrenced
	
};

} // namespace classad

#endif 


