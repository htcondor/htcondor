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
** Author:  Dhrubajyoti Borthakur
**
*/ 


/*********************************************************************
* Find if the jobqueue has got locked, ( send mail if necessary )
*********************************************************************/


#define _POSIX_SOURCE

#include <time.h>
#include <signal.h>
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_config.h"
#include "condor_jobqueue.h"

char *my_hostname();

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

	// Define this to check for memory leaks

char		*Spool;				// dir for condor job queue
char		*CondorAdmin;		// who to send mail to in case of trouble
char		*MyName;			// name this program was invoked by
char		*Mail;              // mailer to use
BOOLEAN		MailFlag;			// true if we should send mail about problems


	// prototypes of local interest
void usage();
void send_mail_file();
void init_params();
void check_spool_dir(int op);
void produce_output();

extern "C" 
{
int SetSyscalls( int foo ) { return foo; }
void install_sig_handler();
}

// Tell folks how to use this program.
// r : READER's lock
// w : WRITER's lock
// m : send mail to ADMIN
void
usage()
{
	fprintf( stderr, "Usage: %s [-r|-w] [-m]\n", MyName );
	exit( 1 );
}

int
main( int argc, char *argv[] )
{
	char *str;
	int op = -1;

		// Initialize things
	MyName = argv[0];
	config( MyName, (CONTEXT *)0 );
	init_params();


		// Parse command line arguments
	for( argv++; *argv; argv++ ) 
	{
		if( (*argv)[0] == '-' ) 
		{
			switch( (*argv)[1] ) 
			{
			  case 'm':
				MailFlag = TRUE;
				break;

			  case 'r':
				op = READER;
				break;

			  case 'w':
				op = WRITER;
				break;

			  default:
				usage();

			}
		} else usage();
	}
	if ( (op != READER) &&( op != WRITER)) usage();

	// set alarm for 5 minutes
	install_sig_handler(SIGALRM, produce_output);
	alarm(5*60);

	// Do the jobQueue checking
	check_spool_dir(op);

	// cancel alarm 
	alarm(0);

	exit(0);
}

/*
	This function sends mail to condor-admin 
*/
void
produce_output()
{
	char	*str;
	FILE	*mailer;
	char	cmd[ 1024 ];

	sprintf( cmd, "%s %s", Mail, CondorAdmin );

	if( MailFlag ) {
		if( (mailer=popen(cmd,"w")) == NULL ) {
			EXCEPT( "Can't do popen(%s,\"w\")", cmd );
		}
	} else {
		mailer = stdout;
	}

	fprintf( mailer, "To: %s\n", CondorAdmin );
	fprintf( mailer, "Subject: JobQueue Locked\n" );
	fprintf( mailer, "\n" );
	fprintf( mailer, "The JobQueue on machine %s is locked\n", my_hostname());

	if( MailFlag ) {
		pclose( mailer );
	}
	exit(2);

}




/*
  Check the condor spool directory for lock on the jobQueue
*/
void
check_spool_dir(int op)
{
	char	queue_name[_POSIX_PATH_MAX];
	DBM	*q;


		/* Open and lock the job queue */
	(void)sprintf( queue_name, "%s/job_queue", Spool );
	if( (q=OpenJobQueue(queue_name,O_RDWR,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue_name );
	}
    if( LockJobQueue(q,op) < 0 ) {
		EXCEPT( "Can't lock job queue : operation = %d\n",op );
	}

		// Clean up
    if( LockJobQueue(q,UNLOCK) < 0 ) {
		EXCEPT( "Can't unlock job queue\n");
	}
	CloseJobQueue( q );
}


	

/*
  Initialize information from the configuration files.
*/
void
init_params()
{
	Spool = param("SPOOL");
    if( Spool == NULL ) {
        EXCEPT( "SPOOL not specified in config file\n" );
    }

    if( (CondorAdmin = param("CONDOR_ADMIN")) == NULL ) {
        EXCEPT( "CONDOR_ADMIN not specified in config file" );
    }
	if ( (Mail = param("MAIL")) == NULL ) {
		EXCEPT( "MAIL not specified in config file");
	}

}
