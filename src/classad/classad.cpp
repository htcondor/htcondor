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
#include "classad/classadItor.h"
#include "classad/source.h"
#include "classad/sink.h"
#include "classad/classadCache.h"

using namespace std;

extern "C" void to_lower (char *);	// from util_lib (config.c)

namespace classad {

// This flag is only meant for use in Condor, which is transitioning
// from an older version of ClassAds with slightly different evaluation
// semantics. It will be removed without warning in a future release.
bool _useOldClassAdSemantics = false;

// Should parsed expressions be cached and shared between multiple ads.
// The default is false.
static bool doExpressionCaching = false;

void ClassAdSetExpressionCaching(bool do_caching) {
	doExpressionCaching = do_caching;
}

bool ClassAdGetExpressionCaching()
{
	return doExpressionCaching;
}

// This is probably not the best place to put these. However, 
// I am reconsidering how we want to do errors, and this may all
// change in any case. 
string CondorErrMsg;
int CondorErrno;

void ClassAdLibraryVersion(int &major, int &minor, int &patch)
{
    major = CLASSAD_VERSION_MAJOR;
    minor = CLASSAD_VERSION_MINOR;
    patch = CLASSAD_VERSION_PATCH;
    return;
}

void ClassAdLibraryVersion(string &version_string)
{
    version_string = CLASSAD_VERSION;
    return;
}

ClassAd::
ClassAd ()
{
	EnableDirtyTracking();
	chained_parent_ad = NULL;
	alternateScope = NULL;
}


ClassAd::
ClassAd (const ClassAd &ad)
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
	AttrList::const_iterator	itr;
	ExprTree 					*tree;
	bool                        succeeded;

    succeeded = true;
	if (this == &ad) 
	{
		succeeded = false;
	} else 
	{
		Clear( );
		
		// copy scoping attributes
		ExprTree::CopyFrom(ad);
		chained_parent_ad = ad.chained_parent_ad;
		alternateScope = ad.alternateScope;
		
		this->do_dirty_tracking = false;
		for( itr = ad.attrList.begin( ); itr != ad.attrList.end( ); itr++ ) {
			if( !( tree = itr->second->Copy( ) ) ) {
				Clear( );
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
                succeeded = false;
                break;
			}
			
			Insert(itr->first, tree, false);
			if (ad.do_dirty_tracking && ad.IsAttributeDirty(itr->first)) {
				dirtyAttrList.insert(itr->first);
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
    bool is_same;

    const ExprTree * pSelfTree = tree->self();
    
    if (this == pSelfTree) {
        is_same = true;
    } else if (pSelfTree->GetKind() != CLASSAD_NODE) {
        is_same = false;
   } else {
       const ClassAd *other_classad;

       other_classad = (const ClassAd *) pSelfTree;

       if (attrList.size() != other_classad->attrList.size()) {
           is_same = false;
       } else {
           is_same = true;
           
           AttrList::const_iterator	itr;
           for (itr = attrList.begin(); itr != attrList.end(); itr++) {
               ExprTree *this_tree;
               ExprTree *other_tree;

               this_tree = itr->second;
               other_tree = other_classad->Lookup(itr->first);
               if (other_tree == NULL) {
                   is_same = false;
                   break;
               } else if (!this_tree->SameAs(other_tree)) {
                   is_same = false;
                   break;
               }
           }
       }
   }
 
    return is_same;
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
	AttrList::iterator	itr;
	for( itr = attrList.begin( ); itr != attrList.end( ); itr++ ) {
		if( itr->second ) delete itr->second;
	}
	attrList.clear( );
}

void ClassAd::
GetComponents( vector< pair< string, ExprTree* > > &attrs ) const
{
	attrs.clear( );
	for( AttrList::const_iterator itr=attrList.begin(); itr!=attrList.end(); 
		itr++ ) {
			// make_pair is a STL function
		attrs.push_back( make_pair( itr->first, itr->second ) );
	}
}


// --- begin integer attribute insertion ----
bool ClassAd::
InsertAttr( const string &name, int value, Value::NumberFactor f )
{
	ExprTree* plit;
	Value val;
	
	val.SetIntegerValue( value );
	plit  = Literal::MakeLiteral( val, f );
	
	return( Insert( name, plit ) );
}


bool ClassAd::
InsertAttr( const string &name, long value, Value::NumberFactor f )
{
	ExprTree* plit;
	Value val;

	val.SetIntegerValue( value );
	plit = Literal::MakeLiteral( val, f );
	return( Insert( name, plit ) );
}


bool ClassAd::
InsertAttr( const string &name, long long value, Value::NumberFactor f )
{
	ExprTree* plit;
	Value val;

	val.SetIntegerValue( value );
	plit = Literal::MakeLiteral( val, f );
	return( Insert( name, plit ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, int value, 
	Value::NumberFactor f )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value, f ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, long value, 
	Value::NumberFactor f )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value, f ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, long long value, 
	Value::NumberFactor f )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value, f ) );
}
// --- end integer attribute insertion ---



