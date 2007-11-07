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
#include "sysapi.h"
#include "sysapi_externs.h"

#define KILO 1024
#define MEG (KILO*KILO)
#define GIG (MEG*KILO)

static int check_free(int before, int after, int write_size, double tolerance, 
					  const char *label ) {
	int		expected;				/* Expected free disk space */
	double	error;


	/* Should never go negative */
	if (after < 0) {
		dprintf(D_ALWAYS, "SysAPI: ERROR! %s free space should never be "
				"negative!\n", label );
		return -1;
	}

	expected = before - write_size;
	error = ( after - expected ) / ( after + 1.0 );
	if ( fabs(error) > tolerance ) {
		dprintf(D_ALWAYS,
				"SysAPI: WARNING! %s free space went from "
				"%d to %d, but probably shouldn't have "
				"changed more than %%%f tolerance. "
				"Another process may have used/freed space.\n",
				label, before, after, tolerance*100);
		dprintf(D_ALWAYS, "exp=%d, error=%.3f\n", expected, error );
		return 1;
	}

	return 0;
}

int free_fs_blocks_test(int trials, double tolerance, double warn_ok_ratio) {
	int		dfree1_raw = 0;			/* "Raw" free disk space *before* write */
	int		dfree1 = 0;				/* Free disk space *before* write */
	int		dfree2_raw = 0;			/* "Raw" free disk space *after* write */
	int		dfree2 = 0;				/* Free disk space *after* write */
	int		i;
	FILE *	stream;
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
	unlink( filename );

	dprintf(D_ALWAYS, "SysAPI: Doing %d trials by filling up /tmp by "
						"half (max 2G), testing & unlinking.\n", trials);

    dfree1_raw = sysapi_disk_space_raw("/tmp");
    dprintf(D_ALWAYS, "SysAPI: Initial sysapi_disk_space_raw() -> %d\n", dfree1_raw);
    dfree1 = sysapi_disk_space("/tmp");
    dprintf(D_ALWAYS, "SysAPI: Initial sysapi_disk_space() -> %d\n", dfree1_raw);

	if (dfree1_raw < 0 || dfree1 < 0) {
		dprintf(D_ALWAYS, "SysAPI: ERROR! Disk space should never be "
				"negative.\n");
		return_val = return_val | 1;
	}

	for (i=0; i<trials; i++) {
		// Take half the free space and fill it up

		write_size = dfree1_raw / 2; /* in K */

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
		
		if ( (stream = safe_fopen_wrapper(filename, "w", 0644)) == NULL) {
			dprintf(D_ALWAYS, "Can't open %s\n", filename);
			unlink( filename );
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
					unlink( filename );
					exit(EXIT_FAILURE);
				}
			}
			if (kilo > 0)
			{
				empty_kilo = malloc(kilo);
				if (empty_kilo == NULL)
				{
					dprintf(D_ALWAYS, "Out of memory!\n");
					unlink( filename );
					exit(EXIT_FAILURE);
				}
			}

			/* write out the gigs worth */
			if (gigs > 0)
			{
				dprintf(D_ALWAYS, "Writing %ld gigs in 1 meg chunks\n", gigs);
				for (i = 0; i < gigs*KILO; i++)
				{
					rval = fwrite(empty_megs, 1 * MEG, 1, stream);
					if (rval == 0)
					{
						dprintf(D_ALWAYS, 
							"An error happened writing the gigs\n");
						unlink( filename );
						exit(EXIT_FAILURE);
					}
				}
			}

			/* write out megs second */
			if (megs > 0)
			{
				dprintf(D_ALWAYS, "Writing %ld megs in 1 meg chunks\n", megs);
				for (i = 0; i < megs; i++)
				{
					rval = fwrite(empty_megs, 1 * MEG, 1, stream);
					if (rval == 0)
					{
						dprintf(D_ALWAYS, 
							"An error happened writing the megs\n");
						unlink( filename );
						exit(EXIT_FAILURE);
					}
				}
			}

			/* write out left over kilos */
			if (kilo > 0)
			{
				dprintf(D_ALWAYS, "Writing %ld kilos\n", kilo);
				rval = fwrite(empty_kilo, kilo, 1, stream);
				if (rval == 0)
				{
					dprintf(D_ALWAYS, "An error happened writing the kilos\n");
					unlink( filename );
					exit(EXIT_FAILURE);
				}
			}

			free(empty_megs);
			free(empty_kilo);

			fclose(stream);
		}
	
    	dfree2_raw = sysapi_disk_space_raw("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After %ldKB write: "
							"sysapi_disk_space_raw() -> %d\n", write_size, dfree2_raw);
    	dfree2 = sysapi_disk_space("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After %ldKB write: "
							"sysapi_disk_space() -> %d\n", write_size, dfree2_raw);
		num_tests += 2;
	
	
		/* Raw free space after write */
		rval = check_free( dfree1_raw, dfree2_raw, write_size, tolerance, "Raw" );
		if ( rval < 0 ) {
			return_val = return_val | 1;
			break;
		} else if ( rval > 0 ) {
			num_warnings++;
		}

		/* Cooked free space after write */
		rval = check_free( dfree1, dfree2, write_size, tolerance, "Cooked" );
		if ( rval < 0 ) {
			return_val = return_val | 1;
			break;
		} else if ( rval > 0 ) {
			num_warnings++;
		}

		if (unlink(filename) < 0) {
			dprintf(D_ALWAYS, "SysAPI: Can't delete %s\n", filename);
			dprintf(D_ALWAYS, "SysAPI: Error reported: %s\n", strerror(errno));
			continue;
		}

	
    	dfree1_raw = sysapi_disk_space_raw("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After unlinking files: "
							"sysapi_disk_space_raw() -> %d\n", dfree1_raw);
    	dfree1 = sysapi_disk_space("/tmp");
    	dprintf(D_ALWAYS, "SysAPI: After unlinking files: "
							"sysapi_disk_space() -> %d\n",dfree1);
		num_tests += 2;
	
		// Raw free space after unlink
		if (dfree1_raw < 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Raw free space should never "
								"be negative!\n");
			return_val = return_val | 1;
			break;
		}
		else if ( (fabs((dfree1_raw-(dfree2_raw+write_size))/((double)dfree1_raw+1))) >
				  tolerance){
			dprintf(D_ALWAYS, "SysAPI: WARNING! Raw free space went from "
								"%d to %d, but probably shouldn't have "
								"changed more than %%%f tolerance. "
								"Another process may have used/freed space.\n",
								dfree2_raw, dfree1_raw, tolerance*100);
			num_warnings++;
		}

		// Cooked free space after unlink
		if (dfree1 < 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked free space should never "
								"be negative!\n");
			return_val = return_val | 1;
			break;
		}
		else if ( (fabs((dfree1-(dfree2+write_size))/((double)dfree1+1)))>tolerance){
			dprintf(D_ALWAYS, "SysAPI: WARNING! Raw free space went from "
								"%d to %d, but probably shouldn't have "
								"changed more than %%%f tolerance. "
								"Another process may have used/freed space.\n",
								dfree1, dfree2, tolerance*100);
			num_warnings++;
		}
	}

	if (((double)num_warnings/(double)num_tests) >= warn_ok_ratio) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning warn_ok exceeded "
								"(%2f%% warnings > %2f%% perc_warn_ok) .\n",
								((double)num_warnings/(double)num_tests)*100, 
								warn_ok_ratio*100);
			return_val = return_val | 1;
	}

	return return_val;
}
