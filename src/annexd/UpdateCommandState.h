#ifndef _CONDOR_UPDATE_COMMAND_STATE_H
#define _CONDOR_UPDATE_COMMAND_STATE_H

// #include "condor_common.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "UpdateCommandState.h"

class UpdateCommandState : public Functor {
	public:
		UpdateCommandState( ClassAdCollection * cac, ClassAd * s, EC2GahpClient * g ) :
			commandState( cac ), scratchpad( s ), gahp( g ) { }
		virtual ~UpdateCommandState() { }

		int operator() ();

	private:
		ClassAdCollection * commandState;
		ClassAd * scratchpad;
		EC2GahpClient * gahp;
};

#endif /* _CONDOR_UPDATE_COMMAND_STATE_H */
