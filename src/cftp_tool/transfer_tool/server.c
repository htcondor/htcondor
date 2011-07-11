#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "frames.h"
#include "server_sm_lib.h"
#include <time.h>

// For command line arguments
#include <argp.h>

const char *argp_program_version =
	"cftp-server v0.01";
const char *argp_program_bug_address =
	"<nmitchel@cs.wisc.edu>";

/* Program documentation. */
static char doc[] =
	"Cluster File Transfer Server - A simple file transfer server designed to be run in parallel across a cluster";

/* A description of the arguments we accept. */
static char args_doc[] = "[-i TIMEOUT] [-I TIMEOUT[:MAX]] [-q QUOTA] [-a HOST[:PORT]] [-l HOST[:PORT]] [-t PATH]";

	/* The options we understand. */
static struct argp_option options[] = {
	{"verbose",
	 'v',
	 0,
	 0,
	 "Produce verbose output" },

	{"debug",
	 'd',
	 0,
	 0,
	 "Produce debug output" },

	{"initial-timeout",
	 'i',
	 "TIMEOUT",
	 0,
	 "Do not wait forever for first client connection. Exit after TIMEOUT if no client connects. Implies single-client option."},

    {"single-client",
     's',
     0,
     0,
     "Accept only a single client" },
        
	{"post-timeout",
	 'I',
	 "TIMEOUT[:MAX]",
	 0,
	 "Wait for TIMEOUT ( or randomly between TIMEOUT and TIMEOUT + MAX ) after transfer before terminating. Implies single-client option."},

	{"disk-quota",
	 'q',	
	 "QUOTA",
	 0,
	 "Only allow files to be transfered that fit within this disk space quota (KB - 1024 Bytes)" },

	{"announce-host" ,
	 'a',
	 "HOST[:PORT]",
	 0,
	 "Upon startup, announce presence this collector host." },

	{"listen-host",
	 'l',
	 "HOST[:PORT]",
	 0,
	 "Bind to this address instead of the default." },

	{"transfer-dir",
	 't',
	 "PATH",
	 0,
	 "Path to store received files. Defaults to './' " },

	{"uuid",
	 'u',
	 "UUID",
	 0,
	 "Unique Identifier that is provided during server announcements. Defaults to 0." },

	{ 0 }
};
     

     
	/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
		/* Get the input argument from argp_parse, which we
		   know is a pointer to our arguments structure. */
	ServerArguments *arguments = state->input;
	char* sep_start;

	int t_min, t_max;

	switch (key)
		{
		case 'i':
			sscanf( arg, "%d", &arguments->itimeout );
            arguments->single_client = 1;
			break;

		case 'I':
			sep_start = strrchr( arg, ':' );
			if( sep_start)
				{
					(*sep_start) = 0;
					sscanf( sep_start+1 , "%d", &t_max);
					sscanf( arg, "%d", &t_min );

					if( t_max < 0 )
						t_max = 0;

					arguments->ptimeout = t_min + ( rand() %  ( t_max ) );
				}
			else
				{
					sscanf( arg, "%d", &arguments->ptimeout );
				}
	
			arguments->single_client = 1;

			break;

		case 'v':
			arguments->verbose = 1;
			break;

        case 's':
            arguments->single_client=1;
            break;

		case 'd':
			arguments->debug = 1;
			arguments->verbose = 1;
			break;

		case 'q':
			sscanf( arg, "%ld", &arguments->quota );
			break;

		case 'a':
			sep_start = strrchr( arg, ':' );
			if( sep_start)
				{
					(*sep_start) = 0;
					sscanf(sep_start+1, "%d", &arguments->aport );
				}
			strcpy( arguments->ahost, arg );
			
			arguments->announce = 1;
			break;

		case 'l':
			sep_start = strrchr( arg, ':' );
			if(sep_start)
				{
					(*sep_start) = 0;
					sscanf(sep_start+1, "%d", &arguments->lport );
				}

			strcpy( arguments->lhost, arg );
			
			break;		

		case 't':
			memset( arguments->tpath, 0, 513 );
			strcpy( arguments->tpath, arg );
			break;

		case 'u':
			memset( arguments->uuid, 0, 129 );
			strcpy( arguments->uuid, arg );
			break;
		

		case ARGP_KEY_ARG:
			if (state->arg_num > 0)
					/* Too many arguments. */
				argp_usage (state);
			break;
     
		case ARGP_KEY_END:
			if (state->arg_num < 0)
					/* Not enough arguments. */
				argp_usage (state);
			break;
     
		default:
			return ARGP_ERR_UNKNOWN;
		}
	return 0;
}

	/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };



int main( int argc, char** argv )
{
	srand ( time(NULL) + clock() );
	
	ServerArguments arguments;
     
	char quotaHR_str[200];

		/* Default values. */
	arguments.debug = 0;
	arguments.verbose = 0;
	arguments.itimeout = -1;
	arguments.ptimeout = 0;
	arguments.quota = -1;
	arguments.announce = 0;
    arguments.single_client = 0;

	arguments.aport = 0;
	arguments.ahost = (char*)malloc( 256 );
	memset( arguments.ahost, 0, 256 );

	arguments.lport = 0; // By default pick random port
	arguments.lhost = (char*)malloc( 256 );
	memset( arguments.lhost, 0, 256 );
	sprintf( arguments.lhost, "localhost" );

	arguments.tpath = (char*)malloc( 513 );
	memset( arguments.tpath, 0, 513 );
	sprintf( arguments.tpath, "." );

	arguments.uuid = (char*)malloc( 129 );
	memset( arguments.uuid, 0, 129 );
	sprintf( arguments.uuid, "0" );
	
	
		/* Parse our arguments; every option seen by parse_opt will
		   be reflected in arguments. */
	argp_parse (&argp, argc, argv, 0, 0, &arguments);

	if( arguments.debug )
		{

			memset( quotaHR_str, 0, 200 );
			if( arguments.quota > 1024l )
				sprintf( quotaHR_str, "(%4f MB)", arguments.quota / 1024.0 );
			if( arguments.quota > 1048576l )
				sprintf( quotaHR_str, "(%4f GB)", arguments.quota / 1048576.0 );
			if( arguments.quota > 1073741824l )
				sprintf( quotaHR_str, "(%4f TB)", arguments.quota / 1073741824.0 );

			printf ("\n--ARGUMENTS--\n"
					"\tLISTEN PORT      = %d\n"
					"\tLISTEN INTERFACE = %s"
					"\n\n--OPTIONS--\n"
					"\tQUOTA            = %ld KB %s\n"
                    "\tSINGLE CLIENT    = %s\n"
					"\tINITIAL TIMEOUT  = %d sec\n"
					"\tPOST TIMEOUT     = %d sec\n"
					"\tVERBOSE          = %s\n"
					"\tANNOUNCE HOST    = %s\n"
					"\tANNOUNCE PORT    = %d\n"
					"\tTRANSFER PATH    = %s\n"
					"\tUUID             = %s\n\n\n",
					arguments.lport,
					arguments.lhost,
					arguments.quota,
					quotaHR_str,
                    arguments.single_client ? "yes" : "no",
					arguments.itimeout,
					arguments.ptimeout,
					arguments.verbose ? "yes" : "no",
					arguments.ahost,
					arguments.aport,
					arguments.tpath,
					arguments.uuid);
		}

		//TODO: Write server main loop and associated handling code.
	run_server(&arguments);

	free( arguments.ahost );
	free( arguments.lhost );
	free( arguments.tpath );
	free( arguments.uuid );

	return 0;
}
