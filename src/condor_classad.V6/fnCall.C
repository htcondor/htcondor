#include "exprTree.h"
#include "functions.h"

FunctionCall::
FunctionCall ()
{
	functionName = NULL;
	argumentList = NULL;
}


FunctionCall::
~FunctionCall ()
{
	if (functionName)	free (functionName);
	if (argumentList) 	delete argumentList;
}


ExprTree *FunctionCall::
copy (void)
{
	FunctionCall *newTree = new FunctionCall;

	if (!newTree) return NULL;
	if (functionName) 
		newTree->functionName = strdup (functionName);
	
	if (argumentList)
		newTree->argumentList = argumentList->copy();

	if (!newTree->argumentList)
	{
		free (newTree->functionName);
		delete newTree;
		return NULL;
	}

	return newTree;
}


bool FunctionCall::
toSink (Sink &s)
{
	// write function name
	if (!functionName||!s.sendToSink((void*)functionName,strlen(functionName)))
		return false;

	// write argument list
	if (argumentList)
	{
		return (argumentList->toSink (s));
	}
	
	return (s.sendToSink((void*)"( )", 3));

	return true;
}


void FunctionCall::
setCall (char *fnName, ArgumentList *argList)
{
	if (functionName) free (functionName);
	if (argumentList) delete argumentList;

	functionName = strdup (fnName);
	argumentList = argList;
}


void FunctionCall::
_evaluate (EvalState &state, Value &value)
{
	ArgumentList *argList = (ArgumentList *) argumentList;

	FunctionTable::dispatchFunction (functionName, argList, state, value);
}
