#ifndef __VALUES_H__
#define __VALUES_H__

#include "list.h"
#include "stringSpace.h"
#include "sink.h"

// the various types of values
enum ValueType 
{
	UNDEFINED_VALUE,
	ERROR_VALUE,
	BOOLEAN_VALUE,
	INTEGER_VALUE,
	REAL_VALUE,
	STRING_VALUE,
	LIST_VALUE,
	CLASSAD_VALUE
};

enum ValueCopyMode
{
	VALUE_COPY,
	VALUE_HANDOVER
};

class Value;
class Literal;

class Value 
{
	public:
		// ctor/dtor
		Value();
		virtual ~Value();

		// output
		bool toSink( Sink & );

		// modifiers  
		void clear (void);
		void copyFrom(Value &, ValueCopyMode = VALUE_COPY);
		void setBooleanValue	(bool);
		void setRealValue 		(double);
		void setIntegerValue 	(int);
		void setUndefinedValue	(void);
		void setErrorValue 		(void);
		void setListValue    	(ExprList*);
		void setClassAdValue 	(ClassAd*);	
		void adoptStringValue	(char *,int);	// "adoption" semantics
		void setStringValue		(char *);		// "duplication" semantics
		void setStringValue		(SSString &);	// "duplication" semantics

		// accessors
		inline ValueType getType() { return valueType; }
		inline bool isBooleanValue(bool&);	
		inline bool isBooleanValue();
		inline bool isIntegerValue(int &); 	
		inline bool isIntegerValue();
		inline bool isRealValue(double &); 	
		inline bool isRealValue();
		inline bool isStringValue(char *&); 	
		inline bool isStringValue();
		inline bool isStringValue( char*, int );
		inline bool isListValue(ExprList*&);
		inline bool isListValue();
		inline bool isClassAdValue(ClassAd *&); 
		inline bool isClassAdValue();
		inline bool isUndefinedValue();
		inline bool isErrorValue();
		inline bool isExceptional();
		bool isNumber (int    &);
		bool isNumber (double &);

	protected:
		ValueType 	valueType;		// the type of the value
		bool		booleanValue;
		int			integerValue;
		double 		realValue;
		SSString	strValue;
		ExprList	*listValue;
		ClassAd		*classadValue;

		static 	StringSpace stringValues;
};


// implementations of the inlined functions
inline bool Value::
isBooleanValue( bool& b )
{
	b = booleanValue;
	return( valueType == BOOLEAN_VALUE );
}

inline bool Value::
isBooleanValue()
{
	return( valueType == BOOLEAN_VALUE );
}

inline bool Value::
isIntegerValue (int &i)
{
    i = integerValue;
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
isIntegerValue ()
{
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
isRealValue (double &r)
{
    r = realValue;
    return (valueType == REAL_VALUE);
}  

inline bool Value::
isRealValue ()
{
    return (valueType == REAL_VALUE);
}  

inline bool Value::
isListValue (ExprList *&l)
{
	l = listValue;
	return (valueType == LIST_VALUE);
}

inline bool Value::
isListValue ()
{
	return (valueType == LIST_VALUE);
}

inline bool Value::
isStringValue(char *&s)
{
    s = strValue.getCharString ();
    return (valueType == STRING_VALUE);
}

inline bool Value::
isStringValue()
{
    return (valueType == STRING_VALUE);
}

inline bool Value::
isStringValue( char *b, int l )
{
	if( valueType == STRING_VALUE ) {
		strncpy( b, strValue.getCharString(), l );
		return true;
	}
	return false;
}

inline bool Value::
isClassAdValue(ClassAd *&ad)
{
	ad = classadValue;
	return( valueType == CLASSAD_VALUE );	
}

inline bool Value:: 
isClassAdValue() 
{
	return( valueType == CLASSAD_VALUE );	
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

inline bool Value::
isExceptional(void)
{
	return( valueType == UNDEFINED_VALUE || valueType == ERROR_VALUE );
}

#endif//__VALUES_H__
