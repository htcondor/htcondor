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
		Value()	{ valueType = UNDEFINED_VALUE; }
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
