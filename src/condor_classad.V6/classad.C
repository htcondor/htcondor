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
			attrList[i].expression = ad.attrList[i].expression->copy();
			attrList[i].valid = true;
		} else {
			attrList[i].valid = false;
		}
	}
}	


ClassAd::
~ClassAd ()
{
	clear();
}


void ClassAd::
clear( )
{
	for (int i = 0; i < last; i++) {
		if (!attrList[i].valid) continue;

		if (attrList[i].attrName) free(attrList[i].attrName);
		if (attrList[i].expression) delete attrList[i].expression;
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

	// the expression is in the scope of this classad
	tree->setParentScope( this );

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
		delete (attrList[index].expression);
	}
	
	// insert the new tree
	attrList[index].expression = tree;

	return true;
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

	return (attrList[index].valid ? attrList[index].expression : NULL);
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
copy( CopyMode )
{
	ClassAd	*newAd = new ClassAd();

	if( !newAd ) return NULL;

	newAd->nodeKind = CLASSAD_NODE;
	newAd->schema = schema;
	newAd->last = last;
	newAd->parentScope = parentScope;

	for( int i = 0 ; i < last ; i++ ) {
		if( attrList[i].valid ) {
			if( attrList[i].attrName ) {
				newAd->attrList[i].attrName = strdup( attrList[i].attrName );
			} else {
				newAd->attrList[i].attrName = NULL;
			}
			newAd->attrList[i].expression = attrList[i].expression->copy();
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


void ClassAd::
_evaluate( EvalState&, EvalValue& val )
{
	val.setClassAdValue( this );
}


bool ClassAd::
_flatten( EvalState& state, EvalValue&, ExprTree*& tree, OpKind* ) 
{
	ClassAd 	*newAd = new ClassAd();
	EvalValue	eval;
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
				return false;
			}

			// if a value was obtained, convert it to a literal
			if( !etree ) {
				etree = Literal::makeLiteral( eval );
				if( !etree ) {
					delete newAd;
					tree = NULL;
					return false;
				}
			}

			newAd->attrList[i].expression = etree;
			newAd->attrList[i].valid = true;
		} else {
			newAd->attrList[i].valid = false;
		}
	}

	newAd->last = last;
	tree = newAd;

	return true;
}


void ClassAd::
setParentScope( ClassAd *ad )
{
	parentScope = ad;
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
    return (s.parseClassAd(&ad) ? &ad : NULL);
}


void ClassAd::
evaluateAttr( char *attr , Value &val )
{
	EvalState	state;
	EvalValue	value;
	ExprTree	*tree;

	state.curAd  = this;
	state.rootAd = this;

	if( ( tree = lookup( attr ) ) ) {
		tree->evaluate( state , value );
		val.copyFrom( value );
		return;
	} else {
		val.setUndefinedValue();
	}
}


bool ClassAd::
evaluate( const char *expr , Value &val , int len )
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
	evaluate( tree , val );
	delete tree;
	return true;	
}


bool ClassAd::
evaluate( char *expr , Value &val , int len )
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
	evaluate( tree , val );
	delete tree;
	return true;	
}


void ClassAd::
evaluate( ExprTree *tree , Value &val )
{
	EvalState	state;
	EvalValue	value;

	state.curAd  = this;
	state.rootAd = this;
	tree->evaluate( state , value );
	val.copyFrom( value );
}

bool ClassAd::
flatten( ExprTree *tree , Value &val , ExprTree *&fexpr )
{
	bool rval;

	EvalState	state;
	EvalValue	value;

	state.curAd  = this;
	state.rootAd = this;
	rval = tree->flatten( state , value , fexpr );
	val.copyFrom( value );

	return rval;
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

