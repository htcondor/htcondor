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

#include "classad/common.h"
#include "classad/classad.h"
#include "classad/source.h"
#include "classad/sink.h"
#include "classad/classadCache.h"
#include "classad/classad_inline_value.h"
#include <limits>
#include <cmath>

extern "C" void to_lower (char *);	// from util_lib (config.c)

namespace classad {

#define ATTR_TOPLEVEL "toplevel"
#define ATTR_ROOT "root"
#define ATTR_SELF "self"
#define ATTR_PARENT "parent"
#define ATTR_MY "my"
#define ATTR_CURRENT_TIME "CurrentTime"

// This flag is only meant for use in Condor, which is transitioning
// from an older version of ClassAds with slightly different evaluation
// semantics. It will be removed without warning in a future release.
bool _useOldClassAdSemantics = false;

// Should parsed expressions be cached and shared between multiple ads.
// The default is false.
static bool doExpressionCaching = false;
size_t  _expressionCacheMinStringSize = 128;

void ClassAdSetExpressionCaching(bool do_caching) {
	doExpressionCaching = do_caching;
}

bool ClassAd::CopyExpr(const std::string& attrName, const InlineExpr& srcExpr)
{
	if (!srcExpr) return false;

	const InlineExprStorage* srcVal = srcExpr.value();

	if (!srcVal) return false;

	if (!srcVal->isInline()) {
		// Out-of-line: copy the ExprTree
		ExprTree* tree = srcVal->asPtr();
		if (tree) {
			return Insert(attrName, tree->Copy());
		}
		return false;
	}

	// Inline value: use the new Insert with InlineExpr
	return Insert(attrName, srcExpr);
}
void ClassAdSetExpressionCaching(bool do_caching, int min_string_size) {
	doExpressionCaching = do_caching;
	_expressionCacheMinStringSize = min_string_size;
}

bool ClassAdGetExpressionCaching()
{
	return doExpressionCaching;
}
bool ClassAdGetExpressionCaching(int & min_string_size) {
	min_string_size = _expressionCacheMinStringSize;
	return doExpressionCaching;
}

// This is probably not the best place to put these. However, 
// I am reconsidering how we want to do errors, and this may all
// change in any case. 
std::string CondorErrMsg;
int CondorErrno;

void ClassAdLibraryVersion(int &major, int &minor, int &patch)
{
    major = CLASSAD_VERSION_MAJOR;
    minor = CLASSAD_VERSION_MINOR;
    patch = CLASSAD_VERSION_PATCH;
    return;
}

void ClassAdLibraryVersion(std::string &version_string)
{
    version_string = CLASSAD_VERSION;
    return;
}

static inline ReferencesBySize &getSpecialAttrNames()
{
	static ReferencesBySize specialAttrNames;
	static bool specialAttrNames_inited = false;
	if ( !specialAttrNames_inited ) {
		specialAttrNames.insert( ATTR_TOPLEVEL );
		specialAttrNames.insert( ATTR_ROOT );
		specialAttrNames.insert( ATTR_SELF );
		specialAttrNames.insert( ATTR_PARENT );
		specialAttrNames_inited = true;
	}
	return specialAttrNames;
}

static FunctionCall *getCurrentTimeExpr()
{
	static classad_shared_ptr<FunctionCall> curr_time_expr;
	if ( !curr_time_expr ) {
		std::vector<ExprTree*> args;
		curr_time_expr.reset( FunctionCall::MakeFunctionCall( "time", args ) );
	}
	return curr_time_expr.get();
}

void SetOldClassAdSemantics(bool enable)
{
	_useOldClassAdSemantics = enable;
	if ( enable ) {
		getSpecialAttrNames().insert( ATTR_MY );
		getSpecialAttrNames().insert( ATTR_CURRENT_TIME );
	} else {
		getSpecialAttrNames().erase( ATTR_MY );
		getSpecialAttrNames().erase( ATTR_CURRENT_TIME );
	}
}

ClassAd::
ClassAd (const ClassAd &ad) : ExprTree()
{
    CopyFrom(ad);
	return;
}	


ClassAd &ClassAd::
operator=(const ClassAd &rhs)
{
	if (this != &rhs) {
		CopyFrom(rhs);
	}
	return *this;
}

bool ClassAd::
CopyFrom( const ClassAd &ad )
{
	ExprTree 					*tree;
	bool                        succeeded;

    succeeded = true;
	if (this == &ad) 
	{
		succeeded = false;
	} else {
		Clear( );
		
		// copy scoping attributes
		ExprTree::CopyFrom(ad);
		chained_parent_ad = ad.chained_parent_ad;
		alternateScope = ad.alternateScope;
		parentScope = ad.parentScope;
		
		this->do_dirty_tracking = false;
		for( const auto& [attr_name, attr_value] : ad.attrList ) {
			ExprTree* base = ad.attrList.materialize(attr_value);
			if( !( tree = (base ? base->Copy() : nullptr) ) ) {
				Clear( );
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
                succeeded = false;
                break;
			}
			
			Insert(attr_name, tree);
			if (ad.do_dirty_tracking && ad.IsAttributeDirty(attr_name)) {
				dirtyAttrList.insert(attr_name);
			}
		}

		do_dirty_tracking = ad.do_dirty_tracking;
	}
	return succeeded;
}

bool ClassAd::
UpdateFromChain( const ClassAd &ad )
{
	ClassAd *parent = ad.chained_parent_ad;
	if(parent) {
		if(!UpdateFromChain(*parent)) {
			return false;
		}
	}
	return Update(ad);
}

bool ClassAd::
CopyFromChain( const ClassAd &ad )
{
	if (this == &ad) return false;

	Clear( );
	ExprTree::CopyFrom(ad);
	return UpdateFromChain(ad);
}

bool ClassAd::
SameAs(const ExprTree *tree) const
{
	const ExprTree * pSelfTree = tree->self();

	if (this == pSelfTree) {
		return true;
	} else if (pSelfTree->GetKind() != CLASSAD_NODE) {
		return false;
	}

	const ClassAd *other_classad = (const ClassAd *) pSelfTree;
	if (attrList.size() != other_classad->attrList.size()) {
		return false;
	}

	auto self_itr = attrList.begin();
	auto self_end = attrList.end();
	auto other_itr = other_classad->attrList.begin();
	const InlineStringBuffer* self_buffer = attrList.stringBuffer();
	const InlineStringBuffer* other_buffer = other_classad->attrList.stringBuffer();

	for (; self_itr != self_end; ++self_itr, ++other_itr) {
		if (strcasecmp(self_itr->first.c_str(), other_itr->first.c_str()) != 0) {
			return false;
		}
		if (!self_itr->second.sameAs(other_itr->second, self_buffer, other_buffer)) {
			return false;
		}
	}

	return true;
}

bool operator==(ClassAd &list1, ClassAd &list2)
{
    return list1.SameAs(&list2);
}

ClassAd::
~ClassAd ()
{
	Clear( );
}


void ClassAd::
Clear( )
{
	Unchain();
	attrList.clear( );
}

void ClassAd::
GetComponents( std::vector<std::pair<std::string, ExprTree* > > &attrs ) const
{
	attrs.clear( );
	for( const auto& [attr_name, inlineVal] : attrList ) {
		attrs.emplace_back(attr_name, attrList.materialize(inlineVal));
	}
}

void ClassAd::
GetComponents(std::vector< std::pair< std::string, ExprTree* > > &attrs,
	const References &whitelist ) const
{
	attrs.clear( );
	for ( const auto& attr_name : whitelist ) {
		AttrList::const_iterator attr_itr = attrList.find( attr_name );
		if ( attr_itr != attrList.end() ) {
			const auto& [found_name, inlineVal] = *attr_itr;
			attrs.emplace_back( found_name, attrList.materialize(inlineVal) );
		}
	}
}


// --- begin helper methods for InsertAttr ---
bool ClassAd::
_InsertAttrInteger(const std::string& name, long long value)
{
	MarkAttributeDirty(name);

	// OPTIMIZATION: Try to store as inline value if possible
	if (value >= std::numeric_limits<int32_t>::min() &&
		value <= std::numeric_limits<int32_t>::max()) {
		attrList.setInt(name, static_cast<int32_t>(value));
		return true;
	}
	// Fall back to literal
	ExprTree* plit = Literal::MakeInteger(value);
	return Insert(name, plit);
}

bool ClassAd::
_InsertAttrReal(const std::string& name, double value)
{
	MarkAttributeDirty(name);

	// OPTIMIZATION: Store float32 as inline value when possible
	float f = static_cast<float>(value);
	// Check if conversion is reasonably safe (not infinity, NaN, etc.)
	if (std::isfinite(f)) {
		attrList.setFloat(name, f);
		return true;
	}
	// Fall back to literal for special values like infinity or NaN
	ExprTree* plit = Literal::MakeReal(value);
	return Insert(name, plit);
}

bool ClassAd::
_InsertAttrBool(const std::string& name, bool value)
{
	MarkAttributeDirty(name);

	// OPTIMIZATION: Store bool as inline value
	attrList.setBool(name, value);
	return true;
}

bool ClassAd::
_InsertAttrString(const std::string& name, const char* str, size_t len)
{
	MarkAttributeDirty(name);

	if (!str) {
		// Store undefined as inline value
		attrList.setUndefined(name);
		return true;
	}

	// OPTIMIZATION: Try to store string as inline value
	if (len <= 0xFFFFFF) {  // 24-bit size limit
		attrList.setString(name, std::string(str, len));
		return true;
	}

	// Fall back to literal
	ExprTree* plit = Literal::MakeString(str, len);
	return Insert(name, plit);
}
// --- end helper methods for InsertAttr ---


// --- begin integer attribute insertion ----
bool ClassAd::
InsertAttr( const std::string &name, int value )
{
	return _InsertAttrInteger(name, value);
}


bool ClassAd::
InsertAttr( const std::string &name, long value )
{
	return _InsertAttrInteger(name, value);
}


bool ClassAd::
InsertAttr( const std::string &name, long long value )
{
	return _InsertAttrInteger(name, value);
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const std::string &name, int value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const std::string &name, long value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const std::string &name, long long value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end integer attribute insertion ---



// --- begin real attribute insertion ---
bool ClassAd::
InsertAttr( const std::string &name, double value )
{
	return _InsertAttrReal(name, value);
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const std::string &name, double value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end real attribute insertion



// --- begin boolean attribute insertion
bool ClassAd::
InsertAttr( const std::string &name, bool value )
{
	return _InsertAttrBool(name, value);
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const std::string &name, bool value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end boolean attribute insertion



// --- begin std::string attribute insertion
bool ClassAd::
InsertAttr( const std::string &name, const char *value )
{
	if (!value) {
		return _InsertAttrString(name, nullptr, 0);
	}
	size_t len = std::strlen(value);
	return _InsertAttrString(name, value, len);
}

bool ClassAd::
InsertAttr( const std::string &name, const char * str, size_t len)
{
	return _InsertAttrString(name, str, len);
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const std::string &name, const char *value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}

bool ClassAd::
InsertAttr( const std::string &name, const std::string &value )
{
	return _InsertAttrString(name, value.c_str(), value.size());
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const std::string &name, const std::string &value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end std::string attribute insertion

#if 0
// disabled, compat layer handles this. use the 3 argument form below to insert through the classad cache.
bool ClassAd::Insert(const std::string& serialized_nvp);
#endif

// Parse and insert an attribute value via cache if the cache is enabled
//
bool ClassAd::InsertViaCache(const std::string& name, const std::string & rhs, bool lazy /*=false*/)
{
	if (name.empty()) return false;

	// use cache if it is enabled, and the attribute name is not 'special' (i.e. doesn't start with a quote)
	bool use_cache = doExpressionCaching;
	if (name[0] == '\'' || ! CachedExprEnvelope::cacheable(rhs)) {
		use_cache = false;
	}

	// check the cache to see if we already have an expr tree, if we do then we
	// get back a new envelope node, which we can just insert into the ad.
	ExprTree * tree = NULL;
	if (use_cache) {
		CachedExprEnvelope * penv = CachedExprEnvelope::check_hit(name, rhs);
		if (penv) {
			tree = penv;
			return Insert(name, tree);
		}
		if (lazy) {
			tree = CachedExprEnvelope::cache_lazy(name, rhs);
			return Insert(name, tree);
		}
	}

	// we did not use the cache, or get a hit in the cache... parse the expression
	ClassAdParser parser;
	parser.SetOldClassAd(true);
	tree = parser.ParseExpression(rhs);
	if ( ! tree) {
		return false;
	}

	// if caching is enabled, and we got to here then we know that the
	// cache doesn't already have an entry for this name:value, so add
	// it to the cache now.
	if (use_cache) {
		tree = CachedExprEnvelope::cache(name, tree, rhs);
	}
	return Insert(name, tree);
}

bool
ClassAd::Insert(const std::string &str)
{
	// this is not the optimial path, it would be better to
	// use either the 2 argument insert, or the const char* form below
	return this->Insert(str.c_str());
}

bool
ClassAd::Insert( const char *str )
{
	std::string attr;
	const char * rhs;

	while (isspace(*str)) ++str;

	// We don't support quoted attribute names in this old ClassAds method.
	if ( *str == '\'' ) {
		return false;
	}

	const char * peq = strchr(str, '=');
	if ( ! peq) return false;

	const char * p = peq;
	while (p > str && ' ' == p[-1]) --p;
	attr.assign(str, p-str);

	if ( attr.empty() ) {
		return false;
	}

	// set rhs to the first non-space character after the =
	p = peq+1;
	while (' ' == *p) ++p;
	rhs = p;

	return InsertViaCache(attr, rhs);
}

bool ClassAd::
AssignExpr(const std::string &name, const char *value)
{
	ClassAdParser par;
	ExprTree *expr = NULL;
	par.SetOldClassAd( true );

	if ( value == NULL ) {
		return false;
	}
	expr = par.ParseExpression(value, true);
	if ( !expr ) {
		return false;
	}
	if ( !Insert( name, expr ) ) {
		delete expr;
		return false;
	}
	return true;
}

bool ClassAd::Insert( const std::string& attrName, ExprTree * tree )
{
		// sanity checks
	if( attrName.empty() ) {
		CondorErrno = ERR_MISSING_ATTRNAME;
		CondorErrMsg= "no attribute name when inserting expression in classad";
		return false;
	}
	if( !tree ) {
		CondorErrno = ERR_BAD_EXPRESSION;
		CondorErrMsg = "no expression when inserting attribute in classad";
		return false;
	}

	// parent of the expression is this classad
	tree->SetParentScope( this );

	auto [itr, inserted] = attrList.emplace(attrName, tree);
	if (!inserted) {
		// replace existing value
		itr->second.release();
		itr->second = InlineExprStorage(tree);
	}

	MarkAttributeDirty(attrName);

	return true;
}

bool ClassAd::Insert( const std::string& attrName, const InlineExpr& inlineExpr )
{
	// sanity checks
	if( attrName.empty() ) {
		CondorErrno = ERR_MISSING_ATTRNAME;
		CondorErrMsg= "no attribute name when inserting InlineValue in classad";
		return false;
	}

	if (!inlineExpr) {
		CondorErrno = ERR_BAD_EXPRESSION;
		CondorErrMsg = "no InlineValue when inserting attribute in classad";
		return false;
	}

	const InlineExprStorage* value = inlineExpr.value();
	const ClassAdFlatMap* sourceMap = inlineExpr.attrList();

	// If it's an out-of-line ExprTree*, set parent scope
	if (!value->isInline()) {
		ExprTree* tree = value->asPtr();
		if (tree) {
			tree->SetParentScope( this );
		}
	} else if (value->getInlineType() == 5) {
		// For inline string refs, materialize from source and insert as new string
		if (!sourceMap) {
			CondorErrno = ERR_BAD_EXPRESSION;
			CondorErrMsg = "cannot insert inline string reference without source ClassAdFlatMap";
			return false;
		}
		// Materialize string from source buffer
		uint32_t offset, size;
		value->getStringRef(offset, size);
		const InlineStringBuffer* srcBuffer = sourceMap->stringBuffer();
		if (!srcBuffer) {
			CondorErrno = ERR_BAD_EXPRESSION;
			CondorErrMsg = "source ClassAdFlatMap has no string buffer";
			return false;
		}
		std::string str = srcBuffer->getString(offset, size);
		// Add to our string buffer
		attrList.setString(attrName, str);
		MarkAttributeDirty(attrName);
		return true;
	}

	auto [itr, inserted] = attrList.emplace(attrName, nullptr);
	if (!inserted) {
		// replace existing value
		itr->second.release();
	}
	itr->second = InlineExprStorage(value->toBits());

	MarkAttributeDirty(attrName);

	return true;
}

bool ClassAd::CopyExprFrom( const std::string& attrName, const ClassAd& source, const std::string& sourceAttr )
{
	// Get the value from source using LookupInline - returns InlineExpr
	InlineExpr srcExpr = source.LookupInline<std::string>(sourceAttr);

	// Check if attribute was found
	if (!srcExpr) {
		return false;
	}

	const InlineExprStorage* srcVal = srcExpr.value();

	// For out-of-line values, copy the tree; for inline values, use Insert with InlineExpr
	if (!srcVal->isInline()) {
		// Out-of-line: copy the tree
		ExprTree* tree = srcVal->asPtr();
		if (tree) {
			return Insert(attrName, tree->Copy());
		} else {
			return false;
		}
	} else {
		// Inline value: use the new Insert with InlineExpr
		return Insert(attrName, srcExpr);
	}
}

bool ClassAd::Swap(const std::string& attrName, ExprTree* tree, ExprTree* & old_tree)
{
	// sanity checks
	if( attrName.empty() ) {
		CondorErrno = ERR_MISSING_ATTRNAME;
		CondorErrMsg= "no attribute name when inserting expression in classad";
		return false;
	}
	if( !tree ) {
		CondorErrno = ERR_BAD_EXPRESSION;
		CondorErrMsg = "no expression when inserting attribute in classad";
		return false;
	}

	// parent of the expression is this classad
	tree->SetParentScope( this );

	auto [itr, inserted] = attrList.emplace(attrName, tree);
	if (!inserted) {
		// replace existing value
		// hand ownership of the old tree to the caller (do NOT delete it here)
		old_tree = attrList.materialize(itr->second);
		// clear without deleting; we'll overwrite next
		itr->second.setBits(0);
		// store the new value
		itr->second = InlineExprStorage(tree);
	} else {
		old_tree = nullptr;
	}

	MarkAttributeDirty(attrName);

	return true;
}


// Optimized code for inserting literals, use when the caller has guaranteed the validity
// of the name and literal and wants a fast code path for insertion.  This function ALWAYS
// bypasses the classad cache, which is fine for numeral literals, but probably a bad idea
// for large std::string literals.
//
bool ClassAd::InsertLiteral(const std::string & name, Literal* lit)
{
	auto [itr, inserted] = attrList.emplace(name, lit);

	if(!inserted) {
		// replace existing value
		itr->second.release();
		itr->second = InlineExprStorage(lit);
	}

	MarkAttributeDirty(name);
	return true;
}


bool ClassAd::
DeepInsert( ExprTree *scopeExpr, const std::string &name, ExprTree *tree )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->Insert( name, tree ) );
}
// --- end expression insertion

// --- begin STL-like functions
ClassAd::iterator ClassAd::
find(std::string const& attrName)
{
    return iterator(attrList.find(attrName), &attrList);
}
 
ClassAd::const_iterator ClassAd::
find(std::string const& attrName) const
{
    return const_iterator(attrList.find(attrName), &attrList);
}
// --- end STL-like functions

// --- begin lookup methods

ExprTree *ClassAd::
LookupIgnoreChain( const std::string &name ) const
{
	ExprTree *tree;
	AttrList::const_iterator itr;

	itr = attrList.find( name );
	if (itr != attrList.end()) {
		tree = attrList.materialize(itr);
	} else {
		tree = NULL;
	}
	return tree;
}

ExprTree *ClassAd::
LookupInScope( const std::string &name, const ClassAd *&finalScope ) const
{
	EvalState	state;
	InlineExpr inlineExpr;
	InlineExprStorage scratch(nullptr);
	int			rval;

	state.SetScopes( this );
	rval = LookupInScope( name, inlineExpr, scratch, state );
	if( rval == EVAL_OK && inlineExpr ) {
		finalScope = state.curAd;
		return inlineExpr.materialize();
	}

	finalScope = NULL;
	return( NULL );
}


int ClassAd::
LookupInScope(const std::string &name, InlineExpr& result, InlineExprStorage& scratch, EvalState &state) const
{
	const ClassAd *current = this, *superScope;

	result = InlineExpr();  // Initialize to empty

	while( !result && current ) {

		// lookups/eval's being done in the 'current' ad
		state.curAd = current;

		// lookup in current scope
		AttrList::const_iterator itr = current->attrList.find( name );
		if (itr != current->attrList.end()) {
			if (itr->second.isInline() || itr->second.asPtr() != nullptr) {
				result = InlineExpr(itr->second, &current->attrList);
				return EVAL_OK;
			}
		}
		classad::ClassAdUnParser unp;
		std::string ad_str;
		unp.Unparse(ad_str, current);

		// chained parent fallback (only from the starting ad)
		if (current->chained_parent_ad) {
			auto parent = current->chained_parent_ad;
			while (parent) {
				AttrList::const_iterator pitr = parent->attrList.find(name);
				if (pitr != parent->attrList.end()) {
					if (pitr->second.isInline() || pitr->second.asPtr() != nullptr) {
						result = InlineExpr(pitr->second, &parent->attrList);
						return EVAL_OK;
					}
				}
				if (parent->chained_parent_ad) {
					parent = parent->chained_parent_ad;
				} else {
					break;
				}
			}
		}

		if ( state.rootAd == current ) {
			superScope = NULL;
		} else {
			superScope = current->parentScope;
		}
		if ( getSpecialAttrNames().find(name) == getSpecialAttrNames().end() ) {
			// continue searching from the superScope ...
			current = superScope;
			if( current == this ) {		// NAC - simple loop checker
				return( EVAL_UNDEF );
			}
		} else if(strcasecmp(name.c_str( ),ATTR_TOPLEVEL)==0 ||
				strcasecmp(name.c_str( ),ATTR_ROOT)==0){
			// if the "toplevel" attribute was requested ...
			scratch.updatePtr(const_cast<ClassAd *>((const ClassAd *)state.rootAd));
			result = InlineExpr(scratch, nullptr);
			if( scratch.asPtr() == nullptr ) {	// NAC - circularity so no root
				return EVAL_FAIL;  	// NAC
			}						// NAC
			return( scratch.asPtr() ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), ATTR_SELF ) == 0 ||
				   strcasecmp( name.c_str( ), ATTR_MY ) == 0 ) {
			// if the "self" ad was requested
			scratch.updatePtr(const_cast<ClassAd *>((const ClassAd *)state.curAd));
			result = InlineExpr(scratch, nullptr);
			return( scratch.asPtr() ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), ATTR_PARENT ) == 0 ) {
			// the lexical parent
			scratch.updatePtr(const_cast<ClassAd *>((const ClassAd*)superScope));
			result = InlineExpr(scratch, nullptr);
			return( scratch.asPtr() ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), ATTR_CURRENT_TIME ) == 0 ) {
			// an alias for time() from old ClassAds
			scratch.updatePtr(getCurrentTimeExpr());
			result = InlineExpr(scratch, nullptr);
			return ( scratch.asPtr() ? EVAL_OK : EVAL_UNDEF );
		}

	}	

