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
_setParentScope( ClassAd* parent )
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

	// convert to lower case (Case insensitive for function names)
	int len = strlen( fnName );
	for( int i = 0 ; i < len ; i++ ) {
		functionName[i] = tolower( functionName[i] );
	}

	function = (functionTable.lookup( functionName , fn )==-1) ? NULL : fn ;

	// preserve the capitalization of the lexical token for external
	// representation purposes
	strcpy( functionName , fnName );
}


void FunctionCall::
appendArgument( ExprTree *tree )
{
	arguments[numArgs++] = tree;
}


bool FunctionCall::
_evaluate (EvalState &state, Value &value)
{
	if( function ) {
		return( (*function)( functionName , arguments , state , value ) );
	} else {
		value.setErrorValue();
		return( true );
	}
}

bool FunctionCall::
_evaluate( EvalState &state, Value &value, ExprTree *& tree )
{
	FunctionCall *tmpSig = new FunctionCall;
	Value		tmpVal;
	ExprTree	*argSig;
	bool		rval;

	if( !tmpSig || !_evaluate( state, value ) ) {
		return false;
	}

	tmpSig->setFunctionName( functionName );
	rval = true;
	for( int i = 0 ; rval && i < numArgs ; i++ ) {
		rval = arguments[i]->evaluate( state, tmpVal, argSig );
		if( rval ) tmpSig->appendArgument( argSig );
	}
	tree = tmpSig;

	if( !rval ) delete tree;
	return( rval );
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
		tree = NULL;
		if( !(*function)( functionName , arguments , state , value ) ) {
			return false;
		}
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


bool FunctionCall::
isUndefined (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return( true );
    }

    // evaluate the argument
    if( !argList[0]->evaluate( state, arg ) ) {
		val.setErrorValue( );
		return false;
	}

    // check if the value was undefined or not
    val.setBooleanValue (arg.isUndefinedValue());
	return( true );
}

bool FunctionCall::
isError (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return( true );
    }

    // evaluate the argument
    if( !argList[0]->evaluate (state, arg) ) {
		val.setErrorValue( );
		return false;
	}

    // check if the value was error or not
    val.setBooleanValue (arg.isErrorValue());
	return( true );
}

bool FunctionCall::
isInteger (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return( true );
    }

    // evaluate the argument
    if( !argList[0]->evaluate (state, arg) ) {
		val.setErrorValue( );
		return false;
	}

    // check if the value was integer or not
    val.setBooleanValue (arg.isIntegerValue());
	return( true );
}


bool FunctionCall::
isReal (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return( true );
    }

    // evaluate the argument
    if( !argList[0]->evaluate (state, arg) ) {
		val.setErrorValue( );
		return false;
	}

    // check if the value was real or not
    val.setBooleanValue (arg.isRealValue());
	return( true );
}


bool FunctionCall::
isString (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return( true );
    }

    // evaluate the argument
    if( !argList[0]->evaluate (state, arg) ) {
		val.setErrorValue( );
		return false;
	}

    // check if the value was string or not
    val.setBooleanValue (arg.isStringValue());
	return( true );
}


bool FunctionCall::
isList (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return( true );
    }

    // evaluate the argument
    if( !argList[0]->evaluate (state, arg) ) {
		val.setErrorValue( );
		return false;
	}

    // check if the value was a list or not
    val.setBooleanValue (arg.isListValue());
	return( true );
}


bool FunctionCall::
isClassAd (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0) {
        val.setErrorValue ();
        return( true );
    }

    // evaluate the argument
    if( !argList[0]->evaluate (state, arg) ) {
		val.setErrorValue( );
		return false;
	}

    // check if the value was classad or not
    val.setBooleanValue (arg.isClassAdValue());
	return true;
}


bool FunctionCall::
isMember (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value     	arg0, arg1, cArg;
    ExprTree  	*tree;
	ExprList	*el;
	bool		b;

    // need two arguments
    if (argList.getlast() != 1) {
        val.setErrorValue();
        return( true );
    }

    // evaluate the arg list
    if( !argList[0]->evaluate(state,arg0) || !argList[1]->evaluate(state,arg1)){
		val.setErrorValue( );
		return false;
	}

    if( arg0.isUndefinedValue() || arg1.isUndefinedValue() ) {
        val.setUndefinedValue();
        return true;
    }

    if (arg0.isErrorValue() || !arg0.isListValue() ||
		arg1.isErrorValue() || arg1.isListValue() || arg1.isClassAdValue() ) {
        val.setErrorValue();
        return true;
    }

    // check for membership
	el->rewind( );
	while( ( tree = el->next( ) ) ) {
		if( !tree->evaluate( state, cArg ) ) {
			val.setErrorValue( );
			return( false );
		}
		Operation::operate( EQUAL_OP, cArg, arg1, val );
		if( val.isBooleanValue( b ) && b ) {
			return true;
		}
	}
	val.setBooleanValue( false );	

    return true;
}


