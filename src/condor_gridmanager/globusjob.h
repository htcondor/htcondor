
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
	void UpdateGlobusState( int new_state, int new_error_code );
	GlobusResource *GetResource();
	int syncIO();

	void setProbeInterval( int new_interval );

	static probeInterval;

	// New variables
	bool resourceDown;
	int condorState;
	int gmState;
	int globusState;
	int globusStateErrorCode;
	bool jmUnreachable;
	GlobusResource *myResource;
	int evaluateStateTid;
	time_t lastProbeTime;
	time_t enteredCurrentGmState;
	time_t enteredCurrentGlobusState;
	int numSubmitAttempts;
	int syncedOutputSize;
	int syncedErrorSize;

	GahpClient gahp;

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
	int exitValue;
	bool submitLogged;
	bool executeLogged;
	bool stateChanged;
	bool newJM;		// This means a jobmanager that supports restart
					// and two-phase commit
	bool restartingJM;
	time_t restartWhen;

 protected:
	bool callbackRegistered;
	//Classad *ad;
};

#endif

