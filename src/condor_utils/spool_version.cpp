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
#include "condor_config.h"
#include "spool_version.h"

void
CheckSpoolVersion(char const *spool, int spool_min_version_i_support, int spool_cur_version_i_support, int &spool_min_version,int &spool_cur_version)
{
	spool_min_version = 0; // before 7.5.5 there was no version stamp
	spool_cur_version = 0;

	std::string vers_fname;
	formatstr(vers_fname,"%s%cspool_version",spool,DIR_DELIM_CHAR);

	FILE *vers_file = safe_fopen_wrapper_follow(vers_fname.c_str(),"r");
	if( vers_file ) {
		if( 1 != fscanf(vers_file,
						"minimum compatible spool version %d\n",
						&spool_min_version) )
		{
			EXCEPT("Failed to find minimum compatible spool version in %s\n",
				   vers_fname.c_str());
		}
		if( 1 != fscanf(vers_file,
						"current spool version %d\n",
						&spool_cur_version) )
		{
			EXCEPT("Failed to find current spool version in %s\n",
				   vers_fname.c_str());
		}
		fclose(vers_file);
	}

	dprintf(D_FULLDEBUG,"Spool format version requires >= %d (I support version %d)\n",
			spool_min_version,
			spool_cur_version_i_support);
	dprintf(D_FULLDEBUG,"Spool format version is %d (I require version >= %d)\n",
			spool_min_version,
			spool_min_version_i_support);

	if( spool_min_version > spool_cur_version_i_support ) {
		EXCEPT("According to %s, the SPOOL directory requires that I support spool version %d, but I only support %d.\n",
			   vers_fname.c_str(),
			   spool_min_version,
			   spool_cur_version_i_support);
	}
	if( spool_cur_version < spool_min_version_i_support ) {
		EXCEPT("According to %s, the SPOOL directory is written in spool version %d, but I only support versions back to %d.\n",
			   vers_fname.c_str(),
			   spool_cur_version,
			   spool_min_version_i_support);
	}
}

void CheckSpoolVersion(
	int spool_min_version_i_support,
	int spool_cur_version_i_support)
{
	std::string spool;
	int spool_min_version;
	int spool_cur_version;
	ASSERT( param(spool,"SPOOL") );

	CheckSpoolVersion(spool.c_str(),spool_min_version_i_support,spool_cur_version_i_support,spool_min_version,spool_cur_version);
}

void
WriteSpoolVersion(char const *spool,int spool_min_version_i_write,int spool_cur_version_i_support) {
	std::string vers_fname;
	formatstr(vers_fname,"%s%cspool_version",spool,DIR_DELIM_CHAR);

	FILE *vers_file = safe_fcreate_replace_if_exists(vers_fname.c_str(),"w");
	if( !vers_file ) {
		EXCEPT("Failed to open %s for writing.\n",vers_fname.c_str());
	}
	if( fprintf(vers_file,"minimum compatible spool version %d\n",
				spool_min_version_i_write) < 0 ||
		fprintf(vers_file,"current spool version %d\n",
				spool_cur_version_i_support) < 0 ||
		fflush(vers_file) != 0 ||
		fsync(fileno(vers_file)) != 0 ||
		fclose(vers_file) != 0 )
	{
		EXCEPT("Error writing spool version to %s\n",vers_fname.c_str());
	}
}
