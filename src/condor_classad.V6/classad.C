#include "condor_common.h"
#include "condor_string.h"
#include "classad.h"

extern "C" void to_lower (char *);	// from util_lib (config.c)

ClassAdDomainManager ClassAd::domMan;

Attribute::
Attribute ()
{
	valid = false;
	attrName = NULL;
	expression = NULL;
}


// start up with an attribute list of size 32
ClassAd::
ClassAd () : attrList( 32 )
{
	Attribute	dummy;

	nodeKind = CLASSAD_NODE;

	// initialize the attribute list
	dummy.valid = false;
	dummy.attrName = NULL;
	dummy.expression = NULL;

	attrList.fill (dummy);

	// initialize additional internal state
	schema = NULL;
	last = 0;
}


ClassAd::
ClassAd (char *domainName) : attrList( 32 )
{
	int domainIndex;
	Attribute attr;

	nodeKind = CLASSAD_NODE;

	// get pointer to schema and stash it for easy access
	domMan.GetDomainSchema (domainName, domainIndex, schema);

	// initialize other internal structures
	attr.valid = false;
	attr.attrName = NULL;
	attr.expression = NULL;
	attrList.fill (attr);
	last = 0;	
}


ClassAd::
ClassAd (const ClassAd &ad) : attrList( 32 )
{
	nodeKind = CLASSAD_NODE;
	schema = ad.schema;
	last = ad.last;
	parentScope = ad.parentScope;
	

	for( int i = 0 ; i < last ; i++ ) {
		if( ad.attrList[i].valid ) {
			if( ad.attrList[i].attrName ) {
				attrList[i].attrName = strnewp( ad.attrList[i].attrName );
			} else {
				attrList[i].attrName = NULL;
			}
			attrList[i].expression = 
				ad.attrList[i].expression->Copy();
			attrList[i].valid = true;
		} else {
			attrList[i].valid = false;
		}
	}
}	


ClassAd::
~ClassAd ()
{
	for( int i = 0 ; i < last ; i++ ) {
		if( attrList[i].valid && attrList[i].attrName ) {
			delete [] ( attrList[i].attrName );
			delete attrList[i].expression;
		}
	}
}


void ClassAd::
Clear( )
{
	for (int i = 0; i < last; i++) {
		if (!attrList[i].valid) continue;

		if (attrList[i].attrName) delete [] (attrList[i].attrName);
		if (attrList[i].expression) delete attrList[i].expression;

		attrList[i].valid = false;
	}
	last = 0;
}


bool ClassAd::
InsertAttr( const char* name, int value, NumberFactor f )
{
	Literal *lit = new Literal( );
	lit->SetIntegerValue( value, f );
	if( !Insert( name, lit ) ) {
		delete lit;
		return false;
	}
	return true;
}


bool ClassAd::
InsertAttr( const char* name, double value, NumberFactor f )
{
	Literal *lit = new Literal( );
	lit->SetRealValue( value, f );
	if( !Insert( name, lit ) ) {
		delete lit;
		return false;
	}
	return true;
}
	

bool ClassAd::
InsertAttr( const char *name, bool value )
{
	Literal *lit = new Literal( );
	lit->SetBooleanValue( value );
	if( !Insert( name, lit ) ) {
		delete lit;
		return false;
	}
	return true;
}


bool ClassAd::
InsertAttr( const char *name, const char *value, bool dup )
{
	const char *val = dup ? strnewp( value ) : value;
	Literal *lit = new Literal( );
	lit->SetStringValue( val );
	if( !Insert( name, lit ) ) {
		delete lit;
		return false;
	} 
	return true;
}

bool ClassAd::
Insert (const char *name, ExprTree *tree)
{
	int			index;
	char		*attr = NULL;

	// sanity check
	if (!name || !tree) return false;

	// parent of the expression is this classad
	tree->SetParentScope( this );

	// if the classad is domainized
	if (schema) {
		SSString s;

		// operate on the common schema array
		index = schema->getCanonical (name, s);
		attrList[index].canonicalAttrName.copy(s);
	} else {
		// use the attrlist as a standard unordered list
		for (index = 0; index < last; index ++) {
			// Case insensitive to attribute names
			if (strcasecmp(attrList[index].attrName, name) == 0)
				break;
		}
		if (index == last) {
			attr = strnewp (name);
			last++;
		}
	}

	// if inserting for the first time
	if (attrList[index].valid == false) {
		if (index > last) last = index+1;

		attrList[index].valid = true;
		attrList[index].attrName = attr;
		attrList[index].expression = tree;
		
		return true;
	}

	// if a tree was previously inserted here, delete it	
	if (attrList[index].expression) {
		delete attrList[index].expression;
	}
	
	// insert the new tree
	attrList[index].expression = tree;

	return true;
}


