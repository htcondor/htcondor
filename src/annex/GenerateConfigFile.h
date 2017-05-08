#ifndef _CONDOR_GENERATE_CONFIG_FILE_H
#define _CONDOR_GENERATE_CONFIG_FILE_H

// #include "condor_common.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "GenerateConfigFile.h"

class GenerateConfigFile : public Functor {
	public:
		GenerateConfigFile( EC2GahpClient * g, ClassAd * s ) :
			gahp( g ), scratchpad(s) { }
		virtual ~GenerateConfigFile() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		EC2GahpClient * gahp;
		ClassAd * scratchpad;
};

#endif /* _CONDOR_GENERATE_CONFIG_FILE_H */
