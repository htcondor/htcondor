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


#ifndef __CLASSAD_VALUE_H__
#define __CLASSAD_VALUE_H__

#include "classad/common.h"
#include "classad/util.h"
#include "classad/classad_stl.h"


namespace classad {

class Literal;
class ExprList;
class ClassAd;

/// Represents the result of an evaluation.
class Value 
{
	public:
			/// Value types
		enum ValueType {
												NULL_VALUE          = 0,
		/** The error value */ 					ERROR_VALUE         = 1<<0,
		/** The undefined value */ 				UNDEFINED_VALUE     = 1<<1,
		/** A boolean value (false, true)*/ 	BOOLEAN_VALUE 		= 1<<2,
		/** An integer value */ 				INTEGER_VALUE       = 1<<3,
		/** A real value */ 					REAL_VALUE          = 1<<4,
		/** A relative time value */ 			RELATIVE_TIME_VALUE = 1<<5,
		/** An absolute time value */ 			ABSOLUTE_TIME_VALUE = 1<<6,
		/** A string value */ 					STRING_VALUE        = 1<<7,
		/** A classad value */ 					CLASSAD_VALUE       = 1<<8,
		/** A list value (not owned here)*/	            LIST_VALUE 			= 1<<9,
		/** A list value (owned via shared_ptr)*/     	SLIST_VALUE 		= 1<<10
		};

			/// Number factors
		enum NumberFactor {
	    /** No factor specified */  NO_FACTOR= 0,
		/** Byte factor */          B_FACTOR = 1,
		/** Kilo factor */          K_FACTOR = 2,
		/** Mega factor */          M_FACTOR = 3,
		/** Giga factor */          G_FACTOR = 4,
		/** Terra factor*/          T_FACTOR = 5
		};

 
		/// Values of number multiplication factors
		static const double ScaleFactor[];

		/// Constructor
		Value();

        /// Copy Constructor
        Value(const Value &value);

		/// Destructor
		~Value();

        /// Assignment operator
        Value& operator=(const Value &value);

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
		void SetIntegerValue(long long i);

		/** Sets the undefined value; previous value discarded.
		*/
		void SetUndefinedValue(void);

		/** Sets the error value; previous value discarded.
		*/
		void SetErrorValue(void);

		/** Sets an expression list value; previous value discarded. Unlike the
			version of this function that takes a raw ExprList pointer, this one
			takes (shared) ownership over the list.  This list will be deleted
			when the last copy of this shared_ptr goes away.
			@param l The list value.
		*/
		void SetListValue(classad_shared_ptr<ExprList> l);

		/** Sets an expression list value; previous value discarded. Unlike the
			version of this function that takes a shared_ptr, you still own the ExprList:: it 
			is not owned by the Value class, so it is your responsibility to delete it. 
			@param l The list value.
		*/
		void SetListValue(ExprList* l);

		/** Sets a ClassAd value; previous value discarded. You still own the ClassA:, it 
            is not owned by the Value class, so it is your responsibility to delete it. 
			@param c The ClassAd value.
		*/
		void SetClassAdValue(ClassAd* c);	

		/** Sets a string value; previous value discarded.
			@param str The string value.
		*/
		void SetStringValue( const std::string &str );

		/** Sets a string value; previous value discarded. The string is copied
            so you may feel free to delete the original if you wish. 
			@param str The string value.
		*/
		void SetStringValue( const char *str );

		/** Sets an absolute time value in seconds since the UNIX epoch, & the 
            time zone it's measured in.
			@param secs - Absolute Time Literal(seconds since the UNIX epoch, timezone offset) .
		*/
		void SetAbsoluteTimeValue( abstime_t secs );

		/** Sets a relative time value.
			@param secs Number of seconds.
		*/
		void SetRelativeTimeValue( time_t secs );
		void SetRelativeTimeValue( double secs );

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

		/** Checks if the value is boolean or something considered
			equivalent by implicit conversion (e.g. a number), if
			old ClassAd evaluation semantics are being used. If they're
			not being used, this method behaves like IsBooleanValue().
			@param b The boolean value if return is true.
			@return true iff the value is boolean or equivalent
		 */
		bool IsBooleanValueEquiv(bool &b) const;