// --- begin real attribute insertion ---
bool ClassAd::
InsertAttr( const string &name, double value, Value::NumberFactor f )
{
	ExprTree* plit;
	Value val;
	
	val.SetRealValue( value );
	plit  = Literal::MakeLiteral( val, f );
	
	return( Insert( name, plit ) );
}
	

bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, double value, 
	Value::NumberFactor f )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value, f ) );
}
// --- end real attribute insertion



// --- begin boolean attribute insertion
bool ClassAd::
InsertAttr( const string &name, bool value )
{
	ExprTree* plit ;
	Value val;
	
	val.SetBooleanValue( value );
	plit  = Literal::MakeLiteral( val );
	
	return( Insert( name, plit ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, bool value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end boolean attribute insertion



// --- begin string attribute insertion
bool ClassAd::
InsertAttr( const string &name, const char *value )
{
	ExprTree* plit;
	Value val;
	
	val.SetStringValue( value );
	plit  = Literal::MakeLiteral( val );
	
	return( Insert( name, plit ) );
}

bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, const char *value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}

bool ClassAd::
InsertAttr( const string &name, const string &value )
{
	ExprTree* plit ;
	Value val;
	
	val.SetStringValue( value );
	plit  = Literal::MakeLiteral( val );
	
	return( Insert( name, plit ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, const string &value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end string attribute insertion

bool ClassAd::Insert( const std::string& serialized_nvp)
{

  bool bRet = false;
  string name, szValue;
  size_t pos, npos, vpos;
  size_t bpos = 0;
  
  // comes in as "name = value" "name= value" or "name =value"
  npos=pos=serialized_nvp.find("=");
  
  // only try to process if the string is valid 
  if ( pos != string::npos  )
  {
    while (npos > 0 && serialized_nvp[npos-1] == ' ')
    {
      npos--;
    }
    while (bpos < npos && serialized_nvp[bpos] == ' ')
    {
      bpos++;
    }
    name = serialized_nvp.substr(bpos, npos - bpos);

    vpos=pos+1;
    while (serialized_nvp[vpos] == ' ')
    {
      vpos++;
    }

    szValue = serialized_nvp.substr(vpos);

	if ( name[0] == '\'' ) {
		// We don't handle quoted attribute names for caching here.
		// Hand the name-value-pair off to the parser as a one-attribute
		// ad and merge the results into this ad.
		ClassAdParser parser;
		ClassAd new_ad;
		name = "[" + serialized_nvp + "]";
		if ( parser.ParseClassAd( name, new_ad, true ) ) {
			return Update( new_ad );
		} else {
			return false;
		}
	}

    // here is the special logic to check
    CachedExprEnvelope * cache_check = NULL;
	if ( doExpressionCaching ) {
		cache_check = CachedExprEnvelope::check_hit( name, szValue );
	}
    if ( cache_check ) 
    {
	ExprTree * in = cache_check;
	bRet = Insert( name, in, false );
    }
    else
    {
      ClassAdParser parser;
      ExprTree * newTree=0;

      // we did not hit in the cache... parse the expression
      newTree = parser.ParseExpression(szValue);

      if ( newTree )
      {
		// if caching is enabled, and we got to here then we know that the 
		// cache doesn't already have an entry for this name:value, so add
		// it to the cache now. 
		if (doExpressionCaching) {
			newTree = CachedExprEnvelope::cache(name, szValue, newTree);
		}
		bRet = Insert(name, newTree, false);
      }

    }
    
  } // end if pos != string::npos

  return bRet;
}


bool ClassAd::Insert( const std::string& attrName, ClassAd *& expr, bool cache )
{
    ExprTree * tree = expr;
    bool bRet =  Insert( attrName, tree, cache );
    
    expr = (ClassAd *)tree;
    return (bRet);
	
}

bool ClassAd::Insert( const std::string& attrName, ExprTree *& pRef, bool cache )
{
	bool bRet = false;
	ExprTree * tree = pRef;
	const std::string * pstrAttr = &attrName;
#ifndef WIN32
	std::string strName; // in case we want to insert attrName into the cache
#endif
	
		// sanity checks
	if( attrName.empty() ) {
		CondorErrno = ERR_MISSING_ATTRNAME;
		CondorErrMsg= "no attribute name when inserting expression in classad";
		return( false );
	}
	if( !pRef ) {
		CondorErrno = ERR_BAD_EXPRESSION;
		CondorErrMsg = "no expression when inserting attribute in classad";
		return( false );
	}

	if (doExpressionCaching && cache)
	{
	  std::string empty; // an empty string to tell the cache to unparse the pRef
#ifndef WIN32
	  // when inserting an attrib into the cache, strName can get overwritten
	  // by the string sharing code.  if it does we want to use strName from now on.
	  strName = attrName; pstrAttr = &strName;
	  tree = CachedExprEnvelope::cache(strName, empty, pRef);
#else
	  // std:string based string sharing is disabled on windows.
	  tree = CachedExprEnvelope::cache(attrName, empty, pRef);
#endif
	  // what goes in may be destroyed in preference for cache.
	  pRef = (ExprTree *)tree->self(); 
	}
	
	if (tree)
	{
		
		// parent of the expression is this classad
		tree->SetParentScope( this );
				
		pair<AttrList::iterator,bool> insert_result =
			attrList.insert( AttrList::value_type(*pstrAttr,tree) );

		if( !insert_result.second ) {
				// replace existing value
			delete insert_result.first->second;
			insert_result.first->second = tree;
		}

		MarkAttributeDirty(*pstrAttr);

		bRet = true;
	}
	
	return( bRet );
}


bool ClassAd::
DeepInsert( ExprTree *scopeExpr, const string &name, ExprTree *tree )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->Insert( name, tree ) );
}
// --- end expression insertion

// --- begin STL-like functions
ClassAd::iterator ClassAd::
find(string const& attrName)
{
    return attrList.find(attrName);
}
 
ClassAd::const_iterator ClassAd::
find(string const& attrName) const
{
    return attrList.find(attrName);
}
// --- end STL-like functions

// --- begin lookup methods
ExprTree *ClassAd::
Lookup( const string &name ) const
{
	ExprTree *tree;
	AttrList::const_iterator itr;

	itr = attrList.find( name );
	if (itr != attrList.end()) {
		tree = itr->second;
	} else if (chained_parent_ad != NULL) {
		tree = chained_parent_ad->Lookup(name);
	} else {
		tree = NULL;
	}
	return tree;
}


ExprTree *ClassAd::
LookupInScope( const string &name, const ClassAd *&finalScope ) const
{
	EvalState	state;
	ExprTree	*tree;
	int			rval;

	state.SetScopes( this );
	rval = LookupInScope( name, tree, state );
	if( rval == EVAL_OK ) {
		finalScope = state.curAd;
		return( tree );
	}

	finalScope = NULL;
	return( NULL );
}


int ClassAd::
LookupInScope(const string &name, ExprTree*& expr, EvalState &state) const
{
	extern int exprHash( const ExprTree* const&, int );
	const ClassAd *current = this, *superScope;
	Value			val;

	expr = NULL;

	while( !expr && current ) {

		// lookups/eval's being done in the 'current' ad
		state.curAd = current;

		// lookup in current scope
		if( ( expr = current->Lookup( name ) ) ) {
			return( EVAL_OK );
		}

		superScope = current->parentScope;
		if(strcasecmp(name.c_str( ),"toplevel")==0 || 
				strcasecmp(name.c_str( ),"root")==0){
			// if the "toplevel" attribute was requested ...
			expr = (ClassAd*)state.rootAd;
			if( expr == NULL ) {	// NAC - circularity so no root
				return EVAL_FAIL;  	// NAC
			}						// NAC
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), "self" ) == 0 ) {
			// if the "self" ad was requested
			expr = (ClassAd*)state.curAd;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), "parent" ) == 0 ) {
			// the lexical parent
			expr = (ClassAd*)state.curAd->parentScope;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else {
			// continue searching from the superScope ...
			current = superScope;
			if( current == this ) {		// NAC - simple loop checker
				return( EVAL_UNDEF );
			}
		}
	}	

	return( EVAL_UNDEF );
}
// --- end lookup methods



