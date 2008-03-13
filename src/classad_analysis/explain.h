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


#ifndef __EXPLAIN_H__
#define __EXPLAIN_H__

#define WANT_CLASSAD_NAMESPACE
#include "condor_fix_iostream.h"
#include "classad/classad_distribution.h"
#include "list.h"
#include "interval.h"

/** A base class representing an annotation for an expression.  Current derived
	current derived classes include MultiProfileExplain, ProfileExplain, and
	ConditionExplain.
 */
class Explain
{
 public:

		/** Virtual method for generating a string equivalent to the Explain.
			@param buffer A string for the explain to be printed to
			@return true on success, false on failure.
		*/
	virtual bool ToString( std::string &buffer ) = 0;

		/** Virtual destructor */
	virtual ~Explain( ) = 0;
 protected:
	bool initialized;
	Explain( );
};

/** A derived class of Explain used to annotate a MultiProfile expression.
	@see MultiProfile
*/
class MultiProfileExplain: Explain
{
 public:

		/** True if the MultiProfile matches some ClassAd, false otherwise */ 
	bool match;

		/** Number of ClassAds matched by the MultiProfile expression */
	int numberOfMatches;

		/** Which ClassAds the job matches */
	IndexSet matchedClassAds;

		/** Total number of ClassAds */
	int numberOfClassAds;

		/** Default Constructor */
	MultiProfileExplain( );

		/** Destructor */
	~MultiProfileExplain( );

		/** Initializes the MultiProfileExplain.
			@param match A boolean value for the match member variable.
			@param numberOfMatches An integer value for the numberOfMatches
			member variable
.			@return true on success, false on failure.
		*/
	bool Init( bool match, int numberOfMatches, IndexSet &matchedClassAds,
			   int numberOfClassAds );

		/** Generate a string representation of the MultiProfileExplain
			@param buffer A string to print the result to.
			@return true on success, false on failure.
		*/ 
	bool ToString( std::string &buffer );
};

/** A derived class of Explain used to annotate a Profile expression.
	@see Profile
*/
class ProfileExplain: Explain
{
 public:

		/** True if the Profile matches some ClassAd, false otherwise */ 
	bool match;

		/** Number of ClassAds matched by the Profile expression */
	int numberOfMatches;

		/** Which Conditions conflict with one another */
	List< IndexSet > *conflicts;

		/** Default Constructor */
	ProfileExplain( );

		/** Destructor */
	~ProfileExplain( );

		/** Initializes the ProfileExplain.
			@param match A boolean value for the match member variable.
			@param numberOfMatches An integer value for the numberOfMatches
			member variable
.			@return true on success, false on failure.
		*/
	bool Init( bool match, int numberOfMatches );

		/** Generate a string representation of the ProfileExplain
			@param buffer A string to print the result to.
			@return true on success, false on failure.
		*/ 
	bool ToString( std::string &buffer );
};

/** A derived class of Explain used to annotate a Condition expression.
	@see Condition
*/
class ConditionExplain: Explain
{
 public:
	enum Suggestion
	{
    /** no suggestion */ NONE,
	/** keep the Condtition*/ KEEP,
	/** remove the Condition */ REMOVE,
	/** modify the value part of the Condition to the Value stored in 
        newValue*/ MODIFY
	};


		/** True if the Condition matches some ClassAd, false otherwise */ 
	bool match;

		/** Number of ClassAds matched by the Condition expression */
	int numberOfMatches;

		/** A suggestion for what to do with the Condition */
	Suggestion suggestion;

		/** If suggestion = MODIFY the new Value will be stored here */
	classad::Value newValue;


		/** Default Constructor */
	ConditionExplain( );

		/** Destructor */
	~ConditionExplain( );

		/** Initializes the ConditionExplain. suggestion is set to NONE
			@param match A boolean value for the match member variable.
			@param numberOfMatches An integer value for the numberOfMatches
			member variable
.			@return true on success, false on failure.
		*/
	bool Init( bool match, int numberOfMatches );