	return( EVAL_UNDEF );
}
// --- end lookup methods



// --- begin deletion methods 
bool ClassAd::
Delete( const std::string &name )
{
	bool deleted_attribute;

    deleted_attribute = false;
	AttrList::iterator itr = attrList.find( name );
	if( itr != attrList.end( ) ) {
		attrList.erase( itr );
		deleted_attribute = true;
	}
	// If the attribute is in the chained parent, we delete define it
	// here as undefined, whether or not it was defined here.  This is
	// behavior copied from old ClassAds. It's also one reason you
	// probably don't want to use this feature in the future.
	if (chained_parent_ad != NULL &&
		chained_parent_ad->Lookup(name) != NULL) {
		
		deleted_attribute = true;
	
		attrList.setUndefined(name);
	}

	if (!deleted_attribute) {
		CondorErrno = ERR_MISSING_ATTRIBUTE;
		CondorErrMsg = "attribute " + name + " not found to be deleted";
	}

	return deleted_attribute;
}

bool ClassAd::
DeepDelete( ExprTree *scopeExpr, const std::string &name )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->Delete( name ) );
}
// --- end deletion methods



// --- begin removal methods
ExprTree *ClassAd::
Remove( const std::string &name )
{
	ExprTree *tree = NULL;
	AttrList::iterator itr = attrList.find( name );
	if( itr != attrList.end( ) ) {
		// Sadly, we have to materialize due to the return value.
		tree = attrList.materialize(itr);
		// Clear the slot before erasing to avoid double-free
		itr->second = InlineExprStorage(nullptr);
		attrList.erase( itr );
		if (tree && dynamic_cast<Literal*>(tree) == nullptr) {
			tree->SetParentScope( NULL );
		}
	}

	// If there is a chained parent ad, we have to check to see if removing
	// the attribute from the child caused the parent attribute to become visible.
	// If it did, then we have to set the child attribute to the UNDEFINED value explicitly
	// so that the parent attribute will not show through.
	if (chained_parent_ad) {
		ExprTree * parent_expr = chained_parent_ad->Lookup(name);
		if (parent_expr) {
			// If there was no attribute in the child ad
			// we want to return a copy of the parent's expr-tree
			// as if we had removed it from the child.
			if ( ! tree) {
				tree = parent_expr->Copy();
				if (tree && dynamic_cast<Literal*>(tree) == nullptr) {
					tree->SetParentScope( NULL );
				}
			}
			ExprTree* plit  = Literal::MakeUndefined();
			Insert(name, plit);
		}
	}
	return tree;
}