// --- begin deletion methods 
bool ClassAd::
Delete( const string &name )
{
	bool deleted_attribute;

    deleted_attribute = false;
	AttrList::iterator itr = attrList.find( name );
	if( itr != attrList.end( ) ) {
		delete itr->second;
		attrList.erase( itr );
		deleted_attribute = true;
	}
	// If the attribute is in the chained parent, we delete define it
	// here as undefined, whether or not it was defined here.  This is
	// behavior copied from old ClassAds. It's also one reason you
	// probably don't want to use this feature in the future.
	if (chained_parent_ad != NULL &&
		chained_parent_ad->Lookup(name) != NULL) {
		Value undefined_value;
		
		undefined_value.SetUndefinedValue();
		deleted_attribute = true;
	
		ExprTree* plit  = Literal::MakeLiteral( undefined_value );
	
		Insert(name, plit);
	}

	if (!deleted_attribute) {
		CondorErrno = ERR_MISSING_ATTRIBUTE;
		CondorErrMsg = "attribute " + name + " not found to be deleted";
	}

	return deleted_attribute;
}

bool ClassAd::
DeepDelete( ExprTree *scopeExpr, const string &name )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->Delete( name ) );
}
// --- end deletion methods



// --- begin removal methods
ExprTree *ClassAd::
Remove( const string &name )
{
	ExprTree *tree;

	tree = NULL;
	AttrList::iterator itr = attrList.find( name );
	if( itr != attrList.end( ) ) {
		tree = itr->second;
		attrList.erase( itr );
		tree->SetParentScope( NULL );
	}

	// If the attribute is in the chained parent, we delete define it
	// here as undefined, whether or not it was defined here.  This is
	// behavior copied from old ClassAds. It's also one reason you
	// probably don't want to use this feature in the future.
	if (chained_parent_ad != NULL &&
		chained_parent_ad->Lookup(name) != NULL) {

		if (tree == NULL) {
			tree = chained_parent_ad->Lookup(name);
		}
		
		Value undefined_value;
		undefined_value.SetUndefinedValue();
		ExprTree* plit  = Literal::MakeLiteral( undefined_value );
		Insert(name, plit);
	}
	return tree;
}

