/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "operators.h"
#include "value.h"

BEGIN_NAMESPACE( classad )

Value::
Value( )
{
	valueType = UNDEFINED_VALUE;
	booleanValue = false;
	integerValue = 0;
	realValue = 0.0;
	listValue = NULL;
	classadValue = NULL;
	timeValueSecs = 0;
	strValue = "";
}


Value::
~Value()
{
}


void Value::
Clear()
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
			strValue = "";
			break;

		default:
			valueType = UNDEFINED_VALUE;
	}
	valueType 	= UNDEFINED_VALUE;
}


bool Value::
IsNumber (int &i) const
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
IsNumber (double &r) const
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
CopyFrom( const Value &val )
{
	valueType = val.valueType;
	switch (val.valueType) {
		case STRING_VALUE:
			strValue = val.strValue;
			return;

		case BOOLEAN_VALUE:
			booleanValue = val.booleanValue;
			return;

		case INTEGER_VALUE:
			integerValue = val.integerValue;
			return;

		case REAL_VALUE:
			realValue = val.realValue;
			return;

		case UNDEFINED_VALUE:
		case ERROR_VALUE:
			return;
	
		case LIST_VALUE:
			listValue = val.listValue;
			return;

		case CLASSAD_VALUE:
			classadValue = val.classadValue;
			return;

		case ABSOLUTE_TIME_VALUE:
		case RELATIVE_TIME_VALUE:
			timeValueSecs = val.timeValueSecs;
			return;

		default:
			SetUndefinedValue ();
	}	
}


void Value::
SetRealValue (double r)
{
    valueType=REAL_VALUE;
    realValue = r;
}

void Value::
SetBooleanValue( bool b )
{
	valueType = BOOLEAN_VALUE;
	booleanValue = b;
}

void Value::
SetIntegerValue (int i)
{
    valueType=INTEGER_VALUE;
    integerValue = i;
}

void Value::
SetUndefinedValue (void)
{
    valueType=UNDEFINED_VALUE;
}

void Value::
SetErrorValue (void)
{
    valueType=ERROR_VALUE;
}

void Value::
SetStringValue( const string &s )
{
	valueType = STRING_VALUE;
	strValue = s;
}

void Value::
SetStringValue( const char *s )
{
	valueType = STRING_VALUE;
	strValue = s;
}

void Value::
SetListValue( const ExprList *l)
{
    valueType = LIST_VALUE;
    listValue = l;
}

void Value::
SetClassAdValue( const ClassAd *ad )
{
	valueType = CLASSAD_VALUE;
	classadValue = ad;
}

void Value::
SetRelativeTimeValue( time_t rsecs ) 
{
	valueType = RELATIVE_TIME_VALUE;
	timeValueSecs = rsecs;
}


void Value::
SetAbsoluteTimeValue( time_t asecs ) 
{
	valueType = ABSOLUTE_TIME_VALUE;
	timeValueSecs = asecs;
}	


END_NAMESPACE // classad
