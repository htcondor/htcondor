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

#include "stringSpace.h"
#include "list.h"
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
	/** A classad value */				CLASSAD_VALUE,
	/** An absolute time value */		ABSOLUTE_TIME_VALUE,
	/** A relative time value */		RELATIVE_TIME_VALUE
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
			@param p Pretty printing options
			@return true if the operation succeeded, false otherwise.
			@see Sink
		*/
		bool ToSink( Sink &s );

		/** Discards the previous value and sets the value to UNDEFINED */
		void Clear (void);

		/** Copies the value of another value object.
			@param v The value copied from.
		*/
		void CopyFrom( Value &v );

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
		void SetUndefinedValue	(void);

		/** Sets the error value; previous value discarded.
		*/
		void SetErrorValue (void);

		/** Sets an expression list value; previous value discarded.
			@param l The list value.
		*/
		void SetListValue(ExprList* l);

		/** Sets a ClassAd value; previous value discarded.
			@param c The ClassAd value.
		*/
		void SetClassAdValue(ClassAd* c);	

		/** Sets a string value; previous value discarded.  This function
				adopts the string passed in.  The storage is assumed to have
				been created with new char[].
			@param str The string value.
		*/
		void SetStringValue( const char *str );

		/** Sets an absolute time value in seconds since the UNIX epoch.
			@param secs Number of seconds since the UNIX epoch.
		*/
		void SetAbsoluteTimeValue( int secs );

		/** Sets a relative time value.
			@param secs Number of seconds.
			@param usecs Number of micro seconds
		*/
		void SetRelativeTimeValue( int secs, int usecs=0 );

		/** Gets the type of the value.
			@return The value type.
			@see ValueType
		*/
		inline ValueType GetType() { return valueType; }

		/** Checks if the value is boolean.
			@param b The boolean value if the value is boolean.
			@return true iff the value is boolean.
		*/
		inline bool IsBooleanValue(bool& b);	
		/** Checks if the value is boolean.
			@return true iff the value is boolean.
		*/	
		inline bool IsBooleanValue();
		/** Checks if the value is integral.
			@param i The integer value if the value is integer.
			@return true iff the value is an integer.
		*/
		inline bool IsIntegerValue(int &i); 	
		/** Checks if the value is integral.
			@return true iff the value is an integer.
		*/
		inline bool IsIntegerValue();
		/** Checks if the value is real.
			@param r The real value if the value is real.
			@return true iff the value is real.
		*/	
		inline bool IsRealValue(double &r); 	
		/** Checks if the value is real.
			@return true iff the value is real.
		*/
		inline bool IsRealValue();
		/** Checks if the value is a string.  The user should \emph{not}
				deallocate the storage pointed to by str.
			@param str A reference to a string which points to the character
				string.
			@return true iff the value is a string.
		*/
		bool IsStringValue( const char *&str ); 	
		/** Checks if the value is a string.
			@return true iff the value is string.
		*/
		inline bool IsStringValue();
		/** Checks if the value is a string.  If the string does not fit in the 
				buffer, only the portion of it which does is copied.
			@param str Pointer to a buffer which is filled with the string
				value if the value is a string.	
			@param len Length of the buffer.
			@param alen The actual length of the string.  Undefined if the 
				value is not a string.
			@return true iff the value is a string
		*/
		bool IsStringValue( char* buf, int len, int &alen );
		/** Checks if the value is an expression list.
			@param l The expression list if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool IsListValue(ExprList*& l);
		/** Checks if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool IsListValue();
		/** Checks if the value is a ClassAd.
			@param c The ClassAd if the value is a ClassAd.
			@return true iff the value is a ClassAd.
		*/
		inline bool IsClassAdValue(ClassAd *&c); 
		/** Checks if the value is a ClassAd.
			@return true iff the value is a ClassAd value.
		*/
		inline bool IsClassAdValue();
		/** Checks if the value is the undefined value.
			@return true iff the value if the undefined value.
		*/
		inline bool IsUndefinedValue();
		/** Checks if the value is the error value.
			@return true iff the value if the error value.
		*/
		inline bool IsErrorValue();
		/** Checks if the value is exceptional.
			@return true iff the value is either undefined or error.
		*/
		inline bool IsExceptional();
		/** Checks if the value is numerical. If the value is a real, it is 
				converted to an integer through truncation.
			@param i The integer value of the value if the value is a number.
			@return true iff the value is a number
		*/
		bool IsNumber (int &i);
		/** Checks if the value is numerical. If the value is an integer, it 
				is promoted to a real.
			@param r The real value of the value if the value is a number.
			@return true iff the value is a number
		*/
		bool IsNumber (double &r);
		/** Checks if the value is an absolute time value.
			@return true iff the value is an absolute time value.
		*/
		bool IsAbsoluteTimeValue( );
		/** Checks if the value is an absolute time value.
			@param secs Number of seconds since the UNIX epoch.
			@return true iff the value is an absolute time value.
		*/
		bool IsAbsoluteTimeValue( int& secs );
		/** Checks if the value is a relative time value.
			@return true iff the value is a relative time value
		*/
		bool IsRelativeTimeValue( );
		/** Checks if the value is a relative time value.
			@param secs Number of seconds
			@return true iff the value is a relative time value
		*/
		bool IsRelativeTimeValue( int& secs );
		/** Checks if the value is a relative time value.
			@param secs Number of seconds.
			@param usecs Number of microseconds
			@return true iff the value is a relative time value
		*/
		bool IsRelativeTimeValue( int& secs, int& usecs );

	private:
		friend class Literal;
		friend class ClassAd;
		friend class ExprTree;

		void WriteRelativeTime( char*, bool, int, int );
		bool WriteString( const char*, Sink& );

		ValueType 	valueType;		// the type of the value
		bool		booleanValue;
		int			integerValue;
		double 		realValue;
		SSString	strValue;
		ExprList	*listValue;
		ClassAd		*classadValue;
		int			timeValueSecs;
		int			timeValueUSecs;

		static		StringSpace stringSpace;
};


