#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "../condor_starter.V6.1/starter.h"

// #include "dc_coroutines.h"
// using namespace condor;


//
// We want to test requestGuidanceJobEnvironment[Unr|R]eady() to
// verify that they will "carry on" properly in the case of any
// of a wide variety of failures.
//
// With only a little refactoring, we can move those functions
// out of the starter proper, which will allow us to mock out
// the starter; likewise, we can refactor to make the jic an
// explicit parameter.  Doing so lets us invoke those functions
// with certain known failure cases; we just need to modify those
// functions to log when the fall-through ("carry on") case was
// to create observable behavior to check for.
//

class MockJIC : public JobInfoCommunicator {

	public:

		MockJIC();
		virtual ~MockJIC();

		virtual bool genericRequestGuidance(
			const ClassAd & request,
			GuidanceResult & rv, ClassAd & guidance
		);

        // Additional mocks.
        virtual int JobCluster() const { return 1; }
        virtual int JobProc() const { return 0; }

        virtual bool init() { EXCEPT("MOCK"); return false; }
        virtual void config() { EXCEPT("MOCK"); }
        virtual void setupJobEnvironment() { EXCEPT("MOCK"); }
        virtual bool holdJob( const char *, int, int ) { EXCEPT("MOCK"); return -1; }
        virtual bool removeJob( const char* ) { EXCEPT("MOCK"); return false; }
        virtual bool terminateJob( const char* ) { EXCEPT("MOCK"); return false; }
        virtual bool requeueJob( const char* ) { EXCEPT("MOCK"); return false; }
        virtual char* getJobStdFile( const char* ) { EXCEPT("MOCK"); return NULL; }
        virtual float bytesSent( void ) { EXCEPT("MOCK"); return -1.0; }
        virtual float bytesReceived( void ) { EXCEPT("MOCK"); return -1.0; }
        virtual void Suspend( void ) { EXCEPT("MOCK"); }
        virtual void Continue( void ) { EXCEPT("MOCK"); }
        virtual bool transferOutput( bool & ) { EXCEPT("MOCK"); return false; }
        virtual bool transferOutputMopUp( void ) { EXCEPT("MOCK"); return false; }
        virtual void allJobsGone( void ) { EXCEPT("MOCK"); }
        virtual int reconnect( ReliSock*, ClassAd* ) { EXCEPT("MOCK"); return -1; }
        virtual void disconnect() { EXCEPT("MOCK"); }
        virtual void notifyJobPreSpawn( void ) { EXCEPT("MOCK"); }
        virtual bool notifyJobExit( int, int, UserProc* ) { EXCEPT("MOCK"); return false; }
        virtual int notifyJobTermination( UserProc* ) { EXCEPT("MOCK"); return -1; }
        virtual bool notifyStarterError( const char*, bool, int, int ) { EXCEPT("MOCK"); return false; }
        virtual void addToOutputFiles( const char* ) { EXCEPT("MOCK"); }
        virtual void removeFromOutputFiles( const char*) { EXCEPT("MOCK"); }
        virtual bool registerStarterInfo( void ) { EXCEPT("MOCK"); return false; }
        virtual bool initUserPriv( void ) { EXCEPT("MOCK"); return false; }
        virtual bool publishUpdateAd( ClassAd* ) { EXCEPT("MOCK"); return false; }

};


MockJIC::MockJIC() {
}


MockJIC::~MockJIC() {
}


bool MockJIC::genericRequestGuidance(
    const ClassAd & /* request */,
    GuidanceResult & rv, ClassAd & guidance
) {
    dprintf( D_ALWAYS, "MockJIC::genericRequestGuidance()\n" );

    // Option 1.
    // return false;

    // Option 2.
    // rv = GuidanceResult::UnknownRequest;
    // return true;

    // Option 3.
    // rv = GuidanceResult::MalformedRequest;
    // return true;

    // Option 4.
    // rv = GuidanceResult::Invalid;
    // return true;

    // Option 5.
    // rv = GuidanceResult::Command;
    // return true;

    // Option 6.
    //
    // This is where we'd test that valid commands get dispatched properly,
    // but our interest is in the reaction to invalid commands, so we'll
    // try: (a) an unknown command name and (b) a known command missing at
    // least one required attribute, instead.  We presently don't do (c),
    // a known command with a required attribute being invalid because that
    // results in a valid (but error-carrying) reply.  We could try
    // intercepting notifyGenericEvent() to inspect the reply, but .. FIXME.
    // rv = GuidanceResult::Command;
    // guidance.InsertAttr( ATTR_COMMAND, "<Invalid Command>" );
    // return true;

    rv = GuidanceResult::Command;
    guidance.InsertAttr( ATTR_COMMAND, COMMAND_RUN_DIAGNOSTIC );
    return true;

    // rv = GuidanceResult::Command;
    // guidance.InsertAttr( ATTR_COMMAND, COMMAND_RUN_DIAGNOSTIC );
    // guidance.InsertAttr( ATTR_DIAGNOSTIC, "<Unknown Diagnostic>" );
    // return true;
}


