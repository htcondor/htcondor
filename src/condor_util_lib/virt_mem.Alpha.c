/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
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
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/table.h>

static char *_FileName_ = __FILE__;

int     executing_as_nonroot = 1;

int HasSigchldHandler = 0;

int parse_swapon_line( const char *line );
int calc_virt_memory();
int get_data_size_limit();

/*
 *  DEFAULT_SWAPSPACE
 *
 *  This constant is used when we can't get the available swap space through
 *  other means.  -- Ajitk
 */

#define DEFAULT_SWAPSPACE       100000 /* ..dhaval 7/20 */

#define TESTING 0
#if TESTING
main()
{
	int	virt_mem;

	virt_mem = calc_virt_memory();
	printf( "%d kbytes of swap free\n", virt_mem );
}

#define dprintf fprintf
#define D_ALWAYS stdout
#define D_FULLDEBUG stdout
#endif /* TESTING */

close_kmem(){}

/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
int
calc_virt_memory()
{
	FILE				*fp, *popen();
	char				buf[1024];
	int					size = -1;
	int					limit;
	struct tbl_swapinfo	swap;

	if (table(TBL_SWAPINFO,-1,(char *)&swap,1,sizeof(swap)) >= 0) {
		return (swap.free * NBPG) / 1024;
	}

	/* if table call fails, then parse swapon output -- gross! */

	dprintf( D_FULLDEBUG, "table call failed, errno = %d\n", errno );

	if( (fp=popen("/sbin/swapon -s","r")) == NULL ) {
		dprintf( D_FULLDEBUG, "popen(swapon -s): errno = %d\n", errno );
		return -1;
	}

    /*
	   If a SIGCHLD is delivered when trying to parse swapon output,
       parsing fails, and results in daemons getting totally confused
       and dying.  So we block SIGCHLD while we parse.  --RR
    */

#if defined (OSF1)
    if (HasSigchldHandler) block_signal (SIGCHLD);
#endif

	buf[0] = '\0';
	while( fgets(buf,sizeof(buf),fp) ) {
		if( (size = parse_swapon_line(buf)) > 0 ) {
			break;
		}
	}

#if defined (OSF1)
    if (HasSigchldHandler) unblock_signal (SIGCHLD);
#endif

	/*
	 * Some programs which call this routine will have their own handler
	 * for SIGCHLD.  In this case, don't cll pclose() because it calls 
	 * wait() and will interfere with the other handler.
	 */
	if ( HasSigchldHandler  ) {
		fclose(fp);
	}
	else {
		pclose(fp);
	}

	if( size < 0 ) {
		if( ferror(fp) ) {
			dprintf( D_ALWAYS, "Error reading from swapon\n" );
		} else {
			dprintf( D_ALWAYS, "Can't parse swapon line \"%s\"\n", buf );
		}
		return DEFAULT_SWAPSPACE;
	}

	limit = get_data_size_limit();

	if( limit < size ) {
		dprintf( D_FULLDEBUG, "Returning %d\n", limit );
		return limit;
	} else {
		dprintf( D_FULLDEBUG, "Returning %d\n", size );
		return size;
	}
}

/*
  Return the soft limit on data  space for any children of
  the calling process.
*/
int
get_data_size_limit()
{
	struct rlimit	lim;

	if( getrlimit(RLIMIT_DATA,&lim) < 0 ) {
		dprintf( D_ALWAYS, "getrlimit() failed - errno = %d", errno );
		return -1;
	}
	return lim.rlim_cur / 1024;
}


/*
  We're given a line of output from "swapon -s".  We're looking for
  the available swap space.  The relevant line looks like:

	Available space:        11309 pages ( 69%)

  If the given line is the one, we convert pages to kilobytes, and
  return the answer, otherwise we return -1.
*/
int
parse_swapon_line( const char *line )
{
	const char	*ptr;
	int		pages;
	int		answer;

		/* Skip leading white space */
	for( ptr=line; *ptr && isspace(*ptr); ptr++ )
		;

		/* See if this is the line we are looking for */
	if( strncmp("Available",ptr,9) == MATCH ) {

			/* Skip to the number */
		while( *ptr && !isdigit(*ptr) ) {
			ptr++;
		}

		if( !isdigit(*ptr) ) {
			return -1;
		}

			/* Convert from pages to kilobytes */
		pages = atoi(ptr);
		answer = pages * NBPG / 1024;

		return answer;
	} else {
		return -1;
	}
}