ExprTree *ClassAd::
DeepRemove( ExprTree *scopeExpr, const string &name )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( (ExprTree*)NULL );
	return( ad->Remove( name ) );
}
// --- end removal methods


void ClassAd::
_SetParentScope( const ClassAd* )
{
	// already set by base class for this node; we shouldn't propagate 
	// the call to sub-expressions because this is a new scope
}


bool ClassAd::
Update( const ClassAd& ad )
{
	AttrList::const_iterator itr;
	for( itr=ad.attrList.begin( ); itr!=ad.attrList.end( ); itr++ ) {
		ExprTree * cpy = itr->second->Copy();
		if(!Insert( itr->first, cpy, false)) {
			return false;
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
		if( ( ctx = _GetDeepScope( (ExprTree*) expr ) ) == NULL ) {
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
		ExprListIterator	itor;
		const char			*attrName;

			// make a first pass to check that it is a list of strings ...
		if( !expr->Evaluate( val ) || !val.IsListValue( list ) ) {
			return;
		}
		itor.Initialize( list );
		while( ( expr = itor.CurrentExpr( ) ) ) {
			if( !expr->Evaluate( val ) || !val.IsStringValue( attrName ) ) {
				return;
			}
			itor.NextExpr( );
		}

			// now go through and delete all the named attributes ...
		itor.Initialize( list );
		while( ( expr = itor.CurrentExpr( ) ) ) {
			if( expr->Evaluate( val ) && val.IsStringValue( attrName ) ) {
				ctx->Delete( attrName );
			}
			itor.NextExpr( );
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

	newAd->DisableDirtyTracking();

	AttrList::const_iterator	itr;
	for( itr=attrList.begin( ); itr != attrList.end( ); itr++ ) {
		if( !( tree = itr->second->Copy( ) ) ) {
			delete newAd;
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
			return( NULL );
		}
		newAd->Insert(itr->first,tree,false);
	}
	newAd->EnableDirtyTracking();
	return newAd;
}


bool ClassAd::
_Evaluate( EvalState&, Value& val ) const
{
	val.SetClassAdValue( (ClassAd*)this );
	return( this );
}


bool ClassAd::
_Evaluate( EvalState&, Value &val, ExprTree *&tree ) const
{
	val.SetClassAdValue( (ClassAd*)this );
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
		if( !itr->second->Flatten( state, eval, etree ) ) {
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
EvaluateAttr( const string &attr , Value &val ) const
{
	EvalState	state;
	ExprTree	*tree;

	state.SetScopes( this );
	switch( LookupInScope( attr, tree, state ) ) {	
		case EVAL_FAIL:
			return false;

		case EVAL_OK:
			return( tree->Evaluate( state, val ) );

		case EVAL_UNDEF:
			val.SetUndefinedValue( );
			return( true );

		case EVAL_ERROR:
			val.SetErrorValue( );
			return( true );

		default:
			return false;
	}
}

bool ClassAd::
EvaluateExpr( const string& buf, Value &result ) const
{
	bool           successfully_evaluated;
	ExprTree       *tree;
	ClassAdParser  parser;

	tree = NULL;
	if (parser.ParseExpression(buf, tree)) {
		successfully_evaluated = EvaluateExpr(tree, result);
	} else {
		successfully_evaluated = false;
	}

	if (NULL != tree) {
		delete tree;
	}

	return successfully_evaluated;
}


bool ClassAd::
EvaluateExpr( const ExprTree *tree , Value &val ) const
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Evaluate( state , val ) );
}

bool ClassAd::
EvaluateExpr( const ExprTree *tree , Value &val , ExprTree *&sig ) const
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Evaluate( state , val , sig ) );
}

bool ClassAd::
EvaluateAttrInt( const string &attr, int &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrInt( const string &attr, long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrInt( const string &attr, long long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrReal( const string &attr, double &r )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsRealValue( r ) );
}

bool ClassAd::
EvaluateAttrNumber( const string &attr, int &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const string &attr, long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const string &attr, long long &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const string &attr, double &r )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( r ) );
}

bool ClassAd::
EvaluateAttrString( const string &attr, char *buf, int len ) const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsStringValue( buf, len ) );
}

