/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __MATCH_CLASSAD_H__
#define __MATCH_CLASSAD_H__

#include "classad.h"

BEGIN_NAMESPACE( classad )

/** Special case of a ClassAd which sets up the scope names for bilateral 
  	matching.  The top-level scope is defined as follows:
	<pre>
|   [
|      symmetricMatch   = leftMatchesRight && rightMatchesLeft;
|      leftMatchesRight = adcr.ad.requirements;
|      rightMatchesLeft = adcl.ad.requirements;
|      leftRankValue    = adcl.ad.rank;
|      rightRankValue   = adcr.ad.rank;
|      adcl             =
|          [
|              other    = adcr.ad;
|              my       = ad;       // for condor backwards compatibility
|              target   = other;    // for condor backwards compatibility
|              ad       = 
|                 [
|                     // the ``left'' match candidate goes here
|                 ]
|   	   ];
|      adcr             =
|          [
|              other    = adcl.ad;
|              my       = ad;       // for condor backwards compatibility
|              target   = other;    // for condor backwards compatibility
|              ad       = 
|                 [
|                     // the ``right'' match candidate goes here
|                 ]
|   	   ];
|   ]
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
		~MatchClassAd();

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

	protected:
		const ClassAd *ladParent, *radParent;
		ClassAd *lCtx, *rCtx, *lad, *rad;
};

END_NAMESPACE // classad

#endif
