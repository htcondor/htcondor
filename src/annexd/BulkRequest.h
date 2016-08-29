// #include <string>
// #include <vector>
// #include "condor_classad.h"
// #include "gahp-client.h"
// #include "BulkRequest.h"

#ifndef _CONDOR_BULKREQUEST_H
#define _CONDOR_BULKREQUEST_H

class BulkRequest : public Service {
	public:
		BulkRequest( EC2GahpClient * egc,
		  const std::string & serviceURL,
		  const std::string & publicKeyFile,
		  const std::string & secretKeyFile
		  ) : gahp( egc ),
		      service_url( serviceURL ),
		      public_key_file( publicKeyFile ),
		      secret_key_file( secretKeyFile )
		{ };
		virtual ~BulkRequest() { }

		bool validateAndStore( ClassAd const * command, std::string & validationError );
		void setReplyStream( Stream * s ) { replyStream = s; }
		bool isValidUntilSet() { return (! valid_until.empty()); }
		void setValidUntil( const std::string & vu ) { valid_until = vu; }
		void operator() () const;

	protected:
		EC2GahpClient * gahp;
		Stream * replyStream;

		std::string service_url, public_key_file, secret_key_file;
		std::string client_token, spot_price, target_capacity;
		std::string iam_fleet_role, allocation_strategy, valid_until;

		std::vector< std::string > launch_specifications;
};

#endif /* _CONDOR_BULKREQUEST_H */