bool ClassAd::
EvaluateAttrString( const string &attr, string &buf ) const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsStringValue( buf ) );
}

bool ClassAd::
EvaluateAttrBool( const string &attr, bool &b ) const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsBooleanValue( b ) );
}

#if 0
// disabled (see header)
bool ClassAd::
EvaluateAttrClassAd( const string &attr, ClassAd *&classad ) const
{
	Value val;
		// TODO: filter out shared_ptr<ClassAd> values that would
		// go out of scope here (if such a thing is ever added),
		// or return a shared_ptr and make a copy here of the
		// ClassAd if it is not already managed by a shared_ptr.
	return( EvaluateAttr( attr, val ) && val.IsClassAdValue( classad ) );
}
#endif

#if 0
// disabled (see header)
bool ClassAd::
EvaluateAttrList( const string &attr, ExprList *&l ) const
{
    Value val;
		// This version of EvaluateAttrList() can only succeed
		// if the result is LIST_VALUE, not SLIST_VALUE, because
		// the shared_ptr<ExprList> goes out of scope before
		// returning to the caller.  Either do as below and filter
		// out SLIST_VALUE, or return a shared_ptr and create a
		// copy of the list here if it is not already managed
		// by a shared_ptr.
	return( EvaluateAttr( attr, val ) && val.GetType() == LIST_VALUE && val.IsListValue( l ) );
}
#endif

bool ClassAd::
GetExternalReferences( const ExprTree *tree, References &refs, bool fullNames )
{
    EvalState       state;

    state.rootAd = this; 
	state.curAd = this;

    return( _GetExternalReferences( tree, this, state, refs, fullNames ) );
}


bool ClassAd::
_GetExternalReferences( const ExprTree *expr, ClassAd *ad, 
	EvalState &state, References& refs, bool fullNames )
{
    switch( expr->GetKind( ) ) {
        case LITERAL_NODE:
                // no external references here
            return( true );

        case ATTRREF_NODE: {
            const ClassAd   *start;
            ExprTree        *tree, *result;
            string          attr;
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
                        string fullName;
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
            switch( start->LookupInScope( attr, result, state ) ) {
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

                    bool rval=_GetExternalReferences(result,ad,state,refs,fullNames);

                    state.depth_remaining++;
					state.curAd = curAd;
					return( rval );
				}

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
            string                      fnName;
            vector<ExprTree*>           args;
            vector<ExprTree*>::iterator itr;

            ((const FunctionCall*)expr)->GetComponents( fnName, args );
            for( itr = args.begin( ); itr != args.end( ); itr++ ) {
                if( !_GetExternalReferences( *itr, ad, state, refs, fullNames ) ) {
					return( false );
				}
            }
            return( true );
        }


        case CLASSAD_NODE: {
                // recurse on subtrees
            vector< pair<string, ExprTree*> >           attrs;
            vector< pair<string, ExprTree*> >::iterator itr;

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
            vector<ExprTree*>           exprs;
            vector<ExprTree*>::iterator itr;

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
			return _GetExternalReferences( ((CachedExprEnvelope*)expr)->get(), ad, state, refs, fullNames );
		}

        default:
            return false;
    }
}

bool ClassAd::
GetExternalReferences( const ExprTree *tree, PortReferences &refs )
{
    EvalState       state;

    state.rootAd = this; 
    state.curAd = tree->GetParentScope( );
	if( !state.curAd ) state.curAd = this;
	
    return( _GetExternalReferences( tree, this, state, refs ) );
}

bool ClassAd::
_GetExternalReferences( const ExprTree *expr, ClassAd *ad, 
	EvalState &state, PortReferences& refs )
{
    switch( expr->GetKind( ) ) {
        case LITERAL_NODE:
                // no external references here
            return( true );

        case ATTRREF_NODE: {
            const ClassAd   *start;
            ExprTree        *tree, *result;
            string          attr;
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
            switch( start->LookupInScope( attr, result, state ) ) {
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
					bool rval=_GetExternalReferences(result,ad,state,refs);
					state.curAd = curAd;
					return( rval );
				}

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
            string                      fnName;
            vector<ExprTree*>           args;
            vector<ExprTree*>::iterator itr;

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
            vector< pair<string, ExprTree*> >           attrs;
            vector< pair<string, ExprTree*> >::iterator itr;

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
            vector<ExprTree*>           exprs;
            vector<ExprTree*>::iterator itr;

            ((const ExprList*)expr)->GetComponents( exprs );
            for( itr = exprs.begin( ); itr != exprs.end( ); itr++ ) {
                if( !_GetExternalReferences( *itr, ad, state, refs ) ) {
					return( false );
				}
            }
            return( true );
        }

		case EXPR_ENVELOPE: {
			return _GetExternalReferences( ( (CachedExprEnvelope*)expr )->get(), ad, state, refs );
		}

        default:
            return false;
    }
}


