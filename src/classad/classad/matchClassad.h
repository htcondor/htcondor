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


#ifndef __CLASSAD_MATCH_CLASSAD_H__
#define __CLASSAD_MATCH_CLASSAD_H__

#include "classad/classad.h"

namespace classad {

/** Special case of a ClassAd which make it easy to do matching.  
    The top-level ClassAd equivalent to the following, with some
    minor implementation differences for efficiency.  Because of
	the use of global-scope references to .LEFT and .RIGHT, this
    ad is not suitable for nesting inside of other ads.  The use
    of global-scope references is done purely for efficiency,
    since attribute lookups tends to be a big part of the time
	spent in matchmaking.
	<pre>
    [
       symmetricMatch   = leftMatchesRight && rightMatchesLeft;
       leftMatchesRight = RIGHT.requirements;
       rightMatchesLeft = LEFT.requirements;
       leftRankValue    = LEFT.rank;
       rightRankValue   = RIGHT.rank;
       RIGHT            = rCtx.ad;
       LEFT             = lCtx.ad;
       lCtx             =
           [
               other    = .RIGHT;
               target   = .RIGHT;   // for condor backwards compatibility
               my       = .LEFT;    // for condor backwards compatibility
               ad       = 
                  [
                      // the ``left'' match candidate goes here
                  ]
    	   ];
       rCtx             =
           [
               other    = .LEFT;
               target   = .LEFT;    // for condor backwards compatibility
               my       = .RIGHT;   // for condor backwards compatibility
               ad       = 
                  [
                      // the ``right'' match candidate goes here
                  ]
    	   ];
    ]
	</pre>
*/
class MatchClassAd : public ClassAd
{
	public:
		/// Default constructor
		MatchClassAd();
		/** Constructor which builds the CondorClassad given two ads
		 	@param al The left candidate ad
			@param ar The right candidate ad
		*/
		MatchClassAd( ClassAd* al, ClassAd* ar );
		/// Default destructor
		virtual ~MatchClassAd();

		/** Factory method to make a MatchClassad given two ClassAds to be
			matched.
			@param al The ad to be placed in the left context.
			@param ar The ad to be placed in the right context.
			@return A CondorClassad, or NULL if the operation failed.
		*/
		static MatchClassAd *MakeMatchClassAd( ClassAd* al, ClassAd* ar );

		/** Method to initialize a MatchClassad given two ClassAds.  The old
		 	expressions in the classad are deleted.
			@param al The ad to be placed in the left context.
			@param ar The ad to be placed in the right context.
			@return true if the operation succeeded, false otherwise
		*/
		bool InitMatchClassAd( ClassAd* al, ClassAd *ar );

		/** @return true if right and left ads match each other
		 */
		bool symmetricMatch();

		/** @return true if the right ad matches the left ad's requirements
		 */
		bool rightMatchesLeft();

		/** @return true if the left ad matches the right ad's requirements
		 */
		bool leftMatchesRight();

		/** Replaces ad in the left context, or insert one if an ad did not
			previously exist
			@param al The ad to be placed in the left context.
			@return true if the operation succeeded and false otherwise.
		*/
		bool ReplaceLeftAd( ClassAd *al );

		/** Replaces ad in the right context, or insert one if an ad did not
			previously exist
			@param ar The ad to be placed in the right context.
			@return true if the operation succeeded and false otherwise.
		*/
		bool ReplaceRightAd( ClassAd *ar );

		/** Gets the ad in the left context.
			@return The ClassAd, or NULL if the ad doesn't exist.
		*/
		ClassAd *GetLeftAd();

		/** Gets the ad in the right context.
			@return The ClassAd, or NULL if the ad doesn't exist.
		*/
		ClassAd *GetRightAd();

		/** Gets the left context ad. (<tt>.adcl</tt> in the above example)
		 	@return The left context ad, or NULL if the MatchClassAd is not
				valid
		*/
		ClassAd *GetLeftContext( );

		/** Gets the right context ad. (<tt>.adcr</tt> in the above example)
		 	@return The left context ad, or NULL if the MatchClassAd is not
				valid
		*/
		ClassAd *GetRightContext( );

		/** Removes the left candidate from the match classad.  If the 
		    candidate ``lives'' in another data structure, this method
			should be called so that the match classad doesn't delete the
			candidate.
			@return The left candidate ad.
		*/
		ClassAd *RemoveLeftAd( );

		/** Removes the right candidate from the match classad.  If the 
		    candidate ``lives'' in another data structure, this method
			should be called so that the match classad doesn't delete the
			candidate.
			@return The right candidate ad.
		*/
		ClassAd *RemoveRightAd( );

		/** Sets an alternate name by which the left/right ad can be
		 *  referred to from either ad (in addition to the relative
		 *  names 'My', 'Target', and 'Other').
		 */
		void SetLeftAlias( const std::string &name );
		void SetRightAlias( const std::string &name );

		/** Modifies the requirements expression in the given ad to
			make matchmaking more efficient.  This will only improve
			efficiency if it is called once and then the resulting
			requirements are used multiple times.  Saves the old
			requirements expression so it can be restored via
			UnoptimizeAdForMatchmaking.
			@param ad The ad to be optimized.
			@param error_msg non-NULL if an error description is desired.
			@return True on success.
		*/
		static bool OptimizeRightAdForMatchmaking( ClassAd *ad, std::string *error_msg, const std::string &left_alias = "", const std::string &right_alias = "" );

		/** Modifies the requirements expression in the given ad to
			make matchmaking more efficient.  This will only improve
			efficiency if it is called once and then the resulting
			requirements are used multiple times.  Saves the old
			requirements expression so it can be restored via
			UnoptimizeAdForMatchmaking.
			@param ad The ad to be optimized.
			@param error_msg non-NULL if an error description is desired.
			@return True on success.
		*/
		static bool OptimizeLeftAdForMatchmaking( ClassAd *ad, std::string *error_msg, const std::string &left_alias = "", const std::string &right_alias = "" );

		/** Restores ad previously optimized with OptimizeAdForMatchmaking.
			@param ad The ad to be unoptimized.
			@return True on success.
		*/
		static bool UnoptimizeAdForMatchmaking( ClassAd *ad );

	protected:
		const ClassAd *ladParent, *radParent;
		ClassAd *lCtx, *rCtx, *lad, *rad;
		ExprTree *symmetric_match, *right_matches_left, *left_matches_right;
		std::string lAlias, rAlias;

    private:
        // The copy constructor and assignment operator are defined
        // to be private so we don't have to write them, or worry about
        // them being inappropriately used. The day we want them, we can 
        // write them. 
        MatchClassAd(const MatchClassAd &) : ClassAd(){ return;       }
        MatchClassAd &operator=(const MatchClassAd &) { return *this; }

		/** Modifies the requirements expression in the given ad to
			make matchmaking more efficient.  This will only improve
			efficiency if it is called once and then the resulting
			requirements are used multiple times.  Saves the old
			requirements expression so it can be restored via
			UnoptimizeAdForMatchmaking.
			@param ad The ad to be optimized.
			@param is_right True if this ad will be the right ad.
			@param error_msg non-NULL if an error description is desired.
			@return True on success.
		*/
		static bool OptimizeAdForMatchmaking( ClassAd *ad, bool is_right, std::string *error_msg, const std::string &left_alias, const std::string &right_alias );

		/**
		   @return true if the given expression evaluates to true
		*/
		bool EvalMatchExpr(ExprTree *match_expr);
};

} // classad

#endif
