#ifndef __VALUES_H__
#define __VALUES_H__

#include "condor_common.h"
#include "list.h"
#include "stringSpace.h"
#include "sink.h"

// the various types of values
enum ValueType 
{
	UNDEFINED_VALUE,
	ERROR_VALUE,
	INTEGER_VALUE,
	REAL_VALUE,
	STRING_VALUE,
	LIST_VALUE
};

class Value;

// A list of values --- uses "list.h" from the c++ lib, except that the
// dtor also destroys objects in the list 
class ValueList : public List<Value>
{
	public:
		// ctor/dtor
		ValueList();
		~ValueList();

		void clearList();
		void copy(ValueList *);
		bool isMember 		(Value &);	// uses == to compare values
		bool isExactMember 	(Value &);	// uses =?= to compare values
};
		

enum
{
	VALUE_LIST_DUP,
	VALUE_LIST_ADOPT
};

// the Value object
class Value 
{
	public:
		// ctor/dtor
		Value()	{ valueType = UNDEFINED_VALUE; listValue = NULL; }
		~Value();

		// modifiers
		void clear(void);
		void setRealValue 	(double r);
		void setIntegerValue (int i);
		void setUndefinedValue(void);
		void setErrorValue 	(void);
		void setStringValue	(char *s);
		void adoptStringValue(char *s, int=SS_ADOPT_C_STRING);
		void setStringValue  (const SSString &);
		void setListValue    (ValueList*, int=VALUE_LIST_ADOPT);

		// accessors
		inline bool isIntegerValue 	(int &i);
		inline bool isRealValue		(double &r);
		inline bool isStringValue	(char *&s);
		inline bool isListValue     (ValueList *&);
		inline bool isUndefinedValue(void) ;
		inline bool isErrorValue	(void);
		inline ValueType getType () { return valueType; }

		bool isNumber (int    &);
		bool isNumber  (double &);

		// other functions
		void copy (Value &);
		bool toSink (Sink &);

	private:
		ValueType 	valueType;		// the type of the value
		int			integerValue;	// the value itself
		double 		realValue;
		SSString 	strValue;
		ValueList	*listValue;

		// this is where the strings live
		static 	StringSpace stringValues;
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
isListValue (ValueList *&l)
{
	l = listValue;
	return (valueType == LIST_VALUE);
}

inline bool Value::
isStringValue(char *&s)
{
    s = strValue.getCharString ();
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
