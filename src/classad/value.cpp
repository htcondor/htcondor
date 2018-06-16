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

void Value::
_Clear()
{
	switch( valueType ) {
		case SLIST_VALUE:
			delete slistValue;
			break;

		case STRING_VALUE:
			delete strValue;
			break;

		case ABSOLUTE_TIME_VALUE:
			delete absTimeValueSecs;
			break;

		default:
		case LIST_VALUE:
		case CLASSAD_VALUE:
			// list and classad values live in the evaluation environment, so they must
			// never be explicitly destroyed
			break;
	}

	classadValue = NULL; // this clears the entire union
	factor = NO_FACTOR;
}

#ifdef TJ_PICKLE

bool UpdateClassad(classad::ClassAd & ad, ExprStream & stm)
{
	return ad.Update(stm);
}

bool Value::Set(ExprStream & stm)
{
	if (valueType & VALUE_OWNS_POINTER) { _Clear(); }

	ExprStream::Mark mk = stm.mark();
	unsigned char t = 0;
	if ( ! stm.readByte(t) || t < ExprStream::LitBase || t > ExprStream::SmallString) {
		goto bail;
	}

	// check for compact representations
	if (t >= ExprStream::ByteValBase) {
		bool success = true;
		// do compact representations
		if (t == ExprStream::SmallString) {
			unsigned char cch = 0;
			if ( ! stm.readByte(cch) || ! stm.hasBytes(cch)) {
				goto bail;
			}
			valueType = STRING_VALUE;
			strValue = new string(stm.readChars(cch), cch);
			success = true;
		} else if (t >= ExprStream::SmallInt) {
			bool is_float = (t&4) != 0;
			int ival = 0;
			switch (t&3) {
			case 0: {
				signed char ival8;
				if ( ! stm.readInteger(ival8)) goto bail;
				ival = ival8;
				} break;
			case 1: {
				signed short ival16;
				if ( ! stm.readInteger(ival16)) goto bail;
				ival = ival16;
				} break;
			case 2: {
				if ( ! stm.readInteger(ival)) goto bail;
				} break;
			}
			if (is_float) {
				valueType = REAL_VALUE;
				realValue = ival;
			} else {
				valueType = INTEGER_VALUE;
				integerValue = ival;
			}
			success = true;
		} else {
			switch (t) {
			case ExprStream::ByteValNull:  valueType = NULL_VALUE; integerValue = 0; break;
			case ExprStream::ByteValError: valueType = ERROR_VALUE; integerValue = 0; break;
			case ExprStream::ByteValUndef: valueType = UNDEFINED_VALUE; integerValue = 0; break;
			case ExprStream::ByteValFalse: valueType = BOOLEAN_VALUE; booleanValue = false; break;
			case ExprStream::ByteValTrue:  valueType = BOOLEAN_VALUE; booleanValue = true; break;
			case ExprStream::ByteValZero:  valueType = INTEGER_VALUE; integerValue = 0; break;
			case ExprStream::ByteValOne:   valueType = INTEGER_VALUE; integerValue = 1; break;
			default: success = false; break;
			}
		}
		if (success) { factor = NO_FACTOR; }
		return success;
	}

	t -= ExprStream::LitBase;
	valueType = t ? (ValueType)(1<<(t-1)) : NULL_VALUE;
	factor = NO_FACTOR;
	switch (valueType) {
	case BOOLEAN_VALUE:
		if ( ! stm.readByte(t) || t > 1) {
			goto bail;
		}
		booleanValue = t ? true : false;
		break;

	case INTEGER_VALUE:
	case REAL_VALUE:
	case RELATIVE_TIME_VALUE:
		if ( ! stm.readInteger(integerValue) || ! stm.readByte(t) || t > NumberFactor::T_FACTOR) {
			goto bail;
		}
		factor = (NumberFactor)t;
		break;

	case ABSOLUTE_TIME_VALUE: {
			abstime_t tm;
			if ( ! stm.readInteger(tm.secs) || ! stm.readInteger(tm.offset)) {
				goto bail;
			}
			absTimeValueSecs = new abstime_t();
			*absTimeValueSecs = tm;
		} break;

	case STRING_VALUE: {
			unsigned int cch = INT_MAX;
			if ( ! stm.readInteger(cch) || ! stm.hasBytes(cch)) {
				goto bail;
			}
			strValue = new string(stm.readChars(cch), cch);
		} break;

	case CLASSAD_VALUE: {
			ExprStream stm2;
			if ( ! stm.readStream(stm2)) {
				goto bail;
			}
			classadValue = new ClassAd();
			if ( ! UpdateClassad(*classadValue, stm2)) {
				goto bail;
			}
		} break;

	case LIST_VALUE:
	case SLIST_VALUE: {
			ExprStream stm2;
			if ( ! stm.readStream(stm2)) {
				goto bail;
			}
			ExprList *lst = ExprList::Make(stm2);
			if ( ! lst)
				goto bail;
			if (valueType == SLIST_VALUE) {
				slistValue = new  classad_shared_ptr<ExprList>(lst);
			} else {
				listValue = lst;
			}
		} break;

	default:
		// nothing more to read from the stream.
		break;
	}

	return true;
bail:
	_Clear();
	stm.unwind(mk);
	return false;
}



