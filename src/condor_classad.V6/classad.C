#include "condor_common.h"
#include "caseSensitivity.h"
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
	parentScope = NULL;
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
	parentScope = NULL;
}


ClassAd::
ClassAd (const ClassAd &ad) : attrList( 32 )
{
	nodeKind = CLASSAD_NODE;
	schema = ad.schema;
	last = ad.last;
	parentScope = ad.parentScope;;

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
			if (CLASSAD_ATTR_NAMES_STRCMP(attrList[index].attrName, name) == 0)
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
            if (CLASSAD_ATTR_NAMES_STRCMP(attrList[index].attrName, name) == 0) 
				break;
        }
    }

	return( attrList[index].valid ? attrList[index].expression : 0 );
}


bool ClassAd::
remove (char *name)
{
    int         index;

	// sanity check
	if (!name) return false;

    // if the classad is domainized
    if (schema) {
        // operate on the common schema array
        index = schema->getCanonical (name);
    } else {
        // use the attrlist as a standard unordered list
        for (index = 0; index < last; index ++) {
            if( CLASSAD_ATTR_NAMES_STRCMP(attrList[index].attrName, name)==0 ) {
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
setParentScope( ClassAd* parent )
{
	parentScope = parent;
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


void ClassAd::
_evaluate( EvalState&, Value& val )
{
	val.setClassAdValue( this );
}


void ClassAd::
_evaluate( EvalState&, Value &val, ExprTree *&tree )
{
	val.setClassAdValue( this );
	tree = copy();
}


bool ClassAd::
_flatten( EvalState& state, Value&, ExprTree*& tree, OpKind* ) 
{
	ClassAd 	*newAd = new ClassAd();
	Value		eval;
	ExprTree	*etree;
	EvalState	intermState;

	intermState.curAd = this;
	intermState.rootAd = state.rootAd ? state.rootAd : this;

	for( int i = 0 ; i < last ; i++ ) {
		if( attrList[i].valid ) {
			// copy attribute name
			if( attrList[i].attrName ) {
				newAd->attrList[i].attrName = strdup( attrList[i].attrName );
			} else {
				newAd->attrList[i].attrName = NULL;
			}

			// flatten expression
			if( !attrList[i].expression->flatten( intermState, eval, etree ) ) {
				delete newAd;
				tree = NULL;
				eval.clear();
				return false;
			}

			// if a value was obtained, convert it to a literal
			if( !etree ) {
				etree = Literal::makeLiteral( eval );
				if( !etree ) {
					delete newAd;
					tree = NULL;
					eval.clear();
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


void ClassAd::
evaluateAttr( char *attr , Value &val )
{
	EvalState	state;
	ExprTree	*tree;

	state.curAd  = this;
	state.rootAd = this;

	if( ( tree = lookup( attr ) ) ) {
		tree->evaluate( state , val );
		return;
	} else {
		val.setUndefinedValue();
	}
}


bool ClassAd::
evaluateExpr( char *expr , Value &val , int len )
{
	static Source	src;
	ExprTree		*tree;

	if( len == -1 ) len = strlen( expr );
	src.setSource( expr , len );
	src.parseExpression( tree );
	if( !tree ) {
		val.setErrorValue();
		return false;
	}
	evaluateExpr( tree , val );
	delete tree;
	return true;	
}


void ClassAd::
evaluateExpr( ExprTree *tree , Value &val )
{
	EvalState	state;

	state.curAd  = this;
	state.rootAd = this;
	tree->evaluate( state , val );
}

void ClassAd::
evaluateExpr( ExprTree *tree , Value &val , ExprTree *&sig )
{
	EvalState	state;

	state.curAd  = this;
	state.rootAd = this;
	tree->evaluate( state , val , sig );
}

bool ClassAd::
evaluateAttrInt( char *attr, int &i ) 
{
	Value val;
	evaluateAttr( attr, val );
	return( val.isIntegerValue( i ) );
}

bool ClassAd::
evaluateAttrReal( char *attr, double &r ) 
{
	Value val;
	evaluateAttr( attr, val );
	return( val.isRealValue( r ) );
}

bool ClassAd::
evaluateAttrNumber( char *attr, int &i ) 
{
	Value val;
	evaluateAttr( attr, val );
	return( val.isNumber( i ) );
}

bool ClassAd::
evaluateAttrNumber( char *attr, double &r ) 
{
	Value val;
	evaluateAttr( attr, val );
	return( val.isNumber( r ) );
}

bool ClassAd::
evaluateAttrString( char *attr, char *buf, int len )
{
	Value val;
	evaluateAttr( attr, val );
	return( val.isStringValue( buf, len ) );
}

bool ClassAd::
evaluateAttrString( char *attr, char *&str )
{
	Value val;
	evaluateAttr( attr, val );
	return( val.isStringValue( str ) );
}

bool ClassAd::
flatten( ExprTree *tree , Value &val , ExprTree *&fexpr )
{
	EvalState	state;

	state.curAd  = this;
	state.rootAd = this;
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
