#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "starter_util.h"

int
computeDesiredExitStatus( const std::string & prefix, ClassAd * ad,
	bool * exitStatusSpecified ) {
	if( ad == NULL ) { return 0; }

	int desiredExitCode = 0;
	std::string exitCode = "Success" + prefix + "ExitCode";
	if( prefix.empty() ) { exitCode = "JobSuccessExitCode"; }
	if( ad->LookupInteger( exitCode, desiredExitCode ) ) {
		if( exitStatusSpecified ) { * exitStatusSpecified = true; }
	}

	int desiredExitSignal = 0;
	std::string exitSignal = "Success" + prefix + "ExitSignal";
	if( prefix.empty() ) { exitSignal = "JobSuccessExitSignal"; }
	if( ad->LookupInteger( exitSignal, desiredExitSignal ) ) {
		if( exitStatusSpecified ) { * exitStatusSpecified = true; }
	}

	bool desiredExitBySignal = false;
	std::string exitBySignal = "Success" + prefix + "ExitBySignal";
	if( prefix.empty() ) { exitBySignal = "JobSuccessExitBySignal"; }
	ad->LookupBool( exitBySignal, desiredExitBySignal );

	int desiredExitStatus = 0;
	if( desiredExitBySignal ) {
		desiredExitStatus = desiredExitSignal;
	} else if( desiredExitCode != 0 ) {
#if defined( WINDOWS )
		desiredExitStatus = desiredExitCode;
#else
		desiredExitStatus = desiredExitCode << 8;
#endif
	}
	return desiredExitStatus;
}

