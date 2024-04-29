#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "dc_coroutines.h"
using namespace condor;


//
// The actual test routine(s).  test_main() should return non-zero if and only
// if the test should exit immediately; otherwise, the test will  go into the
// daemon core event loop.  test_main()'s copies of argc and argv will have
// already been altered by daemon core.
//


#define SHORT_TIMEOUT 5


unsigned int test03_sad_global_counter = 0;

dc::void_coroutine
spawn_test03_subtest() {
{
	dc::AwaitableDeadlineReaper adr;

	int pid = -1;
	OptionalCreateProcessArgs create_process_opts;
	create_process_opts.reaperID(adr.reaper_id());
	std::set<int> a, b, c;


	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "exit 0" },
		create_process_opts
	);
	a.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );

	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "exit 0" },
		create_process_opts
	);
	a.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );

	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "exit 0" },
		create_process_opts
	);
	a.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );

	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 1; exit 1" },
		create_process_opts
	);
	b.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );

	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 1; exit 1" },
		create_process_opts
	);
	b.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );

	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 1; exit 1" },
		create_process_opts
	);
	b.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );


	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 300; exit 0" },
		create_process_opts
	);
	c.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );

	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 300; exit 0" },
		create_process_opts
	);
	c.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );

	pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 300; exit 0" },
		create_process_opts
	);
	c.insert( pid );
	adr.born( pid, SHORT_TIMEOUT );


	std::set<int> d;
	while( adr.living() ) {
		auto [pid, timed_out, status] = co_await( adr );

		if( a.contains(pid) ) {
			ASSERT(! timed_out);
			ASSERT( WIFEXITED(status) );
			ASSERT( WEXITSTATUS(status) == 0 );
		} else if( b.contains(pid) ) {
			ASSERT(! timed_out);
			ASSERT( WIFEXITED(status) );
			ASSERT( WEXITSTATUS(status) == 1 );
		} else if( c.contains(pid) ) {
			ASSERT(timed_out);
			kill( pid, SIGKILL );

			c.erase(pid);
			d.insert(pid);
		} else if( d.contains(pid) ) {
			ASSERT(! timed_out);
			ASSERT( WIFSIGNALED(status) );
			ASSERT( WTERMSIG(status) == SIGKILL );
		} else {
			EXCEPT("AwaitableDeadlineReaper returned unknown PID!");
		}
	}
}


	++test03_sad_global_counter;
	if( test03_sad_global_counter == 3 ) {
		ASSERT(0 == daemonCore->countTimersByDescription("AwaitableDeadlineReaper::timer"));
		ASSERT(0 == daemonCore->numRegisteredReapers());
		dprintf( D_TEST, "Passed test 03.\n" );
		DC_Exit( 0 );
	}
}


void
test_03() {

	spawn_test03_subtest();
	spawn_test03_subtest();
	spawn_test03_subtest();

}


dc::void_coroutine
test_02() {
{
	dc::AwaitableDeadlineReaper adr;


	OptionalCreateProcessArgs create_process_opts;
	int pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 300; exit 0" },
		create_process_opts.reaperID(adr.reaper_id())
	);
	adr.born( pid, SHORT_TIMEOUT );

	{
		auto [the_pid, timed_out, status] = co_await( adr );
		ASSERT(the_pid == pid);
		ASSERT(timed_out);
		kill( pid, SIGKILL );
	}

	{
		auto [the_pid, timed_out, status] = co_await( adr );
		ASSERT(the_pid == pid);
		ASSERT(! timed_out);
		ASSERT( WIFSIGNALED(status) );
		ASSERT( WTERMSIG(status) == SIGKILL );
	}
}


	ASSERT(0 == daemonCore->countTimersByDescription("AwaitableDeadlineReaper::timer"));
	ASSERT(0 == daemonCore->numRegisteredReapers());
	dprintf( D_TEST, "Passed test 02.\n" );

	test_03();
}


dc::void_coroutine
test_01() {
{
	dc::AwaitableDeadlineReaper adr;


	OptionalCreateProcessArgs create_process_opts;
	int pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "/bin/sleep 1; exit 0" },
		create_process_opts.reaperID(adr.reaper_id())
	);
	adr.born( pid, SHORT_TIMEOUT );


	while( adr.living() ) {
		auto [the_pid, timed_out, status] = co_await( adr );
		ASSERT(the_pid == pid);
		ASSERT(! timed_out);
		ASSERT( WIFEXITED(status) );
		ASSERT( WEXITSTATUS(status) == 0 );
	}
}


	ASSERT(0 == daemonCore->countTimersByDescription("AwaitableDeadlineReaper::timer"));
	ASSERT(0 == daemonCore->numRegisteredReapers());
	dprintf( D_TEST, "Passed test 01.\n" );

	test_02();
}


dc::void_coroutine
test_00() {
{
	dc::AwaitableDeadlineReaper adr;


	OptionalCreateProcessArgs create_process_opts;
	int pid = daemonCore->CreateProcessNew(
		"/bin/bash",
		{ "/bin/bash", "-c", "exit 0" },
		create_process_opts.reaperID(adr.reaper_id())
	);
	adr.born( pid, SHORT_TIMEOUT );


	while( adr.living() ) {
		auto [the_pid, timed_out, status] = co_await( adr );
		ASSERT(the_pid == pid);
		ASSERT(! timed_out);
		ASSERT( WIFEXITED(status) );
		ASSERT( WEXITSTATUS(status) == 0 );
	}
}


	ASSERT(0 == daemonCore->countTimersByDescription("AwaitableDeadlineReaper::timer"));
	ASSERT(0 == daemonCore->numRegisteredReapers());
	dprintf( D_TEST, "Passed test 00.\n" );

	test_01();
}


int
test_main( int /* argv */, char ** /* argv */ ) {
	// Initialize the config settings.
	config_ex( CONFIG_OPT_NO_EXIT );

	// Allow uid switching if root.
	set_priv_initialize();

	// Initialize the config system.
	config();


	//
	// Test AwaitableDeadlineReaper.
	//
	test_00();

	// Wait for the test(s) to finish running.
	return 0;
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