ExprTree *ClassAd::
DeepRemove( ExprTree *scopeExpr, const std::string &name )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( (ExprTree*)NULL );
	return( ad->Remove( name ) );
}

int ClassAd::
Compact()
{
	int compacted = 0;

	// Iterate through all attributes in the flat map
	for (auto it = attrList.begin(); it != attrList.end(); ++it) {
		InlineExprStorage& val = it->second;

		// Skip if already inline
		if (val.isInline()) {
			continue;
		}

		// Get the ExprTree pointer
		ExprTree* tree = val.asPtr();
		if (!tree) {
			continue;
		}

		// Only inline if it's already a Literal - don't evaluate complex expressions
		Literal* literal = dynamic_cast<Literal*>(tree);
		if (!literal) {
			continue;
		}

		// Get the value from the literal
		Value v;
		literal->GetValue(v);

		// Try to convert to an inline value
		InlineExprStorage newVal;
		if (tryMakeInlineValue(v, newVal, const_cast<InlineStringBuffer*>(attrList.stringBuffer()))) {
			// Successfully inlined, replace the value
			val.setBits(newVal.toBits());

			// Delete the old ExprTree to free memory
			delete tree;

			compacted++;
		}
	}

	return compacted;
}
// --- end removal methods


void ClassAd::
_SetParentScope( const ClassAd *scope )
{
	// We shouldn't propagate
	// the call to sub-expressions because this is a new scope
	parentScope = scope;
}


