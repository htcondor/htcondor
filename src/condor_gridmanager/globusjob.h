
#ifndef GLOBUSJOB_H
#define GLOBUSJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "globus_utils.h"
#include "gahp-client.h"
#include "globusresource.h"

#define JM_COMMIT_TIMEOUT	300

class GlobusResource;

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
	void GramCallback( int new_state, int new_error_code );
	void GetCallbacks();
	void ClearCallbacks();
	GlobusResource *GetResource();
	int syncIO();

	static int probeInterval;
	static int submitInterval;
	static int restartInterval;
	static int gahpCallTimeout;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setRestartInterval( int new_interval )
		{ restartInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	// New variables
	bool resourceDown;
	bool resourceStateKnown;
	int condorState;
	int gmState;
	int globusState;
	int globusStateErrorCode;
	int globusStateBeforeFailure;
	int callbackGlobusState;
	int callbackGlobusStateErrorCode;
	bool jmUnreachable;
	GlobusResource *myResource;
	int evaluateStateTid;
	time_t lastProbeTime;
	time_t enteredCurrentGmState;
	time_t enteredCurrentGlobusState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	int syncedOutputSize;
	int syncedErrorSize;
	int shadowBirthday;
	char *holdReason;
	int submitFailureCode;
	int lastRestartReason;
	time_t lastRestartAttempt;
	int numRestartAttempts;
	int numRestartAttemptsThisSubmit;

	GahpClient gahp;

	char *buildRSL( ClassAd *classad );

	PROC_ID procID;
	char *jobContact;
	char *RSL;
	char *localOutput;
	char *localError;
	int globusError;
	char *userLogFile;
	int exitValue;
	bool submitLogged;
	bool executeLogged;
	bool submitFailedLogged;
	bool terminateLogged;
	bool abortLogged;
	bool evictLogged;
	bool holdLogged;

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

