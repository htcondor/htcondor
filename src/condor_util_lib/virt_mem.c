/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/* Do not remove the following line! Required for SCCS. */
static char sccsid[] = "@(#)virt_mem.c	4.3 9/18/91"; 

#include "condor_common.h"

#if defined(WIN32)

/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
int
calc_virt_memory()
{
	MEMORYSTATUS status;
	GlobalMemoryStatus(&status);
	return (int)status.dwAvailPageFile/1024;
}

#else

#include "debug.h"
#include "except.h"
#include "condor_uid.h"

/* Fix #3. work around problems with pclose if caller handle SIGCHLD. */
int HasSigchldHandler = 0;

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

/*
*  Prototype for the pstat_swapspace function called everywhere within
*  this file but nowhere outside.  -- Ajitk.
*/

int pstat_swapspace(void);


/*
 *  executing_as_nonroot
 *
 *  Tells us whether root invoked it or someone else.
 */
int     executing_as_nonroot = 1;

/*
 *  pstat_path
 *
 *  Path to the pstat program.
 */
#if !defined(Solaris)
char    *pstat_path;
#endif
/*
 *  DEFAULT_SWAPSPACE
 *
 *  This constant is used when we can't get the available swap space through
 *  other means.  -- Ajitk
 */

#define DEFAULT_SWAPSPACE       100000 /* ..dhaval 7/20 */


#ifdef OSF1
/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.

** Don't know how to do this one.  We just return the soft limit on data
** space for any children of the calling process.
*/
close_kmem() {}
calc_virt_memory()
{
	struct rlimit	lim;

	if( getrlimit(RLIMIT_DATA,&lim) < 0 ) {
		dprintf( D_ALWAYS, "getrlimit() failed - errno = %d", errno );
		return -1;
	}
	return lim.rlim_cur / 1024;
}
#endif OSF1

#ifdef LINUX
/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
close_kmem(){}
calc_virt_memory()
{
	FILE	*proc;
	int	free_swap;
	char	tmp_c[20];
	int	tmp_i;

	proc = fopen("/proc/meminfo", "r");
	if(!proc)	{
		dprintf(D_ALWAYS, "Can't open /proc/meminfo to get free swap.");
		return 0;
	}
	
	fscanf(proc, "%s %s %s %s %s %s", tmp_c, tmp_c, tmp_c, tmp_c, tmp_c, &tmp_c);
	fscanf(proc, "%s %d %d %d %d %d %d", tmp_c, &tmp_i, &tmp_i, &tmp_i, &tmp_i, &tmp_i, &tmp_i);
	fscanf(proc, "%s %d %d %d", tmp_c, &tmp_i, &tmp_i, &free_swap);
	fclose(proc);

	return free_swap / 1024;
}
#endif /*LINUX*/

#ifdef HPUX9
/* The HPUX code here grabs the amount of free swap in Kbyes.
 * I hope it is portable to all HPUX systems...  It grabs it
 * from /dev/kmem, no messy/expensive pstat running -Todd Tannenbaum */
#include <nlist.h>
#include <sys/file.h>

struct nlist swapnl[] = {
	{ "swapspc_cnt" },
	{ NULL }
};
static int kmem;

close_kmem()
{
	close( kmem );
}

