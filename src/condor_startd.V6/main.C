/*
 * Main routine and function for the startd.
 */

#define _STARTD_NO_DECLARE_GLOBALS 1
#include "startd.h"
static char *_FileName_ = __FILE__;

// Define global variables

// Resource manager
ResMgr*	resmgr;			// Pointer to the resource manager object

// Polling variables
int	polling_interval;	// Interval for polling when there are resources in use
int	update_interval;	// Interval to update CM

// Paths
char*	exec_path = NULL;
char*	kbd_dev = NULL;
char*	mouse_dev = NULL;

// Starter paths
char*	PrimaryStarter = NULL;
char*	AlternateStarter[MAX_STARTERS];
char*	RealStarter = NULL;

// Hosts
char*	negotiator_host = NULL;
char*	collector_host = NULL;
char*	condor_view_host = NULL;
char*	accountant_host = NULL;

// Others
int		match_timeout;		// How long you're willing to be
							// matched before claimed 
int		killing_timeout;	// How long you're willing to be in
							// preempting/killing before you drop the
							// hammer on the starter
int		last_x_event = 0;	// Time of the last x event

char*	mySubSystem = "STARTD";

/*
 * Prototypes of static functions.
 */

void usage( const char *s );
int main_init( int argc, char* argv[] );
int init_params(int);
int main_config();
int main_shutdown_fast();
int main_shutdown_graceful();
extern "C" int do_cleanup();
void reaper_loop();
int	handle_dc_sigchld( Service*, int );
int	shutdown_sigchld( Service*, int ); 
int	check_free();

int
main_init( int, char* [] )
{
		// Seed the random number generator for capability generation.
	set_seed( 0 );

		// If we EXCEPT, don't leave any starters lying around.
	_EXCEPT_Cleanup = do_cleanup;

	init_params(1);		// The 1 indicates that this is the first time

	resmgr = new ResMgr;
	resmgr->walk( Resource::init_classad );

		// Do a little sanity checking and cleanup
	check_perms();
	cleanup_execute_dir();

		// register daemoncore stuff

		// Commands
		// RegisterCommand( int cmd, char* "name_of_command",
		// 					(CommandHandler)handler_function,
		//					char* "name_of_handler_function" );

		// These commands all read the capability off the wire, find
		// the resource with that capability, and call appropriate
		// action on that resource.  Plus, all of these commands only
		// make sense when we're in the claimed state.  So, we can
		// handle them all with a common handler.
	daemonCore->Register_Command( ALIVE, "ALIVE", 
								  (CommandHandler)command_handler,
								  "command_handler" );
	daemonCore->Register_Command( RELEASE_CLAIM, "RELEASE_CLAIM", 
								  (CommandHandler)command_handler,
								  "command_handler" );
	daemonCore->Register_Command( DEACTIVATE_CLAIM,
								  "DEACTIVATE_CLAIM",  
								  (CommandHandler)command_handler,
								  "command_handler" );
	daemonCore->Register_Command( DEACTIVATE_CLAIM_FORCIBLY, 
								  "DEACTIVATE_CLAIM_FORCIBLY", 
								  (CommandHandler)command_handler,
								  "command_handler" );
	daemonCore->Register_Command( PCKPT_FRGN_JOB, "PCKPT_FRGN_JOB", 
								  (CommandHandler)command_handler,
								  "command_handler" );
	daemonCore->Register_Command( REQ_NEW_PROC, "REQ_NEW_PROC", 
								  (CommandHandler)command_handler,
								  "command_handler" );

		// These commands are special and need their own handlers
	daemonCore->Register_Command( ACTIVATE_CLAIM, "ACTIVATE_CLAIM",
								  (CommandHandler)command_activate_claim,
								  "command_activate_claim", NULL, WRITE );
	daemonCore->Register_Command( REQUEST_CLAIM, "REQUEST_CLAIM", 
								  (CommandHandler)command_request_claim,
								  "command_request_claim", NULL, WRITE );
	daemonCore->Register_Command( PCKPT_ALL_JOBS, "PCKPT_ALL_JOBS", 
								  (CommandHandler)command_pckpt_all,
								  "command_pckpt_all", NULL, WRITE );
	daemonCore->Register_Command( X_EVENT_NOTIFICATION,
								  "X_EVENT_NOTIFICATION",
								  (CommandHandler)command_x_event,
								  "command_x_event", NULL, WRITE );
	daemonCore->Register_Command( VACATE_ALL_CLAIMS,
								  "VACATE_ALL_CLAIMS",
								  (CommandHandler)command_vacate,
								  "command_vacate", NULL, WRITE);
	daemonCore->Register_Command( GIVE_STATE,
								  "GIVE_STATE",
								  (CommandHandler)command_give_state,
								  "command_give_state", NULL, READ );
	daemonCore->Register_Command( GIVE_CLASSAD,
								  "GIVE_CLASSAD",
								  (CommandHandler)command_give_classad,
								  "command_give_classad", NULL, READ );
	daemonCore->Register_Command( GIVE_REQUEST_AD, "GIVE_REQUEST_AD",
								  (CommandHandler)command_give_request_ad,
								  "command_give_request_ad", NULL, READ );

		// This command should be registered with negotiator permission
	daemonCore->Register_Command( MATCH_INFO, "MATCH_INFO",
								  (CommandHandler)command_match_info,
								  "command_match_info", NULL, NEGOTIATOR );

		// Signals 
	daemonCore->Register_Signal( DC_SIGCHLD, "DC_SIGCHLD", 
								 (SignalHandler)handle_dc_sigchld,
								 "handle_dc_sigchld" );

		// Reapers (not yet, since we're doing it outside DC)

		// Walk through all resources and start an update timer for
		// each one.  
	resmgr->walk( Resource::start_update_timer );

		// Evaluate the state of all resources and update CM
	resmgr->walk( Resource::eval_and_update );

	return TRUE;
}


