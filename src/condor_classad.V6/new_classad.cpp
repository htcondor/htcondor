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

using namespace std;

extern "C" void to_lower (char *);	// from util_lib (config.c)

BEGIN_NAMESPACE( classad )

// This is probably not the best place to put these. However, 
// I am reconsidering how we want to do errors, and this may all
// change in any case. 
#ifdef CLASSAD_DISTRIBUTION
string CondorErrMsg;
int CondorErrno;
#endif

void ClassAdLibraryVersion(int &major, int &minor, int &patch)
{
    major = 1;
    minor = 0;
    patch = 1;
    return;
}

void ClassAdLibraryVersion(string &version_string)
{
    version_string = "1.0.1";
    return;
}

ClassAd::
ClassAd ()
{
	nodeKind = CLASSAD_NODE;
	EnableDirtyTracking();
	chained_parent_ad = NULL;
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
	if (this == &ad) {
		succeeded = false;
	} else {
		Clear( );
        ExprTree::CopyFrom(ad);
		chained_parent_ad = ad.chained_parent_ad;
		
		DisableDirtyTracking();
		for( itr = ad.attrList.begin( ); itr != ad.attrList.end( ); itr++ ) {
			if( !( tree = itr->second->Copy( ) ) ) {
				Clear( );
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
                succeeded = false;
                break;
			}
			tree->SetParentScope(this); // ajr
			attrList[itr->first] = tree;
		}
		EnableDirtyTracking();
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

    if (this == tree) {
        is_same = true;
   } else if (tree->GetKind() != CLASSAD_NODE) {
        is_same = false;
   } else {
       ClassAd *other_classad;

       other_classad = (ClassAd *) tree;

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


ClassAd *ClassAd::
MakeClassAd( vector< pair< string, ExprTree* > > &attrs )
{
	vector< pair<string, ExprTree*> >::iterator	itr;
	ClassAd *newAd = new ClassAd( );

	if( !newAd ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( NULL );
	};

	for( itr = attrs.begin( ); itr != attrs.end( ); itr++ ) {
		if( !newAd->Insert( itr->first, itr->second ) ) {
			delete newAd;
			return( NULL );
		}
		itr->first = "";
		itr->second = NULL;
	}
	return( newAd );
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
	Value val;
	val.SetIntegerValue( value );
	return( Insert( name, Literal::MakeLiteral( val, f ) ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, int value, 
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
	Value val;
	val.SetRealValue( value );
	return( Insert( name, Literal::MakeLiteral( val, f ) ) );
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
	Value val;
	val.SetBooleanValue( value );
	return( Insert( name, Literal::MakeLiteral( val ) ) );
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
	// We could do a cast and call InsertAttr() again, but
	// we'll avoid a copy if avoid the cast.
	Value val;
	val.SetStringValue( value );
	return( Insert( name, Literal::MakeLiteral( val ) ) );
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
	Value val;
	val.SetStringValue( value );
	return( Insert( name, Literal::MakeLiteral( val ) ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, const string &value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end string attribute insertion



// --- begin expression insertion 
bool ClassAd::
Insert( const string &name, ExprTree *tree )
{
		// sanity checks
	if( name == "" ) {
		CondorErrno = ERR_MISSING_ATTRNAME;
		CondorErrMsg= "no attribute name when inserting expression in classad";
		return( false );
	}
	if( !tree ) {
		CondorErrno = ERR_BAD_EXPRESSION;
		CondorErrMsg = "no expression when inserting attribute " + name +
				" in classad";
		return( false );
	}

	// parent of the expression is this classad
	tree->SetParentScope( this );

		// check if attribute already exists in classad
	AttrList::iterator itr = attrList.find( name );
	if( itr != attrList.end( ) ) {
			// delete old expression and replace
		delete itr->second;
	}
	attrList[name] = tree;

	MarkAttributeDirty(name);
        
	return( true );
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
	ClassAd 		*current = (ClassAd*)this, *superScope;
	Value			val;

	expr = NULL;

	while( !expr && current ) {

		// lookups/eval's being done in the 'current' ad
		state.curAd = current;

		// lookup in current scope
		if( ( expr = current->Lookup( name ) ) ) {
			return( EVAL_OK );
		}

		superScope = (ClassAd*)current->parentScope;
		if(strcasecmp(name.c_str( ),"toplevel")==0 || 
				strcasecmp(name.c_str( ),"root")==0){
			// if the "toplevel" attribute was requested ...
			expr = state.rootAd;
			if( expr == NULL ) {	// NAC - circularity so no root
				return EVAL_FAIL;  	// NAC
			}						// NAC
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), "self" ) == 0 ) {
			// if the "self" ad was requested
			expr = state.curAd;
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
		Insert(name, Literal::MakeLiteral(undefined_value));
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
		Insert(name, Literal::MakeLiteral(undefined_value));
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
		if(!Insert( itr->first, itr->second->Copy( ) )) {
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
	newAd->nodeKind = CLASSAD_NODE;
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
		tree->SetParentScope(newAd); // ajr
		newAd->attrList[itr->first] = tree;
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
	ClassAd		*oldAd;
	AttrList::const_iterator	itr;

	tree = NULL; // Just to be safe...  wenger 2003-12-11.

	oldAd = state.curAd;
	state.curAd = (ClassAd*)this;

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

bool ClassAd::
EvaluateAttrClassAd( const string &attr, ClassAd *&classad ) const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsClassAdValue( classad ) );
}

bool ClassAd::
EvaluateAttrList( const string &attr, ExprList *&l ) const
{
    Value val;
	return( EvaluateAttr( attr, val ) && val.IsListValue( l ) );
}

bool ClassAd::
GetExternalReferences( const ExprTree *tree, References &refs, bool fullNames )
{
    EvalState       state;

    state.rootAd = this; 
    state.curAd = (ClassAd*)tree->GetParentScope( );
	if( !state.curAd ) state.curAd = this;
	
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
            ClassAd   		*start;
            ExprTree        *tree, *result;
            string          attr;
            Value           val;
            bool            abs;

            ((AttributeReference*)expr)->GetComponents( tree, attr, abs );
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
                        return( _GetExternalReferences( tree, ad, state, refs, fullNames ));
                    }
                }
                    // otherwise, if the tree didn't evaluate to a classad,
                    // we have a problem
                if( !val.IsClassAdValue( start ) )  {
                    return( false );
                }
            }
                // lookup for attribute
			ClassAd *curAd = state.curAd;
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
					bool rval=_GetExternalReferences(result,ad,state,refs,fullNames);
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
            ((Operation*)expr)->GetComponents( op, t1, t2, t3 );
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

            ((FunctionCall*)expr)->GetComponents( fnName, args );
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

            ((ClassAd*)expr)->GetComponents( attrs );
            for( itr = attrs.begin( ); itr != attrs.end( ); itr++ ) {
                if( !_GetExternalReferences( itr->second, ad, state, refs, fullNames )) {
					return( false );
				}
            }
            return( true );
        }


        case EXPR_LIST_NODE: {
                // recurse on subtrees
            vector<ExprTree*>           exprs;
            vector<ExprTree*>::iterator itr;

            ((ExprList*)expr)->GetComponents( exprs );
            for( itr = exprs.begin( ); itr != exprs.end( ); itr++ ) {
                if( !_GetExternalReferences( *itr, ad, state, refs, fullNames ) ) {
					return( false );
				}
            }
            return( true );
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
    state.curAd = (ClassAd*)tree->GetParentScope( );
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
            ClassAd   		*start;
            ExprTree        *tree, *result;
            string          attr;
            Value           val;
            bool            abs;
			PortReferences::iterator	pitr;

            ((AttributeReference*)expr)->GetComponents( tree, attr, abs );
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
			ClassAd *curAd = state.curAd;
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
            ((Operation*)expr)->GetComponents( op, t1, t2, t3 );
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

            ((FunctionCall*)expr)->GetComponents( fnName, args );
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

            ((ClassAd*)expr)->GetComponents( attrs );
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

            ((ExprList*)expr)->GetComponents( exprs );
            for( itr = exprs.begin( ); itr != exprs.end( ); itr++ ) {
                if( !_GetExternalReferences( *itr, ad, state, refs ) ) {
					return( false );
				}
            }
            return( true );
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

void ClassAd::Unchain(void)
{
	chained_parent_ad = NULL;
	return;
}

ClassAd *ClassAd::GetChainedParentAd(void)
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

bool ClassAd::IsAttributeDirty(const string &name)
{
	bool is_dirty;

	if (dirtyAttrList.find(name) != dirtyAttrList.end()) {
		is_dirty = true;
	} else {
		is_dirty = false;
	}
	return is_dirty;
}

END_NAMESPACE // classad