		/** Checks if the value is integral.
			@param i The integer value if the value is integer.
			If the type of i is smaller than a long long, the value
			may be truncated.
			@return true iff the value is an integer.
		*/
		inline bool IsIntegerValue(int &i) const;
		inline bool IsIntegerValue(long &i) const;
		inline bool IsIntegerValue(long long &i) const;
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
		bool IsStringValue( std::string &str ) const; 	
		/** Checks if the value is a string.  
			@param str A reference to a C string, which will point to the 
				string value.  This pointer must <b>not</b> be deallocated or
				tampered with.
			@return true iff the value is a string.
		*/
		bool IsStringValue( const char *&str ) const; 	
		/** Checks if the value is a string and provides a copy of it in a buffer you provide
			@param str A buffer to hold the string value.
			@param len The size of the buffer.
			@return true iff the value is a string.
		*/
		bool IsStringValue( char *str, int len ) const; 	
		/** Returns length of the string instead of the string
            @param size This is filled in with the size of the string
			@return true iff the value is string.
		*/
        inline bool IsStringValue( int &size ) const;
		/** Checks if the value is a string.
			@return true iff the value is string.
		*/
		inline bool IsStringValue() const;
		/** Checks if the value is an expression list.
			@param l The expression list if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool IsListValue(const ExprList*& l) const;
		/** Checks if the value is an expression list. The ExprList returned is 
			the original list put into the Value, so you only own it if you own 
			the original.  If the original was put into this Value as a shared_ptr,
			the shared_ptr still owns it (i.e. don't delete this pointer or
			make a new shared_ptr for it).  Consider using IsSListValue() instead.
			@param l The expression list if the value is an expression list. 
			@return true iff the value is an expression list.
		*/
		inline bool IsListValue(ExprList*& l);
		/** Checks if the value is an expression list.
			@param l A shared_ptr to the expression list if the value is an expression list.
			         Note that if the list was not set via a shared_ptr, a copy of the
			         list will be made and this Value will convert from type LIST_VALUE
			         to SLIST_VALUE as a side-effect.
			@return true iff the value is an expression list
		*/
		bool IsSListValue(classad_shared_ptr<ExprList>& l);
		/** Checks if the value is an expression list.
			@return true iff the value is an expression list.
		*/
		inline bool IsListValue() const;
		/** Checks if the value is a ClassAd.
			@param c The ClassAd if the value is a ClassAd.
			@return true iff the value is a ClassAd.
		*/
		inline bool IsClassAdValue(const ClassAd *&c) const; 
		/** Checks if the value is a ClassAd. The ClassAd returned is 
            the original list put into the ClassAd, so you only own it if you own 
            the original.
			@param c The ClassAd if the value is a ClassAd.
			@return true iff the value is a ClassAd.
		*/
		inline bool IsClassAdValue(ClassAd *&c) const; 
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
			This includes booleans, which can be used in arithmetic.
			@return true iff the value is a number
		*/
		bool IsNumber () const;
		/** Checks if the value is numerical. If the value is a real, it is 
				converted to an integer through truncation.
				If the value is a boolean, it is converted to 0 (for False)
				or 1 (for True).
			@param i The integer value of the value if the value is a number.
			If the type of i is smaller than a long long, the value
			may be truncated.
			@return true iff the value is a number
		*/
		bool IsNumber (int &i) const;
		bool IsNumber (long &i) const;
		bool IsNumber (long long &i) const;
		/** Checks if the value is numerical. If the value is an integer, it 
				is promoted to a real.
				If the value is a bollean, it is converted to 0.0 (for False)
				or 1.0 (for True).
			@param r The real value of the value if the value is a number.
			@return true iff the value is a number
		*/
		bool IsNumber (double &r) const;
		/** Checks if the value is an absolute time value.
			@return true iff the value is an absolute time value.
		*/
		bool IsAbsoluteTimeValue( ) const;
		/** Checks if the value is an absolute time value.
			@param secs - Absolute time literal (Number of seconds since the UNIX epoch,timezone offset).
			@return true iff the value is an absolute time value.
		*/
		bool IsAbsoluteTimeValue( abstime_t& secs ) const;
		/** Checks if the value is a relative time value.
			@return true iff the value is a relative time value
		*/
		bool IsRelativeTimeValue( ) const;
		/** Checks if the value is a relative time value.
			@param secs Number of seconds
			@return true iff the value is a relative time value
		*/
		bool IsRelativeTimeValue( double& secs ) const;
		bool IsRelativeTimeValue( time_t& secs ) const;

