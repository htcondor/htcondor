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

#include "common.h"
#include "list.h"
#include "sink.h"

BEGIN_NAMESPACE( classad )

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

		/** Discards the previous value and sets the value to UNDEFINED */
		void Clear (void);

		/** Copies the value of another value object.
			@param v The value copied from.
		*/
		void CopyFrom( const Value &v );

		/** Sets a boolean value; previous value discarded.
			@param b The boolean value.
		*/
		void SetBooleanValue(bool b);

		/** Sets a real value; previous value discarded.
			@param r The real value.
		*/
		void SetRealValue(double r);

		/** Sets an integer value; previous value discarded.
			@param i The integer value.
		*/
		void SetIntegerValue(int i);

		/** Sets the undefined value; previous value discarded.
		*/
		void SetUndefinedValue(void);

		/** Sets the error value; previous value discarded.
		*/
		void SetErrorValue(void);

		/** Sets an expression list value; previous value discarded.
			@param l The list value.
		*/
		void SetListValue(const ExprList* l);

		/** Sets a ClassAd value; previous value discarded.
			@param c The ClassAd value.
		*/
		void SetClassAdValue(const ClassAd* c);	

		/** Sets a string value; previous value discarded.
			@param str The string value.
		*/
		void SetStringValue( const string &str );

		/** Sets a string value; previous value discarded.
			@param str The string value.
		*/
		void SetStringValue( const char *str );

		/** Sets an absolute time value in seconds since the UNIX epoch.
			@param secs Number of seconds since the UNIX epoch.
		*/
		void SetAbsoluteTimeValue( time_t secs );

		/** Sets a relative time value.
			@param secs Number of seconds.
		*/
		void SetRelativeTimeValue( time_t secs );

		/** Gets the type of the value.
			@return The value type.
			@see ValueType
		*/
		inline ValueType GetType() const { return valueType; }

		/** Checks if the value is boolean.
			@param b The boolean value if the value is boolean.
			@return true iff the value is boolean.
		*/
		inline bool IsBooleanValue(bool& b) const;
		/** Checks if the value is boolean.
			@return true iff the value is boolean.
		*/	
		inline bool IsBooleanValue() const;
		/** Checks if the value is integral.
			@param i The integer value if the value is integer.
			@return true iff the value is an integer.
		*/
		inline bool IsIntegerValue(int &i) const; 	
		/** Checks if the value is integral.
			@return true iff the value is an integer.
		*/
		inline bool IsIntegerValue() const;
		/** Checks if the value is real.
			@param r The real value if the value is real.
			@return true iff the value is real.
		*/	
		inline bool IsRealValue(double &r) const; 	
		/** Checks if the value is real.
			@return true iff the value is real.
		*/
		inline bool IsRealValue() const;
		/** Checks if the value is a string.  
			@param str A reference to a string object, which is filled with the
				string value.
			@return true iff the value is a string.
		*/
		bool IsStringValue( string &str ) const; 	
		/** Checks if the value is a string.  
			@param str A reference to a C string, which will point to the 
				string value.  This pointer must <b>not</b> be deallocated or
				tampered with.
			@return true iff the value is a string.
		*/
		bool IsStringValue( const char *&str ) const; 	
		/** Checks if the value is a string.
			@return true iff the value is string.
		*/
		inline bool IsStringValue() const;
		/** Checks if the value is an expression list.
			@param l The expression list if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool IsListValue(const ExprList*& l) const;
		/** Checks if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool IsListValue() const;
		/** Checks if the value is a ClassAd.
			@param c The ClassAd if the value is a ClassAd.
			@return true iff the value is a ClassAd.
		*/
		inline bool IsClassAdValue(const ClassAd *&c) const; 
		/** Checks if the value is a ClassAd.
			@return true iff the value is a ClassAd value.
		*/
		inline bool IsClassAdValue() const;
		/** Checks if the value is the undefined value.
			@return true iff the value if the undefined value.
		*/
		inline bool IsUndefinedValue() const;
		/** Checks if the value is the error value.
			@return true iff the value if the error value.
		*/
		inline bool IsErrorValue() const;
		/** Checks if the value is exceptional.
			@return true iff the value is either undefined or error.
		*/
		inline bool IsExceptional() const;
		/** Checks if the value is numerical. 
			@return true iff the value is a number
		*/
		bool IsNumber () const;
		/** Checks if the value is numerical. If the value is a real, it is 
				converted to an integer through truncation.
			@param i The integer value of the value if the value is a number.
			@return true iff the value is a number
		*/
		bool IsNumber (int &i) const;
		/** Checks if the value is numerical. If the value is an integer, it 
				is promoted to a real.
			@param r The real value of the value if the value is a number.
			@return true iff the value is a number
		*/
		bool IsNumber (double &r) const;
		/** Checks if the value is an absolute time value.
			@return true iff the value is an absolute time value.
		*/
		bool IsAbsoluteTimeValue( ) const;
		/** Checks if the value is an absolute time value.
			@param secs Number of seconds since the UNIX epoch.
			@return true iff the value is an absolute time value.
		*/
		bool IsAbsoluteTimeValue( time_t& secs ) const;
		/** Checks if the value is a relative time value.
			@return true iff the value is a relative time value
		*/
		bool IsRelativeTimeValue( ) const;
		/** Checks if the value is a relative time value.
			@param secs Number of seconds
			@return true iff the value is a relative time value
		*/
		bool IsRelativeTimeValue( time_t& secs ) const;

	private:
		friend class Literal;
		friend class ClassAd;
		friend class ExprTree;

		ValueType 		valueType;		// the type of the value
		union {
			bool			booleanValue;
			int				integerValue;
			double 			realValue;
			const ExprList	*listValue;
			const ClassAd	*classadValue;
			time_t			timeValueSecs;
		};
		string			strValue;		// has ctor/dtor cannot be in the union
};


