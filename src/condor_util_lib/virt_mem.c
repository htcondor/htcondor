/* Do not remove the following line! Required for SCCS. */
static char sccsid[] = "@(#)virt_mem.c	4.3 9/18/91"; 
/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#if defined(WIN32)

#include "condor_common.h"

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

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctype.h>
#include <nlist.h>
#include <fcntl.h>
#include "debug.h"
#include "except.h"
#include "condor_uid.h"

#define MATCH 0

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

#ifdef IRIX331
/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
close_kmem() {}
calc_virt_memory()
{
	return free_fs_blocks( "/debug" );
}
#endif /* Irix 3.1.1 code */

#if defined(AIX31) || defined(AIX32)
/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/

static int memfd;

close_kmem()
{
	close( memfd );
}

calc_virt_memory()
{
/*
 * We can look in /dev/kmem and avoid ugliness with pstat for AIX 3. jrhc
 *
 */
	struct {
		int pad0, pad1, pad2, pad3, pad4, pad5, pad6, pad7, pad8;
		int total_real_mem;		/* Real memory ( hardware memory ) */
		int pad10;
		int free_real_mem;      /* Real memory not currently allocated */
		int pad12, pad13;
		int total_virt_mem;		/* Defined swap space on disk. Seems not to 
											include any real memory. */
		int free_virt_mem;		/* Defined swap space which is not allocated */
	} vmker;
	struct nlist knames[1] = {"vmker", 0, 0, 0, 0, 0 };
	static int initialized = 0;

	/* get address of vmker structure */
	if (knlist(knames, 1, sizeof(struct nlist)) == -1) {
		dprintf(D_ALWAYS, "Cannot read kernel structure for swap information.\n");
		return(-1);
	}

	/* open /dev/kmem. Since we do this often, leave it open for next time. */
	if ( !initialized ) {
		memfd = open("/dev/kmem", O_RDONLY);
		if ( memfd < 0 ) {
			dprintf(D_ALWAYS, " Cannot open /dev/kmem.\n");
			return(-1);
		}
		initialized = 1;
	} 
	
	/* Read in vmker structure from /dev/kmem */
	lseek(memfd, knames[0].n_value, SEEK_SET);
	if ( read(memfd, &vmker, sizeof(vmker)) < 0 ) {
		dprintf(D_ALWAYS, "Cannot read /dev/kmem.\n");
		return(-1);
	}

	/* Results are in 4k blocks. Convert to kbytes and return. */
	return( (vmker.free_virt_mem) * 4 );
}
#endif /* AIX 3 code */

#if defined(SUNOS40) || defined(SUNOS41) || defined(CMOS)
#include <kvm.h>
#include <vm/anon.h>
static kvm_t *kd = (kvm_t *)NULL;

close_kmem()
{
	kvm_close( kd );
}

calc_virt_memory()
{
	static int initialized = 0;
	static int page_to_k;              /* for page to Kbyte conversion */
	static unsigned long offset;   /* Store offset to symbol in kernel */
	static char s_anon[] = "_anoninfo";       /* symbol name in kernel */
	static char *namelist, *corefile, *swapfile;
	static char errstr [] = "virt_mem.c";
	struct nlist nl[2];
	struct anoninfo a_info;
	int result, vm_free;
	priv_state	priv;
	
	
	
	/* Operate as root to read /dev/kmem */
	priv = set_root_priv();
	
	/*
	 * First time in this call, get the offset to the anon structure.
	 * and page to kbyte conversion.
	 */
	if ( !initialized ) {
		namelist = corefile = swapfile = (char *) NULL;
		nl [0].n_name = s_anon;
		nl [1].n_name = (char *) NULL;
		kd = kvm_open (namelist, corefile, swapfile, O_RDONLY, errstr);
		if ( kd == (kvm_t *) NULL) {
			set_priv(priv);
			dprintf (D_ALWAYS, "Open failure on kernel. First call\n");
			return (-1);
		}
		if (kvm_nlist (kd, nl) != 0) {
			(void)kvm_close( kd );
			kd = (kvm_t *)NULL;
			set_priv(priv);
			dprintf (D_ALWAYS, "kvm_nlist failed. First call.\n");
			return (-1);
		}
		offset = nl[0].n_value;
		page_to_k = getpagesize () / 1024;
		initialized = 1;
	}
	if( !kd ) {
		kd = kvm_open (namelist, corefile, swapfile, O_RDONLY, errstr);
		if ( kd == (kvm_t *) NULL) {
			set_priv(priv);
			dprintf (D_ALWAYS, "Open failure on kernel.\n");
			return (-1);
		}
	}
	result = kvm_read (kd, offset, &a_info, sizeof (a_info));
	if (result != sizeof (a_info)) {
		kvm_close( kd );
		kd = (kvm_t *)NULL;
		set_priv(priv);
		dprintf (D_ALWAYS, "kvm_read error.\n");
		return (-1);
	}
	vm_free = (int) (a_info.ani_max - a_info.ani_resv);
	vm_free *= page_to_k;
	
	set_priv(priv);
	return (vm_free);
}
#endif /* SunOS4.0 and SunOS4.1 code */

#ifdef HPUX9
/* The HPUX code here grabs the amount of free swap in Kbyes.
 * I hope it is portable to all HPUX systems...  It grabs it
 * from /dev/kmem, no messy/expensive pstat running -Todd Tannenbaum */
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


#if defined(ULTRIX43) || defined(ULTRIX42) || defined(BSD43) || defined(DYNIX) || defined(SUNOS32) 
int parse_pstat_line( str )
char	*str;
{
	char	*ptr,*temp;
	int		n_blocks, block_size, answer, count=0;

        if( strncmp("avail",str,5) != MATCH ) {
		return -1;
	}

	for(ptr=str; *ptr && !isdigit(*ptr); ptr++ )
		;
	if( !ptr ) {
		return -1;
	}
	n_blocks = atoi( ptr );

	while( *ptr && *ptr != '*' ) {
		ptr += 1;
	}
	if( *ptr != '*' ) {
		return -1;
	}
	ptr += 1;
	block_size = atoi( ptr );
#if defined(DYNIX)
	answer =  (n_blocks/2 - 1) * block_size;
#else defined(DYNIX)
	answer =  (n_blocks - 1) * block_size;
#endif defined(DYNIX)
	return answer;
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
