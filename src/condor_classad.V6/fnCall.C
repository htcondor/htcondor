#include "caseSensitivity.h"
#include "exprTree.h"

static int fnHashFcn( const MyString& , int numBkts );
static char* _FileName_ = __FILE__;

bool FunctionCall::initialized = false;
FunctionCall::FuncTable FunctionCall::functionTable( 20 , &fnHashFcn );

// start up with an argument list of size 4
FunctionCall::
FunctionCall () : arguments( 4 )
{
	nodeKind = FN_CALL_NODE;
	functionName = NULL;
	numArgs = 0;

	if( !initialized ) {
#ifdef CLASSAD_FN_CALL_CASE_INSENSITIVE
		// load up the function dispatch table
		functionTable.insert( "isundefined", 	isUndefined );
		functionTable.insert( "iserror",	 	isError );
		functionTable.insert( "isstring",	 	isString );
		functionTable.insert( "isinteger",	 	isInteger );
		functionTable.insert( "isreal",		 	isReal );
		functionTable.insert( "islist",		 	isList );
		functionTable.insert( "isclassad",	 	isClassAd );
		functionTable.insert( "ismember",	 	isMember );
		functionTable.insert( "isexactmember",	isExactMember );
		functionTable.insert( "sumfrom",		sumFrom );
		functionTable.insert( "avgfrom",		avgFrom );
		functionTable.insert( "maxfrom",		boundFrom );
		functionTable.insert( "minfrom",		boundFrom );
#else
		// load up the function dispatch table
		functionTable.insert( "isUndefined", 	isUndefined );
		functionTable.insert( "isError",	 	isError );
		functionTable.insert( "isString",	 	isString );
		functionTable.insert( "isInteger",	 	isInteger );
		functionTable.insert( "isReal",		 	isReal );
		functionTable.insert( "isList",		 	isList );
		functionTable.insert( "isClassAd",	 	isClassAd );
		functionTable.insert( "isMember",	 	isMember );
		functionTable.insert( "isExactMember",	isExactMember );
		functionTable.insert( "sumFrom",		sumFrom );
		functionTable.insert( "avgFrom",		avgFrom );
		functionTable.insert( "maxFrom",		boundFrom );
		functionTable.insert( "minFrom",		boundFrom );
#endif
		initialized = true;
	}
}


FunctionCall::
~FunctionCall ()
{
	if( functionName ) free( functionName );
	for( int i = 0 ; i < numArgs ; i++ ) {
		delete arguments[i];
	}
}


ExprTree *FunctionCall::
_copy( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY ) {
		FunctionCall *newTree = new FunctionCall;
		ExprTree *newArg;

		if (!newTree) return NULL;
		if (functionName) newTree->functionName = strdup (functionName);

		for( int i = 0 ; i < numArgs ; i++ ) {
			newArg = arguments[i]->copy( EXPR_DEEP_COPY );
			if( newArg ) {
				newTree->appendArgument( newArg );
			} else {
				delete newTree;
				return NULL;
			}
		}
		return newTree;
	} else if( cm == EXPR_REF_COUNT ) {
		for( int i = 0 ; i < numArgs ; i++ ) {
			arguments[i]->copy( EXPR_REF_COUNT );
		}
		return this;
	} 

	// will not reach here
	return 0;	
}


void FunctionCall::
setParentScope( ClassAd* parent )
{
	for( int i = 0; i < numArgs ; i++) {
		arguments[i]->setParentScope( parent );
	}
}
	
bool FunctionCall::
toSink (Sink &s)
{
	// write function name
	if (!functionName||!s.sendToSink((void*)functionName,strlen(functionName)))
		return false;

	// write argument list
	if (!s.sendToSink((void*) "( ", 2)) return false;
	
	for( int i = 0; i < numArgs ; i++) {
		if( !arguments[i]->toSink(s) ) return false;
		if( i < numArgs-1 && !s.sendToSink((void*)" , ", 3)) return false;
	}

	return (s.sendToSink((void*)" )", 2));
}

void FunctionCall::
setFunctionName (char *fnName)
{
	ClassAdFunc	fn;

	if (functionName) free (functionName);
	functionName = strdup (fnName);

#ifdef CLASSAD_FN_CALL_CASE_INSENSITIVE
	{
		int len = strlen( fnName );
		for( int i = 0 ; i < len ; i++ ) {
			functionName[i] = tolower( functionName[i] );
		}
	}
#endif

	function = (functionTable.lookup( functionName , fn )==-1) ? NULL : fn ;

#ifdef CLASSAD_FN_CALL_CASE_INSENSITIVE
	// we preserve the capitalization of the lexical token for external
	// representation purposes
	strcpy( functionName , fnName );
#endif
}


