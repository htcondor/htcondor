#ifndef _CONDOR_BULKREQUEST_H
#define _CONDOR_BULKREQUEST_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "BulkRequest.h"

class BulkRequest : public Functor {
	public:
		BulkRequest( ClassAd * r, EC2GahpClient * egc, ClassAd * s,
			const std::string & su, const std::string & pkf, const std::string & skf ) :
			gahp( egc ), reply( r ), scratchpad( s ),
			service_url( su ), public_key_file( pkf ), secret_key_file( skf )
		{ }
		virtual ~BulkRequest() { }

		bool validateAndStore( ClassAd const * command, std::string & validationError );
		void setClientToken( const std::string & s ) { client_token = s; }
		bool isValidUntilSet() { return (! valid_until.empty()); }
		void setValidUntil( const std::string & vu ) { valid_until = vu; }
		int operator() ();

	protected:
		EC2GahpClient * gahp;
		ClassAd * reply;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;
		std::string client_token, spot_price, target_capacity;
		std::string iam_fleet_role, allocation_strategy, valid_until;

		std::vector< std::string > launch_specifications;
};

#endif /* _CONDOR_BULKREQUEST_H */