bool ClassAd::
GetInternalReferences( const ExprTree *tree, References &refs, bool fullNames)
{
    EvalState state;
    state.rootAd = this;
    state.curAd = this;

    return( _GetInternalReferences( tree, this, state, refs, fullNames) );
}

//this is closely modelled off of _GetExternalReferences in the new_classads.
bool ClassAd::
_GetInternalReferences( const ExprTree *expr, ClassAd *ad,
        EvalState &state, References& refs, bool fullNames)
{

    switch( expr->GetKind() ){
        //nothing to be found here!
        case LITERAL_NODE:{
            return true;
        break;
                          }

        case ATTRREF_NODE:{
            const ClassAd   *start;
            ExprTree        *tree, *result;
            string          attr;
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
                if (!tree->Evaluate(state, val) ) {
                    return false;
                }

                /*
                 *
                 * [
                 * A = 3;
                 * B = { 1, 3, 5};
                 * C = D.A + 1;
                 * D = [ A = E; B = 4; ];
                 * E = 4;
                 * ];
                 */

                if( val.IsUndefinedValue() ) {
                    return true;
                }

                string nameToAddToRefs = "";

                string prefixStr;

                if(tree != NULL)
                {
                    ClassAdUnParser unparser;
                    //TODO: figure out how this would handle "B.D.G"
                    unparser.Unparse(prefixStr, tree);
                    //fullNameCpy = prefixStr;
                    nameToAddToRefs = prefixStr;

                    if(fullNames)
                    {
                        nameToAddToRefs += ".";
                        nameToAddToRefs += attr;
                    }
                }

                //WOULD INSERT "" IF tree == NULL! BAD! FIXME
                refs.insert(nameToAddToRefs);

                ExprTree *followRef;
                //TODO: If we get to this point, must there be a prefix?
                //  FIGURE OUT WHAT A SIMPLE / ABSOLUTE ATTR IS
                followRef = ad->Lookup(prefixStr);
                //this is checking to see if the prefix is in
                //  this classad or not.
                // if not, then we just let the loop continue
                if(followRef != NULL)
                {
                    return _GetInternalReferences(followRef, ad,
                                        state, refs, fullNames);
                }

                //otherwise, if the tree didn't evaluate to a classad,
                //we have a problemo, mon.
                //TODO: but why?
                if( !val.IsClassAdValue( start ) ) {
                    return false;
                }
            }

            const ClassAd *curAd = state.curAd;
            switch( start->LookupInScope( attr, result, state) ) {
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
                    refs.insert(attr);
                    if( state.depth_remaining <= 0 ) {
                        state.curAd = curAd;
                        return false;
                    }
                    state.depth_remaining--;

                    bool rval =_GetInternalReferences(result, ad, state, refs, fullNames);

                    state.depth_remaining++;
                    //TODO: Does this actually matter?
                    state.curAd = curAd;
                    return rval;
                break;
                                }

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
            string                          fnName;
            vector<ExprTree*>               args;
            vector<ExprTree*>::iterator     itr;

            ((const FunctionCall*)expr)->GetComponents(fnName, args);
            for( itr = args.begin(); itr != args.end(); itr++){
                if( !_GetInternalReferences( *itr, ad, state, refs, fullNames) ) {
                    return false;
                }
            }

            return true;
        break;
                          }

        case CLASSAD_NODE:{
            //also recurse on subtrees...
            vector< pair<string, ExprTree*> >               attrs;
            vector< pair<string, ExprTree*> >:: iterator    itr;

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
            vector<ExprTree*>               exprs;
            vector<ExprTree*>::iterator     itr;

            ((const ExprList*)expr)->GetComponents(exprs);
            for(itr = exprs.begin(); itr != exprs.end(); itr++){
                if( state.depth_remaining <= 0 ) {
                    return false;
                }
                state.depth_remaining--;

                bool ret = _GetInternalReferences(*itr, ad, state, refs, fullNames);

                state.depth_remaining++;
                if( !ret ) {
                    return false;
                }
            }

            return true;
        break;
            }
        
		case EXPR_ENVELOPE: {
			return _GetInternalReferences( ((CachedExprEnvelope*)expr)->get(), ad, state, refs,fullNames);
		}
           

        default:
            return false;

    }

}