class MockStarter : public Starter {

    public:

        MockStarter();
        virtual ~MockStarter() = default;

		// The "carry on" action if the job environment is ready.
		virtual bool jobWaitUntilExecuteTime();

		// The "carry on" action if the job environmet is unready.
		virtual bool skipJobImmediately();

};


MockStarter::MockStarter() {
    jic = new MockJIC();
}

bool
MockStarter::jobWaitUntilExecuteTime() {
    dprintf( D_ALWAYS, "MockStarter::jobWaitUntilExecuteTime()\n" );
    return true;
}


bool
MockStarter::skipJobImmediately() {
    dprintf( D_ALWAYS, "MockStarter::skipJobImmediately()\n" );
    return true;
};


//
// test_main(); returning non-zero prevents entrance into the event loop.
//

int
test_main( int /* argv */, char ** /* argv */ ) {
    // Initialize the config settings.
    config_ex( CONFIG_OPT_NO_EXIT );

    // Allow uid switching if root.
    set_priv_initialize();

    // Initialize the config system.
    config();


    //
    // ...
    //
    MockStarter mock_starter;
    Starter::requestGuidanceJobEnvironmentReady( & mock_starter );


    // The "unit" tests shouldn't need to return to daemon core.
    return 1;
}


//
// Stubs for daemon core.
//

void main_config() { }

void main_pre_command_sock_init() { }

void main_pre_dc_init( int /* argc */, char ** /* argv */ ) { }

void main_shutdown_fast() { DC_Exit( 0 ); }

void main_shutdown_graceful() { DC_Exit( 0 ); }

void
main_init( int argc, char ** argv ) {
    int rv = test_main( argc, argv );
    if( rv ) { DC_Exit( rv ); }
}


//
// main()
//

int
main( int argc, char * argv[] ) {
    set_mySubSystem( "TOOL", false, SUBSYSTEM_TYPE_TOOL );

    dc_main_init = & main_init;
    dc_main_config = & main_config;
    dc_main_shutdown_fast = & main_shutdown_fast;
    dc_main_shutdown_graceful = & main_shutdown_graceful;
    dc_main_pre_dc_init = & main_pre_dc_init;
    dc_main_pre_command_sock_init = & main_pre_command_sock_init;

    return dc_main( argc, argv );
}


// Additional mocks.  Stubbing these out dramatically decreases the
// number of source files required to build this test, and its complexity.
Starter::Starter() { }
Starter::~Starter() { }

bool Starter::Init(JobInfoCommunicator*, char const*, bool, int, int, int) { EXCEPT("MOCK"); return false; }
void Starter::StarterExit(int code) { EXCEPT("MOCK"); exit(code); }
int Starter::FinalCleanup(int code) { EXCEPT("MOCK"); return code; }
void Starter::Config() { EXCEPT("MOCK"); }
int Starter::SpawnJob() { EXCEPT("MOCK"); return -1; }
void Starter::WriteRecoveryFile(classad::ClassAd*) { EXCEPT("MOCK"); }
void Starter::RemoveRecoveryFile() { EXCEPT("MOCK"); }

int Starter::RemoteSuspend(int i) { EXCEPT("MOCK"); return i; }
bool Starter::Suspend() { EXCEPT("MOCK"); return false; }
int Starter::RemoteContinue(int i) { EXCEPT("MOCK"); return i; }
bool Starter::Continue() { EXCEPT("MOCK"); return false; }
int Starter::RemotePeriodicCkpt(int i) { EXCEPT("MOCK"); return i; }
bool Starter::PeriodicCkpt() { EXCEPT("MOCK"); return false; }
int Starter::RemoteRemove(int i) { EXCEPT("MOCK"); return i; }
bool Starter::Remove() { EXCEPT("MOCK"); return false; }
int Starter::RemoteHold(int i) { EXCEPT("MOCK"); return i; }
bool Starter::Hold() { EXCEPT("MOCK"); return false; }
int Starter::RemoteShutdownGraceful(int i) { EXCEPT("MOCK"); return i; }
bool Starter::ShutdownGraceful() { EXCEPT("MOCK"); return false; }
int Starter::RemoteShutdownFast(int i) { EXCEPT("MOCK"); return i; }
bool Starter::ShutdownFast() { EXCEPT("MOCK"); return false; }

