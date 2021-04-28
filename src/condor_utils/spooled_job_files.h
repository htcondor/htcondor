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

#ifndef _SPOOLED_JOB_FILES_H
#define _SPOOLED_JOB_FILES_H

#include <string>
#include "condor_uid.h"
#include "condor_classad.h"


class SpooledJobFiles {
private:
	static void _getJobSpoolPath(int cluster, int proc, const classad::ClassAd *job_ad, std::string &spool_path);

public:
		/* Query the path of the spool directory for the given job.
		 * The first form is deprecated, as it doesn't consider
		 * alternate spool directory configuration.
		 */
	static void getJobSpoolPath(int cluster,int proc,std::string &spool_path);
	static void getJobSpoolPath(const classad::ClassAd *job_ad, std::string &spool_path);

		/* Create the spool directory and/or chown the spool directory
		   to the desired ownership.  The shared parent spool directories
		   are also created if they do not already exist.  For standard
		   universe, only the parent spool directories are created,
		   not per-job directories, because standard universe does not
		   need per-job directories (it has checkpoint files of the same
		   name and path as the per-job directories).
		   If CHOWN_JOB_SPOOL_FILES=False, desired_priv_state is forced
		   to PRIV_USER.
		 */
	static bool createJobSpoolDirectory(classad::ClassAd const *job_ad,priv_state desired_priv_state );

		/* Like createJobSpoolDirectory, but just create the .swap directory.
		 * Assumes the other (parent) directories have already been created.
		 */
	static bool createJobSwapSpoolDirectory(classad::ClassAd const *job_ad,priv_state desired_priv_state );

		/* Create the shared spool directories but not the actual
		 * per-job directories.
		 */
	static bool createParentSpoolDirectories(classad::ClassAd const *job_ad);

		/* Remove the spool directory belonging to a job.
		 * Also removes the .tmp and .swap directories.
		 * This also removes the shared proc directory from the
		 * hierarchy if possible.
		 */
	static void removeJobSpoolDirectory( classad::ClassAd * ad);

		/* Remove the .swap spool directory belonging to a job.
		 */
	static void removeJobSwapSpoolDirectory( classad::ClassAd * ad);

		/* Remove files spooled for a job cluster.
		 * This also removes the shared cluster directory from the
		 * hierarchy if possible.
		 */
	static void removeClusterSpooledFiles(int cluster, const char * submit_digest=NULL);

		/* Restore ownership of spool directory to condor after job ran.
		   Returns true on success.
		   If CHOWN_JOB_SPOOL_FILES=False, this function does nothing.
		 */
	static bool chownSpoolDirectoryToCondor(classad::ClassAd const *job_ad);

		/* Returns true if this job requires a spool directory.
		 */
	static bool jobRequiresSpoolDirectory(classad::ClassAd const *job_ad);
};

/* Given the location of the SPOOL directory and a job id, return the
 * directory where the job's sandbox should reside if the input sandbox
 * is staged to the schedd.
 * If ICKPT (-1) is given for proc, then return where the job executable
 * should reside if the submitter spooled the executable but not the input
 * sandbox.
 * The buffer returned must be deallocated with free().
 */
char *gen_ckpt_name ( char const *dir, int cluster, int proc, int subproc );

/* Given a job cluster id and SPOOL directory, return the path where
 * the job executable should reside if the submitter spooled the
 * executable apart from the input sandbox (i.e. if copy_to_spool is
 * true in the submit file).
 * If the SPOOL directory argument is NULL, then the SPOOL parameter
 * will be looked up.
 * The buffer returned must be deallocated with free().
 */
char *GetSpooledExecutablePath( int cluster, const char *dir = NULL );

/* Given a job cluster id and SPOOL directory, return the path where
 * the job submit digest should reside if the submitter spooled it
 * If the SPOOL directory argument is NULL, then the SPOOL parameter
 * will be looked up.
 */
void GetSpooledSubmitDigestPath(std::string &path, int cluster, const char *dir = NULL );

/* Given a job cluster id and SPOOL directory, return the path where
 * the job submit digest QUEUE from itemdata file should reside if the submitter spooled it
 * If the SPOOL directory argument is NULL, then the SPOOL parameter
 * will be looked up.
 */
void GetSpooledMaterializeDataPath(std::string &path, int cluster, const char *dir = NULL );

/* Given a job ad, determine where the job's executable resides.
 * If the filename given by gen_ckpt_name(SPOOL,cluster,ICKPT,0) exists,
 * then that is returned.
 * Otherwise, the filename is constructed from the Cmd and Iwd attributes
 * in the job ad. Existence of this file isn't checked.
 */
void GetJobExecutable( const classad::ClassAd *job_ad, std::string &executable );

#endif