        bool SameAs(const Value &otherValue) const;

        friend bool operator==(const Value &value1, const Value &value2);

		friend std::ostream& operator<<(std::ostream &stream, Value &value);

	private:
		void _Clear();

		friend class Literal;
		friend class ClassAd;
		friend class ExprTree;

		ValueType 		valueType;		// the type of the value


		union {
			bool			booleanValue;
			long long		integerValue;
			double 			realValue;
			ExprList        *listValue;
			classad_shared_ptr<ExprList> *slistValue;
			ClassAd			*classadValue;
			double			relTimeValueSecs;
			abstime_t		*absTimeValueSecs;
			std::string		*strValue;
		};
};

bool convertValueToRealValue(const Value value, Value &realValue);
bool convertValueToIntegerValue(const Value value, Value &integerValue);
bool convertValueToStringValue(const Value value, Value &stringValue);

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
    i = (int)integerValue;
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
IsIntegerValue (long &i) const
{
    i = (long)integerValue;
    return (valueType == INTEGER_VALUE);
}  

inline bool Value::
IsIntegerValue (long long &i) const
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
    if (valueType == LIST_VALUE) {
        l = listValue;
        return true;
    } else if (valueType == SLIST_VALUE) {
        l = slistValue->get();
        return true;
    } else {
        return false;
    }
}

inline bool Value::
IsListValue( ExprList *&l)
{
    if (valueType == LIST_VALUE) {
        l = listValue;
        return true;
    } else if (valueType == SLIST_VALUE) {
        l = slistValue->get();
        return true;
    } else {
        return false;
    }
}

inline bool Value::
IsListValue () const
{
	return (valueType == LIST_VALUE || valueType == SLIST_VALUE);
}


inline bool Value::
IsStringValue() const
{
    return (valueType == STRING_VALUE);
}


inline bool Value::
IsStringValue( const char *&s ) const
{
	// We want to be careful not to copy it in here. 
	// People may accumulate in the "s" after this call,
	// So it best to only touch it if it exists.
	// (Example: the strcat classad function)
	if (valueType == STRING_VALUE) {
		s = strValue->c_str( );
		return true;
	} else {
		return false;
	}
}

inline bool Value::
IsStringValue( char *s, int len ) const
{
	if( valueType == STRING_VALUE ) {
		strncpy( s, strValue->c_str( ), len );
		return( true );
	}
	return( false );
}

inline bool Value::
IsStringValue( std::string &s ) const
{
	if ( valueType == STRING_VALUE ) {
		s = *strValue;
		return true;
	} else {
		return false;
	}
}

inline bool Value::
IsStringValue( int &size ) const
{
    if (valueType == STRING_VALUE) {
        size = strValue->size();
        return true;
    } else {
        size = -1;
        return false;
    }
}

inline bool Value::
IsClassAdValue(const ClassAd *&ad) const
{
	if ( valueType == CLASSAD_VALUE ) {
		ad = classadValue;
		return true;
	} else {
		return false;
	}
}

inline bool Value::
IsClassAdValue(ClassAd *&ad) const
{
	if ( valueType == CLASSAD_VALUE ) {
		ad = classadValue;
		return true;
	} else {
		return false;
	}
}

inline bool Value:: 
IsClassAdValue() const
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
IsAbsoluteTimeValue( abstime_t &secs ) const
{
	if ( valueType == ABSOLUTE_TIME_VALUE ) {
		secs = *absTimeValueSecs;
		return true;
	} else {
		return false;
	}
}

inline bool Value::
IsRelativeTimeValue( ) const
{
	return( valueType == RELATIVE_TIME_VALUE );
}

inline bool Value::
IsRelativeTimeValue( double &secs ) const
{
	secs = relTimeValueSecs;
	return( valueType == RELATIVE_TIME_VALUE );
}
inline bool Value::
IsRelativeTimeValue( time_t &secs ) const
{
	secs = (int) relTimeValueSecs;
	return( valueType == RELATIVE_TIME_VALUE );
}
inline bool Value::
IsNumber( ) const
{
	return( valueType==INTEGER_VALUE || valueType==REAL_VALUE );
}
} // classad

#endif//__CLASSAD_VALUE_H__
