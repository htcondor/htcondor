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

#if defined(Solaris) && !defined(Solaris251)
#include "condor_fix_timeval.h"
#include </usr/ucbinclude/sys/rusage.h>
#endif

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "filter.h"
#include "alloc.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

#include "condor_qmgr.h"

char	*param();
char	*MyName;

int		PrioAdjustment;
int		NewPriority;
int		PrioritySet;
int		AdjustmentSet;
int		InvokingUid;		// user id of person ivoking this program
char	*InvokingUserName;	// user name of person ivoking this program

const int MIN_PRIO = -20;
const int MAX_PRIO = 20;

	// Prototypes of local interest only
void usage();
int compute_adj( char * );
void ProcArg( const char * );
int calc_prio( int old_prio );
void init_user_credentials();

char	hostname[512];

	// Tell folks how to use this program
void
usage()
{
	fprintf( stderr, "Usage: %s [{+|-}priority ] [-p priority] ", MyName );
	fprintf( stderr, "[ -a ] [-r host] [user | cluster | cluster.proc] ...\n");
	exit( 1 );
}

int
main( int argc, char *argv[] )
{
	char	*arg;
	int		prio_adj;
	char			*args[argc - 1];
	int				nArgs = 0;
	int				i;
	Qmgr_connection	*q;

	MyName = argv[0];

	config( 0 );

	if( argc < 2 ) {
		usage();
	}

	PrioritySet = 0;
	AdjustmentSet = 0;

	hostname[0] = '\0';
	for( argv++; arg = *argv; argv++ ) {
		if( (arg[0] == '-' || arg[0] == '+') && isdigit(arg[1]) ) {
			PrioAdjustment = compute_adj(arg);
			AdjustmentSet = TRUE;
		} else if( arg[0] == '-' && arg[1] == 'p' ) {
			argv++;
			NewPriority = atoi(*argv);
			PrioritySet = TRUE;
		} else if( arg[0] == '-' && arg[1] == 'r' ) {
			// use the given name as the host name to connect to
			argv++;
			strcpy (hostname, *argv);
		} else {
			args[nArgs] = arg;
			nArgs++;
		}
	}

	if( PrioritySet == FALSE && AdjustmentSet == FALSE ) {
		fprintf( stderr, 
			"You must specify a new priority or priority adjustment.\n");
		usage();
		exit(1);
	}

	if( PrioritySet && (NewPriority < MIN_PRIO || NewPriority > MAX_PRIO) ) {
		fprintf( stderr, 
			"Invalid priority specified.  Must be between %d and %d.\n", 
			MIN_PRIO, MAX_PRIO );
		exit(1);
	}

		/* Open job queue */
	if (hostname[0] == '\0')
	{
		// hostname was not set at command line; obtain from system
		if(gethostname(hostname, 200) < 0)
		{
			EXCEPT("gethostname failed, errno = %d", errno);
		}
	}
	if((q = ConnectQ(hostname)) == 0)
	{
		EXCEPT("Failed to connect to qmgr on host %s", hostname);
	}
	for(i = 0; i < nArgs; i++)
	{
		ProcArg(args[i]);
	}
	DisconnectQ(q);

#if defined(ALLOC_DEBUG)
	print_alloc_stats();
#endif

	return 0;
}

/*
  Given the old priority of a given process, calculate the new
  one based on information gotten from the command line arguments.
*/
int
calc_prio( int old_prio )
{
	int		answer;

	if( AdjustmentSet == TRUE && PrioritySet == FALSE ) {
		answer = old_prio + PrioAdjustment;
		if( answer > MAX_PRIO )
			answer = MAX_PRIO;
		else if( answer < MIN_PRIO )
			answer = MIN_PRIO;
	} else {
		answer = NewPriority;
	}
	return answer;
}

/*
  Given a command line argument specifing a relative adjustment
  of priority of the form "+adj" or "-adj", return the correct
  value as a positive or negative integer.
*/
int
compute_adj( char *arg )
{
	char *ptr;
	int val;
	
	ptr = arg+1;

	val = atoi(ptr);

	if( *arg == '-' ) {
		return( -val );
	} else {
		return( val );
	}
}

extern "C" int SetSyscalls( int foo ) { return foo; }

void UpdateJobAd(int cluster, int proc)
{
	int old_prio, new_prio;
	if (GetAttributeInt(cluster, proc, ATTR_JOB_PRIO, &old_prio) < 0)
	{
		fprintf(stderr, "Couldn't retrieve current prio for %d.%d.\n",
				cluster, proc);
		return;
	}
	new_prio = calc_prio( old_prio );
	SetAttributeInt(cluster, proc, ATTR_JOB_PRIO, new_prio);
}

void ProcArg(const char* arg)
{
	int		cluster, proc;
	char	*tmp;

	if(isdigit(*arg))
	// set prio by cluster/proc #
	{
		cluster = strtol(arg, &tmp, 10);
		if(cluster <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s\n", arg);
			return;
		}
		if(*tmp == '\0')
		// update prio for all jobs in the cluster
		{
			ClassAd	*ad = new ClassAd;
			char constraint[100];
			sprintf(constraint, "%s == %d", ATTR_CLUSTER_ID, cluster);
			int firstTime = 1;
			while(GetNextJobByConstraint(constraint, ad, firstTime) >= 0) {
				ad->LookupInteger(ATTR_PROC_ID, proc);
				delete ad;
				ad = new ClassAd;
				UpdateJobAd(cluster, proc);
				firstTime = 0;
			}
			delete ad;
			return;
		}
		if(*tmp == '.')
		{
			proc = strtol(tmp + 1, &tmp, 10);
			if(proc < 0)
			{
				fprintf(stderr, "Invalid proc # from %s\n", arg);
				return;
			}
			if(*tmp == '\0')
			// update prio for proc
			{
				UpdateJobAd(cluster, proc);
				return;
			}
			fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
			return;
		}
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}
	else if(arg[0] == '-' && arg[1] == 'a')
	{
		ClassAd *ad = new ClassAd;
		int firstTime = 1;
		while(GetNextJob(ad, firstTime) >= 0) {
			ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
			ad->LookupInteger(ATTR_PROC_ID, proc);
			delete ad;
			ad = new ClassAd;
			UpdateJobAd(cluster, proc);
			firstTime = 0;
		}
		delete ad;
	}
	else if(isalpha(*arg))
	// update prio by user name
	{
		char	constraint[100];
		ClassAd	*ad = new ClassAd;
		int firstTime = 1;
		
		sprintf(constraint, "%s == \"%s\"", ATTR_OWNER, arg);

		while (GetNextJobByConstraint(constraint, ad, firstTime) >= 0) {
			ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
			ad->LookupInteger(ATTR_PROC_ID, proc);
			delete ad;
			ad = new ClassAd;
			UpdateJobAd(cluster, proc);
			firstTime = 0;
		}
		delete ad;
	}
	else
	{
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}
}
