#include "condor_common.h"
#include "condor_debug.h"

#include "../condor_shadow.V6.1/shadow.h"
#include "guidance.h"
#include "../condor_shadow.V6.1/pseudo_ops.h"


class MockShadow : public BaseShadow {
    public:
        MockShadow( ClassAd * j );
        virtual ~MockShadow();

        virtual void init(ClassAd*, const char *, const char *) { }
        virtual void spawn() { }
        virtual void reconnect() { }
        virtual bool supportsReconnect() { return false; }
        virtual void gracefulShutDown() { }
        virtual void getFileTransferStats(classad::ClassAd &, classad::ClassAd &) { }
        virtual void getFileTransferStatus(FileTransferStatus &, FileTransferStatus &) { }
        virtual int getExitReason() { return -1; }
        virtual bool claimIsClosing() { return false; }
        virtual int handleJobRemoval( int ) { return -1; }
        virtual int JobSuspend( int ) { return -1; }
        virtual int JobResume( int ) { return -1; }
        virtual int updateFromStarterClassAd( classad::ClassAd * ) { return -1; }
        virtual int64_t getImageSize( int64_t &, int64_t &, int64_t & ) { return -1; }
        virtual int64_t getDiskUsage() { return -1; }
        virtual rusage getRUsage() { return {}; }
        virtual bool setMpiMasterInfo(char*) { return false; }
        virtual void recordFileTransferStateChanges( classad::ClassAd *, classad::ClassAd * ) { }
        virtual void cleanUp( bool ) { }
        virtual bool exitedBySignal() { return false; }
        virtual int exitSignal() { return -1; }
        virtual int exitCode() { return -1; }
        virtual void resourceBeganExecution( RemoteResource * ) { }
        virtual void resourceDisconnected( RemoteResource * ) { }
        virtual void resourceReconnected( RemoteResource * ) { }
        virtual void logDisconnectedEvent( const char * ) { }
        virtual void logReconnectedEvent() { }
        virtual void logReconnectFailedEvent( const char * ) { }
        virtual void emailTerminateEvent( int, update_style_t ) { }
};


MockShadow::MockShadow( ClassAd * j ) {
    jobAd = j;
}

MockShadow::~MockShadow() { }


BaseShadow::BaseShadow() { }
BaseShadow::~BaseShadow() { }


void BaseShadow::config() { }
void BaseShadow::shutDown( int, const char *, int, int ) { }
void BaseShadow::shutDownFast( int, const char *, int, int ) { }
bool BaseShadow::getMachineName( std::string & ) { return false; }
void BaseShadow::holdJob( const char *, int, int ) { }
void BaseShadow::removeJob( const char * ) { }
bool BaseShadow::updateJobAttr( const char *, const char *, bool ) { return false; }
bool BaseShadow::updateJobAttr( const char *, int64_t, bool ) { return false; }
void BaseShadow::watchJobAttr( const std::string & ) { }
void BaseShadow::evictJob( int, const char *, int, int ) { }
void BaseShadow::requeueJob( const char * ) { }
void BaseShadow::terminateJob( update_style_t ) { }
void BaseShadow::holdJobAndExit( const char *, int, int ) { }


BaseShadow * Shadow = NULL;
RemoteResource * thisRemoteResource = NULL;


void
RemoteResource::recordActivationExitExecutionTime(time_t) { }


bool
BaseShadow::updateJobInQueue(update_t) { return true; }


int
main( int, char ** ) {
    ClassAd mockJobAd;
    mockJobAd.InsertAttr(ATTR_JOB_IWD, ".");
// without attr, rv = -2, w/o known = -3, -4 if bad diagnostic,
// but we absolutely can't crash?
    Shadow = new MockShadow( & mockJobAd );

    ClassAd eventAd;
    eventAd.InsertAttr(ATTR_EVENT_TYPE, ETYPE_DIAGNOSTIC_RESULT);

    int rv = pseudo_event_notification( eventAd );
    fprintf( stderr, "%d\n", rv );
    return rv;
}
