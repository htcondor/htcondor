#include "condor_common.h"
#include "operators.h"
#include "values.h"

StringSpace Value::stringValues;

Value::
~Value()
{
	if (valueType == LIST_VALUE) delete listValue;
}


void Value::
clear()
{
	switch (valueType)
	{
		case STRING_VALUE:
			strValue.dispose();
			break;

		case LIST_VALUE:
			delete listValue;
			listValue = NULL;
			break;

		default:
			break;
	}

	valueType = UNDEFINED_VALUE;
}


bool Value::
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


bool Value::
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


void Value::
copy (Value &val)
{
	switch (val.valueType)
	{
		case STRING_VALUE:
			setStringValue (val.strValue);
			return;

		case INTEGER_VALUE:
			setIntegerValue (val.integerValue);
			return;

		case REAL_VALUE:
			setRealValue (val.realValue);
			return;

		case UNDEFINED_VALUE:
			setUndefinedValue ();
			return;

		case ERROR_VALUE:
			setErrorValue ();
			return;
	
		case LIST_VALUE:
			setListValue (val.listValue, VALUE_LIST_DUP);
			return;

		default:
			setUndefinedValue ();
	}	
}


bool Value::
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
			if (!listValue)
			{
				clear();
				return false;
			}
			listValue->Rewind();
			if (!dest.sendToSink((void*)" { ", 3)) return false;
			while ((val = listValue->Next()))
			{
				if (!val->toSink(dest)) return false;
				if (!listValue->AtEnd())
				{
					if (!dest.sendToSink((void*)" , ", 3)) return false;
				}
			}
			if (!dest.sendToSink((void*)" } ", 3)) return false;
			return true;

      	default:
        	return false;
    }

	// should not reach here
	return false;
}


void Value::
setRealValue (double r)
{
    clear();
    valueType=REAL_VALUE;
    realValue = r;
}

void Value::
setIntegerValue (int i)
{
    clear();
    valueType=INTEGER_VALUE;
    integerValue = i;
}

void Value::
setUndefinedValue (void)
{
    clear();
    valueType=UNDEFINED_VALUE;
}

void Value::
setErrorValue (void)
{
    clear();
    valueType=ERROR_VALUE;
}

void Value::
adoptStringValue(char *s, int adopt)
{
    clear();
    valueType = STRING_VALUE;
    stringValues.getCanonical (s, strValue, adopt);
}


void Value::
setStringValue(char *s)
{
    clear();
    valueType = STRING_VALUE;
    stringValues.getCanonical (s, strValue, SS_DUP);
}

void Value::
setStringValue(const SSString &s)
{
    clear();
    valueType = STRING_VALUE;
    strValue.copy (s);
}

void Value::
setListValue (ValueList *l, int mode)
{
    clear();
    valueType = LIST_VALUE;
    if (mode == VALUE_LIST_ADOPT)
        listValue = l;
    else
    if (mode == VALUE_LIST_DUP)
	{
		listValue = new ValueList;
        listValue->copy(l);
	}
}


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
	while ((val = Next()))
	{
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
	while ((val = list->Next()))
	{
		newVal = new Value;
		newVal->copy(*val);
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
	while ((v = Next()))
	{
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
	while ((v = Next()))
	{
		Operation::operate(META_EQUAL_OP, *v, val, result);
		if (result.isIntegerValue(i) && i)
			return true;
	}

	return false;
}
