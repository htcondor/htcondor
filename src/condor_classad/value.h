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
