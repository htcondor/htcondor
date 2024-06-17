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
#include "condor_crontab.h"
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"
#include "shortfile.h"

#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "submit_internal.h"

#include <algorithm>
#include <string>
#include <set>

//====================================================================================
// functions for a simulate schedd q
//====================================================================================

SimScheddQ::SimScheddQ(int starting_cluster)
	: cluster(starting_cluster)
	, proc(-1)
	, close_file_on_disconnect(false)
	, log_all_communication(false)
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

bool SimScheddQ::Connect(FILE* _fp, bool close_on_disconnect, bool log_all) {
	ASSERT(! fp);
	fp = _fp;
	close_file_on_disconnect = close_on_disconnect;
	log_all_communication = log_all;
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

int SimScheddQ::get_NewCluster(CondorError & /*errstack*/) {
	proc = -1;
	if (log_all_communication) fprintf(fp, "::get_newCluster\n");
	return ++cluster;
}

int SimScheddQ::get_NewProc(int cluster_id) {
	ASSERT(cluster == cluster_id);
	if (fp) {
		if (log_all_communication) fprintf(fp, "::get_newProc\n");
		fprintf(fp, "\n");
	}
	return ++proc;
}

int SimScheddQ::destroy_Cluster(int cluster_id, const char * /*reason*/) {
	ASSERT(cluster_id == cluster);
	return 0;
}

int SimScheddQ::get_Capabilities(ClassAd & caps) {
	caps.Assign("LateMaterialize", true);
	caps.Assign("LateMaterializeVersion", 2);
	caps.Assign("UseJobsets", param_boolean("USE_JOBSETS", false));
	return true;
}
int SimScheddQ::get_ExtendedHelp(std::string &content) {
	auto_free_ptr helpfile = param("EXTENDED_SUBMIT_HELPFILE");
	if (helpfile) {
		htcondor::readShortFile(helpfile.ptr(), content);
	}
	return (int)content.size();
}

bool SimScheddQ::has_extended_submit_commands(ClassAd &cmds) {
	auto_free_ptr extended_cmds(param("EXTENDED_SUBMIT_COMMANDS"));
	if (extended_cmds) {
		initAdFromString(extended_cmds, cmds);
	}
	return cmds.size() > 0;
}
bool SimScheddQ::has_extended_help(std::string & filename) {
	return param(filename, "EXTENDED_SUBMIT_HELPFILE");
}

// hack for 8.7.8 testing
extern int attr_chain_depth;

int SimScheddQ::set_Attribute(int cluster_id, int proc_id, const char *attr, const char *value, SetAttributeFlags_t /*flags*/) {
	ASSERT(cluster_id == cluster);
	ASSERT(proc_id == proc || proc_id == -1);
	if (fp) {
		if (attr_chain_depth) fprintf(fp, "%d", attr_chain_depth - 1);
		if (log_all_communication) fprintf(fp, "::set(%d,%d) ", cluster_id, proc_id);
		fprintf(fp, "%s=%s\n", attr, value);
	}
	return 0;
}
int SimScheddQ::set_AttributeInt(int cluster_id, int proc_id, const char *attr, int value, SetAttributeFlags_t /*flags*/) {
	ASSERT(cluster_id == cluster);
	ASSERT(proc_id == proc || proc_id == -1);
	if (fp) {
		if (log_all_communication) fprintf(fp, "::int(%d,%d) ", cluster_id, proc_id);
		fprintf(fp, "%s=%d\n", attr, value);
	}
	return 0;
}


int SimScheddQ::set_Factory(int cluster_id, int qnum, const char * filename, const char * text) {
	ASSERT(cluster_id == cluster);
	if (fp) {
		if (log_all_communication) {
			fprintf(fp, "::setFactory(%d,%d,%s,%s) ", cluster_id, qnum, filename ? filename : "NULL", text ? "<text>" : "NULL");
			if (text) { fprintf(fp, "factory_text=%s\n", text); } else if (filename) { fprintf(fp, "factory_file=%s\n", filename); } else { fprintf(fp, "\n"); }
		}
	}
	return 0;
}

bool SimScheddQ::echo_Itemdata(const char * filename)
{
	echo_itemdata_filepath = filename;
	return true;
}

int SimScheddQ::send_Itemdata(int cluster_id, SubmitForeachArgs & o)
{
	ASSERT(cluster_id == cluster);
	if (fp && ! log_all_communication) {
		// normally get_NewProc would terminate the previous line, but if we get here
		// we know that we are doing a DashDryRun and get_NewProc will never be called
		// and we have just printed Dry-Run jobs(s) without a \n, so print the \n now.
		fprintf(fp, "\n");
	}
	if (o.items.size() > 0) {
		if (log_all_communication && fp) {
			fprintf(fp, "::sendItemdata(%d) %zu items", cluster_id, o.items.size());
		}
		if (!echo_itemdata_filepath.empty()) {

			int fd = safe_open_wrapper_follow(echo_itemdata_filepath.c_str(), O_WRONLY | _O_BINARY | O_CREAT | O_TRUNC | O_APPEND, 0644);
			if (fd == -1) {
				fprintf(stderr, "failed to open itemdata echo file %s : error %d - %s\n",
					echo_itemdata_filepath.c_str(), errno, strerror(errno));
			} else {

				o.foreach_mode = foreach_from;
				o.items_filename = echo_itemdata_filepath;

				// read and canonicalize the items and write them in 64k (ish) chunks
				// because that's what SendMaterializeData does
				const size_t cbAlloc = 0x10000;
				unsigned char buf[cbAlloc];
				int ix = 0;
				std::string item;
				int rval = 0;
				o.items_idx = 0;
				while (AbstractScheddQ::next_rowdata(&o, item) == 1) {
					if (item.size() + ix > cbAlloc) {
						if (ix == 0) { // single item > 64k !!!
							rval = -1;
							break;
						}
						int wrote = write(fd, buf, ix);
						if (wrote != ix) { fprintf(stderr, "write failure %d: %s\n", errno, strerror(errno)); }
						ix = 0;
					}
					memcpy(buf + ix, item.data(), item.size());
					ix += (int)item.size();
				}

				// write the remainder, if any
				if ((rval == 0) && ix) {
					int wrote = write(fd, buf, ix);
					if (wrote != ix) { fprintf(stderr, "write failure %d: %s\n", errno, strerror(errno)); }
				}
				close(fd);
			}
		}
	}
	return 0;
}

int SimScheddQ::send_Jobset(int cluster_id, const ClassAd * jobset_ad)
{
	ASSERT(cluster_id == cluster);
	if (fp) { 
		fprintf(fp, "::send_Jobset(%d,%p):\n", cluster_id, jobset_ad);
		if (jobset_ad) {
			std::string buf; buf.reserve(1000);
			fputs(formatAd(buf, *jobset_ad, "  ", nullptr, false), fp);
		}
	}
	return 0;
}

int SimScheddQ::send_SpoolFile(char const * filename) {
	if (fp) { fprintf(fp, "::send_SpoolFile: %s\n", filename); }
	return 0;
}
int SimScheddQ::send_SpoolFileBytes(char const * filename) {
	if (fp) { fprintf(fp, "::send_SpoolFileBytes: %s\n", filename); }
	return 0;
}