bool ClassAd::
Update( const ClassAd& ad )
{
	AttrList::const_iterator itr;
	for( itr=ad.attrList.begin( ); itr!=ad.attrList.end( ); itr++ ) {
		const InlineExprStorage &srcVal = itr->second;

		// If the source value is inline, we can copy it directly without materializing
		if (srcVal.isInline()) {
			// For inline values, we need to handle string references specially
			if (srcVal.getInlineType() == 5) { // String reference type
				// We need to copy the string data to our own string buffer
				uint32_t offset, size;
				srcVal.getStringRef(offset, size);
				const InlineStringBuffer* srcBuffer = ad.attrList.stringBuffer();
				if (srcBuffer) {
					std::string str = srcBuffer->getString(offset, size);
					
					// Try to add to our string buffer
					attrList.setString(itr->first, str);
					// Check if it succeeded - setString always succeeds for small strings
					if (true) {
						// Success - inline string added
					} else {
						// String buffer full or string too large, fall back to materialization
						ExprTree* base = ad.attrList.materialize(srcVal);
						ExprTree* cpy = base ? base->Copy() : nullptr;
						if(!Insert( itr->first, cpy )) {
							return false;
						}
					}
				} else {
					// No source buffer? This shouldn't happen but fall back to materialization
					ExprTree* base = ad.attrList.materialize(srcVal);
					ExprTree* cpy = base ? base->Copy() : nullptr;
					if(!Insert( itr->first, cpy )) {
						return false;
					}
				}
			} else {
				// For non-string inline values (int, float, bool, error, undefined),
				// we can copy them by type
				int type = srcVal.getInlineType();
				switch (type) {
					case 0: // Error
						attrList.setError(itr->first);
						break;
					case 1: // Undefined
						attrList.setUndefined(itr->first);
						break;
					case 2: // int32
						attrList.setInt(itr->first, srcVal.getInt32());
						break;
					case 3: // float32
						attrList.setFloat(itr->first, srcVal.getFloat32());
						break;
					case 4: // bool
						attrList.setBool(itr->first, srcVal.getBool());
						break;
					default:
						// Shouldn't happen
						return false;
				}
			}
		} else {
			// Out-of-line value, need to materialize and copy
			ExprTree* base = srcVal.asPtr();
			ExprTree * cpy = base ? base->Copy() : nullptr;
			if(!Insert( itr->first, cpy )) {
				return false;
			}
		}
	}
	return true;
}

void ClassAd::
Modify( ClassAd& mod )
{
	ClassAd			*ctx;
	const ExprTree	*expr;
	Value			val;

		// Step 0:  Determine Context
	if( ( expr = mod.Lookup( ATTR_CONTEXT ) ) != NULL ) {
		if( ( ctx = _GetDeepScope( const_cast<ExprTree *>((const ExprTree*) expr))) == nullptr) {
			return;
		}
	} else {
		ctx = this;
	}

		// Step 1:  Process Replace attribute
	if( ( expr = mod.Lookup( ATTR_REPLACE ) ) != NULL ) {
		ClassAd	*ad;
		if( expr->Evaluate( val ) && val.IsClassAdValue( ad ) ) {
			ctx->Clear( );
			ctx->Update( *ad );
		}
	}

		// Step 2:  Process Updates attribute
	if( ( expr = mod.Lookup( ATTR_UPDATES ) ) != NULL ) {
		ClassAd *ad;
		if( expr->Evaluate( val ) && val.IsClassAdValue( ad ) ) {
			ctx->Update( *ad );
		}
	}

		// Step 3:  Process Deletes attribute
	if( ( expr = mod.Lookup( ATTR_DELETES ) ) != NULL ) {
		const ExprList 		*list;
		const char			*attrName;

			// make a first pass to check that it is a list of std::strings ...
		if( !expr->Evaluate( val ) || !val.IsListValue( list ) ) {
			return;
		}
		for (const auto *expr: *list) {
			if( !expr->Evaluate( val ) || !val.IsStringValue( attrName ) ) {
				return;
			}
		}

			// now go through and delete all the named attributes ...
		for (const auto *expr: *list) {
			if( expr->Evaluate( val ) && val.IsStringValue( attrName ) ) {
				ctx->Delete( attrName );
			}
		}
	}

	/*
		// Step 4:  Process DeepModifications attribute
	if( ( expr = mod.Lookup( ATTR_DEEP_MODS ) ) != NULL ) {
		ExprList			*list;
		ExprListIterator	itor;
		ClassAd				*ad;

		if( !expr->Evaluate( val ) || !val.IsListValue( list ) ) {
			return( false );
		}
		itor.Initialize( list );
		while( ( expr = itor.NextExpr( ) ) ) {
			if( !expr->Evaluate( val ) 		|| 
				!val.IsClassAdValue( ad ) 	|| 
				!Modify( *ad ) ) {
				return( false );
			}
		}
	}

	*/
}