#if defined( EXPERIMENTAL )

bool ClassAd::
AddRectangle( const ExprTree* tree, Rectangles &r, const string &allowed,
	const References&irefs )
{
	ExprTree		*ftree;
	bool			rval;
	Value			val;
	int				oldRid = r.rId;
	const ClassAd	*ad;

		// make rectangle from constraint
	ftree = NULL;
	if( !Flatten( tree, val, ftree ) ) {
		return( false );
	}
	if( !ftree ) {
			// hmm ... the requirements flattened to a single value
		bool b;
		if( !val.IsBooleanValue( b ) || !b ) return( false );

			// must be true; just increment the rectangle number.
			// typification will ensure that all exported attrs are
			// unconstrained.  Include imported attrs of course.
		r.NewRectangle( );
	} else {
		rval = _MakeRectangles( ftree, allowed, r, true );
		delete ftree;
		if( !rval ) {
			return( false );
		}
		ftree = NULL;
	}

		// project imported attributes to each rectangle created
	for( References::iterator itr=irefs.begin( );itr!=irefs.end( ); itr++ ) {
		if( !LookupInScope( *itr, ad ) ) {
				// attribute absent --- add to unimported lists
			for( int i = oldRid+1 ; i <= r.rId ; i++ ) {
				r.unimported[*itr].Insert( i );
			}
		} else if( !EvaluateAttr( *itr, val ) || val.IsExceptional() ||
				   val.IsClassAdValue() || val.IsListValue() ) {
				// not a literal; require verification at post-processing
			for( int i = oldRid+1 ; i <= r.rId ; i++ ) {
				r.AddDeviantImported( *itr, i );
			}
		} else {
			for( int i = oldRid+1 ; i <= r.rId ; i++ ) {
					// not open and not constraint; hence (false, false)
				if( r.AddUpperBound( *itr, val, false, false, i ) !=
					Rectangles::RECT_NO_ERROR || 
					r.AddLowerBound( *itr, val, false, false, i ) !=
					Rectangles::RECT_NO_ERROR ) {
					return( false );
				}
			}
		}
	}

	return( true );
}


bool ClassAd::
_MakeRectangles( const ExprTree *tree, const string &allowed, Rectangles &r, 
	bool ORmode )
{
    if( tree->GetKind( ) != OP_NODE ) return( false );

    Operation::OpKind	op;
    ExprTree 			*t1, *t2, *lit, *attr;
	string				attrName;
	ExprTree			*expr;
	bool				absolute;

    ((Operation*)tree)->GetComponents( op, t1, t2, lit );
	lit = NULL;

	if( op == Operation::PARENTHESES_OP ) {
		return( _MakeRectangles( t1, allowed, r, ORmode ) );
	}

		// let only ||, &&, <=, <, ==, is, >, >= through
	if( ( op!=Operation::LOGICAL_AND_OP && op!=Operation::LOGICAL_OR_OP ) && 
		( op<Operation::__COMPARISON_START__||op>Operation::__COMPARISON_END__||
			op == Operation::NOT_EQUAL_OP || op == Operation::ISNT_OP ) ) {
		return( false );
	}

	if( ORmode ) {
		if( op == Operation::LOGICAL_OR_OP ) {
				// continue recursively in OR mode
			return( _MakeRectangles( t1, allowed, r, ORmode ) &&
					_MakeRectangles( t2, allowed, r, ORmode ) );
		} else {
				// switch to AND mode (starting new rectangle)
			ORmode = false;
			r.NewRectangle( );
		}
	}

		// ASSERT:  In AND mode now (i.e., ORmode is false)
	if( op == Operation::LOGICAL_AND_OP ) {
		return( _MakeRectangles( t1, allowed, r, ORmode ) &&
				_MakeRectangles( t2, allowed, r, ORmode ) );
	} else if( op == Operation::LOGICAL_OR_OP ) {
			// something wrong
		printf( "Error:  Found || when making rectangles in AND mode\n" );
		return( false );
	}

	if( t1->GetKind( )==ExprTree::ATTRREF_NODE && 
			t2->GetKind( )==ExprTree::LITERAL_NODE ) {
			// ref <op> lit
		attr = t1;
		lit  = t2;
			// if not the attribute we're interested in, ignore
		if( !_CheckRef( attr, allowed ) ) return( true );
	} else if(t2->GetKind()==ExprTree::ATTRREF_NODE && 
			t1->GetKind()==ExprTree::LITERAL_NODE){
			// lit <op> ref
		attr = t2;
		lit  = t1;
			// if not the attribute we're interested in, ignore
		if( !_CheckRef( attr, allowed ) ) return( true );
		switch( op ) {
			case Operation::LESS_THAN_OP: 
				op = Operation::GREATER_THAN_OP; 
				break;
			case Operation::GREATER_THAN_OP: 
				op = Operation::LESS_THAN_OP; 
				break;
			case Operation::LESS_OR_EQUAL_OP: 
				op = Operation::GREATER_OR_EQUAL_OP; 
				break;
			case Operation::GREATER_OR_EQUAL_OP: 
				op = Operation::LESS_OR_EQUAL_OP; 
				break;
			default:
				break;
		}
	} else {
			// unknown:  add to verify exported
		r.AddDeviantExported( );
		return( true );
	}

		// literal must be a comparable value 
	Value	val;
	((Literal*)lit)->GetValue( val );
	if( val.IsExceptional( ) || val.IsClassAdValue( ) || val.IsListValue( ) ) {
		return( false );
	}

		// get the dimension name; assume already flattened
	((AttributeReference*)attr)->GetComponents( expr, attrName, absolute );
		
		// all these are constraint dimensions, so 'true' in the last arg
    switch( op ) {
        case Operation::LESS_THAN_OP:
			return( r.AddUpperBound( attrName, val, true, true ) ==
					Rectangles::RECT_NO_ERROR); // open

        case Operation::LESS_OR_EQUAL_OP:
			return( r.AddUpperBound( attrName, val, false, true ) ==
					Rectangles::RECT_NO_ERROR);//closed

        case Operation::EQUAL_OP:
        case Operation::IS_OP:
			return( r.AddUpperBound( attrName, val, false, true ) ==
						Rectangles::RECT_NO_ERROR &&//closed
					r.AddLowerBound( attrName, val, false, true ) ==
						Rectangles::RECT_NO_ERROR);//closed

		case Operation::GREATER_THAN_OP:
			return( r.AddLowerBound( attrName, val, true, true ) ==
					Rectangles::RECT_NO_ERROR); // open

        case Operation::GREATER_OR_EQUAL_OP:
			return( r.AddLowerBound( attrName, val, false, true ) ==
					Rectangles::RECT_NO_ERROR);//closed

        default:
            return( false );
    }

    return( false );
}

