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

#include "condor_common.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

#define KILO 1024
#define MEG (KILO*KILO)
#define GIG (MEG*KILO)

int free_fs_blocks_test(int trials, double tolerance, double perc_warn_ok) {
	int		foo = 0;
	int		foo2 = 0;
	int		bar = 0;
	int		bar2 = 0;
	int		i, j;
	FILE *	stream;
	char *	empty;
	char	filename[512];
	int		return_val = 0;
	int		num_warnings = 0;
	int		num_tests = 0;
	long	write_size;
	long	gigs;
	long	megs;
	long	kilo;
	void	*empty_kilo = NULL;
	void	*empty_megs = NULL;
	int		rval;

	sprintf(filename, "/tmp/sysapi_test_free_fs_blocks");

	dprintf(D_ALWAYS, "SysAPI: Doing %d trials by filling up /tmp by "
						"half(or up to 2 gigs, whichever is smaller), "
						"testing, and then unlinking.\n", trials);

    foo = sysapi_disk_space_raw("/tmp");
    dprintf(D_ALWAYS, "SysAPI: Initial sysapi_disk_space_raw() -> %d\n", foo);
    foo2 = sysapi_disk_space("/tmp");
    dprintf(D_ALWAYS, "SysAPI: Initial sysapi_disk_space() -> %d\n", foo);

	if (foo < 0 || foo2 < 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Disk space should never be "
								"negative.\n");
			return_val = return_val | 1;
	}

	for (i=0; i<trials; i++) {
		// Take half the free space and fill it up

		write_size = foo / 2; /* in K */

		/* Here we deal with stupid 2 gig file size limit on ext2fs */
		if (write_size >= 2*1024*KILO) /* 2 gigs */
		{
			dprintf(D_ALWAYS, "Warning! Greater then 2 gigs requested size "
					"so I am assuming 2 gigs max for test!\n");
				
			write_size = 2*1024*KILO - 42*KILO; /* pick something smaller */
		}

		gigs = (write_size / 1024) / 1024; /* total gigs */
		megs = (write_size / 1024) - (gigs * 1024); /* left over megs*/
		kilo = write_size % KILO; /* left over kilo */
		
		if ( (stream = fopen(filename, "w")) == NULL) {
			dprintf(D_ALWAYS, "Can't open %s\n", filename);
			exit(EXIT_FAILURE);
		}
		else {

			/* don't allocate for gigs */

			if (megs > 0)
			{
				empty_megs = malloc(1 * MEG);
				if (empty_megs == NULL)
				{
					dprintf(D_ALWAYS, "Out of memory!\n");
					exit(EXIT_FAILURE);
				}
			}
			if (kilo > 0)
			{
				empty_kilo = malloc(kilo);
				if (empty_kilo == NULL)
				{
					dprintf(D_ALWAYS, "Out of memory!\n");
					exit(EXIT_FAILURE);
				}
			}

			/* write out the gigs worth */
			if (gigs > 0)
			{
				dprintf(D_ALWAYS, "Writing %d gigs in 1 meg chunks\n", gigs);
				for (i = 0; i < gigs*KILO; i++)
				{
					rval = fwrite(empty_megs, 1 * MEG, 1, stream);
					if (rval == 0)
					{
						dprintf(D_ALWAYS, 
							"An error happened writing the gigs\n");
						exit(EXIT_FAILURE);
					}
				}
			}

			/* write out megs second */
			if (megs > 0)
			{
				dprintf(D_ALWAYS, "Writing %d megs in 1 meg chunks\n", megs);
				for (i = 0; i < megs; i++)
				{
					rval = fwrite(empty_megs, 1 * MEG, 1, stream);
					if (rval == 0)
					{
						dprintf(D_ALWAYS, 
							"An error happened writing the megs\n");
						exit(EXIT_FAILURE);
					}
				}
			}

			/* write out left over kilos */
			if (kilo > 0)
			{
				dprintf(D_ALWAYS, "Writing %d kilos\n", kilo);
				rval = fwrite(empty_kilo, kilo, 1, stream);
				if (rval == 0)
				{
					dprintf(D_ALWAYS, "An error happened writing the kilos\n");
					exit(EXIT_FAILURE);
				}
			}

			free(empty_megs);
			free(empty_kilo);

			fclose(stream);
		}
	
    	bar = sysapi_disk_space_raw("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After %dKB write: "
							"sysapi_disk_space_raw() -> %d\n", write_size, bar);
    	bar2 = sysapi_disk_space("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After %dKB write: "
							"sysapi_disk_space() -> %d\n", write_size, bar);
		num_tests += 2;
	
	
		// Raw free space after write
		if (bar < 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Raw free space should never be "
								"negative!\n");
			return_val = return_val | 1;
			break;
		}
		else if ( (fabs((bar - (foo - write_size))/((double)bar+1)))>tolerance){
			dprintf(D_ALWAYS, "SysAPI: WARNING! Raw free space went from "
								"%d to %d, but probably shouldn't have "
								"changed more than %%%f tolerance. "
								"Another process may have used/freed space.\n",
								foo, bar, tolerance*100);
			num_warnings++;
		}

		// Cooked free space after write
		if (bar2 < 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked free space should never "
								"be negative!\n");
			return_val = return_val | 1;
			break;
		}
		else if ( (fabs((bar2-(foo2-write_size)))/((double)bar2+1))>tolerance){
			dprintf(D_ALWAYS, "SysAPI: WARNING! Cooked free space went from "
								"%d to %d, but probably shouldn't have "
								"changed more than %%%f tolerance. "
								"Another process may have used/freed space.\n",
								foo, bar, tolerance*100);
			num_warnings++;
		}
	
	
		if (unlink(filename) < 0) {
			dprintf(D_ALWAYS, "SysAPI: Can't delete %s\n", filename);
			dprintf(D_ALWAYS, "SysAPI: Error reported: %s\n", strerror(errno));
			continue;
		}

	
    	foo = sysapi_disk_space_raw("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After unlinking files: "
							"sysapi_disk_space_raw() -> %d\n", foo);
    	foo2 = sysapi_disk_space("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After unlinking files: "
							"sysapi_disk_space() -> %d\n",foo2);
		num_tests += 2;
	
		// Raw free space after unlink
		if (foo < 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Raw free space should never "
								"be negative!\n");
			return_val = return_val | 1;
			break;
		}
		else if ( (fabs((foo-(bar+write_size))/((double)foo+1))) > tolerance){
			dprintf(D_ALWAYS, "SysAPI: WARNING! Raw free space went from "
								"%d to %d, but probably shouldn't have "
								"changed more than %%%f tolerance. "
								"Another process may have used/freed space.\n",
								bar, foo, tolerance*100);
			num_warnings++;
		}

		// Cooked free space after unlink
		if (foo2 < 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked free space should never "
								"be negative!\n");
			return_val = return_val | 1;
			break;
		}
		else if ( (fabs((foo2-(bar2+write_size))/((double)foo2+1)))>tolerance){
			dprintf(D_ALWAYS, "SysAPI: WARNING! Raw free space went from "
								"%d to %d, but probably shouldn't have "
								"changed more than %%%f tolerance. "
								"Another process may have used/freed space.\n",
								foo2, bar2, tolerance*100);
			num_warnings++;
		}
	}

	if (((double)num_warnings/(double)num_tests) >= perc_warn_ok) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning perc_warn_ok exceeded "
								"(%2f\% warnings > %2f\% perc_warn_ok) .\n",
								((double)num_warnings/(double)num_tests)*100, 
								perc_warn_ok*100);
			return_val = return_val | 1;
	}

	return return_val;
}