ExprTree* ClassAd::
Copy( ) const
{
	ExprTree *tree;
	ClassAd	*newAd = new ClassAd();

	if( !newAd ) return NULL;
	newAd->parentScope = parentScope;
	newAd->chained_parent_ad = chained_parent_ad;

	AttrList::const_iterator	itr;
	for( itr=attrList.begin( ); itr != attrList.end( ); itr++ ) {
		ExprTree* base = attrList.materialize(itr->second);
		if( !( tree = (base ? base->Copy() : nullptr) ) ) {
			delete newAd;
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
			return( NULL );
		}
		newAd->Insert(itr->first,tree);
	}

	return newAd;
}


bool ClassAd::
_Evaluate( EvalState&, Value& val ) const
{
	val.SetClassAdValue( const_cast<ClassAd *>((const ClassAd*)this));
	return( true );
}


bool ClassAd::
_Evaluate( EvalState&, Value &val, ExprTree *&tree ) const
{
	val.SetClassAdValue(const_cast<ClassAd *>((const ClassAd*)this));
	return( ( tree = Copy( ) ) );
}


bool ClassAd::
_Flatten( EvalState& state, Value&, ExprTree*& tree, int* ) const
{
	ClassAd 	*newAd = new ClassAd();
	Value		eval;
	ExprTree	*etree;
	const ClassAd *oldAd;
	AttrList::const_iterator	itr;

	tree = NULL; // Just to be safe...  wenger 2003-12-11.

	oldAd = state.curAd;
	state.curAd = this;

	for( itr = attrList.begin( ); itr != attrList.end( ); itr++ ) {
		// flatten expression
		ExprTree* node = attrList.materialize(itr->second);
		if( !node || !node->Flatten( state, eval, etree ) ) {
			delete newAd;
			tree = NULL;
			eval.Clear();
			state.curAd = oldAd;
			return false;
		}

		// if a value was obtained, convert it to a literal
		if( !etree ) {
			etree = Literal::MakeLiteral( eval );
			if( !etree ) {
				delete newAd;
				tree = NULL;
				eval.Clear();
				state.curAd = oldAd;
				return false;
			}
		}
		newAd->attrList[itr->first] = etree;
		eval.Clear( );
	}

	tree = newAd;
	state.curAd = oldAd;
	return true;
}


ClassAd *ClassAd::
_GetDeepScope( ExprTree *tree ) const
{
	ClassAd	*scope;
	Value	val;

	if( !tree ) return( NULL );
	tree->SetParentScope( this );
	if( !tree->Evaluate( val ) || !val.IsClassAdValue( scope ) ) {
		return( NULL );
	}
	return( (ClassAd*)scope );
}


bool ClassAd::
EvaluateAttr( const std::string &attr , Value &val, Value::ValueType mask ) const
{
	bool successfully_evaluated = false;
	EvalState	state;

	state.SetScopes( this );
	InlineExpr inlineExpr;
	InlineExprStorage scratch(nullptr);
	switch( LookupInScope( attr, inlineExpr, scratch, state ) ) {
		case EVAL_OK:
			// OPTIMIZATION: Try to extract inline value first
			if (inlineExpr.value() && inlineExpr.value()->isInline()) {
				// Successfully extracted inline value
				inlineExpr.attrList()->inlineValueToValue(*inlineExpr.value(), val);
				successfully_evaluated = val.SafetyCheck(state, mask);
			} else if (inlineExpr.value()) {
				// Fall back to normal evaluation
				successfully_evaluated = inlineExpr.value()->asPtr()->Evaluate( state, val );
				if ( ! val.SafetyCheck(state, mask)) {
					successfully_evaluated = false;
				}
			}
			break;

		case EVAL_UNDEF:
			successfully_evaluated = true;
			val.SetUndefinedValue( );
			break;

		case EVAL_ERROR:
			successfully_evaluated = true;
			val.SetErrorValue( );
			break;

		case EVAL_FAIL:
		default:
			successfully_evaluated = false;
			break;
	}

	return successfully_evaluated;
}


bool ClassAd::
EvaluateExpr( const std::string& buf, Value &result ) const
{
	bool           successfully_evaluated = false;
	ExprTree       *tree = nullptr;
	ClassAdParser  parser;
	EvalState      state;

	tree = parser.ParseExpression(buf);
	if (tree != nullptr) {
		state.AddToDeletionCache(tree);
		state.SetScopes( this );
		successfully_evaluated = tree->Evaluate( state , result );
		if (successfully_evaluated && ! result.SafetyCheck(state, Value::ValueType::SAFE_VALUES)) {
			successfully_evaluated = false;
		}
	}

	return successfully_evaluated;
}


// mask is a hint about what types are wanted back, but a mismatch between tye mask
// and the evaluated type will not result in the function returning false.
// this is for backward compatibility
//
bool ClassAd::
EvaluateExpr( const ExprTree *tree , Value &val, Value::ValueType mask ) const
{
	EvalState	state;

	state.SetScopes( this );
	bool res = tree->Evaluate( state , val );
	if ( ! res) {
		return res;
	}
	if ( ! val.SafetyCheck(state, mask)) {
		res = false;
	}

	return res;
}

bool ClassAd::
EvaluateExpr( const ExprTree *tree , Value &val , ExprTree *&sig ) const
{
	EvalState	state;

	state.SetScopes( this );
	bool res = tree->Evaluate( state , val , sig );
	if (res && ! val.SafetyCheck(state, Value::ValueType::SAFE_VALUES)) {
		res = false;
	}
	return res;
}

bool ClassAd::
EvaluateAttrInt( const std::string &attr, int &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrInt( const std::string &attr, long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrInt( const std::string &attr, long long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrReal( const std::string &attr, double &r )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsRealValue( r ) );
}

bool ClassAd::
EvaluateAttrNumber( const std::string &attr, int &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const std::string &attr, long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const std::string &attr, long long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const std::string &attr, double &r )  const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( r ) );
}

bool ClassAd::
EvaluateAttrString( const std::string &attr, char *buf, int len ) const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::STRING_VALUE ) && val.IsStringValue( buf, len ) );
}

bool ClassAd::
EvaluateAttrString( const std::string &attr, std::string &buf ) const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::STRING_VALUE ) && val.IsStringValue( buf ) );
}

bool ClassAd::
EvaluateAttrBool( const std::string &attr, bool &b ) const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsBooleanValue( b ) );
}

bool ClassAd::
EvaluateAttrBoolEquiv( const std::string &attr, bool &b ) const
{
	Value val;
	return( EvaluateAttr( attr, val, Value::ValueType::NUMBER_VALUES ) && val.IsBooleanValueEquiv( b ) );
}

bool ClassAd::
EvaluateExprBoolEquiv( ExprTree* expr, bool &boolValue ) const
{
	if (!expr) {
		return false;
	}

	Value val;
	EvalState state;

	// Set the evaluation scopes to this ClassAd
	state.SetScopes(this);

	// Evaluate the expression
	if (!expr->Evaluate(state, val)) {
		return false;
	}

	// Check if the result is boolean-equivalent
	return val.IsBooleanValueEquiv(boolValue);
}