unsigned int Value::Pickle(ExprStreamMaker & stm, bool compact) const
{
	ExprStreamMaker::Mark mk = stm.mark();

	// handle the compact cases
	if (compact && (valueType & (NULL_VALUE|ERROR_VALUE|UNDEFINED_VALUE|BOOLEAN_VALUE|INTEGER_VALUE|REAL_VALUE|STRING_VALUE))) {
		if (valueType == STRING_VALUE) {
			size_t len = 0;
			if (strValue) len = strValue->length();
			if (len < 255) {
				unsigned char cch = 0;
				stm.putByte(ExprStream::SmallString);
				stm.putByte((unsigned char)len);
				if (len) stm.putBytes(strValue->data(), len);
				return stm.added(mk);
			}
		} else {

			int ival = 0;
			unsigned char ct = 0;

			switch (valueType) {
			case NULL_VALUE:      ct = ExprStream::ByteValNull; break;
			case ERROR_VALUE:	  ct = ExprStream::ByteValError; break;
			case UNDEFINED_VALUE: ct = ExprStream::ByteValUndef; break;
			case BOOLEAN_VALUE:   ct = booleanValue ? ExprStream::ByteValTrue : ExprStream::ByteValFalse; break;
			case INTEGER_VALUE:
				if (integerValue == 0) {
					ct = ExprStream::ByteValZero;
				} else if (integerValue == 1) {
					ct = ExprStream::ByteValOne;
				} else {
					ival = (int)integerValue;
					if (ival == integerValue) { ct = ExprStream::SmallInt; }
				}
				break;
			case REAL_VALUE:
				ival = (int)realValue;
				if (ival == realValue) { ct = ExprStream::SmallFloat; }
				break;
			}

			// if a compact type, write it now, otherwise fall through to do non-compact
			if (ct) {
				if (ct < ExprStream::SmallInt) {
					stm.putByte(ct);
				} else {
					signed char ival8 = (signed char)ival;
					signed short int ival16 = (signed short int)ival;
					if (ival8 == ival) {
						stm.putByte(ct);
						stm.putInteger(ival8);
					} else if (ival16 == ival) {
						stm.putByte(ct+1);
						stm.putInteger(ival16);
					} else {
						stm.putByte(ct+2);
						stm.putInteger(ival);
					}
				}
				return stm.added(mk);
			}
		}
	}

	//unsigned char t = 0;
	switch (valueType) {
		case NULL_VALUE:
			stm.putByte(ExprStream::LitBase + 0);
			break;

		case ERROR_VALUE:
			stm.putByte(ExprStream::LitBase + 1+0);
			break;

		case UNDEFINED_VALUE:
			stm.putByte(ExprStream::LitBase + 1+1);
			break;

		case BOOLEAN_VALUE:
			stm.putByte(ExprStream::LitBase + 1+2);
			stm.putByte(booleanValue ? 0 : 1);
			break;

		case INTEGER_VALUE:
			stm.putByte(ExprStream::LitBase + 1+3);
			stm.putInteger(integerValue);
			stm.putByte((unsigned char)factor);
			break;

		case REAL_VALUE:
			stm.putByte(ExprStream::LitBase + 1+4);
			stm.putInteger(integerValue);
			stm.putByte((unsigned char)factor);
			break;

		case RELATIVE_TIME_VALUE:
			stm.putByte(ExprStream::LitBase + 1+5);
			stm.putInteger(integerValue);
			stm.putByte((unsigned char)factor);
			break;

		case ABSOLUTE_TIME_VALUE:
			stm.putByte(ExprStream::LitBase + 1+6);
			stm.putInteger(absTimeValueSecs->secs);
			stm.putInteger(absTimeValueSecs->offset);
			break;

		case STRING_VALUE:
			stm.putByte(ExprStream::LitBase + 1+7);
			{
				unsigned int cch = 0;
				if (strValue) { cch = (unsigned int)strValue->size(); }
				stm.putInteger(cch);
				if (cch) { stm.putBytes(strValue->data(), cch); }
			}
			break;

		case CLASSAD_VALUE:
			stm.putByte(ExprStream::LitBase + 1+8);
			{
				unsigned int cb = 0;
				ExprStreamMaker::Mark mkSize = stm.mark(sizeof(cb));
				cb = classadValue->Pickle(stm, compact, NULL);
				stm.updateAt(mkSize, cb);
			}
			break;

		case LIST_VALUE:
			stm.putByte(ExprStream::LitBase + 1+9);
			{
				unsigned int cb = 0;
				ExprStreamMaker::Mark mkSize = stm.mark(sizeof(cb));
				cb = listValue->Pickle(stm, compact);
				stm.updateAt(mkSize, cb);
			}
			break;

		case SLIST_VALUE:
			stm.putByte(ExprStream::LitBase + 1+10);
			{
				unsigned int cb = 0;
				ExprStreamMaker::Mark mkSize = stm.mark(sizeof(cb));
				cb = listValue->Pickle(stm, compact);
				stm.updateAt(mkSize, cb);
			}
			break;
	}

	return stm.added(mk);
}
#endif

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

} // classad
