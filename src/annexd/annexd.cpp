#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "gahp-client.h"

#include <algorithm>

// Required by GahpServer::Startup().
char * GridmanagerScratchDir = NULL;

// Start a GAHP client.  Each GAHP client can only have one request outstanding
// at a time, and it distinguishes between them based only on the name of the
// command.  As a result, we want each annex (or bulk request) to have its own
// GAHP client, to make sure that the requests don't collide.  If two GAHP
// clients shared a 'name' parameter, they will share a GAHP server process;
// to make the RequestLimitExceeded stuff work right, we start a GAHP server
// for each {public key file, service URL} tuple.
EC2GahpClient *
startOneGahpClient() {
	dprintf( D_ALWAYS, "startOneGahpClient()\n" );

	std::string gahpName;
	formatstr( gahpName, "annex-%s@%s", "publicKeyFile.c_str()", "m_serviceURL.c_str()" );

	ArgList args;

	// Configure dprintf using this name.
	args.AppendArg( "-l" );
	args.AppendArg( "ANNEX_GAHP" );

	// FIXME: The EC2 GAHP should accept a command-line argument from us
	// instead of looking up EC2_GAHP_RATE_LIMIT.

	args.AppendArg( "-w" );
	int minWorkerCount = param_integer( "ANNEX_GAHP_WORKER_MIN_NUM", 1 );
	args.AppendArg( minWorkerCount );

	args.AppendArg( "-m" );
	int maxWorkerCount = param_integer( "ANNEX_GAHP_WORKER_MAX_NUM", 1 );
	args.AppendArg( maxWorkerCount );

	args.AppendArg( "-d" );
	char * gahp_debug = param( "ANNEX_GAHP_DEBUG" );
	if( gahp_debug == NULL ) {
		args.AppendArg( "D_ALWAYS" );
	} else {
		args.AppendArg( gahp_debug );
		free( gahp_debug );
	}

	char * gahp_path = param( "ANNEX_GAHP" );
	if( gahp_path == NULL ) {
		EXCEPT( "ANNEX_GAHP must be defined." );
	}

	EC2GahpClient * gahp = new EC2GahpClient( gahpName.c_str(), gahp_path, & args );
	free( gahp_path );

	gahp->setMode( GahpClient::normal );

	int gct = param_integer( "ANNEX_GAHP_CALL_TIMEOUT", 10 * 60 );
	gahp->setTimeout( gct );

	if( gahp->Startup() == false ) {
		EXCEPT( "Failed to start GAHP." );
	}

	return gahp;
}

// Implement the demo hack.  In the future, we would probably want to
// record the arguments we passed to bulk_start(), rather than assume
// that the ones fetched from configuration haven't changed since the
// last time we looked them up.
class BulkRequest : public Service {
	public:
		BulkRequest( EC2GahpClient * egc ) : gahp( egc ) { };
		virtual ~BulkRequest() { }

		void operator() () const;

	protected:
		EC2GahpClient * gahp;
};

