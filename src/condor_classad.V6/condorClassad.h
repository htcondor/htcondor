#ifndef __CONDOR_CLASSAD_H__
#define __CONDOR_CLASSAD_H__

#include "classad.h"

class CondorClassAd : public ClassAd
{
	public:
		CondorClassAd();
		CondorClassAd( ClassAd*, ClassAd* );
		~CondorClassAd();

		static CondorClassAd *makeCondorClassAd( ClassAd*, ClassAd* );
		bool replaceLeftAd(  ClassAd * );
		bool replaceRightAd( ClassAd * );
		ClassAd *getLeftAd();
		ClassAd *getRightAd();
};

#endif
