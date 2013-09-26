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
#include "test.h"

#define KILO 1024
#define MEG (KILO*KILO)
#define GIG (MEG*KILO)

static double
k2g( double kbytes )
{
	return 0.000001 * kbytes;
}

static int
check_free(const char *type,
		   double before,
		   const char *before_label,
		   double after,
		   const char *after_label,
		   double write_size,
		   double tolerance )
{
	int		expected;				/* Expected free disk space */
	double	error;


	/* Should never go negative */
	if ( after < 0.0 ) {
		dprintf(D_ALWAYS,
				"SysAPI: ERROR! %s free space should never be negative!\n",
				type );
		return -1;
	}

	expected = before - write_size;
	error = ( after - expected ) / ( after + 0.1 );
	if ( fabs(error) > tolerance ) {
		dprintf(D_ALWAYS,
				"SysAPI: WARNING! %s free space went from "
				"%.0fk (%.2fg) [%s] to %.0fk (%.2fg) [%s], "
				"but probably shouldn't have "
				"changed more than %.2f%% tolerance. "
				"Another process may have used/freed space.\n",
				type,
				before, k2g(before), before_label,
				after, k2g(after), after_label,
				tolerance*100.0);
		dprintf(D_ALWAYS, "exp=%d (%.2fg), error=%.2f%%\n",
				expected, k2g(expected), 100.0*error );
		return 1;
	}

	return 0;
}

int free_fs_blocks_test(const char *dir, int trials, double tolerance,
						double warn_ok_ratio) {
	long long	dfree1_raw = 0;			/* "Raw" free disk space *before* write */
	long long	dfree1 = 0;				/* Free disk space *before* write */
	long long	dfree2_raw = 0;			/* "Raw" free disk space *after* write */
	long long	dfree2 = 0;				/* Free disk space *after* write */
	long long	dfree3_raw = 0;			/* "Raw" free disk space *after* unlink */
	long long	dfree3 = 0;				/* Free disk space *after* unlink */
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
	double	warn_ratio;

	sprintf(filename, "%s/sysapi_test_free_fs_blocks", dir);
	unlink( filename );

	dprintf(D_ALWAYS,
			"SysAPI: Doing %d trials by filling up %s by "
			"half (max 2G), testing & unlinking.\n", trials, dir );

	/*
	 * Check the space before doing anything
	 */

	sync( );
    dfree1_raw = sysapi_disk_space_raw(dir);
    dprintf(D_ALWAYS, "SysAPI: Initial sysapi_disk_space_raw(): %" PRIi64d "k (%.2fg)\n",
			dfree1_raw, k2g(dfree1_raw) );
    dfree1 = sysapi_disk_space(dir);
    dprintf(D_ALWAYS, "SysAPI: Initial sysapi_disk_space(): %" PRIi64d " (%.2fg)\n",
			dfree1, k2g(dfree1) );

	if (dfree1_raw < 0 || dfree1 < 0) {
		dprintf(D_ALWAYS, "SysAPI: ERROR! Disk space should never be "
				"negative.\n");
		return_val = return_val | 1;
	}

	for (i=0; i<trials; i++) {
		/* Take half the free space and fill it up */

		write_size = dfree1_raw / 2; /* in K */

		/* Here we deal with stupid 2 gig file size limit on ext2fs */
		if (write_size >= 2*1024*KILO) /* 2 gigs */
		{
			dprintf(D_ALWAYS,
					"Warning! Greater then 2 gigs requested size "
					"so I am assuming 2 gigs max for test!\n");

			write_size = 2*1024*KILO - 42*KILO; /* pick something smaller */
		}

		gigs = (write_size / 1024) / 1024;			/* total gigs */
		megs = (write_size / 1024) - (gigs * 1024); /* left over megs*/
		kilo = write_size % KILO; 					/* left over kilo */

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


		/*
		 * Check the space after writing the file
		 */

		sync( );
    	dfree2_raw = sysapi_disk_space_raw( dir );
    	dprintf(D_ALWAYS,
				"SysAPI: After %ldKB (%.2fg) write raw: %" PRIi64d "k (%.2fg)\n",
				write_size, k2g(write_size), dfree2_raw, k2g(dfree2_raw) );
    	dfree2 = sysapi_disk_space( dir );
    	dprintf(D_ALWAYS,
				"SysAPI: After %ldKB (%.2fg) write: %" PRIi64d "k (%.2fg)\n",
				write_size, k2g(write_size), dfree2, k2g(dfree2) );
		num_tests += 2;


		/* Check Raw free space */
		rval = check_free( "Raw", dfree1_raw, "before write",
						   dfree2_raw, "after write", write_size, tolerance );
		if ( rval < 0 ) {
			return_val = return_val | 1;
			break;
		} else if ( rval > 0 ) {
			num_warnings++;
		}

		/* Check cooked free space */
		rval = check_free( "Cooked", dfree1, "before write",
						   dfree2, "after write", write_size, tolerance );
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


		/*
		 * Check the space after unlinking the file
		 */

		sync( );
    	dfree3_raw = sysapi_disk_space_raw( dir );
    	dprintf(D_ALWAYS,
				"SysAPI: After unlinking files raw: %" PRIi64d "k (%.2fg)\n",
				dfree3_raw, k2g(dfree3_raw) );
    	dfree3 = sysapi_disk_space( dir );
    	dprintf(D_ALWAYS,
				"SysAPI: After unlinking files: %" PRIi64d "k (%.2fg)\n",
				dfree3, k2g( dfree3) );
		num_tests += 2;

		/* Check raw free space */
		rval = check_free( "Raw", dfree1_raw, "before write",
						   dfree3_raw, "after unlink", 0.0, tolerance );
		if ( rval < 0 ) {
			return_val = return_val | 1;
			break;
		} else if ( rval > 0 ) {
			num_warnings++;
		}

		/* Check cooked free space */
		rval = check_free( "Cooked", dfree1, "before write",
						   dfree3, "after unlink", 0.0, tolerance );
		if ( rval < 0 ) {
			return_val = return_val | 1;
			break;
		} else if ( rval > 0 ) {
			num_warnings++;
		}
	}

	warn_ratio = (double)num_warnings / (double)num_tests;
	if ( warn_ratio >= warn_ok_ratio) {
			dprintf(D_ALWAYS,
					"SysAPI: ERROR! Warning warn_ok exceeded "
					"(%.2f%% warnings > %.2f%% perc_warn_ok) .\n",
					100.0 * warn_ratio,
					100.0 * warn_ok_ratio );
			return_val = return_val | 1;
	}

	return return_val;
}