bool Starter::createTempExecuteDir() { EXCEPT("MOCK"); return false; }
bool Starter::jobWaitUntilExecuteTime() { EXCEPT("MOCK"); return false; }
bool Starter::skipJobImmediately() { EXCEPT("MOCK"); return false; }
bool Starter::removeDeferredJobs() { EXCEPT("MOCK"); return false; }
int Starter::jobEnvironmentReady() { EXCEPT("MOCK"); return -1; }
int Starter::jobEnvironmentCannotReady(int i, UnreadyReason const&) { EXCEPT("MOCK"); return i; }
void Starter::SpawnPreScript(int) { EXCEPT("MOCK"); }
void Starter::SkipJobs(int) { EXCEPT("MOCK"); }
bool Starter::allJobsDone() { EXCEPT("MOCK"); return false; }
int Starter::Reaper(int, int) { EXCEPT("MOCK"); }
bool Starter::transferOutput() { EXCEPT("MOCK"); return false; }
bool Starter::cleanupJobs() { EXCEPT("MOCK"); return false; }
void Starter::RecordJobExitStatus(int) { EXCEPT("MOCK"); }
bool Starter::removeTempExecuteDir(int&) { EXCEPT("MOCK"); return false; }


VolumeManager::Handle::~Handle() { }


JobInfoCommunicator::JobInfoCommunicator() { }
JobInfoCommunicator::~JobInfoCommunicator() { }

const char* JobInfoCommunicator::jobInputFilename() { EXCEPT("MOCK"); return NULL; }
const char* JobInfoCommunicator::jobOutputFilename() { EXCEPT("MOCK"); return NULL; }
const char* JobInfoCommunicator::jobErrorFilename() { EXCEPT("MOCK"); return NULL; }
bool JobInfoCommunicator::streamInput() { EXCEPT("MOCK"); return false; }
bool JobInfoCommunicator::streamOutput() { EXCEPT("MOCK"); return false; }
bool JobInfoCommunicator::streamError() { EXCEPT("MOCK"); return false; }
bool JobInfoCommunicator::streamStdFile(char const*) { EXCEPT("MOCK"); return false; }
const char* JobInfoCommunicator::jobIWD() { EXCEPT("MOCK"); return NULL; }
const char* JobInfoCommunicator::jobRemoteIWD() { EXCEPT("MOCK"); return NULL; }
const char* JobInfoCommunicator::origJobName() { EXCEPT("MOCK"); return NULL; }
ClassAd* JobInfoCommunicator::jobClassAd() { EXCEPT("MOCK"); return NULL; }
ClassAd* JobInfoCommunicator::jobExecutionOverlayAd() { EXCEPT("MOCK"); return NULL; }
ClassAd* JobInfoCommunicator::machClassAd() { EXCEPT("MOCK"); return NULL; }
int JobInfoCommunicator::jobCluster() const { EXCEPT("MOCK"); return -1; }
int JobInfoCommunicator::jobProc() const { EXCEPT("MOCK"); return -1; }
void JobInfoCommunicator::allJobsSpawned() { EXCEPT("MOCK"); }
bool JobInfoCommunicator::allJobsDone() { EXCEPT("MOCK"); return false; }
void JobInfoCommunicator::gotShutdownFast() { EXCEPT("MOCK"); }
void JobInfoCommunicator::gotShutdownGraceful() { EXCEPT("MOCK"); }
void JobInfoCommunicator::gotRemove() { EXCEPT("MOCK"); }
void JobInfoCommunicator::gotHold() { EXCEPT("MOCK"); }
bool JobInfoCommunicator::periodicJobUpdate(classad::ClassAd*) { EXCEPT("MOCK"); return false; }
bool JobInfoCommunicator::usingFileTransfer() { EXCEPT("MOCK"); return false; }
bool JobInfoCommunicator::updateX509Proxy(int, ReliSock*) { EXCEPT("MOCK"); return false; }
bool JobInfoCommunicator::initJobInfo() { EXCEPT("MOCK"); return false; }
void JobInfoCommunicator::checkForStarterDebugging() { EXCEPT("MOCK"); }
void JobInfoCommunicator::writeExecutionVisa(classad::ClassAd&) { EXCEPT("MOCK"); }