calc_virt_memory()
{
	static char *addr;
	static int initialized = 0;
	int freeswap;
	priv_state priv;

	/* Become root so we have no hassles reading /dev/kmem */
	priv = set_root_priv();

	if( !initialized ) {

		/*
         * Look up addresses of variables.
         */
#if defined(HPUX10)
        if ((nlist("/stand/vmunix", swapnl) < 0) || (swapnl[0].n_type == 0)) {
#else
        if ((nlist("/hp-ux", swapnl) < 0) || (swapnl[0].n_type == 0)) {
#endif
			set_priv(priv);
			return(-1);
        }
		addr = (char *)swapnl[0].n_value;

        /*
         * Open kernel memory and read variables.
         */
        if( (kmem = open("/dev/kmem",O_RDONLY)) < 0 ) {
			set_priv(priv);
			return(-1);
        }

		initialized = 1;
	}

	if (-1==lseek(kmem, (long) swapnl[0].n_value, 0)){
		set_priv(priv);
		return(-1);
	}
	if (read(kmem, (char *) &freeswap, sizeof(int)) < 0){
		set_priv(priv);
		return(-1);
	}

	set_priv(priv);
	return ( 4 * freeswap );
}
#endif /* of the code for HPUX */


#if defined(IRIX53)

#include <sys/stat.h>
#include <sys/swap.h>

close_kmem() {}
calc_virt_memory()
{
	int freeswap;

	if( swapctl(SC_GETFREESWAP, &freeswap) == -1 ) {
		return(-1);
	}

	return( freeswap / 2 );

}

#endif /* of the code for IRIX 53 */

	
#if !defined(LINUX) && !defined(IRIX331) && !defined(AIX31) && !defined(AIX32) && !defined(SUNOS40) && !defined(SUNOS41) && !defined(CMOS) && !defined(HPUX9) && !defined(OSF1) && !defined(IRIX53)
/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
close_kmem() {}
calc_virt_memory()
{
	FILE			*fp, *popen();
	char			buf[1024];
	char			buf2[1024];
	int				size = -1;
	int				limit;
	struct rlimit	lim;
	int				read_error = 0;
	int				configured, reserved;
	priv_state		priv;

/*
** Have to be root to run pstat on some systems...
*/
	priv = set_root_priv();

	buf[0] = '\0';
#if defined(Solaris)
    if( (fp=popen("/etc/swap -s","r")) == NULL ) {
		set_priv(priv);
		dprintf( D_FULLDEBUG, "popen(swap -s): errno = %d\n", errno );
		return -1;
	}
#else
	if( (fp=popen("/etc/pstat -s","r")) == NULL ) {
		set_priv(priv);
		dprintf( D_FULLDEBUG, "popen(pstat -s): errno = %d\n", errno );
		return -1;
	}
#endif

	set_priv(priv);

#if defined(ULTRIX42) || defined(ULTRIX43)
	(void)fgets( buf, sizeof(buf), fp );
	(void)fgets( buf2, sizeof(buf2), fp );
	configured = atoi( buf );
	reserved = atoi( buf2 );
	if( configured == 0 || reserved == 0 ) {
		size = -1;
	} else {
		size = configured - reserved;
	}
#else
	while( fgets(buf,sizeof(buf),fp) ) {
		size = parse_pstat_line( buf );
		if( size > 0 ) {
			break;
		}
	}
#endif
	/*
	 * Some programs which call this routine will have their own handler
	 * for SIGCHLD.  In this case, don't cll pclose() because it calls 
	 * wait() and will interfere with the other handler.
	 * Fix #3 from U of Wisc. 
	 */
#if !defined(Solaris)
	if ( HasSigchldHandler  ) {
		fclose(fp);
	}
	else 
#endif
	{
		pclose(fp);
	}

	if( size < 0 ) {
		if( ferror(fp) ) {
			dprintf( D_ALWAYS, "Error reading from pstat\n" );
		} else {
			dprintf( D_ALWAYS, "Can't parse pstat line \"%s\"\n", buf );
		}
		return DEFAULT_SWAPSPACE;
	}

	if( getrlimit(RLIMIT_DATA,&lim) < 0 ) {
		dprintf( D_ALWAYS, "Can't do getrlimit()\n" );
		return -1;
	}
	limit = lim.rlim_max / 1024;

	if( limit < size ) {
		return limit;
	} else {
		return size;
	}
}
#endif

#if  defined(Solaris)
int parse_pstat_line( str )
char    str[200]; /* large enough */
{
        char    *ptr,temp[200];
        int count=0;

ptr=str;
while(*ptr!='\0') {ptr++;}

while(*ptr!='k') {ptr--; }
while(*ptr!=' ') {ptr--; }

while(*ptr!='k'){
temp[count++]=*ptr;
ptr++;
}

count=atoi(temp);
/*printf("Returning virtual memory size %d \n",count);*/
return count;
}
#endif

#endif /* !defined(WIN32) */