bool ClassAd::
EvaluateExprBoolEquiv( const InlineExpr& inlineExpr, bool &boolValue ) const
{
	if (!inlineExpr) {
		return false;
	}

	// If the value is inline, evaluate it directly without materializing
	if (inlineExpr.value() && inlineExpr.value()->isInline()) {
		// String values cannot be boolean-equivalent, so reject them
		if (inlineExpr.value()->getInlineType() == 5) {
			return false;
		}
		Value val;
		// Evaluate the inline value directly - convert to Value first
		classad::inlineValueToValue(*inlineExpr.value(), val, inlineExpr.attrList()->stringBuffer());
		return val.IsBooleanValueEquiv(boolValue);
	}

	// For out-of-line values, materialize and evaluate
	ExprTree* expr = inlineExpr.materialize();
	if (!expr) {
		return false;
	}

	Value val;
	EvalState state;
	state.SetScopes(this);

	if (!expr->Evaluate(state, val)) {
		return false;
	}

	return val.IsBooleanValueEquiv(boolValue);
}

bool ClassAd::
_EvaluateInlineExpr( const InlineExpr& inlineExpr, Value &result, Value::ValueType mask ) const
{
	if (!inlineExpr) {
		return false;
	}

	// If the value is inline, check type compatibility before converting
	if (inlineExpr.value() && inlineExpr.value()->isInline()) {
		unsigned char inlineType = inlineExpr.value()->getInlineType();

		// Type check optimization: avoid string allocation if caller doesn't want strings
		// inlineType: 0=error, 1=undef, 2=bool, 3=int32, 4=float32, 5=string
		if (inlineType == 5 && !(mask & Value::ValueType::STRING_VALUE)) {
			// It's a string but caller doesn't want strings - return false
			return false;
		}

		// Convert inline value to Value
		classad::inlineValueToValue(*inlineExpr.value(), result, inlineExpr.attrList()->stringBuffer());
		// Inline values are already safe, no need for SafetyCheck
		return true;
	}

	// For out-of-line values, materialize and evaluate
	ExprTree* expr = inlineExpr.materialize();
	if (!expr) {
		return false;
	}

	return EvaluateExpr(expr, result, mask);
}

bool ClassAd::
EvaluateExpr( const InlineExpr& inlineExpr, Value &result, Value::ValueType mask ) const
{
	return _EvaluateInlineExpr(inlineExpr, result, mask);
}

bool ClassAd::
EvaluateExprInt( const InlineExpr& inlineExpr, int &i ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsIntegerValue( i );
}

bool ClassAd::
EvaluateExprInt( const InlineExpr& inlineExpr, long &i ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsIntegerValue( i );
}

bool ClassAd::
EvaluateExprInt( const InlineExpr& inlineExpr, long long &i ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsIntegerValue( i );
}

bool ClassAd::
EvaluateExprReal( const InlineExpr& inlineExpr, double &r ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsRealValue( r );
}

bool ClassAd::
EvaluateExprNumber( const InlineExpr& inlineExpr, int &i ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( i );
}

bool ClassAd::
EvaluateExprNumber( const InlineExpr& inlineExpr, long &i ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( i );
}

bool ClassAd::
EvaluateExprNumber( const InlineExpr& inlineExpr, long long &i ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( i );
}

bool ClassAd::
EvaluateExprNumber( const InlineExpr& inlineExpr, double &r ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsNumber( r );
}

bool ClassAd::
EvaluateExprString( const InlineExpr& inlineExpr, char *buf, int len ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::STRING_VALUE ) && val.IsStringValue( buf, len );
}

bool ClassAd::
EvaluateExprString( const InlineExpr& inlineExpr, std::string &buf ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::STRING_VALUE ) && val.IsStringValue( buf );
}

bool ClassAd::
EvaluateExprBool( const InlineExpr& inlineExpr, bool &b ) const
{
	Value val;
	return _EvaluateInlineExpr( inlineExpr, val, Value::ValueType::NUMBER_VALUES ) && val.IsBooleanValue( b );
}

bool ClassAd::
GetExternalReferences( const ExprTree *tree, References &refs, bool fullNames ) const
{
    EvalState       state;

    // Treat this ad as the root of the tree for reference tracking.
    // If an attribute is only present in a parent scope of this ad,
    // then we want to treat it as an external reference.
    state.rootAd = this; 
	state.curAd = this;

    return( _GetExternalReferences( tree, this, state, refs, fullNames ) );
}


bool ClassAd::
_GetExternalReferences( const ExprTree *expr, const ClassAd *ad, 
	EvalState &state, References& refs, bool fullNames )
{
    switch( expr->GetKind()) {
		case ERROR_LITERAL:
		case UNDEFINED_LITERAL:
		case BOOLEAN_LITERAL:
		case INTEGER_LITERAL:
		case REAL_LITERAL:
		case RELTIME_LITERAL:
		case ABSTIME_LITERAL:
		case STRING_LITERAL:
                // no external references here
            return( true );

        case ATTRREF_NODE: {
            const ClassAd   *start;
            ExprTree        *tree;
            InlineExprStorage      result;
            std::string          attr;
            Value           val;
            bool            abs;

            ((const AttributeReference*)expr)->GetComponents( tree, attr, abs );
                // establish starting point for attribute search
            if( tree==NULL ) {
                start = abs ? state.rootAd : state.curAd;
				if( abs && ( start == NULL ) ) {// NAC - circularity so no root
					return false;				// NAC
				}								// NAC
            } else {
                if( !tree->Evaluate( state, val ) ) {
                    return( false );
                }

                    // if the tree evals to undefined, the external references
                    // are in the tree part
                if( val.IsUndefinedValue( ) ) {
                    if (fullNames) {
                        std::string fullName;
                        if (tree != NULL) {
                            ClassAdUnParser unparser;
                            unparser.Unparse(fullName, tree);
                            fullName += ".";
                        }
                        fullName += attr;
                        refs.insert( fullName );
                        return true;
                    } else {
                        if( state.depth_remaining <= 0 ) {
                            return false;
                        }
                        state.depth_remaining--;

                        bool ret = _GetExternalReferences( tree, ad, state, refs, fullNames );

                        state.depth_remaining++;
                        return ret;
                    }
                }
                    // otherwise, if the tree didn't evaluate to a classad,
                    // we have a problem
                if( !val.IsClassAdValue( start ) )  {
                    return( false );
                }
            }
                // lookup for attribute
			const ClassAd *curAd = state.curAd;
			InlineExpr inlineExpr;
			InlineExprStorage scratch(nullptr);
            switch( start->LookupInScope( attr, inlineExpr, scratch, state ) ) {
                case EVAL_ERROR:
                        // some error
                    return( false );

                case EVAL_UNDEF:
                        // attr is external
                    refs.insert( attr );
					state.curAd = curAd;
                    return( true );

                case EVAL_OK: {
                        // attr is internal; find external refs in result
                    if( state.depth_remaining <= 0 ) {
                        state.curAd = curAd;
                        return false;
                    }
                    state.depth_remaining--;

                    auto expr = inlineExpr.materialize();
                    bool rval=_GetExternalReferences(expr,ad,state,refs,fullNames);

                    state.depth_remaining++;
					state.curAd = curAd;
					return( rval );
				}

                case EVAL_FAIL:
                default:    
                        // enh??
                    return( false );
            }
        }
            
        case OP_NODE: {
                // recurse on subtrees
            Operation::OpKind	op;
            ExprTree    		*t1, *t2, *t3;
            ((const Operation*)expr)->GetComponents( op, t1, t2, t3 );
            if( t1 && !_GetExternalReferences( t1, ad, state, refs, fullNames ) ) {
                return( false );
            }
            if( t2 && !_GetExternalReferences( t2, ad, state, refs, fullNames ) ) {
                return( false );
            }
            if( t3 && !_GetExternalReferences( t3, ad, state, refs, fullNames ) ) {
                return( false );
            }
            return( true );
        }

        case FN_CALL_NODE: {
                // recurse on subtrees
            std::string                      fnName;
            std::vector<ExprTree*>           args;
            std::vector<ExprTree*>::iterator itr;

            ((const FunctionCall*)expr)->GetComponents( fnName, args );
            for( itr = args.begin( ); itr != args.end( ); itr++ ) {
				if( state.depth_remaining <= 0 ) {
					return false;
				}
				state.depth_remaining--;
				bool rval=_GetExternalReferences(*itr, ad, state, refs, fullNames);
				state.depth_remaining++;
				if( ! rval) {
					return( false );
				}
            }
            return( true );
        }


        case CLASSAD_NODE: {
                // recurse on subtrees
            std::vector<std::pair<std::string, ExprTree*> >           attrs;
            std::vector<std::pair<std::string, ExprTree*> >::iterator itr;

            ((const ClassAd*)expr)->GetComponents( attrs );
            for( itr = attrs.begin( ); itr != attrs.end( ); itr++ ) {
                if( state.depth_remaining <= 0 ) {
                    return false;
                }
                state.depth_remaining--;

                bool ret = _GetExternalReferences( itr->second, ad, state, refs, fullNames );

                state.depth_remaining++;
                if( !ret ) {
                    return( false );
                }
            }
            return( true );
        }


        case EXPR_LIST_NODE: {
                // recurse on subtrees
            std::vector<ExprTree*>           exprs;
            std::vector<ExprTree*>::iterator itr;

            ((const ExprList*)expr)->GetComponents( exprs );
            for( itr = exprs.begin( ); itr != exprs.end( ); itr++ ) {
                if( state.depth_remaining <= 0 ) {
                    return false;
                }
                state.depth_remaining--;

                bool ret = _GetExternalReferences( *itr, ad, state, refs, fullNames );

                state.depth_remaining++;
                if( !ret ) {
                    return( false );
                }
            }
            return( true );
        }

		case EXPR_ENVELOPE: {
			return _GetExternalReferences( ((const CachedExprEnvelope*)expr)->get(), ad, state, refs, fullNames );
		}

        default:
            return false;
    }
}