bool ClassAd::
Insert( const char *name, const char *expr, int len )
{
	static Source 	src;
	ExprTree 		*tree;

	src.SetSource( expr, len );
	return( src.ParseExpression( tree ) && Insert( name, tree ) );
}


ExprTree *ClassAd::
Lookup (const char *name)
{
	int			index;

	// sanity check
	if (!name) return NULL;

    // if the classad is domainized
    if (schema) {
        // operate on the common schema array
        index = schema->getCanonical (name);
    } else {
        // use the attrlist as a standard unordered list
        for (index = 0; index < last; index ++) {
			// Case insensitive to attribute names
            if (strcasecmp(attrList[index].attrName, name) == 0) 
				break;
        }
    }

	return( attrList[index].valid ? attrList[index].expression : 0 );
}


ExprTree *ClassAd::
LookupInScope( const char* name, ClassAd *&ad ) 
{
	EvalState	state;
	ExprTree	*tree;
	int			rval;

	state.SetScopes( this );
	rval = LookupInScope( name, tree, state );
	if( rval == EVAL_OK ) {
		ad = state.curAd;
		return( tree );
	}

	ad = NULL;
	return( NULL );
}


int ClassAd::
LookupInScope( const char* name, ExprTree*& expr, EvalState &state )
{
	extern int exprHash( ExprTree* const&, int );
	HashTable<ExprTree*,bool> superChase( 16, &exprHash );
	ClassAd 	*current = this, *superScope;
	ExprTree	*super;
	bool		visited = false;
	Value		val;

	expr = NULL;

	while( !expr && current ) {
		// lookups/eval's being done in the 'current' ad
		state.curAd = current;

		// have we already visited this scope?
		if( superChase.lookup( current, visited ) < 0 ) {
			// not yet visited until now --- mark as visited
			if( superChase.insert( current, true ) < 0 ) {
				return( EVAL_ERROR );
			}
		} else if( visited ) {
			// have already visited this scope
			return( EVAL_UNDEF );
		}

		// lookup in current scope
		if( ( expr = current->Lookup( name ) ) ) {
			return( EVAL_OK );
		}

		// not in current scope; try superScope
		if( !( super = current->Lookup( "super" ) ) ) {
			// no explicit super attribute; get lexical parent
			superScope = current->parentScope;
		} else {
			// explicit super attribute
			if( !super->Evaluate( state, val ) ) {
				return( EVAL_FAIL );
			}

			if( !val.IsClassAdValue( superScope ) ) {
				return( val.IsUndefinedValue( ) ? EVAL_UNDEF : EVAL_ERROR );
			}
		}

		// Case insensitive to "reserved keywords"
		if( strcasecmp( name, "super" ) == 0 ) {
			// if the "super" attribute was requested ...
			expr = superScope;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name, "toplevel" ) == 0 ) {
			// if the "toplevel" attribute was requested ...
			expr = state.rootAd;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name, "self" ) == 0 ) {
			// if the "self" ad was requested
			expr = state.curAd;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name, "parent" ) == 0 ) {
			// the lexical parent
			expr = state.curAd->parentScope;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else {
			// continue searching from the superScope ...
			current = superScope;
		}
	}	

	return( EVAL_UNDEF );
}


bool ClassAd::
Delete (const char *name)
{
    int         index;

	// sanity check
	if (name == NULL) return false;

    // if the classad is domainized
    if (schema) {
        // operate on the common schema array
        index = schema->getCanonical (name);
    } else {
        // use the attrlist as a standard unordered list
        for (index = 0; index < last; index ++) {
			// Case insensitive to attribute names
            if( strcasecmp(attrList[index].attrName, name)==0 ) {
				delete [] attrList[index].attrName;
				attrList[last] = attrList[index];
				attrList[index]= attrList[last-1];
				attrList[last-1].valid = false;
				attrList[last-1].attrName = NULL;
				attrList[last-1].expression = NULL;

				index = last;
				last --;
				
				break;
			}
        }
    }

	if (attrList[index].valid) {
		attrList[index].valid = false;
		delete attrList[index].expression;

		return true;
	}

	return false;
}