// implementations of the inlined functions
inline bool Value::
IsBooleanValue( bool& b )
{
	b = booleanValue;
	return( valueType == BOOLEAN_VALUE );
}

inline bool Value::
IsBooleanValue()
{
	return( valueType == BOOLEAN_VALUE );
}

inline bool Value::
IsIntegerValue (int &i)
{
    i = integerValue;
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
IsIntegerValue ()
{
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
IsRealValue (double &r)
{
    r = realValue;
    return (valueType == REAL_VALUE);
}  

inline bool Value::
IsRealValue ()
{
    return (valueType == REAL_VALUE);
}  

inline bool Value::
IsListValue (ExprList *&l)
{
	l = listValue;
	return (valueType == LIST_VALUE);
}

inline bool Value::
IsListValue ()
{
	return (valueType == LIST_VALUE);
}


inline bool Value::
IsStringValue()
{
    return (valueType == STRING_VALUE);
}


inline bool Value::
IsStringValue( const char *&s )
{
    if( valueType == STRING_VALUE ) {
        return( ( s = strValue.getCharString() ) != NULL );
    }
    return( false );
}

inline bool Value::
IsClassAdValue(ClassAd *&ad)
{
	ad = classadValue;
	return( valueType == CLASSAD_VALUE );	
}

inline bool Value:: 
IsClassAdValue() 
{
	return( valueType == CLASSAD_VALUE );	
}

inline bool Value::
IsUndefinedValue (void) 
{ 
	return (valueType == UNDEFINED_VALUE);
}

inline bool Value::
IsErrorValue(void)       
{ 
	return (valueType == ERROR_VALUE); 
}

inline bool Value::
IsExceptional(void)
{
	return( valueType == UNDEFINED_VALUE || valueType == ERROR_VALUE );
}

inline bool Value::
IsAbsoluteTimeValue( )
{
	return( valueType == ABSOLUTE_TIME_VALUE );
}

inline bool Value::
IsAbsoluteTimeValue( int &secs )
{
	secs = timeValueSecs;
	return( valueType == ABSOLUTE_TIME_VALUE );
}

inline bool Value::
IsRelativeTimeValue( )
{
	return( valueType == RELATIVE_TIME_VALUE );
}

inline bool Value::
IsRelativeTimeValue( int &secs )
{
	secs = timeValueSecs;
	return( valueType == RELATIVE_TIME_VALUE );
}

inline bool Value::
IsRelativeTimeValue( int &secs, int &usecs )
{
	secs = timeValueSecs;
	usecs = timeValueUSecs;
	return( valueType == RELATIVE_TIME_VALUE );
}

#endif//__VALUES_H__
