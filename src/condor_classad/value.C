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
#include "value.h"


Value::
Value()
{
	valueType = UNDEFINED_VALUE;
	integerValue = 0;
	realValue = 0.0;
	strValue = NULL;
}


Value::
~Value()
{
}


const Value& Value::
operator=( const Value& val )
{
	valueType = val.valueType;
	integerValue = val.integerValue;
	realValue = val.realValue;
	strValue = val.strValue;
	return( *this );
}


void Value::
clear()
{
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
setStringValue(char *s)
{
    clear();
    valueType = STRING_VALUE;
    strValue = s;
}
