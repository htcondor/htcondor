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
	LIST_VALUE,
	CLASSAD_VALUE
};

class Value;
class Literal;

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
		

// The EvalValue object is used for holding values that are being generated
// during the evaluation process.  Pointers to classads are passed around
// instead of deep copies of classads to make evaluation more efficient,
// and provide the semantics of L-mode evaluation.  The copyFrom() operation
// implements "handoff" semantics, so after A.copyFrom(B), B will have an 
// undefined value.
class EvalValue 
{
	public:
		// ctor/dtor
		EvalValue();
		virtual ~EvalValue();

		// output
		bool toSink( Sink & );

		// modifiers  
		void clear (void);
		void copyFrom(EvalValue &);				// "handoff" semantics
		void setRealValue 		(double);
		void setIntegerValue 	(int );
		void setUndefinedValue	(void);
		void setErrorValue 		(void);
		void setListValue    	(ValueList*);	// "adoption" semantics
		void setClassAdValue 	(ClassAd*);		// "adoption" semantics
		void adoptStringValue	(char *,int);	// "adoption" semantics
		void setStringValue		(char *);		// "duplication" semantics
		void setStringValue		(SSString &);	// "duplication" semantics

		// accessors
		inline ValueType getType () { return valueType; }
		inline bool isIntegerValue(int &); 		inline bool isIntegerValue();
		inline bool isRealValue	  (double &); 	inline bool isRealValue();
		inline bool isStringValue (char *&); 	inline bool isStringValue();
		inline bool isListValue   (ValueList*&);inline bool isListValue();
		inline bool isClassAdValue(ClassAd *&); inline bool isClassAdValue();
		inline bool isUndefinedValue(void);
		inline bool isErrorValue	(void);
		bool isNumber (int    &);
		bool isNumber (double &);

	protected:
		ValueType 	valueType;		// the type of the value
		int			integerValue;
		double 		realValue;
		SSString	strValue;
		ValueList	*listValue;
		ClassAd		*classadValue;

		static 	StringSpace stringValues;
		friend	class Value;
};


// The Value object extends the EvalValue object which makes deep copies of
// classads; the user does not have to worry about life-times of classad Values 
// in relation to the evaluation environment
class Value : public EvalValue
{
	public:
		Value();
		Value(const Value &);		// deep copy
		~Value();

		void clear();
		void copyFrom(EvalValue &);	// also copies from derived Value class
};


// implementations of the inlined functions
inline bool EvalValue::
isIntegerValue (int &i)
{
    i = integerValue;
    return (valueType == INTEGER_VALUE);
}  

inline bool EvalValue::
isIntegerValue ()
{
    return (valueType == INTEGER_VALUE);
}  

inline bool EvalValue::
isRealValue (double &r)
{
    r = realValue;
    return (valueType == REAL_VALUE);
}  

inline bool EvalValue::
isRealValue ()
{
    return (valueType == REAL_VALUE);
}  

inline bool EvalValue::
isListValue (ValueList *&l)
{
	l = listValue;
	return (valueType == LIST_VALUE);
}

inline bool EvalValue::
isListValue ()
{
	return (valueType == LIST_VALUE);
}

inline bool EvalValue::
isStringValue(char *&s)
{
    s = strValue.getCharString ();
    return (valueType == STRING_VALUE);
}

inline bool EvalValue::
isStringValue()
{
    return (valueType == STRING_VALUE);
}

inline bool EvalValue::
isClassAdValue(ClassAd *&ad)
{
	ad = classadValue;
	return( valueType == CLASSAD_VALUE );	
}

inline bool EvalValue:: 
isClassAdValue() 
{
	return( valueType == CLASSAD_VALUE );	
}

inline bool EvalValue::
isUndefinedValue (void) 
{ 
	return (valueType == UNDEFINED_VALUE);
}

inline bool EvalValue::
isErrorValue(void)       
{ 
	return (valueType == ERROR_VALUE); 
}

#endif//__VALUES_H__