ExprTree *ClassAd::
Remove (const char *name)
{
    int         index;

	// sanity check
	if (name == NULL) return NULL;

    // if the classad is domainized
    if (schema) {
        // operate on the common schema array
        index = schema->getCanonical (name);
    } else {
        // use the attrlist as a standard unordered list
        for (index = 0; index < last; index ++) {
			// Case insensitive to attribute names
            if( strcasecmp(attrList[index].attrName, name)==0 ) {
				delete [] attrList[index].attrName;
				attrList[last] = attrList[index];
				attrList[index]= attrList[last-1];
				attrList[last-1].valid = false;
				attrList[last-1].attrName = NULL;
				attrList[last-1].expression = NULL;
				index = last;
				last --;
				
				break;
			}
        }
    }

	if (attrList[index].valid) {
		attrList[index].valid = false;
		return( attrList[index].expression );
	}

	return NULL;
}

void ClassAd::
_SetParentScope( ClassAd* )
{
	// already set by base class for this node; we shouldn't propagate 
	// the call to sub-expressions because this is a new scope
}


bool ClassAd::
ToSink( Sink &s )
{
	FormatOptions *p = s.GetFormatOptions( );

	const char *attrName;
	bool    first = true;
	int		indentLevel = p ? p->GetIndentLevel( ) : 0;
	int		indentLength;
	bool	wantIndentation;

	if( p ) {
		p->GetClassAdIndentation( wantIndentation, indentLength );
	} else {
		wantIndentation = false;
		indentLength = 0;
	}

	if( wantIndentation ) {
		if( !s.SendToSink( (void*)"\n", 1 ) || !p->Indent( s ) ) return false;
	}

	if (!s.SendToSink ((void*)"[", 1)) return false;

	if( wantIndentation ) {
		p->SetIndentLevel( indentLevel+indentLength );
	}

	for (int index = 0; index < last; index++) {
		if (attrList[index].valid && attrList[index].attrName && 
			attrList[index].expression)
		{
			attrName=schema ? attrList[index].canonicalAttrName.getCharString()
							: attrList[index].attrName;

			// if this is not the first attribute, print the ";" separator
			if (!first) {
				if (!s.SendToSink ((void*)";", 1)) return false;
			}

			// if indenting is required, indent the appropriate amount
			if( wantIndentation ) {
				if( !s.SendToSink( (void*)"\n", 1 ) || !p->Indent( s ) ) 
					return false;
			}

			if (!s.SendToSink ((void*)attrName, strlen(attrName))	||
				!s.SendToSink ((void*)" = ", 3) )
					return false;

			if( wantIndentation ) p->SetIndentLevel(indentLevel+2*indentLength);
			if( !(attrList[index].expression)->ToSink(s) ) return false;
			if( wantIndentation ) p->SetIndentLevel( indentLevel+indentLength );

			first = false;
		}
	}

	if( wantIndentation ) {
		if( !s.SendToSink( (void*)"\n", 1 ) ) return false;
		p->SetIndentLevel( indentLevel );
		if( !p->Indent( s ) ) return false;
	}

	return (s.SendToSink ((void*)"]", 1));
}


ClassAd* ClassAd::
Copy( )
{
	ClassAd	*newAd = new ClassAd();

	if( !newAd ) return NULL;
	newAd->nodeKind = CLASSAD_NODE;
	newAd->schema = schema;
	newAd->last = last;
	newAd->parentScope = parentScope;

	for	( int i = 0 ; i < last ; i++ ) {
		if( attrList[i].valid ) {
			if( attrList[i].attrName ) {
				newAd->attrList[i].attrName=strnewp( attrList[i].attrName );
			} else {
				newAd->attrList[i].attrName = NULL;
			}
			newAd->attrList[i].expression = 
				attrList[i].expression->Copy( );
			if( !newAd->attrList[i].expression ) {
				delete newAd;
				return NULL;
			}
			newAd->attrList[i].valid = true;
		} else {
			newAd->attrList[i].valid = false;
		}
	}

	return newAd;
}


bool ClassAd::
_Evaluate( EvalState&, Value& val )
{
	val.SetClassAdValue( this );
	return( this );
}


