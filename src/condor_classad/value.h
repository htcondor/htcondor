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

#ifndef __VALUES_H__
#define __VALUES_H__

#include "condor_common.h"

// the various types of values
enum ValueType 
{
	UNDEFINED_VALUE,
	ERROR_VALUE,
	INTEGER_VALUE,
	REAL_VALUE,
	STRING_VALUE,
};

class Value;

// the Value object
class Value 
{
	public:
		// ctor/dtor
	    Value();
		~Value();

		// modifiers
		void clear(void);
		void setRealValue 	(double r);
		void setIntegerValue (int i);
		void setUndefinedValue(void);
		void setErrorValue 	(void);
		void setStringValue	(char *s);

		// accessors
		inline bool isIntegerValue 	(int &i);
		inline bool isRealValue		(double &r);
		inline bool isStringValue	(char *&s);
		inline bool isUndefinedValue(void) ;
		inline bool isErrorValue	(void);
		inline ValueType getType () { return valueType; }

		bool isNumber (int    &);
		bool isNumber  (double &);

		// operators
		const Value &operator= (const Value &);

	private:
		ValueType 	valueType;		// the type of the value
		int			integerValue;	// the value itself
		double 		realValue;
		char		*strValue;
};



// implementations of the inlined functions
inline bool Value::
isIntegerValue (int &i)
{
    i = integerValue;
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
isRealValue (double &r)
{
    r = realValue;
    return (valueType == REAL_VALUE);
}  


inline bool Value::
isStringValue(char *&s)
{
    s = strValue;
    return (valueType == STRING_VALUE);
}

inline bool Value::
isUndefinedValue (void) 
{ 
	return (valueType == UNDEFINED_VALUE);
}

inline bool Value::
isErrorValue(void)       
{ 
	return (valueType == ERROR_VALUE); 
}

#endif//__VALUES_H__
