/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_debug.h"
#include "starter.h"

extern int errno;
pid_t	StarterPid;

typedef struct {
	EVENT	ev;
	char	*str;
	char	response;
} MENU_ITEM;

MENU_ITEM	Menu[] = {
	{ SUSPEND,		 	"S - Suspend",									'S'	},
	{ CONTINUE,		 	"C - Continue",									'C'	},
	{ ALARM,		 	"A - Alarm",									'A'	},
	{ DIE,		 		"D - Die (don't create ckpt, don't send one)",	'D'	},
	{ VACATE,		 	"V - Vacate (send ckpt, but don't create one)",	'V'	},
	{ CKPT_and_VACATE,	"N - Nice Vacate (create new ckpt, send it)",	'N'	},
	{ NO_EVENT,			NULL,											'\0' }
};

EVENT	get_command();
void	usage( char * );
void	send_sig( int sig );
pid_t	get_starter_pid( const char * );
void	check_starter();
int		contains( const char *, const char * );
pid_t	get_pid( const char * );


void
usage( char *my_name )
{
	fprintf( stderr, "Usage: %s [starter_pid]\n", my_name );
	exit( 1 );
}



int
main( int argc, char *argv[] )
{
	EVENT	command;
		
	if( (argc != 1) && (argc != 2) ) {
		usage( argv[0] );
	}

	if( argc == 2 ) {
		StarterPid = (pid_t)atoi( argv[1] );
	} else {
		StarterPid = get_starter_pid( argv[0] );
	}

	while( StarterPid ) {
		check_starter();
		command = get_command();
		switch( command ) {
			case SUSPEND:
				send_sig( SIGUSR1 );
				break;
			case CONTINUE:
				send_sig( SIGCONT );
				break;
			case VACATE:
				send_sig( SIGTSTP );
				break;
			case ALARM:
				send_sig( SIGALRM );
				break;
			case DIE:
				send_sig( SIGINT );
				break;
			case CKPT_and_VACATE:
				send_sig( SIGUSR2 );
				break;
			case NO_EVENT:
				exit( 0 );
			default:
				printf( "Unknown command (%d)\n", command );
		}
	}
	return 0;
}

EVENT
get_command()
{
	MENU_ITEM	*ptr;
	char		ch;
	char		buf[1024];

	for( ptr=Menu; ptr->str; ptr++ ) {
		printf( "%s\n", ptr->str );
	};
	printf( "(Send EOF to quit)\n" );


	for(;;) {
		if( fgets(buf,sizeof(buf),stdin) == NULL ) {
			return NO_EVENT;
		}

		ch = toupper( (int)buf[0] );
		for( ptr=Menu; ptr->str; ptr++ ) {
			if( ch == ptr->response ) {
				return ptr->ev;
			}
		}
		printf( "%c - Unknown command\n", ch );
	}

}

void
send_sig( int sig )
{
	if( sig != SIGCONT ) {
		if( kill(StarterPid,SIGCONT) < 0 ) {
			if( errno == ESRCH ) {
				StarterPid = 0;
				return;
			}
			fprintf( stderr, "Can't send SIGCONT to process %d", StarterPid );
			perror( " " );
			exit( 1 );
		}
	}
	printf( "Sent signal SIGCONT to process %d\n", StarterPid );

	if( kill(StarterPid,sig) < 0 ) {
		if( errno == ESRCH ) {
			StarterPid = 0;
			return;
		}
		fprintf( stderr, "Can't send signal %d to process %d", sig, StarterPid );
		perror( " " );
		exit( 1 );
	}
	printf( "Sent signal %d to process %d\n", sig, StarterPid );
}

const char	* const PROG_NAME = "condor_starter";

pid_t
get_starter_pid( const char *my_name)
{
	char	buf[1024];
	FILE	*fp;
	char	cmd[ 1024];
	pid_t	answer = 0;

#if defined(AIX32) || defined(HPUX)	// really SysV
	sprintf( cmd, "ps -ef | egrep %s", PROG_NAME );
#else
	sprintf( cmd, "ps -ax | egrep %s", PROG_NAME );
#endif

	if( (fp=popen(cmd,"r")) == NULL ) {
		fprintf( stderr, "Can't execute \"%s\"\n", cmd );
		exit( 1 );
	}
	while( fgets(buf,sizeof(buf),fp) ) {
		if( contains(buf,"egrep") ) {
			continue;
		}
		if( contains(buf,my_name) ) {
			continue;
		}
		if( contains(buf,PROG_NAME) ) {
			if( answer ) {
				fprintf( stderr,
					"Multiple processes named \"%s\" exist\n", PROG_NAME
				);
				exit( 1 );
			}
			answer = get_pid(buf);
		}
	}
	(void)pclose( fp );
	if( answer ) {
		printf( "Process \"%s\" PID is %d\n", PROG_NAME, answer );
		return answer;
	} else {
		printf( "Can't find \"%s\"\n", PROG_NAME );
		return 0;
	}
}

void
check_starter()
{
	if( kill(StarterPid,0) < 0 ) {
		if( errno == ESRCH ) {
			printf( "Starter %d exited\n", StarterPid );
			exit( 0 );
		} else {
			printf( "Can't send signal to starter %d\n", StarterPid );
			exit( 1 );
		}
	}
}

/*
  Return TRUE if the string contains the target, and false otherwise.
*/
int
contains( const char *str, const char *targ )
{
	int		len = strlen( targ );
	const char	*ptr;

	for( ptr=str; *ptr; ptr++ ) {
		if( strncmp(ptr,targ,len) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}

pid_t
get_pid( const char *str )
{
	const char	*ptr;

	for( ptr=str; *ptr; ptr++ ) {
		if( isdigit(*ptr) ) {
			return atoi(ptr);
		}
	}
	return 0;
}

