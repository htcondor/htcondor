#ifndef _CONDOR_ADD_TARGET_H
#define _CONDOR_ADD_TARGET_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "AddTarget.h"

class AddTarget : public Functor {
	public:
		AddTarget( ClassAd * r, EC2GahpClient * g, ClassAd * s,
			const std::string & su, const std::string & pkf, const std::string & skf ) :
			reply( r ), gahp( g ), scratchpad( s ),
            service_url( su ), public_key_file( pkf ), secret_key_file( skf )
		{ }

		virtual ~AddTarget() { }

		int operator() ();

	private:
		ClassAd * reply;
		EC2GahpClient * gahp;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;
};

#endif /* _CONDOR_CREATE_RULE_H */
