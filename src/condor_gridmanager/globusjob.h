
#ifndef GLOBUSJOB_H
#define GLOBUSJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "globus_utils.h"
#include "globusresource.h"

#define JM_COMMIT_TIMEOUT	300

class GlobusJob : public Service
{
 public:

	GlobusJob( GlobusJob& copy );
	GlobusJob( ClassAd *classad, GlobusResource *resource );

	~GlobusJob();

	void SetEvaluateState();
	int doEvaluateState();
	void NotifyResourceDown();
	void NotifyResourceUp();
	void UpdateCondorState( int new_state );
	void UpdateGlobusState( int new_state );
	GlobusResource *GetResource();

	// New variables
	bool resourceDown;
	int condorState;
	int gmState;
	int globusState;
	bool jmUnreachable;
	GlobusResource *myResource;
	int evaluateStateTid;
	time_t lastProbeTime;

	const char *errorString();

	char *buildRSL( ClassAd *classad );

	PROC_ID procID;
	char *jobContact;
	char *RSL;
	char *localOutput;
	char *localError;
	int globusError;
	int jmFailureCode;
	char *userLogFile;
	bool removedByUser;
	int exitValue;
	bool executeLogged;
	bool exitLogged;
	bool stateChanged;
	bool newJM;		// This means a jobmanager that supports restart
					// and two-phase commit
	bool restartingJM;
	time_t restartWhen;
	bool durocRequest;

 protected:
	bool callbackRegistered;
	bool ignore_callbacks;
	//Classad *ad;
};

#endif
