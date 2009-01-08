/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "classad/common.h"
#include "classad/operators.h"
#include "classad/sink.h"
#include "classad/value.h"
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
	relTimeValueSecs = 0;
	absTimeValueSecs.secs = 0;
	absTimeValueSecs.offset = 0;
}


Value::
Value(const Value &value)
{
    CopyFrom(value);
    return;
}

Value::
~Value()
{
}

Value& Value::
operator=(const Value &value)
{
    if (this != &value) {
        CopyFrom(value);
    }
    return *this;
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
		  	absTimeValueSecs = val.absTimeValueSecs;
			return;

		case RELATIVE_TIME_VALUE:
			relTimeValueSecs = val.relTimeValueSecs;
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
SetListValue( ExprList *l)
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
	relTimeValueSecs = (double) rsecs;
}

void Value::
SetRelativeTimeValue( double rsecs ) 
{
	valueType = RELATIVE_TIME_VALUE;
	relTimeValueSecs = rsecs;
}

void Value::
SetAbsoluteTimeValue( abstime_t asecs ) 
{
	valueType = ABSOLUTE_TIME_VALUE;
	absTimeValueSecs = asecs;
}	

bool Value::
SameAs(const Value &otherValue) const
{
    bool is_same = false;
    if (valueType != otherValue.valueType) {
        is_same = false;
    } else {
        switch (valueType) {
        case Value::NULL_VALUE:
        case Value::ERROR_VALUE:
        case Value::UNDEFINED_VALUE:
            is_same = true;
            break;
        case Value::BOOLEAN_VALUE:
            is_same = (booleanValue == otherValue.booleanValue);
            break;
        case Value::INTEGER_VALUE:
            is_same = (integerValue == otherValue.integerValue);
            break;
        case Value::REAL_VALUE:
            is_same = (realValue == otherValue.realValue);
            break;
        case Value::LIST_VALUE:
            is_same = listValue->SameAs(otherValue.listValue);
            break;
        case Value::CLASSAD_VALUE:
            is_same = classadValue->SameAs(otherValue.classadValue);
            break;
        case Value::RELATIVE_TIME_VALUE:
            is_same = (relTimeValueSecs == otherValue.relTimeValueSecs);
            break;
        case Value::ABSOLUTE_TIME_VALUE:
            is_same = (   absTimeValueSecs.secs   == otherValue.absTimeValueSecs.secs
                       && absTimeValueSecs.offset == otherValue.absTimeValueSecs.offset);
            break;
        case Value::STRING_VALUE:
            is_same = (strValue == otherValue.strValue);
            break;
        }
    }
    return is_same;
}

bool operator==(const Value &value1, const Value &value2)
{
    return value1.SameAs(value2);
}

ostream& operator<<(ostream &stream, Value &value)
{
	ClassAdUnParser unparser;
	string          unparsed_text;

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
	case Value::LIST_VALUE:
	case Value::CLASSAD_VALUE:
	case Value::RELATIVE_TIME_VALUE: 
	case Value::ABSOLUTE_TIME_VALUE: {
		unparser.Unparse(unparsed_text, value);
		stream << unparsed_text;
		break;
	}
	case Value::STRING_VALUE:
		stream << value.strValue;
		break;
	}

	return stream;
}

bool convertValueToRealValue(const Value value, Value &realValue)
{
    bool                could_convert;
	string	            buf;
	const char	        *start;
    char                *end;
	int		            ivalue;
	time_t	            rtvalue;
	abstime_t           atvalue;
	bool	            bvalue;
	double	            rvalue;
	Value::NumberFactor nf;

	switch(value.GetType()) {
		case Value::UNDEFINED_VALUE:
			realValue.SetUndefinedValue();
			could_convert = false;
            break;

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::LIST_VALUE:
			realValue.SetErrorValue();
			could_convert = false;
            break;

		case Value::STRING_VALUE:
            could_convert = true;
			value.IsStringValue(buf);
            start = buf.c_str();
			rvalue = strtod(start, &end);

				// On HPUX11, an input value of "INF" fails in
				// a strange way: end points beyond the null.
			if( classad_isinf(rvalue) && end > start+strlen(start) ) {
				end = (char *)start+strlen(start);
			}

			if (end == start && rvalue == 0.0) {
				// strtod() returned an error
				realValue.SetErrorValue();
				could_convert = false;
			}
			switch (toupper( *end )) {
				case 'B': nf = Value::B_FACTOR; break;
				case 'K': nf = Value::K_FACTOR; break;
				case 'M': nf = Value::M_FACTOR; break;
				case 'G': nf = Value::G_FACTOR; break;
				case 'T': nf = Value::T_FACTOR; break;
				case '\0': nf = Value::NO_FACTOR; break;
				default:
                    nf = Value::NO_FACTOR; // Prevent uninitialized variable warning
					realValue.SetErrorValue();
					could_convert = false;
                    break;
			}
            if (could_convert) {
                realValue.SetRealValue(rvalue*Value::ScaleFactor[nf]);
            }
            break;

		case Value::BOOLEAN_VALUE:
			value.IsBooleanValue(bvalue);
			realValue.SetRealValue(bvalue ? 1.0 : 0.0);
			could_convert = true;
            break;

		case Value::INTEGER_VALUE:
			value.IsIntegerValue(ivalue);
			realValue.SetRealValue((double)ivalue);
			could_convert = true;
            break;

		case Value::REAL_VALUE:
			realValue.CopyFrom(value);
			could_convert = true;
            break;

		case Value::ABSOLUTE_TIME_VALUE:
			value.IsAbsoluteTimeValue(atvalue);
			realValue.SetRealValue(atvalue.secs);
			could_convert = true;
            break;

		case Value::RELATIVE_TIME_VALUE:
			value.IsRelativeTimeValue(rtvalue);
			realValue.SetRealValue(rtvalue);
			could_convert = true;
            break;

		default:
            could_convert = false; // Make gcc's -Wuninitalized happy
			CLASSAD_EXCEPT( "Should not reach here" );
            break;
    }
    return could_convert;
}

