#include "condor_common.h"
#include "operators.h"
#include "value.h"

Value::
Value()
{
	listValue = NULL;
	classadValue = NULL;
	valueType = UNDEFINED_VALUE;
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
			// list values live in the evaluation environment, so they must
			// never be explicitly destroyed
			listValue = NULL;
			break;

		case CLASSAD_VALUE:
			// classad values live in the evaluation environment, so they must 
			// never be explicitly destroyed
			classadValue = NULL;
			break;

		case STRING_VALUE:
			// strValue is assumed to be a pointer to memory that is not
			// handled by the Value class; so don't attempt to free it
			strValue = NULL;
			break;

		default:
			valueType = UNDEFINED_VALUE;
	}
	valueType 	= UNDEFINED_VALUE;
}


bool Value::
isNumber (int &i)
{
	switch (valueType) {
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
	switch (valueType) {
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
copyFrom(Value &val)
{
	switch (val.valueType) {
		case STRING_VALUE:
			setStringValue( val.strValue );
			return;

		case BOOLEAN_VALUE:
			setBooleanValue( val.booleanValue );
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
			setListValue (val.listValue);
			return;

		case CLASSAD_VALUE:
			setClassAdValue (val.classadValue);
			return;

		default:
			setUndefinedValue ();
	}	
}


bool Value::
toSink (Sink &dest)
{
	char tempBuf[512];

    switch (valueType) {
      	case UNDEFINED_VALUE:
        	return (dest.sendToSink((void*)"undefined", strlen("undefined")));

      	case ERROR_VALUE:
        	return (dest.sendToSink ((void*)"error", strlen("error")));

		case BOOLEAN_VALUE:
			sprintf( tempBuf, "%s", booleanValue ? "true" : "false" );
			return( dest.sendToSink( (void*)tempBuf, booleanValue?4:5 ) );

      	case INTEGER_VALUE:
			sprintf (tempBuf, "%d", integerValue);
			return (dest.sendToSink ((void*)tempBuf, strlen(tempBuf)));

      	case REAL_VALUE:
        	sprintf (tempBuf, "%f", realValue);
			return (dest.sendToSink ((void*)tempBuf, strlen(tempBuf)));

      	case STRING_VALUE:
			return (dest.sendToSink ((void*) "\"", 1) 					&&
					dest.sendToSink ((void*)strValue, strlen(strValue))	&&
					dest.sendToSink ((void*) "\"", 1));

		case LIST_VALUE:
			// sanity check
			if (!listValue) {
				clear();
				return false;
			}
			listValue->toSink( dest );
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


void Value::
setRealValue (double r)
{
    clear();
    valueType=REAL_VALUE;
    realValue = r;
}

void Value::
setBooleanValue( bool b )
{
	clear();
	valueType = BOOLEAN_VALUE;
	booleanValue = b;
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
adoptStringValue(char *s, int )
{
    clear();
	valueType = STRING_VALUE;
    strValue = s;
}

void Value::
setStringValue(char *s)
{
    clear();
    valueType = STRING_VALUE;
    strValue = s;
}

void Value::
setListValue (ExprList *l)
{
    clear();
    valueType = LIST_VALUE;
    listValue = l;
}

void Value::
setClassAdValue( ClassAd *ad )
{
	clear();
	valueType = CLASSAD_VALUE;
	classadValue = ad;
}