// Implement the demo hack.  Based on the way the GAHP client detects new
// commands, we probably don't need to recreate the whole set of arguments
// each time, but that's an optimization / simplification that can wait.
void
BulkRequest::operator() () const {
	dprintf( D_ALWAYS, "BulkRequest::operator()()\n" );

	std::string serviceURL, public_key_file, secret_key_file;
	param( serviceURL, "ANNEX_DEFAULT_SERVICE_URL" );
	param( public_key_file, "ANNEX_DEFAULT_PUBLIC_KEY_FILE" );
	param( secret_key_file, "ANNEX_DEFAULT_SECRET_KEY_FILE" );

	std::string client_token, spot_price, target_capacity;
	std::string iam_fleet_role, allocation_strategy;
	param( spot_price, "ANNEX_DEFAULT_SPOT_PRICE" );
	param( target_capacity, "ANNEX_DEFAULT_TARGET_CAPACITY" );
	param( iam_fleet_role, "ANNEX_DEFAULT_IAM_FLEET_ROLE" );
	param( allocation_strategy, "ANNEX_DEFAULT_ALLOCATION_STRATEGY" );

	std::vector< std::string> launch_configurations;
	std::string lc;
	std::string adlci;
	std::string adlc = "ANNEX_DEFAULT_LAUNCH_CONFIGURATION_";
	for( int i = 0; ; ++i ) {
		lc.clear();
		formatstr( adlci, "%s%d", adlc.c_str(), i );
		if(! param( lc, adlci.c_str() )) { break; }
		lc.erase( std::remove( lc.begin(), lc.end(), '\r' ), lc.end() );
		lc.erase( std::remove( lc.begin(), lc.end(), '\n' ), lc.end() );
		launch_configurations.push_back( lc );
	}

	int rc;
	std::string errorCode;
	std::string bulkRequestID;

	dprintf( D_ALWAYS, "Calling bulk_start()...\n" );
	rc = gahp->bulk_start(
				serviceURL, public_key_file, secret_key_file,
				client_token, spot_price, target_capacity,
				iam_fleet_role, allocation_strategy,
				launch_configurations,
				bulkRequestID, errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We should exit here the first time.
		return;
	}

	if( rc == 0 ) {
		dprintf( D_ALWAYS, "Bulk start request ID: %s\n", bulkRequestID.c_str() );
	} else if( errorCode == "NEED_CHECK_BULK_START" ) {
		// FIXME: We should probably retry, instead.
		dprintf( D_ALWAYS, "Bulk start request failed but may have left a Spot Fleet behind.\n" );
	} else {
		std::string gahpErrorString = gahp->getErrorString();
		dprintf( D_ALWAYS, "Bulk start request failed: '%s' (%d): '%s'.\n", errorCode.c_str(), rc, gahpErrorString.c_str() );
	}
}

// Implement the demo hack.
void
createOneAnnex() {
	dprintf( D_ALWAYS, "createOneAnnex()\n" );

	// Create the GAHP client.  The first time we call a GAHP client function,
	// it will send that command to the GAHP server, but the GAHP server may
	// take some time to get the result.  The GAHP client will fire the
	// notification timer when the result is ready, and we can get it by
	// calling the GAHP client function a second time with the same arguments.
	EC2GahpClient * gahp = startOneGahpClient();

	// Create a timer for the gahp to fire when it gets a result.  Use it,
	// as long as we have to create it anyway, to make the initial bulk
	// request.  We must use TIMER_NEVER to ensure that the timer hasn't
	// been reaped when the GAHP client needs to fire it.
	BulkRequest * br = new BulkRequest( gahp );
	int gahpNotificationTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		(void (Service::*)()) & BulkRequest::operator(),
		"BulkRequest::operator()()", br );
	gahp->setNotificationTimerId( gahpNotificationTimer );
}

void
main_init( int /* argc */, char ** /* argv */ ) {
	dprintf( D_ALWAYS, "main_init()\n" );

	daemonCore->Register_Timer( 0, 0, & createOneAnnex, "createOneAnnex()" );
}

void
main_config() {
	dprintf( D_ALWAYS, "main_config()\n" );
}

void
main_shutdown_fast() {
	dprintf( D_ALWAYS, "main_shutdown_fast()\n" );
	DC_Exit( 0 );
}

void
main_shutdown_graceful() {
	dprintf( D_ALWAYS, "main_shutdown_graceful()\n" );
	DC_Exit( 0 );
}

void
main_pre_dc_init( int /* argc */, char ** /* argv */ ) {
	dprintf( D_ALWAYS, "main_pre_dc_init()\n" );
}

void
main_pre_command_sock_init() {
	dprintf( D_ALWAYS, "main_pre_command_sock_init()\n" );
}

int
main( int argc, char ** argv ) {
	set_mySubSystem( "ANNEXD", SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}
