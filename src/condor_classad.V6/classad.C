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
	domMan.getDomainSchema (domainName, domainIndex, schema);

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
				attrList[i].attrName = strdup( ad.attrList[i].attrName );
			} else {
				attrList[i].attrName = NULL;
			}
			attrList[i].expression = 
				ad.attrList[i].expression->copy(EXPR_DEEP_COPY);
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
			free( attrList[i].attrName );
			delete attrList[i].expression;
		}
	}
}


void ClassAd::
clear( )
{
	for (int i = 0; i < last; i++) {
		if (!attrList[i].valid) continue;

		if (attrList[i].attrName) free(attrList[i].attrName);
		if (attrList[i].expression) delete attrList[i].expression;

		attrList[i].valid = false;
	}
	last = 0;
}


bool ClassAd::
insert (char *name, ExprTree *tree)
{
	int			index;
	char		*attr = NULL;

	// sanity check
	if (!name || !tree) return false;

	// parent of the expression is this classad
	tree->setParentScope( this );

	// get rid of prefix or postfix white space
	name = strtok( name, " \t\n" );
	
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
			attr = strdup (name);
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
insert( char *name, char *expr, int len )
{
	static Source 	src;
	ExprTree 		*tree;

	if( len < 0 ) len = strlen( expr );	
	src.setSource( expr, len );
	return( src.parseExpression( tree ) && insert( name, tree ) );
}


ExprTree *ClassAd::
lookup (char *name)
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
lookupInScope( char* name, ClassAd *&ad ) 
{
	EvalState	state;
	ExprTree	*tree;
	int			rval;

	state.setScopes( this );
	rval = lookupInScope( name, tree, state );
	if( rval == EVAL_OK ) {
		ad = state.curAd;
		return( tree );
	}

	ad = NULL;
	return( NULL );
}


int ClassAd::
lookupInScope( char* name, ExprTree*& expr, EvalState &state )
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
		if( ( expr = current->lookup( name ) ) ) {
			return( EVAL_OK );
		}

		// not in current scope; try superScope
		if( !( super = lookup( "super" ) ) ) {
			// no explicit super attribute; get lexical parent
			superScope = current->parentScope;
		} else {
			// explicit super attribute
			if( !super->evaluate( state, val ) ) {
				return( EVAL_FAIL );
			}

			if( !val.isClassAdValue( superScope ) ) {
				return( val.isUndefinedValue( ) ? EVAL_UNDEF : EVAL_ERROR );
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
remove (char *name)
{
    int         index;

	// sanity check
	if (!name) return false;

    // get rid of prefix or postfix white space
    name = strtok( name, " \t\n" );

    // if the classad is domainized
    if (schema) {
        // operate on the common schema array
        index = schema->getCanonical (name);
    } else {
        // use the attrlist as a standard unordered list
        for (index = 0; index < last; index ++) {
			// Case insensitive to attribute names
            if( strcasecmp(attrList[index].attrName, name)==0 ) {
				free (attrList[index].attrName);
				attrList[last] = attrList[index];
				attrList[index]= attrList[last-1];
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


void ClassAd::
_setParentScope( ClassAd* )
{
	// already set by base class for this node; we shouldn't propagate 
	// the call to sub-expressions because this is a new scope
}


bool ClassAd::
toSink (Sink &s)
{
	char	*attrName;
	bool    first = true;

	if (!s.sendToSink ((void*)"[ ", 2)) return false;

	for (int index = 0; index < last; index++) {
		if (attrList[index].valid && attrList[index].attrName && 
			attrList[index].expression)
		{
			attrName=schema ? attrList[index].canonicalAttrName.getCharString()
							: attrList[index].attrName;

			// if this is not the first attribute, print the ";" separator
			if (!first) {
				if (!s.sendToSink ((void*)" ; ", 3)) return false;
			}

			if (!s.sendToSink ((void*)attrName, strlen(attrName))	||
				!s.sendToSink ((void*)" = ", 3)						||
				!(attrList[index].expression)->toSink(s))
					return false;

			first = false;
		}
	}

	return (s.sendToSink ((void*)" ] ", 3));
}


ExprTree* ClassAd::
_copy( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY ) {
		ClassAd	*newAd = new ClassAd();

		if( !newAd ) return NULL;
		newAd->nodeKind = CLASSAD_NODE;
		newAd->schema = schema;
		newAd->last = last;
		newAd->parentScope = parentScope;

		for	( int i = 0 ; i < last ; i++ ) {
			if( attrList[i].valid ) {
				if( attrList[i].attrName ) {
					newAd->attrList[i].attrName=strdup( attrList[i].attrName );
				} else {
					newAd->attrList[i].attrName = NULL;
				}
				newAd->attrList[i].expression = 
					attrList[i].expression->copy( EXPR_DEEP_COPY );
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
	} else if( cm == EXPR_REF_COUNT ) {
		// reference counted copy
		for( int i = 0 ; i < last ; i++ ) {
			if( attrList[i].valid ) {
				attrList[i].expression->copy( EXPR_REF_COUNT );
			}
		}
		return this;
	}

	// will not get here
	return 0;
}


bool ClassAd::
_evaluate( EvalState&, Value& val )
{
	val.setClassAdValue( this );
	return( this );
}


bool ClassAd::
_evaluate( EvalState&, Value &val, ExprTree *&tree )
{
	val.setClassAdValue( this );
	return( !( tree = copy( ) ) );
}


bool ClassAd::
_flatten( EvalState& state, Value&, ExprTree*& tree, OpKind* ) 
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
				newAd->attrList[i].attrName = strdup( attrList[i].attrName );
			} else {
				newAd->attrList[i].attrName = NULL;
			}

			// flatten expression
			if( !attrList[i].expression->flatten( state, eval, etree ) ) {
				delete newAd;
				tree = NULL;
				eval.clear();
				state.curAd = oldAd;
				return false;
			}

			// if a value was obtained, convert it to a literal
			if( !etree ) {
				etree = Literal::makeLiteral( eval );
				if( !etree ) {
					delete newAd;
					tree = NULL;
					eval.clear();
					state.curAd = oldAd;
					return false;
				}
			}

			newAd->attrList[i].expression = etree;
			newAd->attrList[i].valid = true;
		} else {
			newAd->attrList[i].valid = false;
		}

		eval.clear();
	}

	newAd->last = last;
	tree = newAd;
	state.curAd = oldAd;
	return true;
}


ClassAd *ClassAd::
fromSource (Source &s)
{
	ClassAd	*ad = new ClassAd;

	if (!ad) return NULL;
	if (!s.parseClassAd (ad)) {
		delete ad;
		return (ClassAd*)NULL;
	}

	return ad;
}

ClassAd *ClassAd::
augmentFromSource (Source &s, ClassAd &ad)
{
    return( s.parseClassAd(&ad) ? &ad : 0 );
}


bool ClassAd::
evaluateAttr( char *attr , Value &val )
{
	EvalState	state;
	ExprTree	*tree;

	state.setScopes( this );
	switch( lookupInScope( attr, tree, state ) ) {	
		case EVAL_FAIL:
			return false;

		case EVAL_OK:
			return( tree->evaluate( state , val ) );

		case EVAL_UNDEF:
			val.setUndefinedValue( );
			return( true );

		case EVAL_ERROR:
			val.setErrorValue( );
			return( true );

		default:
			return false;
	}
}


bool ClassAd::
evaluateExpr( char *expr , Value &val , int len )
{
	Source		src;
	ExprTree	*tree;
	bool		rval;

	if( len == -1 ) len = strlen( expr );
	src.setSource( expr , len );
	src.parseExpression( tree );
	if( !tree ) {
		val.setErrorValue();
		return false;
	}
	rval = evaluateExpr( tree , val );
	delete tree;
	return( rval );	
}


bool ClassAd::
evaluateExpr( ExprTree *tree , Value &val )
{
	EvalState	state;

	state.setScopes( this );
	return( tree->evaluate( state , val ) );
}

bool ClassAd::
evaluateExpr( ExprTree *tree , Value &val , ExprTree *&sig )
{
	EvalState	state;

	state.setScopes( this );
	return( tree->evaluate( state , val , sig ) );
}

bool ClassAd::
evaluateAttrInt( char *attr, int &i ) 
{
	Value val;
	return( evaluateAttr( attr, val ) && val.isIntegerValue( i ) );
}

bool ClassAd::
evaluateAttrReal( char *attr, double &r ) 
{
	Value val;
	return( evaluateAttr( attr, val ) && val.isRealValue( r ) );
}

bool ClassAd::
evaluateAttrNumber( char *attr, int &i ) 
{
	Value val;
	return( evaluateAttr( attr, val ) && val.isNumber( i ) );
}

bool ClassAd::
evaluateAttrNumber( char *attr, double &r ) 
{
	Value val;
	return( evaluateAttr( attr, val ) && val.isNumber( r ) );
}

bool ClassAd::
evaluateAttrString( char *attr, char *buf, int len )
{
	Value val;
	return( evaluateAttr( attr, val ) && val.isStringValue( buf, len ) );
}

bool ClassAd::
evaluateAttrString( char *attr, char *&str )
{
	Value val;
	return( evaluateAttr( attr, val ) && val.isStringValue( str ) );
}

bool ClassAd::
flatten( ExprTree *tree , Value &val , ExprTree *&fexpr )
{
	EvalState	state;

	state.setScopes( this );
	return( tree->flatten( state , val , fexpr ) );
}

bool ClassAdIterator::
nextAttribute (char *&attr, ExprTree *&expr)
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
previousAttribute (char *&attr, ExprTree *&expr)
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
currentAttribute (char *&attr, ExprTree *&expr)
{
	if (!ad || !ad->attrList[index].valid ) return false;

	attr = (ad->schema) ? ad->attrList[index].canonicalAttrName.getCharString()
					: ad->attrList[index].attrName;
    expr = ad->attrList[index].expression;
	return true;	
}