void FunctionCall::
appendArgument( ExprTree *tree )
{
	arguments[numArgs++] = tree;
}


void FunctionCall::
_evaluate (EvalState &state, Value &value)
{
	if( function )
		(*function)( functionName , arguments , state , value );
	else
		value.setErrorValue();
}

void FunctionCall::
_evaluate( EvalState &state, Value &value, ExprTree *& tree )
{
	FunctionCall *tmpSig = new FunctionCall;
	Value		tmpVal;
	ExprTree	*argSig;

	_evaluate( state, value );
	tmpSig->setFunctionName( functionName );
	for( int i = 0 ; i < numArgs ; i++ ) {
		arguments[i]->evaluate( state, tmpVal, argSig );
		tmpSig->appendArgument( argSig );
	}
	tree = tmpSig;
}

bool FunctionCall::
_flatten( EvalState &state, Value &value, ExprTree*&tree, OpKind* )
{
	FunctionCall *newCall;
	ExprTree	*argTree;
	Value		argValue;
	bool		fold = true;

	// if the function cannot be resolved, the value is "error"
	if( !function ) {
		value.setErrorValue();
		tree = NULL;
		return true;
	}

	// create a residuated function call with flattened args
	if( ( newCall = new FunctionCall() ) == NULL ) return false;
	newCall->setFunctionName( functionName );

	// flatten the arguments
	for( int i = 0 ; i < numArgs ; i++ ) {
		if( arguments[i]->flatten( state, argValue, argTree ) ) {
			if( argTree ) {
				newCall->appendArgument( argTree );
				fold = false;
				continue;
			} else {
				ASSERT( argTree == NULL );
				argTree = Literal::makeLiteral( argValue );
				if( argTree ) {
					newCall->appendArgument( argTree );
					continue;
				}
			}
		} 

		// we get here only when something bad happens
		delete newCall;
		value.setErrorValue();
		tree = NULL;
		return false;
	} 
	
	// assume all functions are "pure" (i.e., side-affect free)
	if( fold ) {
		(*function)( functionName , arguments , state , value );
		tree = NULL;
	} else {
		tree = newCall;
	}

	return true;
}


static int fnHashFcn( const MyString &fnName , int numBkts )
{
	int	acc = 0;
	int len = fnName.Length();
	for( int i = 0 ; i < len ; i++ ) {
		acc += fnName[i];			
	}
	return( acc % numBkts );
}


void FunctionCall::
isUndefined (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate( state, arg );

    // check if the value was undefined or not
    val.setBooleanValue (arg.isUndefinedValue());
}

void FunctionCall::
isError (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was error or not
    val.setBooleanValue (arg.isErrorValue());
}

void FunctionCall::
isInteger (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was integer or not
    val.setBooleanValue (arg.isIntegerValue());
}


void FunctionCall::
isReal (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was real or not
    val.setBooleanValue (arg.isRealValue());
}


void FunctionCall::
isString (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was string or not
    val.setBooleanValue (arg.isStringValue());
}


void FunctionCall::
isList (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was a list or not
    val.setBooleanValue (arg.isListValue());
}


void FunctionCall::
isClassAd (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was classad or not
    val.setBooleanValue (arg.isClassAdValue());
}


void FunctionCall::
isMember (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value     	arg0, arg1, cArg;
    ExprTree  	*tree;
	ExprList	*el;
	bool		b;

    // need two arguments
    if (argList.getlast() != 1) {
        val.setErrorValue();
        return;
    }

    // evaluate the arg list
    argList[0]->evaluate (state, arg0);
    argList[1]->evaluate (state, arg1);

    if (arg0.isErrorValue() || arg1.isErrorValue() || !arg0.isListValue(el) ) {
        val.setErrorValue();
        return;
    }

    if (arg0.isUndefinedValue() || arg1.isUndefinedValue() || 
		arg1.isClassAdValue() || arg1.isListValue() ) {
        val.setUndefinedValue();
        return;
    }

    // check for membership
	el->rewind( );
	while( ( tree = el->next( ) ) ) {
		tree->evaluate( state, cArg );
		Operation::operate( EQUAL_OP, cArg, arg1, val );
		if( val.isBooleanValue( b ) && b ) {
			return;
		}
	}
	val.setBooleanValue( false );	

    return;
}


