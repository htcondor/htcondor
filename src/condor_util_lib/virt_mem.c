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

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctype.h>
#include <nlist.h>
#include <fcntl.h>
#include "debug.h"
#include "except.h"

#define MATCH 0

/* Fix #3. work around problems with pclose if caller handle SIGCHLD. */
int HasSigchldHandler = 0;

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

#ifdef IRIX331
/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
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
	static int memfd;

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
        kvm_t *kd;
        int result, vm_free;

        /*
         * First time in this call, get the offset to the anon structure.
         * and page to kbyte conversion.
         */
        if ( !initialized ) {
                initialized = 1;
                namelist = corefile = swapfile = (char *) NULL;
                nl [0].n_name = s_anon;
                nl [1].n_name = (char *) NULL;
                set_root_euid();
                kd = kvm_open (namelist, corefile, swapfile, O_RDONLY, errstr);
                if ( kd == (kvm_t *) NULL) {
                    dprintf (D_ALWAYS, "Open failure on kernel. First call\n");
                    set_condor_euid();
                    return (-1);
                }
                if (kvm_nlist (kd, nl) != 0) {
                        dprintf (D_ALWAYS, "kvm_nlist failed. First call.\n");
                        set_condor_euid();
                        return (-1);
                }
                if (kvm_close (kd) != 0) {
                        dprintf (D_ALWAYS, "kvm_close failure. First call.\n");
                        set_condor_euid();
                        return (-1);
                }
                offset = nl[0].n_value;
                page_to_k = getpagesize () / 1024;
        }
        set_root_euid();
        kd = kvm_open (namelist, corefile, swapfile, O_RDONLY, errstr);
        if ( kd == (kvm_t *) NULL) {
                dprintf (D_ALWAYS, "Open failure on kernel.\n");
                set_condor_euid();
                return (-1);
        }
        result = kvm_read (kd, offset, &a_info, sizeof (a_info));
        if (result != sizeof (a_info)) {
                dprintf (D_ALWAYS, "kvm_read error.\n");
                set_condor_euid();
                return (-1);
        }
        if (kvm_close (kd) != 0) {
                dprintf (D_ALWAYS, "kvm_close failure.\n");
                set_condor_euid ();
                return (-1);
        }
        set_condor_euid ();
        vm_free = (int) (a_info.ani_max - a_info.ani_resv);
        vm_free *= page_to_k;
        return (vm_free);
}
#endif /* SunOS4.0 and SunOS4.1 code */

#if defined(HPUX8)
/*
** This is wrong!  This only gets the system defined limit on the data
** segment size of an individual process.  It really has nothing to do
** with available swap space.  -- mike
*/
#include <ulimit.h>
#define DATA_SEG_ADDR 0x40000000
calc_virt_memory()
{
	long		lim;

	lim = ulimit( UL_GETMAXBRK );

	return (lim - DATA_SEG_ADDR) / 1024;
}
#endif
	
#if !defined(IRIX331) && !defined(AIX31) && !defined(AIX32) && !defined(SUNOS40) && !defined(SUNOS41) && !defined(CMOS) && !defined(HPUX8)
/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
calc_virt_memory()
{
	FILE	*fp, *popen();
	char	buf[1024];
	char	buf2[1024];
	int		size = -1;
	int		limit;
	struct	rlimit lim;
	int		read_error = 0;
	int		configured, reserved;


/*
** Have to be root to run pstat on some systems...
*/
#ifdef NFSFIX
	set_root_euid();
#endif NFSFIX

	buf[0] = '\0';
	if( (fp=popen("/etc/pstat -s","r")) == NULL ) {
#ifdef NFSFIX
		set_condor_euid();
#endif NFSFIX
		dprintf( D_FULLDEBUG, "popen(pstat -s): errno = %d\n", errno );
		return -1;
	}

#ifdef NFSFIX
	set_condor_euid();
#endif NFSFIX

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
	if ( HasSigchldHandler  ) {
		fclose(fp);
	}
	else {
		pclose(fp);
	}

	if( size < 0 ) {
		if( ferror(fp) ) {
			dprintf( D_ALWAYS, "Error reading from pstat\n" );
		} else {
			dprintf( D_ALWAYS, "Can't parse pstat line \"%s\"\n", buf );
		}
		return -1;
	}

	if( getrlimit(RLIMIT_DATA,&lim) < 0 ) {
		dprintf( D_ALWAYS, "Can't do getrlimit()\n" );
		return -1;
	}
	limit = lim.rlim_max / 1024;

	if( limit < size ) {
		dprintf( D_FULLDEBUG, "Returning %d\n", limit );
		return limit;
	} else {
		dprintf( D_FULLDEBUG, "Returning %d\n", size );
		return size;
	}
}
#endif


#if defined(ULTRIX43) || defined(ULTRIX42) || defined(BSD43) || defined(DYNIX) || defined(SUNOS32)
parse_pstat_line( str )
char	*str;
{
	char	*ptr;
	int		n_blocks, block_size, answer;

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