bool convertValueToIntegerValue(const Value value, Value &integerValue)
{
    bool                could_convert;
	string	            buf;
    char                *end;
	int		            ivalue;
	time_t	            rtvalue;
	abstime_t           atvalue;
	bool	            bvalue;
	double	            rvalue;
	Value::NumberFactor nf;

	switch(value.GetType()) {
		case Value::UNDEFINED_VALUE:
			integerValue.SetUndefinedValue();
			could_convert = false;
            break;

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::LIST_VALUE:
			integerValue.SetErrorValue();
			could_convert = false;
            break;

		case Value::STRING_VALUE:
			value.IsStringValue( buf );
			ivalue = (int) strtod( buf.c_str( ), (char**) &end);
			if( end == buf && ivalue == 0 ) {
				// strtol() returned an error
				integerValue.SetErrorValue( );
                could_convert = false;
			} else {
                could_convert = true;
                switch( toupper( *end ) ) {
                case 'B':  nf = Value::B_FACTOR; break;
                case 'K':  nf = Value::K_FACTOR; break;
                case 'M':  nf = Value::M_FACTOR; break;
                case 'G':  nf = Value::G_FACTOR; break;
                case 'T':  nf = Value::T_FACTOR; break;
                case '\0': nf = Value::NO_FACTOR; break;
                default:  
                    nf = Value::NO_FACTOR; // avoid uninitialized warning
                    integerValue.SetErrorValue( );
                    could_convert = false;
                    break;
                }
                if (could_convert) {
                    integerValue.SetIntegerValue((int) (ivalue*Value::ScaleFactor[nf]));
                }
            }
            break;

		case Value::BOOLEAN_VALUE:
			value.IsBooleanValue(bvalue);
			integerValue.SetIntegerValue(bvalue ? 1 : 0);
			could_convert = true;
            break;

		case Value::INTEGER_VALUE:
            integerValue.CopyFrom(value);
			could_convert = true;
            break;

		case Value::REAL_VALUE:
            value.IsRealValue(rvalue);
            integerValue.SetIntegerValue((int) rvalue);
            could_convert = true;
            break;

		case Value::ABSOLUTE_TIME_VALUE:
			value.IsAbsoluteTimeValue(atvalue);
			integerValue.SetIntegerValue(atvalue.secs);
			could_convert = true;
            break;

		case Value::RELATIVE_TIME_VALUE:
			value.IsRelativeTimeValue(rtvalue);
			integerValue.SetIntegerValue((int) rtvalue);
			could_convert = true;
            break;

		default:
            could_convert = false; // Make gcc's -Wuninitalized happy
			CLASSAD_EXCEPT( "Should not reach here" );
            break;
    }
    return could_convert;
}

bool convertValueToStringValue(const Value value, Value &stringValue)
{
    bool             could_convert;
	time_t	         rtvalue;
	abstime_t        atvalue;
    string           string_representation;
    ClassAdUnParser  unparser;

	switch(value.GetType()) {
		case Value::UNDEFINED_VALUE:
            stringValue.SetUndefinedValue();
			could_convert = false;
            break;

        case Value::ERROR_VALUE:
			stringValue.SetErrorValue();
			could_convert = false;
            break;

        case Value::STRING_VALUE:
            stringValue.CopyFrom(value);
            could_convert = true;
            break;

		case Value::CLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::BOOLEAN_VALUE:
		case Value::INTEGER_VALUE:
		case Value::REAL_VALUE:
            unparser.Unparse(string_representation, value);
            stringValue.SetStringValue(string_representation);
            could_convert = true;
            break;

		case Value::ABSOLUTE_TIME_VALUE:
			value.IsAbsoluteTimeValue(atvalue);
            absTimeToString(atvalue, string_representation);
			stringValue.SetStringValue(string_representation);
			could_convert = true;
            break;

		case Value::RELATIVE_TIME_VALUE:
			value.IsRelativeTimeValue(rtvalue);
            relTimeToString(rtvalue, string_representation);
			stringValue.SetStringValue(string_representation);
			could_convert = true;
            break;

		default:
            could_convert = false; // Make gcc's -Wuninitalized happy
			CLASSAD_EXCEPT( "Should not reach here" );
            break;
    }
    return could_convert;
}

END_NAMESPACE // classad