void FunctionCall::
isExactMember (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value     	arg0, arg1, cArg;
    ExprTree  	*tree;
	ExprList	*el;
	bool		b;

    // need two arguments
    if (argList.getlast() != 1) {
        val.setErrorValue();
        return;
    }

    // evaluate the arg list
    argList[0]->evaluate (state, arg0);
    argList[1]->evaluate (state, arg1);

    if (arg0.isErrorValue() || arg1.isErrorValue() || !arg0.isListValue(el) ) {
        val.setErrorValue();
        return;
    }

    if (arg0.isUndefinedValue() || arg1.isUndefinedValue() || 
		arg1.isClassAdValue() || arg1.isListValue() ) {
        val.setUndefinedValue();
        return;
    }

    // check for membership
	el->rewind( );
	while( ( tree = el->next( ) ) ) {
		tree->evaluate( state, cArg );
		Operation::operate( IS_OP, cArg, arg1, val );
		if( val.isBooleanValue( b ) && b ) {
			return;
		}
	}
	val.setBooleanValue( false );	

    return;
}


void FunctionCall::
sumFrom (char *, ArgumentList &argList, EvalState &state, Value &val)
{
	Value		caVal, listVal;
	ExprTree	*ca;
	Value		tmp, result;
	ExprList	*el;
	ClassAd		*ad;
	bool		first = true;

	// need two arguments
	if( argList.getlast() != 1 ) {
		val.setErrorValue();
		return;
	}

	// first argument must evaluate to a list
	argList[0]->evaluate( state, listVal );
	if( !listVal.isListValue( el ) ) {
		val.setErrorValue();
		return;
	}

	el->rewind();
	result.setUndefinedValue();
	while( ( ca = el->next() ) ) {
		ca->evaluate( state, caVal );
		if( !caVal.isClassAdValue( ad ) ) {
			val.setErrorValue();
			return;
		}
		ad->evaluateExpr( argList[1], tmp );
		if( first ) {
			result.copyFrom( tmp );
			first = false;
		} else {
			Operation::operate( ADDITION_OP, result, tmp, result );
		}
		tmp.clear();
	}

	val.copyFrom( result );
	return;
}


void FunctionCall::
avgFrom (char *, ArgumentList &argList, EvalState &state, Value &val)
{
	Value		caVal, listVal;
	ExprTree	*ca;
	Value		tmp, result;
	ExprList	*el;
	ClassAd		*ad;
	bool		first = true;
	int			len;

	// need two arguments
	if( argList.getlast() != 1 ) {
		val.setErrorValue();
		return;
	}

	// first argument must evaluate to a list
	argList[0]->evaluate( state, listVal );
	if( !listVal.isListValue( el ) ) {
		val.setErrorValue();
		return;
	}

	el->rewind();
	result.setUndefinedValue();
	while( ( ca = el->next() ) ) {
		ca->evaluate( state, caVal );
		if( !caVal.isClassAdValue( ad ) ) {
			val.setErrorValue();
			return;
		}
		ad->evaluateExpr( argList[1], tmp );
		if( first ) {
			result.copyFrom( tmp );
			first = false;
		} else {
			Operation::operate( ADDITION_OP, result, tmp, result );
		}
		tmp.clear();
	}

	len = el->number();
	if( len > 0 ) {
		tmp.setRealValue( len );
		Operation::operate( DIVISION_OP, result, tmp, result );
	} else {
		val.setUndefinedValue();
	}

	val.copyFrom( result );
	return;
}


void FunctionCall::
boundFrom (char *fn, ArgumentList &argList, EvalState &state, Value &val)
{
	Value		caVal, listVal, cmp;
	ExprTree	*ca;
	Value		tmp, result;
	ExprList	*el;
	ClassAd		*ad;
	bool		first = true, b=false, min;

	// need two arguments
	if( argList.getlast() != 1 ) {
		val.setErrorValue();
		return;
	}

	// first argument must evaluate to a list
	argList[0]->evaluate( state, listVal );
	if( !listVal.isListValue( el ) ) {
		val.setErrorValue();
		return;
	}

	// fn is either "min..." or "max..."
	min = ( tolower( fn[1] ) == 'i' );

	el->rewind();
	result.setUndefinedValue();
	while( ( ca = el->next() ) ) {
		ca->evaluate( state, caVal );
		if( !caVal.isClassAdValue( ad ) ) {
			val.setErrorValue();
			return;
		}
		ad->evaluateExpr( argList[1], tmp );
		if( first ) {
			result.copyFrom( tmp );
			first = false;
		} else {
			Operation::operate(min?LESS_THAN_OP:GREATER_THAN_OP,tmp,result,cmp);
			if( cmp.isBooleanValue( b ) && b ) {
				result.copyFrom( tmp );
			}
		}
	}

	val.copyFrom( result );
	return;
}
