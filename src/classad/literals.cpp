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
#include "classad/exprTree.h"
#include "classad/util.h"

#include <charconv>
#include <algorithm>

using std::string;
using std::vector;
using std::pair;


namespace classad {

static inline void nextDigitChar(const string &Str, int &index);
static inline void prevNonSpaceChar(const string &Str, int &index);
static int revInt(std::string revNumStr);
static double revDouble(const string &revNumStr);
static bool extractTimeZone(string &timeStr, int &tzhr, int &tzmin);

AbstimeLiteral*
Literal::MakeAbsTime( time_t secs, int offset )
{
	abstime_t abst{secs, offset};
	return new AbstimeLiteral(abst);
}

AbstimeLiteral*
Literal::MakeAbsTime( abstime_t *tim )
{
    abstime_t abst;
    if (tim == NULL) { // => current time/offset
        time_t now;
        time( &now );
        abst.secs = now;
        abst.offset = timezone_offset( now, false );
    }
    else { //make a literal out of the passed value
        abst = *tim;
    }
    return new AbstimeLiteral(abst);
}

/* Creates an absolute time literal, from the string timestr, 
 *parsing it as the regular expression:
 D* dddd [D* dd [D* dd [D* dd [D* dd [D* dd D*]]]]] [-dd:dd | +dd:dd | z | Z]
 D => non-digit, d=> digit
 Ex - 2003-01-25T09:00:00-06:00
*/
AbstimeLiteral * 
Literal::MakeAbsTime(string timeStr )
{    
	abstime_t abst;
	Value val;
	bool offset = false; // to check if the argument conatins a timezone offset parameter
	
	struct tm abstm;
    memset(&abstm, 0, sizeof(abstm));
	int tzhr = 0; // corresponds to 1st "dd" in -|+dd:dd
	int tzmin = 0; // corresponds to 2nd "dd" in -|+dd:dd
	
	int len = (int)timeStr.length();
	int i=len-1; 
	prevNonSpaceChar(timeStr,i);
	if ((i > 0) && ((timeStr[i] == 'z') || (timeStr[i] == 'Z'))) { // z|Z corresponds to a timezone offset of 0
		offset = true;
		timeStr.erase(i,1); // remove the offset section from the string
		tzhr = 0;
		tzmin = 0;
    } else if ((len > 5) && (timeStr[len-5] == '+' || timeStr[len-5] == '-')) {
        offset = extractTimeZone(timeStr, tzhr, tzmin);
    } else if ((len > 6) && ((timeStr[len-6] == '+' || timeStr[len-6] == '-') && timeStr[len-3] == ':')) {
        timeStr.erase(len-3, 1);
        offset = extractTimeZone(timeStr, tzhr, tzmin);
    }

	i=0;
	len = (int)timeStr.length();
	
	nextDigitChar(timeStr,i);
	if(i > len-4) { // string has to contain dddd (year)
		return nullptr;
	}    
	
	abstm.tm_year = atoi(timeStr.substr(i,4).c_str()) - 1900;
	i += 4;
	nextDigitChar(timeStr,i);
	
	if(i<=len-2) {
		abstm.tm_mon = atoi(timeStr.substr(i,2).c_str()) - 1;
		i += 2;
	}
	nextDigitChar(timeStr,i);
	
	if(i<=len-2) {
		abstm.tm_mday = atoi(timeStr.substr(i,2).c_str());	
		i += 2;
	}
	nextDigitChar(timeStr,i);
	
	if(i<=len-2) {
		abstm.tm_hour += atoi(timeStr.substr(i,2).c_str()); 
		i += 2;
	}  
	nextDigitChar(timeStr,i);
	
	if(i<=len-2) {
		abstm.tm_min += atoi(timeStr.substr(i,2).c_str());
		i += 2;
	}  
	nextDigitChar(timeStr,i);
	
	if(i<=len-2) {
		abstm.tm_sec = atoi(timeStr.substr(i,2).c_str());	
		i += 2;
	}
	nextDigitChar(timeStr,i);
	
	if((i<=len-1) && (isdigit(timeStr[i]))) {  // there should be no more digit characters once the required
		return nullptr;
	}      
	
	abst.secs = mktime(&abstm);
	
	if(abst.secs == -1)  { // the time should be one, which can be supported by the time_t type
		return nullptr;
	}      

	if(offset) {
		abst.offset = (tzhr*3600) + (tzmin*60);
	}
	else { // if offset is not specified, the offset of the current locality is taken
		abst.offset = findOffset(abst.secs);
		//abst.secs -= abst.offset;
	}

	// mktime() creates the time assuming we specified something in
	// local time.  We want the time as if we were in Greenwich (we'll
	// call gmTime later to extract it, not localtime()), so we adjust
	// by our timezone.
	abst.secs += timezone_offset( abst.secs, true );

	// If the time string we converted had a timezone offset (either
	// explicit or implicit), we need to adjust the resulting time_t
	// so that our final value is UTC.
	abst.secs -= abst.offset;
	
	if(abst.offset == -1) { // corresponds to illegal offset
		return nullptr;
	}
	else {
		return new AbstimeLiteral(abst);
	}
}

ReltimeLiteral *
Literal::MakeRelTime( time_t t1, time_t t2 )
{
	if( t1<0 ) time( &t1 );
	if( t2<0 ) time( &t2 );
	return new ReltimeLiteral(t1 - t2);
}


ReltimeLiteral *
Literal::MakeRelTime( time_t secs )
{
	struct	tm 	lt;

	if( secs<0 ) {
		time(&secs );
		getLocalTime( &secs, &lt );
		return new ReltimeLiteral((time_t) (lt.tm_hour*3600 + lt.tm_min*60 + lt.tm_sec));
	} else {
		return new ReltimeLiteral((time_t) secs);
	}
}

ReltimeLiteral *
Literal::MakeRelTime( double secs )
{
	return new ReltimeLiteral(secs);
}


/* Creates a relative time literal, from the string timestr, 
 *parsing it as [[[days+]hh:]mm:]ss
 * Ex - 1+00:02:00
 */
ReltimeLiteral * 
Literal::MakeRelTime(const string &timeStr)
{
	double rsecs;
	
	int len = (int)timeStr.length();
	double secs = 0;
	int mins = 0;
	int hrs = 0;
	int days = 0;
	bool negative = false;
    
	int i=len-1; 
	prevNonSpaceChar(timeStr,i);
	// checking for 'sec' parameter & collecting it if present (ss.sss)
	if((i>=0) &&((timeStr[i] == 's') || (timeStr[i] == 'S') || (isdigit(timeStr[i])))) {
		if((timeStr[i] == 's') || (timeStr[i] == 'S')) {
			i--;
		}
		prevNonSpaceChar(timeStr,i);
		string revSecStr;
		while((i>=0) &&(isdigit(timeStr[i]))) {
			revSecStr += timeStr[i--];
		}
		if((i>=0) &&(timeStr[i] == '.')) {
			revSecStr += timeStr[i--];
			while((i>=0) && (isdigit(timeStr[i]))) {
				revSecStr += timeStr[i--];
			}
		}
		secs = revDouble(revSecStr);
	}
	
	prevNonSpaceChar(timeStr,i);
	// checking for 'min' parameter
	if((i>=0) &&((timeStr[i] == 'm') || (timeStr[i] == 'M') || (timeStr[i] == ':'))) {
		i--;
		string revMinStr;
		prevNonSpaceChar(timeStr,i);
		while((i>=0) &&(isdigit(timeStr[i]))) {
			revMinStr += timeStr[i--];
		}
		mins = revInt(revMinStr);
	}
	
	prevNonSpaceChar(timeStr,i);
	// checking for 'hrs' parameter
	if((i>=0) &&((timeStr[i] == 'h') || (timeStr[i] == 'H') || (timeStr[i] == ':'))) {
		i--;
		string revHrStr;
		prevNonSpaceChar(timeStr,i);
		while((i>=0) &&(isdigit(timeStr[i]))) {
			revHrStr += timeStr[i--];
		}
		hrs = revInt(revHrStr);
	}   
	
	prevNonSpaceChar(timeStr,i);
	// checking for 'days' parameter
	if((i>=0) &&((timeStr[i] == 'd') || (timeStr[i] == 'D') || (timeStr[i] == '+'))) {
		i--;
		string revDayStr;
		prevNonSpaceChar(timeStr,i);
		while((i>=0) &&(isdigit(timeStr[i]))) {
			revDayStr += timeStr[i--];
		}
		days = revInt(revDayStr);
	}     
	
	prevNonSpaceChar(timeStr,i);
	// checking for '-' operator
	if((i>=0) &&(timeStr[i] == '-')) {
		negative = true;
		i--;
	}
	
	prevNonSpaceChar(timeStr,i);
    
	if((i>=0) && (!(isspace(timeStr[i])))) { // should not conatin any non-space char beyond -,d,h,m,s
		return nullptr;
	}   
	
	rsecs = ( negative ? -1 : +1 ) * ( days*86400 + hrs*3600 + mins*60 + secs );
	
	return new ReltimeLiteral(rsecs);
}


/*
static AbstimeLiteral   *MakeAbsTime( abstime_t *now=NULL );
static AbstimeLiteral   *MakeAbsTime( std::string timestr);
static ReltimeLiteral   *MakeRelTime( time_t secs=-1 );
static ReltimeLiteral   *MakeRelTime( time_t t1, time_t t2 );
static ReltimeLiteral   *MakeRelTime( double secs );
static ReltimeLiteral   *MakeRelTime(const std::string &str);
static UndefinedLiteral *MakeUndefined();
static ErrorLiteral     *MakeError();
static BooleanLiteral   *MakeBool(bool val);
static IntegerLiteral   *MakeInteger(int64_t i);
static RealLiteral      *MakeReal(double d);
static StringLiteral    *MakeString(const std::string &str);
static StringLiteral    *MakeString(const char *);
static StringLiteral    *MakeString(const char *, size_t size);
static Literal          *MakeLiteral( const Value& v, Value::NumberFactor f=Value::NO_FACTOR);
*/

#ifdef TJ_PICKLE

/* static */
Literal * Literal::Make(ExprStream & stm)
{
	Literal * retval = nullptr;

	ExprStream::Mark mk = stm.mark();
	unsigned char ct = 0;
	if ( ! stm.readByte(ct) || ct < ExprStream::EsLitBase || ct > ExprStream::EsBigString) {
		goto bail;
	}

	// check for compact representations
	if (ct <= ExprStream::EsDouble) {
		// do numbers int and real
		if (ct >= ExprStream::EsInt8) {
			bool is_float = (ct&4) != 0;
			if ((ct &3) == 3) { // full 64 bit values
				if (is_float) {
					double rval = 0;
					if ( ! stm.readInteger(rval)) goto bail;
					retval = MakeReal(rval);
				} else {
					int64_t ival = 0;
					if ( ! stm.readInteger(ival)) goto bail;
					retval = MakeInteger(ival);
				}
			} else {
				int ival = 0;
				switch (ct&3) {
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
					retval = MakeReal(ival);
				} else {
					retval = MakeInteger(ival);
				}
			}
		} else {
			// literals with no payload
			switch (ct) {
			case ExprStream::EsLitError:  retval = MakeError(); break;
			case ExprStream::EsLitUndef:  retval = MakeUndefined(); break;
			case ExprStream::EsBoolFalse: retval = MakeBool(false); break;
			case ExprStream::EsBoolTrue:  retval = MakeBool(true); break;
			case ExprStream::EsIntZero:   retval = MakeInteger(0); break;
			case ExprStream::EsIntOne:    retval = MakeInteger(1); break;
			case ExprStream::EsFloatZero: retval = MakeReal(0.0); break;
			case ExprStream::EsEmptyString: retval = MakeString(""); break;
			default: goto bail; break;
			}
		}
		return retval;
	}

	// remaining literals:
	switch (ct) {
	case ExprStream::EsLitRelTime: {
		double secs = 0;
		if ( ! stm.readInteger(secs)) { goto bail; }
		retval = MakeRelTime(secs);
		} break;

	case ExprStream::EsLitAbsTime: {
		int64_t secs; int32_t offset;
		if ( ! stm.readInteger(secs) || ! stm.readInteger(offset)) { goto bail; }
		retval = MakeAbsTime(secs, offset);
		} break;

	case ExprStream::EsString:
	case ExprStream::EsBigString: {
		std::string_view str;
		if ( ! stm.readSizeString(str, ct == ExprStream::EsString)) { goto bail; }
		retval = MakeString(str.data(), str.size());
		} break;

	case ExprStream::EsSimpleStr:
	case ExprStream::EsBigSimpleStr: {
		std::string_view str;
		if ( ! stm.readSizeString(str, ct == ExprStream::EsSimpleStr)) { goto bail; }
		retval = MakeString(str.data(), str.size());
	} break;

	default:
		// nothing more to read from the stream.
		break;
	}

	return retval;
bail:
	stm.unwind(mk);
	return nullptr;
}

/*static*/ unsigned int Literal::Scan(ExprStream & stm, NodeKind & kind)
{
	ExprStream::Mark mk = stm.mark();
	unsigned char ct = 0;
	if ( ! stm.readByte(ct) || ct < ExprStream::EsLitBase || ct > ExprStream::EsNullVal) {
		goto bail;
	}

	// check for compact representations
	if (ct <= ExprStream::EsDouble) {
		// do numbers int and real
		if (ct >= ExprStream::EsInt8) {
			bool is_float = (ct&4) != 0;
			const unsigned char sizes[]{1,2,4,8};
			if ( ! stm.readBytes(sizes[ct&3])) goto bail;
			kind =  is_float ? REAL_LITERAL : INTEGER_LITERAL;
		} else {
			// literals with no payload
			switch (ct) {
			case ExprStream::EsLitError:  kind = ERROR_LITERAL; break;
			case ExprStream::EsLitUndef:  kind = UNDEFINED_LITERAL; break;
			case ExprStream::EsBoolFalse:
			case ExprStream::EsBoolTrue:  kind = BOOLEAN_LITERAL; break;
			case ExprStream::EsIntZero:
			case ExprStream::EsIntOne:    kind = INTEGER_LITERAL; break;
			case ExprStream::EsFloatZero: kind = REAL_LITERAL; break;
			case ExprStream::EsEmptyString: kind = STRING_LITERAL; break;
			default: goto bail; break;
			}
		}
	} else {

		// remaining literals:
		switch (ct) {

		case ExprStream::EsLitRelTime:
			if ( ! stm.readBytes(8)) { goto bail; }
			kind = RELTIME_LITERAL;
			break;

		case ExprStream::EsLitAbsTime:
			if ( ! stm.readBytes(8+4)) { goto bail; }
			kind = ABSTIME_LITERAL;
			break;

		case ExprStream::EsString:
		case ExprStream::EsBigString:
			if ( ! stm.skipSizeStream(ct == ExprStream::EsString)) { goto bail; }
			kind = STRING_LITERAL;
			break;
		case ExprStream::EsSimpleStr:
		case ExprStream::EsBigSimpleStr:
			if ( ! stm.skipSizeStream(ct == ExprStream::EsSimpleStr)) { goto bail; }
			kind = STRING_LITERAL;
			break;

		default:
			goto bail;
			break;
		}
	}

	return stm.size(mk);
bail:
	stm.unwind(mk);
	return 0;
}


unsigned int Literal::Pickle(ExprStreamMaker & stm) const
{
	ExprStreamMaker::Mark mk = stm.mark();

	NodeKind kind = this->GetKind();

	// handle the compact versions
	{
		// handle small and empty strings here, big strings later
		if (kind == NodeKind::STRING_LITERAL) {
			auto * sl = dynamic_cast<const StringLiteral *>(this);
			if ( ! sl) return 0;
			unsigned int len = (unsigned int)sl->_theString.size();
			if (len < 256) {
				if (len == 0) {
					stm.putByte(ExprStream::EsEmptyString);
				} else {
					unsigned char cch = (unsigned char)len;
					stm.putByte(ExprStream::EsString);
					stm.putByte(cch);
					stm.putBytes(sl->_theString.data(), len);
				}
				return stm.added(mk);
			}
		} else {

			int ival = 0;
			unsigned char ct = 0;

			switch (kind) {
			case NodeKind::ERROR_LITERAL:	  ct = ExprStream::EsLitError; break;
			case NodeKind::UNDEFINED_LITERAL: ct = ExprStream::EsLitUndef; break;
			case NodeKind::BOOLEAN_LITERAL: {
				auto * bl = dynamic_cast<const BooleanLiteral *>(this);
				if ( ! bl) return 0;
				ct = bl->_theBoolean ? ExprStream::EsBoolTrue : ExprStream::EsBoolFalse;
				} break;
			case NodeKind::INTEGER_LITERAL: {
				auto * il = dynamic_cast<const IntegerLiteral *>(this);
				if ( ! il) return 0;
				if (il->_theInteger == 0) {
					ct = ExprStream::EsIntZero;
				} else if (il->_theInteger == 1) {
					ct = ExprStream::EsIntOne;
				} else {
					ival = (int)il->_theInteger;
					if (ival == il->_theInteger) { ct = ExprStream::EsInt8; }
				}
				} break;
			case NodeKind::REAL_LITERAL: {
				auto * rl = dynamic_cast<const RealLiteral *>(this);
				if ( ! rl) return 0;
				if (rl->_theReal == 0.0) {
					ct = ExprStream::EsFloatZero;
				} else {
					ival = (int)rl->_theReal;
					if (ival == rl->_theReal) { ct = ExprStream::EsFloat8; }
				}
				} break;
			// this is unnecessary, but gcc gets pissy when you switch on an enum
			default: ct = 0; break;
			}

			// if a compact type, write it now, otherwise fall through to do non-compact
			if (ct) {
				if (ct < ExprStream::EsInt8) {
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

	// store non compact, or not compactible

	//unsigned char t = 0;
	switch (kind) {
	case NodeKind::INTEGER_LITERAL: {
		auto * lit = dynamic_cast<const IntegerLiteral *>(this);
		stm.putByte(ExprStream::EsInt64);
		stm.putInteger(lit->_theInteger);
		} break;

	case NodeKind::REAL_LITERAL: {
		auto * lit = dynamic_cast<const RealLiteral *>(this);
		stm.putByte(ExprStream::EsDouble);
		stm.putInteger(lit->_theReal);
		} break;

	case NodeKind::RELTIME_LITERAL: {
		auto * lit = dynamic_cast<const ReltimeLiteral *>(this);
		stm.putByte(ExprStream::EsLitRelTime);
		stm.putInteger(lit->_theReltime); // time value is union'ed with int value
		} break;

	case NodeKind::ABSTIME_LITERAL: {
		auto * lit = dynamic_cast<const AbstimeLiteral *>(this);
		stm.putByte(ExprStream::EsLitAbsTime);
		stm.putInteger((int64_t)(lit->_theAbstime.secs));
		stm.putInteger((int32_t)(lit->_theAbstime.offset));
		} break;

	case NodeKind::STRING_LITERAL: {
		auto * sl = dynamic_cast<const StringLiteral *>(this);
		stm.putByte(ExprStream::EsBigString);
		unsigned int cch = (unsigned int)sl->_theString.size();
		stm.putInteger(cch);
		if (cch) { stm.putBytes(sl->_theString.data(), cch); }
		} break;

	// default case to shut gcc up
	default: break;
	}

	return stm.added(mk);
}

#endif // TJ_PICKLE

/* Function which iterates through the string Str from the location 'index', 
 *returning the index of the next digit-char 
 */
static inline void nextDigitChar(const string &Str, int &index) 
{
	int len = (int)Str.length();
    while((index<len) &&(!isdigit(Str[index]))) {
		index++;
    }
}


/* Function which iterates through the string Str backwards from the location 'index'
 *returning the index of the first occuring non-space character
 */
static inline void prevNonSpaceChar(const string &Str, int &index) 
{
    while((index>=0) &&(isspace(Str[index]))) {
		index--;
    }
}


/* Function which takes a number in string format, and reverses the
 * order of the digits & returns the corresponding number as an
 * integer.
 */
static int revInt(std::string revNumStr) // by value, as we mutate the parameter
{
    int number = 0;

	std::reverse(revNumStr.begin(), revNumStr.end());
	std::ignore = std::from_chars(revNumStr.data(), revNumStr.data() + revNumStr.size(), number, 10);

	return number;
}

/* Function which takes a number in string format, and reverses the
 * order of the digits & returns the corresponding number as a double.
 */
static double revDouble(const string &revNumStr) 
{
	string numStr = "";
    double number;
    const char *cNumStr;

	int len = (int)revNumStr.length();
	for(int i=len-1; i>=0 ; i--) {
		numStr += revNumStr[i];
	}

    cNumStr = numStr.c_str();
    
    number = strtod(cNumStr, NULL);
	return number;
}

/* function which returns the timezone offset corresponding to the argument epochsecs,
 *  which is the number of seconds since the epoch 
 */
int Literal::
findOffset(time_t epochsecs) 
{
	return timezone_offset( epochsecs, false );
} 


bool 
operator==(Literal &literal1, Literal &literal2)
{
    return literal1.SameAs(&literal2);
}

static bool extractTimeZone(string &timeStr, int &tzhr, int &tzmin) 
{
	int    len    = (int)timeStr.length();
	int    i      = len-1; 
    bool   offset = false;
    string offStr = timeStr.substr(i-4,5);

    if (((offStr[0] == '+') || (offStr[0] == '-')) && 
        (isdigit(offStr[1])) && (isdigit(offStr[2])) && (isdigit(offStr[3])) && (isdigit(offStr[4]))) {
        offset = true;
        timeStr.erase(i-4,5);
        if (offStr[0] == '+') {
            tzhr = atoi(offStr.substr(1,2).c_str());
            tzmin = atoi(offStr.substr(3,2).c_str());
        }
        else {
            tzhr = -atoi(offStr.substr(1,2).c_str());
            tzmin = -atoi(offStr.substr(3,2).c_str());
        }
    }
    return offset;
}

} // classad
