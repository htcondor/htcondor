#include "caseSensitivity.h"
#include "functions.h"

// Number of functions in the list
static const int NumFuncs = 11;

// the functions
static void isUndefined (char *, ArgumentList *, EvalState &, Value &);
static void isString (char *, ArgumentList *, EvalState &, Value &);
static void isInteger (char *, ArgumentList *, EvalState &, Value &);
static void isReal (char *, ArgumentList *, EvalState &, Value &);
static void isError (char *, ArgumentList *, EvalState &, Value &);
static void isMember (char *, ArgumentList *, EvalState &, Value &);
static void isExactMember (char *, ArgumentList *, EvalState &, Value &);


// This table must be in sorted order
FunctionTable::FunctionTableEntry FunctionTable::table [] = 
{
	{ "isUndefined", isUndefined },
	{ "isError", isError },
	{ "isReal", isReal },
	{ "isInteger", isInteger },
	{ "isString", isString },
	{ "isMember", isMember },
	{ "isExactMember", isExactMember }
};

void FunctionTable::
dispatchFunction (char *name, ArgumentList *list, EvalState &state, Value &val)
{
	// should make this search binary instead of linear
	for (int i = 0; i < NumFuncs; i++)
	{
		if (CLASSAD_FN_CALL_STRCMP (name, table[i].functionName) == 0)
		{
			// call the action routine
			(*(table[i].actionRoutine)) (name, list, state, val);
			return;
		}
	}

	// function was not found
	val.setUndefinedValue ();

	return;
}


// the action routines
static void 
isUndefined (char *, ArgumentList *argList, EvalState &state, Value &val)
{
	Value 	arg;

	// need a single argument
	if (argList->getNumberOfArguments() != 1)
	{
		val.setErrorValue ();
		return;
	}

	// evaluate the argument
	argList->evalArgument (state, 0, arg);

	// check if the value was undefined or not
	val.setIntegerValue (arg.isUndefinedValue());
}



static void 
isError (char *, ArgumentList *argList, EvalState &state, Value &val)
{
	Value 	arg;

	// need a single argument
	if (argList->getNumberOfArguments() != 1)
	{
		val.setErrorValue ();
		return;
	}

	// evaluate the argument
	argList->evalArgument (state, 0, arg);

	// check if the value was undefined or not
	val.setIntegerValue (arg.isErrorValue());
}



static void 
isInteger (char *, ArgumentList *argList, EvalState &state, Value &val)
{
    Value   arg;
	int		i;

    // need a single argument
    if (argList->getNumberOfArguments() != 1)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList->evalArgument (state, 0, arg);


	// check if the value was undefined or not
	val.setIntegerValue (arg.isIntegerValue(i));
}


static void 
isReal (char *, ArgumentList *argList, EvalState &state, Value &val)
{
    Value   arg;
	double	r;

    // need a single argument
    if (argList->getNumberOfArguments() != 1)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList->evalArgument (state, 0, arg);


	// check if the value was undefined or not
	val.setIntegerValue (arg.isRealValue(r));
}


static void 
isString (char *, ArgumentList *argList, EvalState &state, Value &val)
{
    Value   arg;
	char	*s;

    // need a single argument
    if (argList->getNumberOfArguments() != 1)
    {
        val.setErrorValue ();
        return;
    }

    // evaluate the argument
    argList->evalArgument (state, 0, arg);


	// check if the value was undefined or not
	val.setIntegerValue (arg.isStringValue(s));
}


static void
isMember (char *, ArgumentList *argList, EvalState &state, Value &val)
{
	Value	  arg0, arg1;
	ValueList *vl;

	// need two arguments
	if (argList->getNumberOfArguments() != 2)
	{
		val.setErrorValue();
		return;
	}

	// evaluate the arg list
	argList->evalArgument (state, 0, arg0);
	argList->evalArgument (state, 1, arg1);

	if (arg0.isErrorValue() || arg1.isErrorValue())
	{
		val.setErrorValue();
		return;
	}

	if (arg0.isUndefinedValue() || arg1.isUndefinedValue()) 
	{
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


static void
isExactMember (char *, ArgumentList *argList, EvalState &state, Value &val)
{
    Value     arg0, arg1;
    ValueList *vl;

    // need two arguments
    if (argList->getNumberOfArguments() != 2)
    {
        val.setErrorValue();
        return;
    }

    // evaluate the arg list
    argList->evalArgument (state, 0, arg0);
    argList->evalArgument (state, 1, arg1);

    // check for membership
    if (arg0.isListValue(vl))
        val.setIntegerValue(int(vl->isExactMember(arg1)));
    else
        val.setErrorValue();

    return;
}