bool ClassAd::
_Evaluate( EvalState&, Value &val, ExprTree *&tree )
{
	val.SetClassAdValue( this );
	return( !( tree = Copy( ) ) );
}


bool ClassAd::
_Flatten( EvalState& state, Value&, ExprTree*& tree, OpKind* ) 
{
	ClassAd 	*newAd = new ClassAd();
	Value		eval;
	ExprTree	*etree;
	ClassAd		*oldAd;

	oldAd = state.curAd;
	state.curAd = this;

	for( int i = 0 ; i < last ; i++ ) {
		if( attrList[i].valid ) {
			// copy attribute name
			if( attrList[i].attrName ) {
				newAd->attrList[i].attrName = strnewp( attrList[i].attrName );
			} else {
				newAd->attrList[i].attrName = NULL;
			}

			// flatten expression
			if( !attrList[i].expression->Flatten( state, eval, etree ) ) {
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

			newAd->attrList[i].expression = etree;
			newAd->attrList[i].valid = true;
		} else {
			newAd->attrList[i].valid = false;
		}

		eval.Clear();
	}

	newAd->last = last;
	tree = newAd;
	state.curAd = oldAd;
	return true;
}


ClassAd *ClassAd::
FromSource (Source &s)
{
	ClassAd	*ad = new ClassAd;

	if (!ad) return NULL;
	if (!s.ParseClassAd (ad)) {
		delete ad;
		return (ClassAd*)NULL;
	}

	return ad;
}

ClassAd *ClassAd::
AugmentFromSource (Source &s, ClassAd &ad)
{
    return( s.ParseClassAd(ad) ? &ad : 0 );
}


bool ClassAd::
EvaluateAttr( const char *attr , Value &val )
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
EvaluateExpr( const char *expr , Value &val , int len )
{
	Source		src;
	ExprTree	*tree;
	bool		rval;

	src.SetSource( expr , len );
	src.ParseExpression( tree );
	if( !tree ) {
		val.SetErrorValue();
		return false;
	}
	rval = EvaluateExpr( tree , val );
	delete tree;
	return( rval );	
}


bool ClassAd::
EvaluateExpr( ExprTree *tree , Value &val )
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Evaluate( state , val ) );
}

bool ClassAd::
EvaluateExpr( ExprTree *tree , Value &val , ExprTree *&sig )
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Evaluate( state , val , sig ) );
}

bool ClassAd::
EvaluateAttrInt( const char *attr, int &i ) 
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrReal( const char *attr, double &r ) 
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsRealValue( r ) );
}

bool ClassAd::
EvaluateAttrNumber( const char *attr, int &i ) 
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const char *attr, double &r ) 
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( r ) );
}

bool ClassAd::
EvaluateAttrString( const char *attr, char *buf, int len, int &alen )
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsStringValue( buf, len, alen ) );
}

bool ClassAd::
EvaluateAttrString( const char *attr, char *&str )
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsStringValue( str ) );
}

bool ClassAd::
EvaluateAttrBool( const char *attr, bool &b )
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsBooleanValue( b ) );
}

bool ClassAd::
Flatten( ExprTree *tree , Value &val , ExprTree *&fexpr )
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Flatten( state , val , fexpr ) );
}

bool ClassAdIterator::
NextAttribute (const char *&attr, ExprTree *&expr)
{
	if (!ad) return false;

	index++;
	while (index < ad->last && !(ad->attrList[index].valid)) index++;
	if (index == ad->last) return false;
	attr = (ad->schema) ? ad->attrList[index].canonicalAttrName.getCharString()
					: ad->attrList[index].attrName;
	expr = ad->attrList[index].expression;
	return true;
}


bool ClassAdIterator::
PreviousAttribute (const char *&attr, ExprTree *&expr)
{
	if (!ad) return false;

    index--;
    while (index > -1 && !(ad->attrList[index].valid)) index--;
    if (index == -1) return false;
	attr = (ad->schema) ? ad->attrList[index].canonicalAttrName.getCharString()
					: ad->attrList[index].attrName;
    expr = ad->attrList[index].expression;
	return true;	
}


bool ClassAdIterator::
CurrentAttribute (const char *&attr, ExprTree *&expr)
{
	if (!ad || !ad->attrList[index].valid ) return false;

	attr = (ad->schema) ? ad->attrList[index].canonicalAttrName.getCharString()
					: ad->attrList[index].attrName;
    expr = ad->attrList[index].expression;
	return true;	
}
