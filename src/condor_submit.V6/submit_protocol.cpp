/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_string.h"
#include "spooled_job_files.h"
#include "subsystem_info.h"
#include "env.h"
#include "basename.h"
#include "condor_getcwd.h"
#include <time.h>
#include "write_user_log.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_distribution.h"
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#else
// WINDOWS only
#include "store_cred.h"
#endif
#include "internet.h"
#include "my_hostname.h"
#include "domain_tools.h"
#include "get_daemon_name.h"
#include "condor_qmgr.h"
#include "sig_install.h"
#include "access.h"
#include "daemon.h"
#include "match_prefix.h"

#include "extArray.h"
#include "HashTable.h"
#include "MyString.h"
#include "string_list.h"
#include "which.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "my_username.h"
#include "globus_utils.h"
#include "enum_utils.h"
#include "setenv.h"
#include "classad_hashtable.h"
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "dc_transferd.h"
#include "condor_ftp.h"
#include "condor_crontab.h"
#include <scheduler.h>
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"

#include "list.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "condor_md.h"
#include "submit_internal.h"

#include <algorithm>
#include <string>
#include <set>


ActualScheddQ::~ActualScheddQ()
{
	// The queue management protocol is finicky, for now we do NOT disconnect in the destructor.
	qmgr = NULL;
}

bool ActualScheddQ::Connect(DCSchedd & MySchedd, CondorError & errstack) {
	if (qmgr) return true;
	qmgr = ConnectQ(MySchedd.addr(), 0 /* default */, false /* default */, &errstack, NULL, MySchedd.version());
	return qmgr != NULL;
}

bool ActualScheddQ::disconnect(bool commit_transaction, CondorError & errstack) {
	bool rval = false;
	if (qmgr) {
		rval = DisconnectQ(qmgr, commit_transaction, &errstack);
	}
	qmgr = NULL;
	return rval;
}

int ActualScheddQ::get_NewCluster() { return NewCluster(); }
int ActualScheddQ::get_NewProc(int cluster_id) { return NewProc(cluster_id); }
int ActualScheddQ::destroy_Cluster(int cluster_id, const char *reason) { return DestroyCluster(cluster_id, reason); }

int ActualScheddQ::set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags) {
	return SetAttribute(cluster, proc, attr, value, flags);
}

int ActualScheddQ::set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags) {
	return SetAttributeInt(cluster, proc, attr, value, flags);
}

int ActualScheddQ::send_SpoolFileIfNeeded(ClassAd& ad) { return SendSpoolFileIfNeeded(ad); }
int ActualScheddQ::send_SpoolFile(char const *filename) { return SendSpoolFile(filename); }
int ActualScheddQ::send_SpoolFileBytes(char const *filename) { return SendSpoolFileBytes(filename); }

//====================================================================================
// functions for a simulate schedd q
//====================================================================================

SimScheddQ::SimScheddQ(int starting_cluster)
	: cluster(starting_cluster)
	, proc(-1)
	, close_file_on_disconnect(false)
	, fp(NULL)
{
}

SimScheddQ::~SimScheddQ()
{
	if (fp && close_file_on_disconnect) {
		fclose(fp);
	}
	fp = NULL;
}

bool SimScheddQ::Connect(FILE* _fp, bool close_on_disconnect) {
	ASSERT( ! fp);
	fp = _fp;
	close_file_on_disconnect = close_on_disconnect;
	return fp != NULL;
}

bool SimScheddQ::disconnect(bool /*commit_transaction*/, CondorError & /*errstack*/)
{
	if (fp && close_file_on_disconnect) {
		fclose(fp);
	}
	fp = NULL;
	return true;
}

int SimScheddQ::get_NewCluster() {
	proc = -1;
	return ++cluster;
}

int SimScheddQ::get_NewProc(int cluster_id) {
	ASSERT(cluster == cluster_id);
	if (fp) { fprintf (fp, "\n"); }
	return ++proc;
}

int SimScheddQ::destroy_Cluster(int cluster_id, const char * /*reason*/) {
	ASSERT(cluster_id == cluster);
	return 0;
}

int SimScheddQ::set_Attribute(int cluster_id, int proc_id, const char *attr, const char *value, SetAttributeFlags_t /*flags*/) {
	ASSERT(cluster_id == cluster);
	ASSERT(proc_id == proc || proc_id == -1);
	if (fp) {
		fprintf(fp, "%s=%s\n", attr, value);
	}
	return 0;
}
int SimScheddQ::set_AttributeInt(int cluster_id, int proc_id, const char *attr, int value, SetAttributeFlags_t /*flags*/) {
	ASSERT(cluster_id == cluster);
	ASSERT(proc_id == proc || proc_id == -1);
	if (fp) {
		fprintf(fp, "%s=%d\n", attr, value);
	}
	return 0;
}

int SimScheddQ::send_SpoolFileIfNeeded(ClassAd& /*ad*/) {
	return 0;
}
int SimScheddQ::send_SpoolFile(char const * /*filename*/) {
	return 0;
}
int SimScheddQ::send_SpoolFileBytes(char const * /*filename*/) {
	return 0;
}

