
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
	int submitFailureCode;
	int lastRestartReason;
	time_t lastRestartAttempt;
	int numRestartAttempts;
	int numRestartAttemptsThisSubmit;

	GahpClient gahp;

	char *buildSubmitRSL();
	char *buildRestartRSL();
	char *buildStdioUpdateRSL();

	void UpdateJobAd( const char *name, const char *value );
	void UpdateJobAdInt( const char *name, int value );
	void UpdateJobAdFloat( const char *name, float value );
	void UpdateJobAdBool( const char *name, int value );
	void UpdateJobAdString( const char *name, const char *value );

	PROC_ID procID;
	char *jobContact;
		// If we're in the middle of a globus call that requires an RSL,
		// the RSL is stored here (so that we don't have to reconstruct the
		// RSL every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
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
	int jmVersion;
	bool restartingJM;
	time_t restartWhen;

	ClassAd *ad;

 protected:
	bool callbackRegistered;
};

#endif

