#ifndef _CONDOR_PUT_TARGETS_H
#define _CONDOR_PUT_TARGETS_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "PutTargets.h"

class PutTargets : public Functor {
	public:
		PutTargets( ClassAd * r, EC2GahpClient * g, ClassAd * s,
			const std::string & su, const std::string & pkf, const std::string & skf ) :
			reply( r ), gahp( g ), scratchpad( s ),
            service_url( su ), public_key_file( pkf ), secret_key_file( skf )
		{ }

		virtual ~PutTargets() { }

		int operator() ();

	private:
		ClassAd * reply;
		EC2GahpClient * gahp;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;
};

#endif /* _CONDOR_PUT_TARGETS_H */
