#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <climits>
#include <time.h>


unsigned next_size = 0;


void
allocate_gib(long count) {
	fprintf( stderr, "(%d)  allocate_gib(%ld)\n", getpid(), count );
	size_t size =  1024 * 1024 * 1024 * count;

	u_int64_t * lost_and_forgotten = (u_int64_t *)malloc( size );
	if( lost_and_forgotten == NULL ) {
		fprintf( stderr, "malloc(%zu) failed, aborting.\n", size );
		exit( -1 );
	}

	for( size_t i = 0; i < (size / sizeof(u_int64_t)); i += (4096 / sizeof(u_int64_t)) ) {
		lost_and_forgotten[i] = 0xDEADBEEF;
	}
}


int
spawn_child( long second_usage ) {
	int childPID = fork();
	if( childPID == 0 ) {
		allocate_gib( second_usage );
		sleep( UINT_MAX );
		exit( 0 );
	} else {
		return childPID;
	}
}


void
sig_usr_handler(int signal) {
	if( signal == SIGUSR1 ) {
		next_size = 1;
	} else if( signal == SIGUSR2 ) {
		next_size = 2;
	}
}

int
print_usage( const char * argv0 ) {
	fprintf( stderr, "Usage: %s <memory-usage-1> <memory-usage-2> <cycle-interval> <iterations>\n", argv0 );
	return 1;
}


int
main( int argc, char ** argv ) {
	if( argc < 5 ) {
		return print_usage(argv[0]);
	}


	// Argument parsing.

	char * endptr = NULL;
	long first_usage = strtol( argv[1], & endptr, 10 );
	if( errno != 0 ) {
		fprintf( stderr, "Parsing memory-usage-1: %s (%d)\n", strerror(errno), errno );
		return print_usage(argv[0]);
	}
	if( * endptr != '\0' ) {
		fprintf( stderr, "Parsing memory-usage-1: '%s' must be entirely a number\n", argv[1] );
		return print_usage(argv[0]);
	}

	long second_usage = strtol( argv[2], & endptr, 10 );
	if( errno != 0 ) {
		fprintf( stderr, "Parsing memory-usage-2: %s (%d)\n", strerror(errno), errno );
		return print_usage(argv[0]);
	}
	if( * endptr != '\0' ) {
		fprintf( stderr, "Parsing memory-usage-2: '%s' must be entirely a number\n", argv[2] );
		return print_usage(argv[0]);
	}

	long cycle_interval = strtol( argv[3], & endptr, 10 );
	if( errno != 0 ) {
		fprintf( stderr, "Parsing cycle-interval: %s (%d)\n", strerror(errno), errno );
		return print_usage(argv[0]);
	}
	if( * endptr != '\0' ) {
		fprintf( stderr, "Parsing cycle-interval: '%s' must be entirely a number\n", argv[3] );
		return print_usage(argv[0]);
	}

	long iterations = strtol( argv[4], & endptr, 10 );
	if( errno != 0 ) {
		fprintf( stderr, "Parsing iterations: %s (%d)\n", strerror(errno), errno );
		return print_usage(argv[0]);
	}
	if( * endptr != '\0' ) {
		fprintf( stderr, "Parsing cycle-interval: '%s' must be entirely a number\n", argv[4] );
		return print_usage(argv[0]);
	}


	// Register signal handlers.
	struct sigaction sa_old;
	struct sigaction sa_new;

	sa_new.sa_handler = sig_usr_handler;
	sigemptyset(& sa_new.sa_mask);
	sigaddset(& sa_new.sa_mask, SIGALRM);
	sa_new.sa_flags = 0;
	sigaction( SIGUSR1, &sa_new, &sa_old );
	sigaction( SIGUSR2, &sa_new, &sa_old );


	// Main loop.
	next_size = 1;
	allocate_gib( first_usage );
	for( long i = 0; i < (iterations * 2); ++i ) {
		fprintf( stderr, "%ld\n", time(NULL) );

		if( next_size == 1 ) {
			next_size = 2;
			sleep( cycle_interval );
			continue;
		}

		if( next_size == 2 ) {
			next_size = 1;
			int childPID = spawn_child( second_usage );
			sleep( cycle_interval );
			kill( childPID, SIGKILL );
			continue;
		}
	}


	fprintf( stderr, "%ld\n", time(NULL) );
	return 0;
}
