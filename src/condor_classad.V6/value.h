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

#include "list.h"
#include "stringSpace.h"
#include "sink.h"

/// The various kinds of values
enum ValueType 
{
	/** The undefined value */			UNDEFINED_VALUE,
	/** The error value */				ERROR_VALUE,
	/** A boolean value (false, true)*/	BOOLEAN_VALUE,
	/** An integer value */				INTEGER_VALUE,
	/** A real value */					REAL_VALUE,
	/** A string value */				STRING_VALUE,
	/** An expression list value */		LIST_VALUE,
	/** A classad value */				CLASSAD_VALUE
};

class Value;
class Literal;

/// Represents the result of an evaluation.
class Value 
{
	public:
		/// Constructor
		Value();

		/// Destructor
		~Value();

		/** Sends the value to a sink.
			@param s The sink object.
			@return true if the operation succeeded, false otherwise.
			@see Sink
		*/
		bool toSink( Sink &s );

		/** Discards the previous value and sets the value to UNDEFINED */
		void clear (void);

		/** Copies the value of another value object.
			@param v The value copied from.
		*/
		void copyFrom(Value &v);

		/** Sets a boolean value; previous value discarded.
			@param b The boolean value.
		*/
		void setBooleanValue(bool b);

		/** Sets a real value; previous value discarded.
			@param r The real value.
		*/
		void setRealValue(double r);

		/** Sets an integer value; previous value discarded.
			@param i The integer value.
		*/
		void setIntegerValue(int i);

		/** Sets the undefined value; previous value discarded.
		*/
		void setUndefinedValue	(void);

		/** Sets the error value; previous value discarded.
		*/
		void setErrorValue (void);

		/** Sets an expression list value; previous value discarded.
			@param l The list value.
		*/
		void setListValue(ExprList* l);

		/** Sets a ClassAd value; previous value discarded.
			@param c The ClassAd value.
		*/
		void setClassAdValue(ClassAd* c);	

		/** Sets a string value; previous value discarded.  This function
				just stores the pointer to the buffer, and does not perform 
				any other memory management for the string.
			@param str The string value.
		*/
		void setStringValue(char *str);

		// Undocumented, experimental and perhaps broken; --Rajesh
		void adoptStringValue(char *str, int i);

		/** Gets the type of the value.
			@return The value type.
			@see ValueType
		*/
		inline ValueType getType() { return valueType; }

		/** Checks if the value is boolean.
			@param b The boolean value if the value is boolean.
			@return true iff the value is boolean.
		*/
		inline bool isBooleanValue(bool& b);	
		/** Checks if the value is boolean.
			@return true iff the value is boolean.
		*/	
		inline bool isBooleanValue();
		/** Checks if the value is integral.
			@param i The integer value if the value is integer.
			@return true iff the value is an integer.
		*/
		inline bool isIntegerValue(int &i); 	
		/** Checks if the value is integral.
			@return true iff the value is an integer.
		*/
		inline bool isIntegerValue();
		/** Checks if the value is real.
			@param r The real value if the value is real.
			@return true iff the value is real.
		*/	
		inline bool isRealValue(double &r); 	
		/** Checks if the value is real.
			@return true iff the value is real.
		*/
		inline bool isRealValue();
		/** Checks if the value is a string.  The storage pointed to by str 
				should NOT be deallocated by the user.
			@param str A reference to a string which points to the character
				string.
			@return true iff the value is a string.
		*/
		inline bool isStringValue(char *&str); 	
		/** Checks if the value is a string.
			@return true iff the value is string.
		*/
		inline bool isStringValue();
		/** Checks if the value is a string.  If the string does not fit in the 
				buffer, only the portion of it which does is copied.
			@param str Pointer to a buffer which is filled with the string
				value if the value is a string.	
			@param len Length of the buffer.
			@return true iff the value is a string which fits in the given 
				buffer.
		*/
		inline bool isStringValue( char* buf, int len );
		/** Checks if the value is an expression list.
			@param l The expression list if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool isListValue(ExprList*& l);
		/** Checks if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool isListValue();
		/** Checks if the value is a ClassAd.
			@param c The ClassAd if the value is a ClassAd.
			@return true iff the value is a ClassAd.
		*/
		inline bool isClassAdValue(ClassAd *&c); 
		/** Checks if the value is a ClassAd.
			@return true iff the value is a ClassAd value.
		*/
		inline bool isClassAdValue();
		/** Checks if the value is the undefined value.
			@return true iff the value if the undefined value.
		*/
		inline bool isUndefinedValue();
		/** Checks if the value is the error value.
			@return true iff the value if the error value.
		*/
		inline bool isErrorValue();
		/** Checks if the value is exceptional.
			@return true iff the value is either undefined or error.
		*/
		inline bool isExceptional();
		/** Checks if the value is numerical. If the value is a real, it is 
				converted to an integer through truncation.
			@param i The integer value of the value if the value is a number.
			@return true iff the value is a number
		*/
		bool isNumber (int &i);
		/** Checks if the value is numerical. If the value is an integer, it 
				is promoted to a real.
			@param r The real value of the value if the value is a number.
			@return true iff the value is a number
		*/
		bool isNumber (double &r);

	private:
		friend	class Literal;

		ValueType 	valueType;		// the type of the value
		bool		booleanValue;
		int			integerValue;
		double 		realValue;
		char		*strValue;
		ExprList	*listValue;
		ClassAd		*classadValue;
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
    s = strValue;
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
		strncpy( b, strValue, l );
		return( strcmp( strValue, b ) == 0 );
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
