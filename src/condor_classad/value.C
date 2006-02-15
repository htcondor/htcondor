/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