		/** Initializes the ConditionExplain.
			@param match A boolean value for the match member variable.
			@param numberOfMatches An integer value for the numberOfMatches
			member variable
			@param suggestion A Suggestion value ( usually KEEP or REMOVE ) 
.			@return true on success, false on failure.
		*/
	bool Init( bool match, int numberOfMatches, Suggestion suggestion );

		/** Initializes the ConditionExplain.  Suggestion is set to MODIFY.
			@param match A boolean value for the match member variable.
			@param numberOfMatches An integer value for the numberOfMatches
			member variable
			@param newValue A Value to be stored in the newValue member
			variable
.			@return true on success, false on failure.
		*/
	bool Init( bool match, int numberOfMatches, classad::Value &newValue );

		/** Generate a string representation of the ConditionExplain
			@param buffer A string to print the result to.
			@return true on success, false on failure.
		*/ 
	bool ToString( std::string &buffer );
};

/** A derived class of Explain used to suggest a modification to an attribute
	of a ClassAd. */
class AttributeExplain: Explain
{
 public:
	enum Suggestion
	{
	/** no suggestion */ NONE,
	/** modify the value */ MODIFY
	};

		/** The attribute to be modified or left alone */
	std::string attribute;

		/** The suggestion for what to do with the attribute */
	Suggestion suggestion;

		/** true if suggesting a range of values, false if suggesting a
			discrete value */
	bool isInterval;

		/** A value for the attribute to be modified to */
	classad::Value discreteValue;

		/** A range of values for the attribute to be modified to */
	Interval *intervalValue;

		/** Default Constructor */
	AttributeExplain( );

		/** Destructor */
	~AttributeExplain( );

		/** Initializes the ClassAdExplain.  suggestion is set to NONE.
			@param _attribute A string representing an attribute to be left
			alone.*/
	bool Init( std::string _attribute );

		/** Initializes the ClassAdExplain.  suggestion is set to MODIFY.
			The suggestion is to replace the value of the attribute with
			the value represented by discreteValue.
			@param _attribute A string representing an attribute to modify
			@param _discreteValue The suggested new value for the attribute.
		*/
	bool Init( std::string _attribute, classad::Value &_discreteValue );

		/** Initializes the ClassAdExplain.  suggestion is set to MODIFY.
			The suggestion is to replace the value of the attribute with
			a value in the range represented by intervalValue.
			@param _attribute A string representing an attribute to modify
			@param _intervalValue The suggested range for the new value for the
			attribute.
		*/
	bool Init( std::string _attribute, Interval *_intervalValue );

		/** Generate a string representation of the AttributeExplain
			@param buffer A string to print the result to.
			@return true on success, false on failure.
		*/ 
	bool ToString( std::string &buffer );
};

/** A derived class of Explain used to suggest modification of attributes of a
	ClassAd. */
class ClassAdExplain: Explain
{
 public:

		/** A list of attributes that are undefined in the ClassAd */
	List<std::string> undefAttrs;

		/** A list of AttrExplain objects corresponding attributes in the
			ClassAd.*/
	List<AttributeExplain> attrExplains;

		/** Default Constructor */
	ClassAdExplain( );

		/** Destructor */
	~ClassAdExplain( );

		/** Initializes the ClassAdExplain.
			@param _undefAttrs A list of strings which will be copied
			@param _attrExplains A list of pointers to AttrExplain objects. The
			pointers are copied into attrExplains, the objects are not copied.
		*/
	bool Init( List<std::string> &_undefAttrs,
			   List<AttributeExplain> &_attrExplains );

		/** Generate a string representation of the ClassAdExplain
			@param buffer A string to print the result to.
			@return true on success, false on failure.
		*/ 
	bool ToString( std::string &buffer );
};
#endif	// __EXPLAIN_H__