bool ClassAd::
GetExternalReferences( const ExprTree *tree, PortReferences &refs ) const
{
    EvalState       state;

    // Treat this ad as the root of the tree for reference tracking.
    // If an attribute is only present in a parent scope of this ad,
    // then we want to treat it as an external reference.
    state.rootAd = this; 
    state.curAd = this;
	
    return( _GetExternalReferences( tree, this, state, refs ) );
}

bool ClassAd::
_GetExternalReferences( const ExprTree *expr, const ClassAd *ad, 
	EvalState &state, PortReferences& refs ) const
{
    switch( expr->GetKind( ) ) {
		case ERROR_LITERAL:
		case UNDEFINED_LITERAL:
		case BOOLEAN_LITERAL:
		case INTEGER_LITERAL:
		case REAL_LITERAL:
		case RELTIME_LITERAL:
		case ABSTIME_LITERAL:
		case STRING_LITERAL:
                // no external references here
            return( true );

        case ATTRREF_NODE: {
            const ClassAd   *start;
            ExprTree        *tree;
            InlineExprStorage      result;
            std::string          attr;
            Value           val;
            bool            abs;
			PortReferences::iterator	pitr;

            ((const AttributeReference*)expr)->GetComponents( tree, attr, abs );
                // establish starting point for attribute search
            if( tree==NULL ) {
                start = abs ? state.rootAd : state.curAd;
				if( abs && ( start == NULL ) ) {// NAC - circularity so no root
					return false;				// NAC
				}								// NAC
            } else {
                if( !tree->Evaluate( state, val ) ) return( false );

                    // if the tree evals to undefined, the external references
                    // are in the tree part
                if( val.IsUndefinedValue( ) ) {
                    return( _GetExternalReferences( tree, ad, state, refs ));
                }
                    // otherwise, if the tree didn't evaluate to a classad,
                    // we have a problem
                if( !val.IsClassAdValue( start ) ) return( false );

					// make sure that we are starting from a "valid" scope
				if( ( pitr = refs.find( start ) ) == refs.end( ) && 
						start != this ) {
					return( false );
				}
            }
                // lookup for attribute
            const ClassAd *curAd = state.curAd;
            InlineExpr inlineExpr;
            InlineExprStorage scratch(nullptr);
            switch( start->LookupInScope( attr, inlineExpr, scratch, state ) ) {
                case EVAL_ERROR:
                        // some error
                    return( false );

                case EVAL_UNDEF:
                        // attr is external
                    pitr->second.insert( attr );
					state.curAd = curAd;
                    return( true );

                case EVAL_OK: {
                        // attr is internal; find external refs in result
					auto expr = inlineExpr.materialize();
					bool rval = _GetExternalReferences(expr,ad,state,refs);
					state.curAd = curAd;
					return( rval );
				}

                case EVAL_FAIL:
                default:    
                        // enh??
                    return( false );
            }
        }
            
        case OP_NODE: {
                // recurse on subtrees
            Operation::OpKind   op;
            ExprTree    		*t1, *t2, *t3;
            ((const Operation*)expr)->GetComponents( op, t1, t2, t3 );
            if( t1 && !_GetExternalReferences( t1, ad, state, refs ) ) {
                return( false );
            }
            if( t2 && !_GetExternalReferences( t2, ad, state, refs ) ) {
                return( false );
            }
            if( t3 && !_GetExternalReferences( t3, ad, state, refs ) ) {
                return( false );
            }
            return( true );
        }

        case FN_CALL_NODE: {
                // recurse on subtrees
            std::string                      fnName;
            std::vector<ExprTree*>           args;
            std::vector<ExprTree*>::iterator itr;

            ((const FunctionCall*)expr)->GetComponents( fnName, args );
            for( itr = args.begin( ); itr != args.end( ); itr++ ) {
                if( !_GetExternalReferences( *itr, ad, state, refs ) ) {
					return( false );
				}
            }
            return( true );
        }


        case CLASSAD_NODE: {
                // recurse on subtrees
            std::vector< std::pair<std::string, ExprTree*> >           attrs;
            std::vector< std::pair<std::string, ExprTree*> >::iterator itr;

            ((const ClassAd*)expr)->GetComponents( attrs );
            for( itr = attrs.begin( ); itr != attrs.end( ); itr++ ) {
                if( !_GetExternalReferences( itr->second, ad, state, refs )) {
					return( false );
				}
            }
            return( true );
        }


        case EXPR_LIST_NODE: {
                // recurse on subtrees
            std::vector<ExprTree*>           exprs;
            std::vector<ExprTree*>::iterator itr;

            ((const ExprList*)expr)->GetComponents( exprs );
            for( itr = exprs.begin( ); itr != exprs.end( ); itr++ ) {
                if( !_GetExternalReferences( *itr, ad, state, refs ) ) {
					return( false );
				}
            }
            return( true );
        }

		case EXPR_ENVELOPE: {
			return _GetExternalReferences( ( (const CachedExprEnvelope*)expr )->get(), ad, state, refs );
		}

        default:
            return false;
    }
}


bool ClassAd::
GetInternalReferences( const ExprTree *tree, References &refs, bool fullNames) const
{
    EvalState state;

    // Treat this ad as the root of the tree for reference tracking.
    // If an attribute is only present in a parent scope of this ad,
    // then we want to treat it as an external reference.
    state.rootAd = this;
    state.curAd = this;

    return( _GetInternalReferences( tree, this, state, refs, fullNames) );
}