bool FunctionCall::
isExactMember (char *, ArgumentList &argList, EvalState &state, Value &val)
{
    Value     	arg0, arg1, cArg;
    ExprTree  	*tree;
	ExprList	*el;
	bool		b;

    // need two arguments
    if (argList.getlast() != 1) {
        val.setErrorValue();
        return( true );
    }

    // evaluate the arg list
    if( !argList[0]->evaluate(state,arg0) || !argList[1]->evaluate(state,arg1)){
		val.setErrorValue( );
		return false;
	}

    if( arg0.isUndefinedValue( ) ) {
        val.setUndefinedValue();
        return true;
    }

    if( !arg0.isListValue( ) || arg1.isClassAdValue( ) || arg1.isListValue( ) ){
        val.setErrorValue();
        return true;
    }

    // check for membership
	el->rewind( );
	while( ( tree = el->next( ) ) ) {
		if( !tree->evaluate( state, cArg ) ) {
			val.setErrorValue( );
			return( false );
		}
		Operation::operate( IS_OP, cArg, arg1, val );
		if( val.isBooleanValue( b ) && b ) {
			return true;
		}
	}
	val.setBooleanValue( false );	

    return true;
}


bool FunctionCall::
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
		return true;
	}

	// first argument must evaluate to a list
	if( !argList[0]->evaluate( state, listVal ) ) {
		val.setErrorValue( );
		return( false );
	} else if( listVal.isUndefinedValue() ) {
		val.setUndefinedValue( );
		return( true );
	} else if( !listVal.isListValue( el ) ) {
		val.setErrorValue();
		return( true );
	}

	el->rewind();
	result.setUndefinedValue();
	while( ( ca = el->next() ) ) {
		if( !ca->evaluate( state, caVal ) ) {
			val.setErrorValue( );
			return( false );
		} else if( !caVal.isClassAdValue( ad ) ) {
			val.setErrorValue();
			return( true );
		} else if( !ad->evaluateExpr( argList[1], tmp ) ) {
			val.setErrorValue( );
			return( false );
		}
		if( first ) {
			result.copyFrom( tmp );
			first = false;
		} else {
			Operation::operate( ADDITION_OP, result, tmp, result );
		}
		tmp.clear();
	}

	val.copyFrom( result );
	return true;
}


bool FunctionCall::
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
		return true;
	}

	// first argument must evaluate to a list
	if( !argList[0]->evaluate( state, listVal ) ) {
		val.setErrorValue( );
		return( false );
	} else if( listVal.isUndefinedValue() ) {
		val.setUndefinedValue( );
		return( true );
	} else if( !listVal.isListValue( el ) ) {
		val.setErrorValue();
		return( true );
	}

	el->rewind();
	result.setUndefinedValue();
	while( ( ca = el->next() ) ) {
		if( !ca->evaluate( state, caVal ) ) {
			val.setErrorValue( );
			return( false );
		} else if( !caVal.isClassAdValue( ad ) ) {
			val.setErrorValue();
			return( true );
		} else if( !ad->evaluateExpr( argList[1], tmp ) ) {
			val.setErrorValue( );
			return( false );
		}
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
	return true;
}


bool FunctionCall::
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
		return( true );
	}

	// first argument must evaluate to a list
	if( !argList[0]->evaluate( state, listVal ) ) {
		val.setErrorValue( );
		return( false );
	} else if( listVal.isUndefinedValue ) {
		val.setUndefinedValue( );
		return( true );
	} else if( !listVal.isListValue( el ) ) {
		val.setErrorValue();
		return( true );
	}

	// fn is either "min..." or "max..."
	min = ( tolower( fn[1] ) == 'i' );

	el->rewind();
	result.setUndefinedValue();
	while( ( ca = el->next() ) ) {
		if( !ca->evaluate( state, caVal ) ) {
			val.setErrorValue( );
			return false;
		} else if( !caVal.isClassAdValue( ad ) ) {
			val.setErrorValue();
			return( true );
		} else if( !ad->evaluateExpr( argList[1], tmp ) ) {
			val.setErrorValue( );
			return( true );
		}
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
	return true;
}
