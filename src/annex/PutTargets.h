#ifndef _CONDOR_PUT_TARGETS_H
#define _CONDOR_PUT_TARGETS_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "PutTargets.h"

class PutTargets : public Functor {
	public:
		PutTargets( const std::string & t, time_t l,
			ClassAd * r, EC2GahpClient * g, ClassAd * s,
			const std::string & su, const std::string & pkf, const std::string & skf,
			ClassAdCollection * c, const std::string & cid,
			const std::string & aid ) :
			target( t ), leaseExpiration( l ),
			reply( r ), gahp( g ), scratchpad( s ),
            service_url( su ), public_key_file( pkf ), secret_key_file( skf ),
            commandID( cid ), commandState( c ),
            annexID( aid )
		{ ASSERT(! target.empty()); }

		virtual ~PutTargets() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		std::string target;
		time_t leaseExpiration;

		ClassAd * reply;
		EC2GahpClient * gahp;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;

		std::string commandID;
		ClassAdCollection * commandState;
		std::string annexID;
};

#endif /* _CONDOR_PUT_TARGETS_H */
