#include "caseSensitivity.h"
#include "exprTree.h"

static int hashFcn( const MyString& , int numBkts );

bool FunctionCall::initialized = false;
FunctionCall::FuncTable FunctionCall::functionTable( 20 , &hashFcn );

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
#endif
		initialized = true;
	}
}


FunctionCall::
~FunctionCall ()
{
	if (functionName)	free (functionName);
	for( int i = 0 ; i < numArgs ; i++ )
		delete arguments[i];
}


ExprTree *FunctionCall::
copy (CopyMode)
{
	FunctionCall *newTree = new FunctionCall;

	if (!newTree) return NULL;
	if (functionName) newTree->functionName = strdup (functionName);

	for( int i = 0 ; i < numArgs ; i++ )
	{
		if( ( newTree->arguments[i] = arguments[i]->copy() ) == NULL )
		{
			while( i-- ) delete newTree->arguments[i];
			delete newTree;
			return NULL;
		}
	}

	newTree->numArgs = numArgs;

	return newTree;
}


bool FunctionCall::
toSink (Sink &s)
{
	// write function name
	if (!functionName||!s.sendToSink((void*)functionName,strlen(functionName)))
		return false;

	// write argument list
	if (!s.sendToSink((void*) "( ", 2)) return false;
	
	for( int i = 0; i < numArgs ; i++)
	{
		if( !arguments[i]->toSink(s) ) return false;
		if( i < numArgs-1 && !s.sendToSink((void*)" , ", 3)) return false;
	}

	return (s.sendToSink((void*)" )", 2));
}


void FunctionCall::
setParentScope( ClassAd *ad )
{
	for( int i = 0 ; i < numArgs ; i++ )
		arguments[i]->setParentScope( ad );
}
		
void FunctionCall::
setFunctionName (char *fnName)
{
	ClassAdFunc	fn;

	if (functionName) free (functionName);
	functionName = strdup (fnName);

	function = (functionTable.lookup( fnName , fn )==-1) ? NULL : fn ;
}


void FunctionCall::
appendArgument( ExprTree *tree )
{
	arguments[numArgs++] = tree;
}


void FunctionCall::
_evaluate (EvalState &state, EvalValue &value)
{
	if( function )
		(*function)( functionName , arguments , state , value );
	else
		value.setErrorValue();
}

static int hashFcn( const MyString &fnName , int numBkts )
{
	int	acc = 0;
	int len = fnName.Length();
	for( int i = 0 ; i < len ; i++ ) {
		acc += 
#ifdef CLASSAD_FN_CALL_CASE_INSENSITIVE
				tolower( fnName[i] );
#else
				fnName[i];			
#endif
	}
	return( acc % numBkts );
}


void FunctionCall::
isUndefined (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate( state, arg );

    // check if the value was undefined or not
    val.setIntegerValue (arg.isUndefinedValue());
}

void FunctionCall::
isError (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was undefined or not
    val.setIntegerValue (arg.isErrorValue());
}

void FunctionCall::
isInteger (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was undefined or not
    val.setIntegerValue (arg.isIntegerValue());
}


void FunctionCall::
isReal (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was undefined or not
    val.setIntegerValue (arg.isRealValue());
}


void FunctionCall::
isString (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was undefined or not
    val.setIntegerValue (arg.isStringValue());
}


void FunctionCall::
isList (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was undefined or not
    val.setIntegerValue (arg.isListValue());
}


void FunctionCall::
isClassAd (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value   arg;

    // need a single argument
    if (argList.getlast() != 0)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList[0]->evaluate (state, arg);

    // check if the value was undefined or not
    val.setIntegerValue (arg.isClassAdValue());
}


void FunctionCall::
isMember (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value     arg0, arg1;
    ValueList *vl;

    // need two arguments
    if (argList.getlast() != 1) {
        val.setErrorValue();
        return;
    }

    // evaluate the arg list
    argList[0]->evaluate (state, arg0);
    argList[1]->evaluate (state, arg1);

    if (arg0.isErrorValue() || arg1.isErrorValue()) {
        val.setErrorValue();
        return;
    }

    if (arg0.isUndefinedValue() || arg1.isUndefinedValue()) {
        val.setUndefinedValue();
        return;
    }

    // check for membership
    if (arg0.isListValue(vl))
        val.setIntegerValue(int(vl->isMember(arg1)));
    else
        val.setErrorValue();

    return;
}


void FunctionCall::
isExactMember (char *, ArgumentList &argList, EvalState &state, EvalValue &val)
{
    Value     arg0, arg1;
    ValueList *vl;

    // need two arguments
    if (argList.getlast() != 1) {
        val.setErrorValue();
        return;
    }

    // evaluate the arg list
    argList[0]->evaluate (state, arg0);
    argList[1]->evaluate (state, arg1);

    if (arg0.isErrorValue() ) {
        val.setErrorValue();
        return;
    }

    if (arg0.isUndefinedValue() ) {
        val.setUndefinedValue();
        return;
    }

    // check for membership
    if (arg0.isListValue(vl))
        val.setIntegerValue(int(vl->isExactMember(arg1)));
    else
        val.setErrorValue();

    return;
}
