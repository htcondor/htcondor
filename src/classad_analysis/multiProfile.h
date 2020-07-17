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


#ifndef __MULTIPROFILE_H__
#define __MULTIPROFILE_H__

#include "boolExpr.h"
#include "list.h"

/** A BoolExpr in Disjunctive Profile Form, which is a disjunction of Profiles.
	Currently the only way to initialize a MultiProfile is with the
	ExprToMultiProfile method in the BoolExpr class.
	@see BoolExpr
*/
class MultiProfile : public BoolExpr
{
	friend class BoolExpr;
 public:

		/** Default Constructor */
	MultiProfile( );

		/** Destructor */
	~MultiProfile( );

		/** Determines if the MultiProfile represents a literal value
			@return true if MulitProfile is a literal value, false otherwise.
		*/
	bool IsLiteral( ) const;

		/** Gets the BoolValue if the MultiProfile represents a literal value
			@param result the BoolValue represented by the MultiProfile
			@return true on success, false on failure or if MultiProfile is not
			a literal value.
		*/
	bool GetLiteralValue( BoolValue &result );

		/** Gets the number of profiles contained in the MultiProfile
			@param result The number of profiles.
			@return true on success, false on failure.
		*/
	bool GetNumberOfProfiles( int &result );
		
		/** Rewinds the list of Profiles in the MultiProfile
			@return true on success, false on failure
		*/
	bool Rewind( );

		/** Gets the next Profile in the MultiProfile
			@param result the next profile in the MultiProfile
			@return true on success, false on failure
		*/
	bool NextProfile( Profile *&result );

		/** Generate a string representation of the MultiProfile.
			@param buffer A string to print the result to.
			@return true on success, false on failure.
		*/ 
	bool ToString( std::string &buffer );


		// unsupported methods
	bool AddProfile( Profile & );
	bool RemoveProfile( Profile & );
	bool RemoveAllProfiles( );

		/** A repository for annotations for the MultiProfile.
			@see MultiProfileExplain
		*/
	MultiProfileExplain explain;

 protected:
	bool InitVal( classad::Value & );
 private:
	// The assignment overload was VERY broken. Make it private and remove the
	// old implementation. If someone uses it, they'll know at
	// compile time, check here, read this comment, be upset, and hopefully
	// implement the correct assignment overload. -psilord
    MultiProfile & operator=(const MultiProfile& /* copy */);

	bool isLiteral;
	BoolValue literalValue;
	bool AppendProfile( Profile * );
	List<Profile> profiles;
};	

#endif // __MULTIPROFILE_H__