int
main_config()
{
		// Re-read config file, and rebuild ads for each resource.  
	resmgr->walk( Resource::init_classad );  
		// Re-read config file for startd-wide stuff.
	init_params(0);
		// Re-evaluate and update the CM for each resource.
	resmgr->walk( Resource::eval_and_update );
	return TRUE;
}


int
init_params( int first_time)
{
	char *tmp, buf[1024];
	int i;
	struct hostent *hp;

	if( exec_path ) {
		free( exec_path );
	}
	exec_path = param( "EXECUTE" );
	if( exec_path == NULL ) {
		EXCEPT( "No Execute file specified in config file" );
	}

#if defined(WIN32)
	// switch delimiter char in exec path on WIN32
	for (i=0; exec_path[i]; i++) {
		if (exec_path[i] == '/') {
			exec_path[i] = '\\';
		}
	}
#endif

	// make sure we have the canonical name for the negotiator host
	tmp = param( "NEGOTIATOR_HOST" );
	if( tmp == NULL ) {
		EXCEPT("No Negotiator host specified in config file");
	}
	if( (hp = gethostbyname(tmp)) == NULL ) {
		EXCEPT( "gethostbyname failed for negotiator host" );
	}
	free( tmp );
	if( negotiator_host ) {
		free( negotiator_host );
	}
	negotiator_host = strdup( hp->h_name );

	if( collector_host ) {
		free( collector_host );
	}
	collector_host = param( "COLLECTOR_HOST" );
	if( collector_host == NULL ) {
		EXCEPT( "No Collector host specified in config file" );
	}

	if( condor_view_host ) {
		free( condor_view_host );
	}
	condor_view_host = param( "CONDOR_VIEW_HOST" );

	tmp = param( "POLLING_INTERVAL" );
	if( tmp == NULL ) {
		polling_interval = 5;
	} else {
		polling_interval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "UPDATE_INTERVAL" );
	if( tmp == NULL ) {
		update_interval = 300;
	} else {
		update_interval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "STARTD_DEBUG" );
	if( tmp == NULL ) {
		EXCEPT( "STARTD_DEBUG not defined in config file" );
	}
	free( tmp );

	if( PrimaryStarter ) {
		free(PrimaryStarter);
	}
	PrimaryStarter = param( "STARTER" );
	if( PrimaryStarter == NULL ) {
		EXCEPT("No Starter file specified in config file\n");
	}
	RealStarter = PrimaryStarter;

	for( i = 0; i < MAX_STARTERS; i++ ) {
		sprintf( buf, "ALTERNATE_STARTER_%d", i );
		if( ! first_time && AlternateStarter[i] ) { 
			free( AlternateStarter[i] );
		}
		AlternateStarter[i] = param( buf );
	}

	if( accountant_host ) {
		free( accountant_host );
	}
	accountant_host = param("ACCOUNTANT_HOST");

	tmp = param( "MATCH_TIMEOUT" );
	if( !tmp ) {
		match_timeout = 120;
	} else {
		match_timeout = atoi( tmp );
		free( tmp );
	}

	tmp = param( "KILLING_TIMEOUT" );
	if( !tmp ) {
		killing_timeout = 15;
	} else {
		killing_timeout = atoi( tmp );
		free( tmp );
	}

	if( kbd_dev ) {
		free( kbd_dev );
	}
	kbd_dev = param("KBD_DEVICE");

	if( mouse_dev ) {
		free( mouse_dev );
	}
	mouse_dev = param("MOUSE_DEVICE");

	return TRUE;
}


