#include "condor_common.h"
#include "operators.h"
#include "values.h"

StringSpace EvalValue::stringValues;

EvalValue::
EvalValue()
{
	listValue = NULL;
	classadValue = NULL;
	valueType = UNDEFINED_VALUE;
}


EvalValue::
~EvalValue()
{
	clear();
}


void EvalValue::
clear()
{
	switch( valueType ) {
		case LIST_VALUE:
			// all list values are explicitly created on evaluation, so they 
			// must be explicitly destroyed
			if( listValue ) delete listValue;
			listValue = NULL;
			break;

		case CLASSAD_VALUE:
			// classad values live in the evaluation environment, so they must 
			// never be explicitly destroyed
			classadValue = NULL;
			break;

		case STRING_VALUE:
			// string values are handled with reference counts
			strValue.dispose();
			break;

		default:
			valueType = UNDEFINED_VALUE;
	}
	valueType 	= UNDEFINED_VALUE;
}


bool EvalValue::
isNumber (int &i)
{
	switch (valueType)
	{
		case INTEGER_VALUE:
			i = integerValue;
			return true;

		case REAL_VALUE:
			i = (int) realValue;	// truncation	
			return true;

		default:
			return false;
	}
}


bool EvalValue::
isNumber (double &r)
{
	switch (valueType)
	{
		case INTEGER_VALUE:
			r = (double) integerValue;
			return true;

		case REAL_VALUE:
			r = realValue;	
			return true;

		default:
			return false;
	}
}


// this function implements "handoff" semantics
void EvalValue::
copyFrom(EvalValue &val)
{
	switch (val.valueType)
	{
		case STRING_VALUE:
			setStringValue( val.strValue );
			val.strValue.dispose();
			val.valueType = UNDEFINED_VALUE;
			return;

		case INTEGER_VALUE:
			setIntegerValue( val.integerValue );
			val.valueType = UNDEFINED_VALUE;
			return;

		case REAL_VALUE:
			setRealValue( val.realValue );
			val.valueType = UNDEFINED_VALUE;
			return;

		case UNDEFINED_VALUE:
			setUndefinedValue ();
			return;

		case ERROR_VALUE:
			setErrorValue ();
			val.valueType = UNDEFINED_VALUE;
			return;
	
		case LIST_VALUE:
			setListValue (val.listValue);
			val.listValue = NULL;
			val.valueType = UNDEFINED_VALUE;
			return;

		case CLASSAD_VALUE:
			setClassAdValue (val.classadValue);
			val.classadValue = NULL;
			val.valueType = UNDEFINED_VALUE;
			return;

		default:
			setUndefinedValue ();
	}	
}


bool EvalValue::
toSink (Sink &dest)
{
	char tempBuf[512];
	Value *val;
	char *str;

    switch (valueType) {
      	case UNDEFINED_VALUE:
        	return (dest.sendToSink((void*)"undefined", strlen("undefined")));

      	case ERROR_VALUE:
        	return (dest.sendToSink ((void*)"error", strlen("error")));

      	case INTEGER_VALUE:
			sprintf (tempBuf, "%d", integerValue);
			return (dest.sendToSink ((void*)tempBuf, strlen(tempBuf)));

      	case REAL_VALUE:
        	sprintf (tempBuf, "%f", realValue);
			return (dest.sendToSink ((void*)tempBuf, strlen(tempBuf)));

      	case STRING_VALUE:
			str = strValue.getCharString();
			return (dest.sendToSink ((void*) "\"", 1) 			&&
					dest.sendToSink ((void*)str, strlen(str))	&&
					dest.sendToSink ((void*) "\"", 1));

		case LIST_VALUE:
			// sanity check
			if (!listValue) {
				clear();
				return false;
			}
			listValue->Rewind();
			if (!dest.sendToSink((void*)" { ", 3)) return false;
			while ((val = listValue->Next())) {
				if (!val->toSink(dest)) return false;
				if (!listValue->AtEnd()) {
					if (!dest.sendToSink((void*)" , ", 3)) return false;
				}
			}
			if (!dest.sendToSink((void*)" } ", 3)) return false;
			return true;

		case CLASSAD_VALUE:
			// sanity check
			if (!classadValue) {
				clear();
				return false;
			}
			return classadValue->toSink( dest );

      	default:
        	return false;
    }

	// should not reach here
	return false;
}