// implementations of the inlined functions
inline bool Value::
IsBooleanValue( bool& b ) const
{
	b = booleanValue;
	return( valueType == BOOLEAN_VALUE );
}

inline bool Value::
IsBooleanValue() const
{
	return( valueType == BOOLEAN_VALUE );
}

inline bool Value::
IsIntegerValue (int &i) const
{
    i = integerValue;
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
IsIntegerValue () const
{
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
IsRealValue (double &r) const
{
    r = realValue;
    return (valueType == REAL_VALUE);
}  

inline bool Value::
IsRealValue () const
{
    return (valueType == REAL_VALUE);
}  

inline bool Value::
IsListValue( const ExprList *&l) const
{
	l = listValue;
	return(valueType == LIST_VALUE);
}

inline bool Value::
IsListValue () const
{
	return (valueType == LIST_VALUE);
}


inline bool Value::
IsStringValue() const
{
    return (valueType == STRING_VALUE);
}


inline bool Value::
IsStringValue( const char *&s ) const
{
	s = strValue.c_str( );
    return( valueType == STRING_VALUE );
}

inline bool Value::
IsStringValue( string &s ) const
{
	s = strValue;
    return( valueType == STRING_VALUE );
}

inline bool Value::
IsClassAdValue(const ClassAd *&ad) const
{
	ad = classadValue;
	return( valueType == CLASSAD_VALUE );	
}

inline bool Value:: 
IsClassAdValue()  const
{
	return( valueType == CLASSAD_VALUE );	
}

inline bool Value::
IsUndefinedValue (void) const
{ 
	return (valueType == UNDEFINED_VALUE);
}

inline bool Value::
IsErrorValue(void) const
{ 
	return (valueType == ERROR_VALUE); 
}

inline bool Value::
IsExceptional(void) const
{
	return( valueType == UNDEFINED_VALUE || valueType == ERROR_VALUE );
}

inline bool Value::
IsAbsoluteTimeValue( ) const
{
	return( valueType == ABSOLUTE_TIME_VALUE );
}

inline bool Value::
IsAbsoluteTimeValue( time_t &secs ) const
{
	secs = timeValueSecs;
	return( valueType == ABSOLUTE_TIME_VALUE );
}

inline bool Value::
IsRelativeTimeValue( ) const
{
	return( valueType == RELATIVE_TIME_VALUE );
}

inline bool Value::
IsRelativeTimeValue( time_t &secs ) const
{
	secs = timeValueSecs;
	return( valueType == RELATIVE_TIME_VALUE );
}
inline bool Value::
IsNumber( ) const
{
	return( valueType==INTEGER_VALUE || valueType==REAL_VALUE );
}
END_NAMESPACE // classad

#endif//__VALUES_H__
