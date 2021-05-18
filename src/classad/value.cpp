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

namespace classad {

const double Value::ScaleFactor[] = {
	1.0, 						// none
	1.0, 						// B
	1024.0,						// Kilo
	1024.0*1024.0, 				// Mega
	1024.0*1024.0*1024.0, 		// Giga
	1024.0*1024.0*1024.0*1024.0	// Terra
};

void Value::ApplyFactor()
{
	if (factor == NO_FACTOR) return;

	double r = 0;
	switch( valueType ) {
	case INTEGER_VALUE:
		r = integerValue;
		break;
	case REAL_VALUE:
		r = realValue;
		break;
	default:
		factor = NO_FACTOR;
		return;
	}

	valueType = REAL_VALUE;
	realValue = r * ScaleFactor[factor];
	factor = NO_FACTOR;
}

bool Value::
IsNumber (int &i) const
{
	long long i2;
	if ( IsNumber( i2 ) ) {
		i = (int)i2;		// truncation
		return true;
	} else {
		return false;
	}
}


bool Value::
IsNumber (long &i) const
{
	long long i2;
	if ( IsNumber( i2 ) ) {
		i = (long)i2;		// possible truncation
		return true;
	} else {
		return false;
	}
}


bool Value::
IsNumber (long long &i) const
{
	switch (valueType) {
		case INTEGER_VALUE:
			i = integerValue;
			return true;

		case REAL_VALUE:
			i = (long long) realValue;	// truncation	
			return true;

		case BOOLEAN_VALUE:
			i = booleanValue ? 1 : 0;
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

		case BOOLEAN_VALUE:
			r = booleanValue ? 1.0 : 0.0;
			return true;

		default:
			return false;
	}
}

bool Value::
IsBooleanValueEquiv(bool &b) const
{
	if ( !_useOldClassAdSemantics ) {
		return IsBooleanValue( b );
	}

	switch (valueType) {
		case BOOLEAN_VALUE:
			b = booleanValue;
			return true;

		case INTEGER_VALUE:
			b = ( integerValue ) ? true : false;
			return true;

		case REAL_VALUE:
			b = ( realValue ) ? true : false;
			return true;
		default: return false;
	}
	return false;
}


void Value::
CopyFrom( const Value &val )
{
	// optimization, when copying string to string, we can skip the delete/new of the string buffer
	if (valueType == STRING_VALUE && val.valueType == valueType) {
		*strValue = *(val.strValue);
		return;
	}

	_Clear();
	valueType = val.valueType;
	factor = val.factor;

	switch (val.valueType) {
		case STRING_VALUE:
			strValue = new string( *val.strValue);
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

		case SLIST_VALUE:
			slistValue = new classad_shared_ptr<ExprList>(*val.slistValue);
			return;

		case CLASSAD_VALUE:
			classadValue = val.classadValue;
			return;

		case SCLASSAD_VALUE:
			sclassadValue = new classad_shared_ptr<ClassAd>(*val.sclassadValue);
			return;

		case ABSOLUTE_TIME_VALUE:
			absTimeValueSecs = new abstime_t();
		  	*absTimeValueSecs = *val.absTimeValueSecs;
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
	_Clear();
    valueType=REAL_VALUE;
    realValue = r;
}

void Value::
SetBooleanValue( bool b )
{
	_Clear();
	valueType = BOOLEAN_VALUE;
	booleanValue = b;
}

void Value::
SetIntegerValue (long long i)
{
	_Clear();
    valueType=INTEGER_VALUE;
    integerValue = i;
}

void Value::
SetUndefinedValue (void)
{
	_Clear();
    valueType=UNDEFINED_VALUE;
}

void Value::
SetErrorValue (void)
{
	_Clear();
    valueType=ERROR_VALUE;
}

void Value::
SetStringValue( const string &s )
{
	// optimization, when copying string to string, we can skip the delete/new of the string buffer
	if (valueType == STRING_VALUE) {
		*strValue = s;
		return;
	}
	_Clear();
	valueType = STRING_VALUE;
	strValue = new string( s );
}

void Value::
SetStringValue( const char *s )
{
	// optimization, when copying string to string, we can skip the delete/new of the string buffer
	if (valueType == STRING_VALUE) {
		*strValue = s;
		return;
	}
	_Clear();
	valueType = STRING_VALUE;
	strValue = new string( s );
}

void Value::
SetStringValue( const char *s, size_t cch )
{
	// optimization, when copying string to string, we can skip the delete/new of the string buffer
	if (valueType == STRING_VALUE) {
		strValue->assign(s, cch);
		return;
	}
	_Clear();
	valueType = STRING_VALUE;
	strValue = new string( s, cch );
}

void Value::
SetListValue( ExprList *l)
{
	_Clear();
    valueType = LIST_VALUE;
    listValue = l;
}

void Value::
SetListValue( classad_shared_ptr<ExprList> l)
{
	_Clear();
    valueType = SLIST_VALUE;
    slistValue = new classad_shared_ptr<ExprList>(l);
}

void Value::
SetClassAdValue( classad_shared_ptr<ClassAd> ad )
{
	_Clear();
	valueType = SCLASSAD_VALUE;
    sclassadValue = new classad_shared_ptr<ClassAd>(ad);
}


void Value::
SetClassAdValue( ClassAd *ad )
{
	_Clear();
	valueType = CLASSAD_VALUE;
	classadValue = ad;
}

void Value::
SetRelativeTimeValue( time_t rsecs ) 
{
	_Clear();
	valueType = RELATIVE_TIME_VALUE;
	relTimeValueSecs = (double) rsecs;
}

void Value::
SetRelativeTimeValue( double rsecs ) 
{
	_Clear();
	valueType = RELATIVE_TIME_VALUE;
	relTimeValueSecs = rsecs;
}

void Value::
SetAbsoluteTimeValue( abstime_t asecs ) 
{
	_Clear();
	valueType = ABSOLUTE_TIME_VALUE;
	absTimeValueSecs = new abstime_t();
	*absTimeValueSecs = asecs;
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
        case Value::SLIST_VALUE:
            is_same = (*slistValue)->SameAs(otherValue.slistValue->get());
            break;
        case Value::CLASSAD_VALUE:
            is_same = classadValue->SameAs(otherValue.classadValue);
            break;
        case Value::SCLASSAD_VALUE:
            is_same = classadValue->SameAs(otherValue.sclassadValue->get());
            break;
        case Value::RELATIVE_TIME_VALUE:
            is_same = (relTimeValueSecs == otherValue.relTimeValueSecs);
            break;
        case Value::ABSOLUTE_TIME_VALUE:
            is_same = (   absTimeValueSecs->secs   == otherValue.absTimeValueSecs->secs
                       && absTimeValueSecs->offset == otherValue.absTimeValueSecs->offset);
            break;
        case Value::STRING_VALUE:
            is_same = (*strValue == *otherValue.strValue);
            break;
		default:
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
	case Value::SLIST_VALUE:
	case Value::CLASSAD_VALUE:
	case Value::SCLASSAD_VALUE:
	case Value::RELATIVE_TIME_VALUE:
	case Value::ABSOLUTE_TIME_VALUE: {
		unparser.Unparse(unparsed_text, value);
		stream << unparsed_text;
		break;
	}
	case Value::STRING_VALUE:
		stream << *value.strValue;
		break;
	default:
		break;
	}

	return stream;
}

bool convertValueToRealValue(const Value value, Value &realValue)
{
    bool                could_convert;
	string	            buf;
	const char	        *start;
	const char          *end;
	char                *end_tmp;
	long long           ivalue;
	time_t	            rtvalue;
	abstime_t           atvalue = { 0, 0 };
	bool	            bvalue = false;
	double	            rvalue;
	Value::NumberFactor nf;

	switch(value.GetType()) {
		case Value::UNDEFINED_VALUE:
			realValue.SetUndefinedValue();
			could_convert = false;
            break;

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::SCLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
			realValue.SetErrorValue();
			could_convert = false;
            break;

		case Value::STRING_VALUE:
            could_convert = true;
			value.IsStringValue(buf);
            start = buf.c_str();
			rvalue = strtod(start, &end_tmp);
			end = end_tmp;

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
                    nf = Value::NO_FACTOR;
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
	long long           ivalue;
	time_t	            rtvalue;
	abstime_t           atvalue = { 0, 0 };
	bool	            bvalue = false;
	double	            rvalue;
	Value::NumberFactor nf;

	switch(value.GetType()) {
		case Value::UNDEFINED_VALUE:
			integerValue.SetUndefinedValue();
			could_convert = false;
            break;

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::SCLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
			integerValue.SetErrorValue();
			could_convert = false;
            break;

		case Value::STRING_VALUE:
			value.IsStringValue( buf );
			ivalue = (long long) strtod( buf.c_str( ), (char**) &end);
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
                    nf = Value::NO_FACTOR;
                    break;
                }
                if (could_convert) {
                    integerValue.SetIntegerValue((long long) (ivalue*Value::ScaleFactor[nf]));
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
            integerValue.SetIntegerValue((long long) rvalue);
            could_convert = true;
            break;

		case Value::ABSOLUTE_TIME_VALUE:
			value.IsAbsoluteTimeValue(atvalue);
			integerValue.SetIntegerValue(atvalue.secs);
			could_convert = true;
            break;

		case Value::RELATIVE_TIME_VALUE:
			value.IsRelativeTimeValue(rtvalue);
			integerValue.SetIntegerValue((long long) rtvalue);
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
		case Value::SCLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
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

bool Value::
IsSListValue(classad_shared_ptr<ExprList>& l)
{
    if (valueType == SLIST_VALUE) {
        l = (*slistValue);
        return true;
    } else if (valueType == LIST_VALUE) {
            // we must copy our list, because it does not belong
            // to a shared_ptr
        l = classad_shared_ptr<ExprList>( (ExprList*)listValue->Copy() );
        if( !l ) {
            return false;
        }
            // in case we are called multiple times, stash a shared_ptr
            // to the copy of the list
        SetListValue(l);
        return true;
    } else {
        return false;
    }
}

bool Value::
IsSClassAdValue(classad_shared_ptr<ClassAd>& c)
{
    if (valueType == SCLASSAD_VALUE) {
        c = (*sclassadValue);
        return true;
    } else if (valueType == CLASSAD_VALUE) {
            // we must copy our list, because it does not belong
            // to a shared_ptr
        c = classad_shared_ptr<ClassAd>( (ClassAd*)classadValue->Copy() );
        if( !c ) {
            return false;
        }
            // in case we are called multiple times, stash a shared_ptr
            // to the copy of the list
        SetClassAdValue(c);
        return true;
    } else {
        return false;
    }
}

} // classad
