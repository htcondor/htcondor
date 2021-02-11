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
#include "condor_qmgr.h"
#include "sig_install.h"
#include "access.h"
#include "daemon.h"
#include "match_prefix.h"

#include "extArray.h"
#include "MyString.h"
#include "string_list.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "my_username.h"
#include "globus_utils.h"
#include "enum_utils.h"
#include "setenv.h"
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "dc_transferd.h"
#include "condor_ftp.h"
#include "condor_crontab.h"
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"

#include "list.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "submit_protocol.h"
#include "submit_utils.h"

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
	allows_late = has_late = false;
	if (qmgr) {
		CondorVersionInfo cvi(MySchedd.version());
		if (cvi.built_since_version(8,7,1)) {
			has_late = true;
			allows_late = param_boolean("SCHEDD_ALLOW_LATE_MATERIALIZE",has_late);
		}
	}
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

int ActualScheddQ::init_capabilities() {
	int rval = 0;
	if ( ! tried_to_get_capabilities) {
		rval = GetScheddCapabilites(0, capabilities);
		tried_to_get_capabilities = true;

		// fetch late materialize caps from the capabilities ad.
		allows_late = has_late = false;
		if ( ! capabilities.LookupBool("LateMaterialize", allows_late)) {
			allows_late = has_late = false;
		} else {
			has_late = true; // schedd knows about late materialize
			int version = 1;
			if (capabilities.LookupInteger("LateMaterializeVersion", version) && version < 128) {
				late_ver = (char)version;
			} else {
				late_ver = 1;
			}
		}
	}
	return rval;
}
bool ActualScheddQ::has_late_materialize(int &ver) {
	init_capabilities();
	ver = late_ver;
	return has_late;
}
bool ActualScheddQ::allows_late_materialize() {
	init_capabilities();
	return allows_late;
}
int ActualScheddQ::get_Capabilities(ClassAd & caps) {
	int rval = init_capabilities();
	if (rval == 0) {
		caps.Update(capabilities);
	}
	return rval;
}

int ActualScheddQ::set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags) {
	return SetAttribute(cluster, proc, attr, value, flags);
}

int ActualScheddQ::set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags) {
	return SetAttributeInt(cluster, proc, attr, value, flags);
}

int ActualScheddQ::set_Factory(int cluster, int qnum, const char * filename, const char * text) {
	return SetJobFactory(cluster, qnum, filename, text);
}

// helper function used as 3rd argument to SendMaterializeData.
// it treats pv as a pointer to SubmitForeachArgs, calls next() on it and then formats the
// resulting rowdata for SendMaterializeData to use.  This could be a free function
// I made it part of the AbstractScheddQ just to put it in a namespace.
//
/*static*/ int AbstractScheddQ::next_rowdata(void* pv, std::string & rowdata) {
	SubmitForeachArgs &fea = *((SubmitForeachArgs *)pv);

	rowdata.clear();
	char *str = fea.items.next();
	if ( ! str)
		return 0;

	// check to see if the data is already using the ASCII 'unit separator' character
	// if not, then we want to split and re-assemble multi field data using US as the separator
	bool got_US = strchr(str, '\x1F') != NULL;

	// we only need to split and re-assemble the field data if there are multiple fields
	if (fea.vars.number() > 1 && ! got_US) {
		auto_free_ptr tmp(strdup(str));
		std::vector<const char *> splits;
		if (fea.split_item(tmp.ptr(), splits) <= 0)
			return -1;
		for (auto it = splits.begin(); it != splits.end(); ++it) {
			if ( ! rowdata.empty()) rowdata += "\x1F";
			rowdata += *it;
		}
	} else {
		rowdata = str;
	}
	// terminate the data with a newline if it does not already have one.
	size_t cch = rowdata.size();
	if ( ! cch || rowdata[cch-1] != '\n') { rowdata += "\n"; }

	return 1;
}

int ActualScheddQ::send_Itemdata(int cluster_id, SubmitForeachArgs & o)
{
	if (o.items.number() > 0) {
		int row_count = 0;
		o.items.rewind();
		int rval = SendMaterializeData(cluster_id, 0, AbstractScheddQ::next_rowdata, &o, o.items_filename, &row_count);
		if (rval) return rval;
		if (row_count != o.items.number()) {
			fprintf(stderr, "\nERROR: schedd returned row_count=%d after spooling %d items\n", row_count, o.items.number());
			return -1;
		}
		o.foreach_mode = foreach_from;
	}
	return 0;
}

/*
int ActualScheddQ::set_Foreach(int cluster, int itemnum, const char * filename, const char * text) {
	return SetMaterializeData(cluster, itemnum, filename, text);
}
*/

int ActualScheddQ::send_SpoolFile(char const *filename) { return SendSpoolFile(filename); }
int ActualScheddQ::send_SpoolFileBytes(char const *filename) { return SendSpoolFileBytes(filename); }