void
startd_exit() 
{
	dprintf( D_FULLDEBUG, "About to send final update to the central manager\n" );
	resmgr->final_update();
	dprintf( D_ALWAYS, "All resources are free, exiting.\n" );
	exit(0);
}

int
main_shutdown_fast()
{
		// If the machine is free, we can just exit right away.
	check_free();

	daemonCore->Cancel_Signal( DC_SIGCHLD );
	daemonCore->Register_Signal( DC_SIGCHLD, "DC_SIGCHLD", 
								 (SignalHandler)shutdown_sigchld,
								 "shutdown_sigchld" );

		// Quickly kill all the starters that are running
	resmgr->walk( Resource::kill_claim );

	daemonCore->Register_Timer( 0, 5, 
								(TimerHandler)check_free,
								 "check_free" );
	return TRUE;
}


int
main_shutdown_graceful()
{
		// If the machine is free, we can just exit right away.
	check_free();

	daemonCore->Cancel_Signal( DC_SIGCHLD );
	daemonCore->Register_Signal( DC_SIGCHLD, "DC_SIGCHLD", 
								 (SignalHandler)shutdown_sigchld,
								 "shutdown_sigchld" );

		// Release all claims, active or not
	resmgr->walk( Resource::release_claim );

	daemonCore->Register_Timer( 0, 5, 
								(TimerHandler)check_free,
								 "check_free" );
	return TRUE;
}


int
handle_dc_sigchld( Service*, int )
{
	reaper_loop();
	return TRUE;
}


void
reaper_loop()
{
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	int pid, status;
	Resource* rip;

	while( (pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0 ) {
		if (WIFSTOPPED(status)) {
			dprintf(D_ALWAYS, "pid %d stopped.\n", pid);
			continue;
		}
		if( WIFSIGNALED(status) ) {
			dprintf(D_ALWAYS, "pid %d died on signal %d\n",
					pid, WTERMSIG(status));
		} else {
			dprintf(D_ALWAYS, "pid %d exited with status %d\n",
					pid, WEXITSTATUS(status));
		}
		rip = resmgr->get_by_pid(pid);
		if( rip == NULL ) {
			continue;
		}		
		rip->starter_exited();
	}
#endif
}


int
shutdown_sigchld( Service*, int )
{
	reaper_loop();
	check_free();
	return TRUE;
}


int
do_cleanup()
{
	static int already_excepted = FALSE;

	if ( already_excepted == FALSE ) {
		already_excepted = TRUE;
		main_shutdown_fast();  // this will exit if successful
	}
	return TRUE;
}


int
check_free()
{
	if( ! resmgr->in_use() ) {
		startd_exit();
	} 
	return TRUE;
}
