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