bool ClassAd::
_CheckRef( ExprTree *tree, const string &allowed )
{
	ExprTree 	*expr, *exprSub;
	string		attrName, attrNameSub;
	bool		absolute;

	if( !tree->GetKind() == ATTRREF_NODE ) return( false );
	((AttributeReference*)tree)->GetComponents( expr, attrName, absolute );
	if( absolute || !expr || expr->GetKind() != ATTRREF_NODE ) return( false );
	((AttributeReference*)expr)->GetComponents(exprSub, attrNameSub, absolute);
	if( exprSub || absolute ) return( false );
	if( strcasecmp( attrNameSub.c_str(), allowed.c_str() )!=0 ) return( false );

	return( true );
}


#endif  // EXPERIMENTAL


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

bool ClassAdIterator::
NextAttribute( string &attr, const ExprTree *&expr )
{
	if (!ad) return false;

	attr = "";
	expr = NULL;
	if( itr==ad->attrList.end( ) ) return( false );
	itr++;
	if( itr==ad->attrList.end( ) ) return( false );
	attr = itr->first;
	expr = itr->second;
	return( true );
}


bool ClassAdIterator::
CurrentAttribute (string &attr, const ExprTree *&expr) const
{
	if (!ad ) return( false );
	if( itr==ad->attrList.end( ) ) return( false );
	attr = itr->first;
	expr = itr->second;
	return true;	
}



void ClassAd::ChainToAd(ClassAd *new_chain_parent_ad)
{
	if (new_chain_parent_ad != NULL) {
		chained_parent_ad = new_chain_parent_ad;
	}
	return;
}

int ClassAd::PruneChildAd()
{
	int iRet =0;
	
	if (chained_parent_ad)
	{
		// loop through cleaning all expressions which are the same.
		AttrList::const_iterator	itr= attrList.begin( );
		ExprTree 					*tree;
	
		while (itr != attrList.end() )
		{
			tree = chained_parent_ad->Lookup(itr->first);
				
			if(  tree && tree->SameAs(itr->second) ) {
				AttrList::const_iterator rm_itr = itr;
				itr++; // once 
				// 1st remove from dirty list
				MarkAttributeClean(rm_itr->first);
				delete rm_itr->second;
				attrList.erase( rm_itr->first );
				iRet++;
			}
			else
			{
				itr++;
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

void ClassAd::MarkAttributeDirty(const string &name)
{
	if (do_dirty_tracking) {
		dirtyAttrList.insert(name);
	}
	return;
}

void ClassAd::MarkAttributeClean(const string &name)
{
	if (do_dirty_tracking) {
		dirtyAttrList.erase(name);
	}
	return;
}

bool ClassAd::IsAttributeDirty(const string &name) const
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