//this is closely modelled off of _GetExternalReferences in the new_classads.
bool ClassAd::
_GetInternalReferences( const ExprTree *expr, const ClassAd *ad,
        EvalState &state, References& refs, bool fullNames) const
{

    switch( expr->GetKind() ){
        //nothing to be found here!
		case ERROR_LITERAL:
		case UNDEFINED_LITERAL:
		case BOOLEAN_LITERAL:
		case INTEGER_LITERAL:
		case REAL_LITERAL:
		case RELTIME_LITERAL:
		case ABSTIME_LITERAL:
		case STRING_LITERAL:
            return true;
        break;

        case ATTRREF_NODE:{
            const ClassAd   *start;
            ExprTree        *tree;
            InlineExprStorage      result;
            std::string          attr;
            Value           val;
            bool            abs;

            ((const AttributeReference*)expr)->GetComponents(tree,attr,abs);

            //figuring out which state to base this off of
            if( tree == NULL ){
                start = abs ? state.rootAd : state.curAd;
                //remove circularity
                if( abs && (start == NULL) ) {
                    return false;
                }
            } else {
                bool orig_inAttrRefScope = state.inAttrRefScope;
                state.inAttrRefScope = true;
                bool rv = _GetInternalReferences( tree, ad, state, refs, fullNames );
                state.inAttrRefScope = orig_inAttrRefScope;
                if ( !rv ) {
                    return false;
                }

                if (!tree->Evaluate(state, val) ) {
                    return false;
                }

                // TODO Do we need extra handling for list values?
                //   Should types other than undefined, error, or list
                //   cause a failure?
                if( val.IsUndefinedValue() ) {
                    return true;
                }

                //otherwise, if the tree didn't evaluate to a classad,
                //we have a problemo, mon.
                //TODO: but why?
                if( !val.IsClassAdValue( start ) ) {
                    return false;
                }
            }

            const ClassAd *curAd = state.curAd;
            InlineExpr inlineExpr;
            InlineExprStorage scratch(nullptr);
            switch( start->LookupInScope( attr, inlineExpr, scratch, state) ) {
                case EVAL_ERROR:
                    return false;
                break;

                //attr is external, so let's find the internals in that
                //result
                //JUST KIDDING
                case EVAL_UNDEF:{
                    
                    //bool rval = _GetInternalReferences(result, ad, state, refs, fullNames);
                    //state.curAd = curAd;
                    return true;
                break;
                                }

                case EVAL_OK:   {
                    //whoo, it's internal.
                    // Check whether the attribute was found in the root
                    // ad for this evaluation and that the attribute isn't
                    // one of our special ones (self, parent, my, etc.).
                    // If the ad actually has an attribute with the same
                    // name as one of our special attributes, then count
                    // that as an internal reference.
                    // TODO LookupInScope() knows whether it's returning
                    //   the expression of one of the special attributes
                    //   or that of an attribute that actually appears in
                    //   the ad. If it told us which one, then we could
                    //   avoid the Lookup() call below.
                    if ( state.curAd == state.rootAd &&
                         state.curAd->Lookup( attr ) ) {
                        refs.insert(attr);
                    }
                    if( state.depth_remaining <= 0 ) {
                        state.curAd = curAd;
                        return false;
                    }
                    state.depth_remaining--;

                    auto expr = inlineExpr.materialize();
                    bool rval = _GetInternalReferences(expr, ad, state, refs, fullNames);

                    state.depth_remaining++;
                    //TODO: Does this actually matter?
                    state.curAd = curAd;
                    return rval;
                break;
                                }

                case EVAL_FAIL:
                default:
                    // "enh??"
                    return false;
                break;
            }

        break;
                          }
    

        case OP_NODE:{

            //recurse on subtrees
            Operation::OpKind       op;
            ExprTree                *t1, *t2, *t3;
            ((const Operation*)expr)->GetComponents(op, t1, t2, t3);
            if( t1 && !_GetInternalReferences(t1, ad, state, refs, fullNames)) {
                return false;
            }

            if( t2 && !_GetInternalReferences(t2, ad, state, refs, fullNames)) {
                return false;
            }

            if( t3 && !_GetInternalReferences(t3, ad, state, refs, fullNames)) {
                return false;
            }

            return true;
        break;
        }

        case FN_CALL_NODE:{
            //recurse on the subtrees!
            std::string                          fnName;
            std::vector<ExprTree*>               args;

            ((const FunctionCall*)expr)->GetComponents(fnName, args);
            for( auto arg : args ){
                if( !_GetInternalReferences( arg, ad, state, refs, fullNames) ) {
                    return false;
                }
            }

            return true;
        break;
                          }

        case CLASSAD_NODE:{
            //also recurse on subtrees...
            std::vector<std::pair<std::string, ExprTree*> >               attrs;
            std::vector<std::pair<std::string, ExprTree*> >:: iterator    itr;

            // If this ClassAd is only being used here as the scoping
            // for an attribute reference, don't recurse into all of
            // its attributes.
            if ( state.inAttrRefScope ) {
                return true;
            }

            ((const ClassAd*)expr)->GetComponents(attrs);
            for(itr = attrs.begin(); itr != attrs.end(); itr++){
                if( state.depth_remaining <= 0 ) {
                    return false;
                }
                state.depth_remaining--;

                bool ret = _GetInternalReferences(itr->second, ad, state, refs, fullNames);

                state.depth_remaining++;
                if( !ret ) {
                    return false;
                }
            }

            return true;
        break;
            }

        case EXPR_LIST_NODE:{
            std::vector<ExprTree*>               exprs;

            ((const ExprList*)expr)->GetComponents(exprs);
            for(auto tree : exprs){
                if( state.depth_remaining <= 0 ) {
                    return false;
                }
                state.depth_remaining--;

                bool ret = _GetInternalReferences(tree, ad, state, refs, fullNames);

                state.depth_remaining++;
                if( !ret ) {
                    return false;
                }
            }

            return true;
        break;
            }
        
		case EXPR_ENVELOPE: {
			return _GetInternalReferences( ((const CachedExprEnvelope*)expr)->get(), ad, state, refs,fullNames);
		}
           

        default:
            return false;

    }
}

bool ClassAd::
Flatten( const ExprTree *tree , Value &val , ExprTree *&fexpr ) const
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Flatten( state , val , fexpr ) );
}

bool ClassAd::	// NAC
FlattenAndInline( const ExprTree *tree , Value &val , ExprTree *&fexpr ) const
{
	EvalState	state;

	state.SetScopes( this );
	state.flattenAndInline = true;
	return( tree->Flatten( state , val , fexpr ) );
}

void ClassAd::ChainToAd(ClassAd *new_chain_parent_ad)
{
	if (new_chain_parent_ad != NULL) {
		chained_parent_ad = new_chain_parent_ad;
	}
	return;
}

bool ClassAd::PruneChildAttr(const std::string & attrName, bool if_child_matches /*=true*/)
{
	if ( ! chained_parent_ad)
		return false;

	AttrList::iterator itr = attrList.find(attrName);
	if (itr == attrList.end())
		return false;

	bool prune_it = true;
	if (if_child_matches) {
		ExprTree * tree = chained_parent_ad->Lookup(itr->first);
		ExprTree* child_tree = attrList.materialize(itr->second);
		prune_it = (tree && tree->SameAs(child_tree));
	}

	if (prune_it) {
		attrList.erase(itr);
		return true;
	}
	return false;
}

int ClassAd::PruneChildAd()
{
	int iRet =0;
	
	if (chained_parent_ad)
	{
		// loop through cleaning all expressions which are the same.
		AttrList::const_iterator	itr= attrList.begin( );
		ExprTree 					*tree;
	
		std::list<std::string> victims;

		while (itr != attrList.end() )
		{
			tree = chained_parent_ad->Lookup(itr->first);
				
			ExprTree* child_tree = attrList.materialize(itr->second);
			if(  tree && tree->SameAs(child_tree) ) {
				MarkAttributeClean(itr->first);
				victims.push_back(itr->first);
				iRet++;
			}

			itr++;
		}

		for (auto &s: victims) {
			AttrList::iterator itr = attrList.find(s);
			if( itr != attrList.end( ) ) {
				attrList.erase( itr );
			}
		}
	}
	
	return iRet;
}

void ClassAd::Unchain(void)
{
	chained_parent_ad = NULL;
	return;
}

ClassAd *ClassAd::GetChainedParentAd(void)
{
	return chained_parent_ad;
}

const ClassAd *ClassAd::GetChainedParentAd(void) const
{
	return chained_parent_ad;
}

void ClassAd::ClearAllDirtyFlags(void)
{ 
	dirtyAttrList.clear();
	return;
}

void ClassAd::MarkAttributeClean(const std::string &name)
{
	if (do_dirty_tracking) {
		dirtyAttrList.erase(name);
	}
	return;
}

bool ClassAd::IsAttributeDirty(const std::string &name) const
{
	bool is_dirty;

	if (dirtyAttrList.find(name) != dirtyAttrList.end()) {
		is_dirty = true;
	} else {
		is_dirty = false;
	}
	return is_dirty;
}

} // classad
