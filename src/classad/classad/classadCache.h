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
    CacheEntry();
    CacheEntry(const std::string & szNameIn, const std::string & szValueIn, ExprTree * pDataIn);
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
	virtual ~CachedExprEnvelope();
	
	/// node type
	virtual NodeKind GetKind (void) const { return EXPR_ENVELOPE; }

	/**
	 * cache () - will cache a copy of the pTree and return 
	 * an indirect envelope
	 */
#ifndef WIN32
	static ExprTree * cache ( std::string & pName, const std::string & szValue, ExprTree * pTree );
#else
	static ExprTree * cache (const std::string & pName, const std::string & szValue, ExprTree * pTree);
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
	
	ExprTree * get();

	/**
	 * Used to obtain the string 
	 */
	void getAttributeName(std::string & szFillMe);
	
protected:
	
	CachedExprEnvelope(){;};
	
	/**
	 * SameAs() - determines if two elements are the same.
	 */
	virtual bool SameAs(const ExprTree* tree) const;
	
	/**
	 * To eliminate the mass of external checks to see if the ExprTree is 
	 * a classad.
	 */ 
	virtual bool isClassad( ClassAd ** ptr =0 );
	
	/** Makes a deep copy of the expression tree
	 * 	@return A deep copy of the expression, or NULL on failure.
	 */
	virtual ExprTree *Copy( ) const;
	
	virtual const ExprTree* self() const;

	/* This version is for shared-library compatibility.
	 * Remove it the next time we have to bump the ClassAds SO version.
	 */
	virtual const ExprTree* self();
	
	virtual void _SetParentScope( const ClassAd* );
	virtual bool _Evaluate( EvalState& st, Value& v ) const;
	virtual bool _Evaluate( EvalState& st, Value& v , ExprTree*& t) const;
	virtual bool _Flatten( EvalState& st, Value& v, ExprTree*& t, int* i )const;
	
	pCacheData  m_pLetter; ///< Pointer to the actual element refrenced
	
};
	
}

#endif 


