/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <signal.h>
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

#if defined(AIX32)	// really SysV
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