void EvalValue::
setRealValue (double r)
{
    clear();
    valueType=REAL_VALUE;
    realValue = r;
}

void EvalValue::
setIntegerValue (int i)
{
    clear();
    valueType=INTEGER_VALUE;
    integerValue = i;
}

void EvalValue::
setUndefinedValue (void)
{
    clear();
    valueType=UNDEFINED_VALUE;
}

void EvalValue::
setErrorValue (void)
{
    clear();
    valueType=ERROR_VALUE;
}

void EvalValue::
adoptStringValue(char *s, int adopt)
{
    clear();
    valueType = STRING_VALUE;
    stringValues.getCanonical (s, strValue, adopt);
}


void EvalValue::
setStringValue(char *s)
{
    clear();
    valueType = STRING_VALUE;
    stringValues.getCanonical (s, strValue, SS_DUP);
}

void EvalValue::
setStringValue(SSString &s)
{
    clear();
    valueType = STRING_VALUE;
    strValue.copy (s);
}

void EvalValue::
setListValue (ValueList *l)
{
    clear();
    valueType = LIST_VALUE;
    listValue = l;
}

void EvalValue::
setClassAdValue( ClassAd *ad )
{
	clear();
	valueType = CLASSAD_VALUE;
	classadValue = ad;
}


// Implementation of (User-level) Value 
Value::
Value() : EvalValue()
{
}


Value::
Value( const Value& val )
{
	// assuming base class EvalValue already constructed
	copyFrom( (Value&) val );
}


Value::
~Value()
{
	clear();
}

void Value::
clear()
{
	switch( valueType ) {
		case LIST_VALUE:
			// all list values are explicitly created on evaluation, so they 
			// must be explicitly destroyed
			if( listValue ) delete listValue;
			listValue = NULL;
			break;

		case CLASSAD_VALUE:
			// classad values live in the evaluation environment --- don't
			// delete them
			classadValue = NULL;
			break;

		case STRING_VALUE:
			// string values are handled with reference counts
			strValue.dispose();
			break;

		default:
			valueType = UNDEFINED_VALUE;
	}
	valueType 	= UNDEFINED_VALUE;
}


void Value::
copyFrom( EvalValue &val )
{
	switch (val.valueType)
	{
		case STRING_VALUE:
			setStringValue( val.strValue );
			return;

		case INTEGER_VALUE:
			setIntegerValue( val.integerValue );
			return;

		case REAL_VALUE:
			setRealValue( val.realValue );
			return;

		case UNDEFINED_VALUE:
			setUndefinedValue ();
			return;

		case ERROR_VALUE:
			setErrorValue ();
			return;
	
		case LIST_VALUE:
			setListValue( val.listValue );
			val.listValue = NULL;
			val.valueType = UNDEFINED_VALUE;
			return;

		case CLASSAD_VALUE:
			setClassAdValue( val.classadValue );
			val.classadValue = NULL;
			val.valueType = UNDEFINED_VALUE;
			return;

		default:
			setUndefinedValue ();
	}	
}


// Implementation of value lists follows

ValueList::
ValueList()
{
}


ValueList::
~ValueList()
{
	clearList();
}


void ValueList::
clearList()
{
	Value *val;

	Rewind();
	while ((val = Next())) {
		delete val;
		DeleteCurrent ();
	}
}


void ValueList::
copy (ValueList *list)
{
	Value 	*val = NULL;
	Value	*newVal;

	clearList();

	// copy all values in the list
	list->Rewind();
	while ((val = list->Next())) {
		newVal = new Value;
		newVal->copyFrom(*val);
		Append (newVal);
	}
}


bool ValueList::
isMember (Value &val)
{
	Value *v;
	Value result;
	int	  i = 0;

	Rewind();
	while ((v = Next())) {
		Operation::operate(EQUAL_OP, *v, val, result);
		if (result.isIntegerValue(i) && i)
			return true;
	}

	return false;
}


bool ValueList::
isExactMember (Value &val)
{
	Value *v;
	Value result;
	int	  i = 0;

	Rewind();
	while ((v = Next())) {
		Operation::operate(META_EQUAL_OP, *v, val, result);
		if (result.isIntegerValue(i) && i)
			return true;
	}

	return false;
}
