#ifndef __CONDOR_CLASSAD_H__
#define __CONDOR_CLASSAD_H__

#include "classad.h"

/** Special case of a ClassAd which sets up the scope names ``self'' and 
	``other'' for a bilateral matching.
*/
class CondorClassAd : public ClassAd
{
	public:
		/// Default constructor
		CondorClassAd();
		/// Constructor which builds the CondorClassad given two ads
		CondorClassAd( ClassAd*, ClassAd* );
		/// Default destructor
		~CondorClassAd();

		/** Factory method to make a CondorClassad given two ClassAds to be
			matched.
			@param al The ad to be placed in the left context.
			@param ar The ad to be placed in the right context.
			@return A CondorClassad, or NULL if the operation failed.
		*/
		static CondorClassAd *makeCondorClassAd( ClassAd*, ClassAd* );

		/** Replaces ad in the left context, or insert one if an ad did not
			previously exist
			@param al The ad to be placed in the left context.
			@return true if the operation succeeded and false otherwise.
		*/
		bool replaceLeftAd(  ClassAd *al );

		/** Replaces ad in the right context, or insert one if an ad did not
			previously exist
			@param ar The ad to be placed in the right context.
			@return true if the operation succeeded and false otherwise.
		*/
		bool replaceRightAd( ClassAd *ar );

		/** Gets the ad in the left context.
			@return The ClassAd, or NULL if the ad doesn't exist.
		*/
		ClassAd *getLeftAd();

		/** Gets the ad in the right context.
			@return The ClassAd, or NULL if the ad doesn't exist.
		*/
		ClassAd *getRightAd();
};

#endif
