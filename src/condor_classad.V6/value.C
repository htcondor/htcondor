/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include "common.h"
#include "operators.h"
#include "value.h"
#include <iostream>

using namespace std;

BEGIN_NAMESPACE( classad )

const double Value::ScaleFactor[] = {
	1.0, 						// none
	1.0, 						// B
	1024.0,						// Kilo
	1024.0*1024.0, 				// Mega
	1024.0*1024.0*1024.0, 		// Giga
	1024.0*1024.0*1024.0*1024.0	// Terra
};


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
SetClassAdValue( ClassAd *ad )
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


ostream& operator<<(ostream &stream, Value &value)
{
	switch (value.valueType) {
	case Value::NULL_VALUE:
		stream << "(null)";
		break;
	case Value::ERROR_VALUE:
		stream << "error";
		break;
	case Value::UNDEFINED_VALUE:
		stream << "undefined";
		break;
	case Value::BOOLEAN_VALUE:
		if (value.booleanValue) {
			stream << "true";
		} else {
			stream << "false";
		}
		break;
	case Value::INTEGER_VALUE:
		stream << value.integerValue;
		break;
	case Value::REAL_VALUE:
		stream << value.realValue;
		break;
	case Value::RELATIVE_TIME_VALUE:
		stream << "time value";
		break;
	case Value::ABSOLUTE_TIME_VALUE:
		stream << "time value";
		break;
	case Value::STRING_VALUE:
		stream << value.strValue;
		break;
	case Value::CLASSAD_VALUE:
		stream << "classad value";
		break;
	case Value::LIST_VALUE:
		stream << "list value";
		break;
	}

	return stream;
}


END_NAMESPACE // classad
