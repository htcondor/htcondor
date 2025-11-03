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
#include "ad_printmask.h"
#include "condor_arglist.h"
#include "basename.h"
#include "condor_state.h"
#include "format_time.h"
#include "condor_q.h"
#include "proc.h"
#include "condor_config.h"
#include "metric_units.h"
#include "ipv6_hostname.h"
#include "internet.h"
#include <string>
#include <iterator>

/*
*	This is for tools print tables to have a singular globaly shared
*	attribute print information formatting. The original tables are 
*	from HTCondor V9.9 condor_q.V6/queue.cpp, condor_tools/history.cpp, 
*	and condor_status.V6/prettyPrint.cpp. 
*
*	Note: Most of these Functions were written by John (TJ) Knoller
*	      and not Cole Bollig (I just moved them)
*
*	To add print format option:
*		1. Add a table item to the Global table at bottom of file
*		2. Make a render function in 'Render/Format Functions' section
*			a. Add functions called by render_/format_ functions
*			   in the 'Helper Functions' section.
*
*	If you want to have any of these functions to be overwritten for a certain
*	tools print mask, then make a local print formats table and custom functions
*	(append functions with _local) in the tool. See queue.cpp for reference.
*/

//================================Helper Functions===================================
//	Format functions are functions called by the render functions to find and
//	format value that will be returned for display. These only need to exist locally

const char* digest_state_and_activity (char * sa, State st, Activity ac)
{
	const char state_letters[] = "~OUMCPSXFD#?";
	const char act_letters[] = "0ibrvsek#?";

	sa[1] = sa[0] = ' ';
	sa[2] = 0;
	if (st > no_state && st <= _state_threshold_) {
		sa[0] = state_letters[st];
	}
	if (ac > no_act && ac <= _act_threshold_) {
		sa[1] = act_letters[ac];
	}
	return sa;
}

const char * extractStringsFromList ( const classad::Value & value, Formatter &, std::string &prettyList ) {
	const classad::ExprList * list = NULL;
	if( ! value.IsListValue( list ) ) {
		return "[Attribute not a list.]";
	}

	prettyList.clear();
	classad::ExprList::const_iterator i = list->begin();
	for( ; i != list->end(); ++i ) {
		std::string item;
		if (dynamic_cast<classad::Literal *>(*i) == nullptr) continue;
		classad::Value val;
		reinterpret_cast<classad::Literal*>(*i)->GetValue(val);
		if (val.IsStringValue(item)) {
			prettyList += item + ", ";
		}
	}
	if( prettyList.length() > 0 ) {
		prettyList.erase( prettyList.length() - 2 );
	}

	return prettyList.c_str();
}

// extract the set of unique items from the given list.
const char * extractUniqueStrings ( const classad::Value & value, Formatter &, std::string &list_out ) {
	std::set<std::string> uniq;

	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );

	const classad::ExprList * list = NULL;
	if (value.IsListValue(list)) {
		// for lists, unparse each item into the uniqueness set.
		for(classad::ExprList::const_iterator it = list->begin() ; it != list->end(); ++it ) {
			std::string item;
			if (dynamic_cast<classad::Literal *>(*it) == nullptr) {
				unparser.Unparse( item, *it );
			} else {
				classad::Value val;
				reinterpret_cast<classad::Literal*>(*it)->GetValue(val);
				if ( ! val.IsStringValue(item)) {
					unparser.Unparse( item, *it );
				}
			}
			uniq.insert(item);
		}
	} else if (value.IsStringValue(list_out)) {
		// for strings, parse as a string list, and add each unique item into the set
		for (auto& psz: StringTokenIterator(list_out)) {
			uniq.insert(psz);
		}
	} else {
		// for other types treat as a single item, no need to uniqify
		list_out.clear();
		ClassAdValueToString(value, list_out);
		return list_out.c_str();
	}

	list_out.clear();
	for (std::set<std::string>::const_iterator it = uniq.begin(); it != uniq.end(); ++it) {
		if (list_out.empty()) list_out = *it;
		else {
			list_out += ", ";
			list_out += *it;
		}
	}

	return list_out.c_str();
}

static const bool platform_includes_arch = true;
bool format_platform_name (std::string & str, ClassAd* al)
{
	std::string opsys, arch;
	bool got_name = false;
	if (al->LookupString(ATTR_OPSYS, opsys) && opsys == "WINDOWS") {
		got_name = al->LookupString(ATTR_OPSYS_SHORT_NAME, opsys);
	} else {
		got_name = al->LookupString(ATTR_OPSYS_AND_VER, opsys);
	}
	if (got_name) {
		if (platform_includes_arch) {
			al->LookupString(ATTR_ARCH, str);
			if (str == "X86_64") str = "x64";
			else if (str == "X86") str = "x86";
			str += "/";
			str += opsys;
		} else {
			str = opsys;
		}
		return true;
	}
	return false;
}

// extract version and build id from $CondorVersion string
const char * format_version (const char * condorver, Formatter & fmt)
{
	bool no_build_id = !(fmt.options & FormatOptionAutoWidth) && (fmt.width > -10 && fmt.width < 10);
	static char version_ret[9+12+2];
	char * r = version_ret;
	char * rend = version_ret+sizeof(version_ret)-2;
	const char * p = condorver;
	while (*p && *p != ' ') ++p;  // skip $CondorVersion:
	while (*p == ' ') ++p;
	while (*p && *p != ' ') {
		if (r < rend) *r++ = *p++; // copy X.Y.Z version
		else p++;
	}
	while (*p == ' ') ++p;
	// check for date in YYYY-MM-DD format
	if (strchr(p, '-') == p+4 && strchr(p+5, '-') == p+7) {
		while (*p && *p != ' ') ++p; // skip YYYY-MM-DD
	} else {
		// old date format Mon DD YYYY
		while (*p && *p != ' ') ++p; // skip Feb
		while (*p == ' ') ++p;
		while (*p && *p != ' ') ++p; // skip 12
		while (*p == ' ') ++p;
		while (*p && *p != ' ') ++p; // skip 2016
	}
	while (*p == ' ') ++p;
	// *p now points to a BuildId:, or "PRE-RELEASE"
	if (*p == 'B') {
		while (*p && *p != ' ') ++p; // skip BuildId:
		while (*p == ' ') ++p;
	}
	if (*p != '$' && ! no_build_id) {
		*r++ = '.';
		while (*p && *p != '-' && *p != ' ') {
			if (r < rend) *r++ = *p++; // copy PRE or NNNNN buildid
			else p++;
		}
	}
	*r = 0;
	return version_ret;
}

//================================Render/Format Functions===================================
//	Main functions that will be called by the format table to render values
//	for various attributes. These need to have function prototypes in ad_printmask.h

bool render_batch_name (std::string & out, ClassAd *ad, Formatter & /*fmt*/)
{

	int universe = 0;
	std::string tmp;
	if (ad->LookupString(ATTR_JOB_BATCH_NAME, out)) {
		// got it.
	} else if (ad->LookupInteger(ATTR_JOB_UNIVERSE, universe) && universe == CONDOR_UNIVERSE_SCHEDULER) {
		// set batch name to dag id, but not if we allow folding of multiple root dagmans into a single batchname
		int cluster = 0;
		ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
		formatstr(out, "DAG: %d", cluster);
	} else if (ad->LookupExpr(ATTR_DAGMAN_JOB_ID)
				&& ad->LookupString(ATTR_DAG_NODE_NAME, out)) {
		out.insert(0,"NODE: ");
#if 1 // don't batch jobs by exe
#else // batch jobs that have a common exe together
	} else if (ad->LookupString(ATTR_JOB_CMD, tmp)) {
		const char * name = tmp.c_str();
		if (tmp.length() > 24) { name = condor_basename(name); }
		formatstr(out, "CMD: %s", name);
#endif
	} else {
		return false;
	}
	return true;
}

bool render_buffer_io_misc (std::string & misc, ClassAd *ad, Formatter & /*fmt*/)
{
	misc.clear();

	int ix = 0;
	bool bb = false;
	ad->LookupBool(ATTR_TRANSFERRING_INPUT, bb);
	ix += bb?1:0;

	bb = false;
	ad->LookupBool(ATTR_TRANSFERRING_OUTPUT,bb);
	ix += bb?2:0;

	bb = false;
	ad->LookupBool(ATTR_TRANSFER_QUEUED,bb);
	ix += bb?4:0;

	if (ix) {
		const char * const ax[] = { "in", "out", "in,out", "queued", "in,queued", "out,queued", "in,out,queued" };
		formatstr(misc, " transfer=%s", ax[ix-1]);
	}

	return true;
}

bool render_cpu_util (double & cputime, ClassAd *ad, Formatter & /*fmt*/)
{
	if ( ! ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, cputime))
		return false;

	int ckpt_time = 0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time);
	if (ckpt_time == 0) return false;
	double util = cputime/ckpt_time*100.0;
	if (util > 100.0) util = 100.0;
	else if (util < 0.0) return false;
	cputime = util;
	// printf(result_format, "  %6.1f%%", util);
	return true;
}

bool render_dag_owner (std::string & out, ClassAd *ad, Formatter & fmt)
{
	if (ad->LookupExpr(ATTR_DAGMAN_JOB_ID)) {
		if (ad->LookupString(ATTR_DAG_NODE_NAME, out)) {
			return true;
		} else {
			fprintf(stderr, "DAG node job with no %s attribute!\n", ATTR_DAG_NODE_NAME);
		}
	}
	return render_owner(out, ad, fmt);
}

bool render_owner (std::string & out, ClassAd *ad, Formatter & /*fmt*/)
{
	if ( ! ad->LookupString(ATTR_OWNER, out))
		return false;

	return true;
}

bool render_grid_job_id (std::string & jid, ClassAd *ad, Formatter & /*fmt*/ )
{
	std::string str;
	std::string host;

	if ( ! ad->LookupString(ATTR_GRID_JOB_ID, str))
		return false;

	std::string grid_type = "globus";
	char grid_res[64];
	if (ad->LookupString( ATTR_GRID_RESOURCE, grid_res, COUNTOF(grid_res) )) {
		char * r = grid_res;
		while (*r && *r != ' ') {
			++r;
		}
		*r = 0;
		grid_type = grid_res;
	}
	bool gram = (MATCH == grid_type.compare("gt5")) || (MATCH == grid_type.compare("gt2"));

	size_t ix2 = str.find_last_of(" ");
	ix2 = (ix2 < str.length()) ? ix2 + 1 : 0;

	size_t ix3 = str.find("://", ix2);
	ix3 = (ix3 < str.length()) ? ix3+3 : ix2;
	size_t ix4 = str.find_first_of("/",ix3);
	ix4 = (ix4 < str.length()) ? ix4 : ix3;
	host = str.substr(ix3, ix4-ix3);

	if (gram) {
		jid = host;
		jid += " : ";
		if (str[ix4] == '/') ix4 += 1;
		size_t ix5 = str.find_first_of("/",ix4);
		jid = str.substr(ix4, ix5-ix4);
		if (ix5 < str.length()) {
			if (str[ix5] == '/') ix5 += 1;
			size_t ix6 = str.find_first_of("/",ix5);
			jid += ".";
			jid += str.substr(ix5, ix6-ix5);
		}
	} else {
		jid.clear();
		//jid = grid_type;
		//jid += " : ";
		jid += str.substr(ix4);
	}

	return true;
}

bool render_grid_resource (std::string & result, ClassAd * ad, Formatter & /*fmt*/ )
{
	std::string grid_type;
	std::string str;
	std::string mgr = "[?]";
	std::string host = "[???]";
	const bool fshow_host_port = false;

	if ( ! ad->LookupString(ATTR_GRID_RESOURCE, str))
		return false;
	/* GridResource is a string with the format 
	*       "type host_url manager" (where manager can contain whitespace)
	*  or   "type host_url/jobmanager-manager"
	*/
	size_t ixHost = str.find_first_of(' ');
	if (ixHost < str.length()) {
		grid_type = str.substr(0, ixHost);
		ixHost += 1; // skip over space.
	} else {
		grid_type = "globus";
		ixHost = 0;
	}

	size_t ix2 = str.find_first_of(' ', ixHost);
	if (ix2 < str.length()) {
		mgr = str.substr(ix2+1);
	} else {
		size_t ixMgr = str.find("jobmanager-", ixHost);
		if (ixMgr < str.length()) 
			mgr = str.substr(ixMgr+11);	//sizeof("jobmanager-") == 11
		ix2 = ixMgr;
	}

	size_t ix3 = str.find("://", ixHost);
	ix3 = (ix3 < str.length()) ? ix3+3 : ixHost;
	size_t ix4 = str.find_first_of(fshow_host_port ? "/" : ":/",ix3);
	if (ix4 > ix2) ix4 = ix2;
	host = str.substr(ix3, ix4-ix3);

	replace_str(mgr, " ", "/");

	char result_str[1024];
	if( MATCH == grid_type.compare( "ec2" ) ) {
		// mgr = str.substr( ixHost, ix3 - ixHost - 3 );
		char rvm[MAXHOSTNAMELEN];
		if( ad->LookupString( ATTR_EC2_REMOTE_VM_NAME, rvm, sizeof( rvm ) ) ) {
			host = rvm;
		}
		snprintf( result_str, 1024, "%s %s", grid_type.c_str(), host.c_str() );
	} else {
		snprintf(result_str, 1024, "%s->%s %s", grid_type.c_str(), mgr.c_str(), host.c_str());
	}
	result_str[COUNTOF(result_str)-1] = 0;

	ix2 = strlen(result_str);
	result_str[ix2] = 0;
	result = result_str;
	return true;
}

bool render_grid_status ( std::string & result, ClassAd * ad, Formatter & /* fmt */ )
{
	if (ad->LookupString(ATTR_GRID_JOB_STATUS, result)) {
		return true;
	} 

	int jobStatus;
	if ( ! ad->LookupInteger(ATTR_GRID_JOB_STATUS, jobStatus))
		return false;

	static const struct {
		int status;
		const char * psz;
	} states[] = {
		{ IDLE, "IDLE" },
		{ RUNNING, "RUNNING" },
		{ COMPLETED, "COMPLETED" },
		{ HELD, "HELD" },
		{ SUSPENDED, "SUSPENDED" },
		{ REMOVED, "REMOVED" },
		{ TRANSFERRING_OUTPUT, "XFER_OUT" },
		{ JOB_STATUS_FAILED, "FAILED" },
		{ JOB_STATUS_BLOCKED, "BLOCKED" },
	};
	for (size_t ii = 0; ii < COUNTOF(states); ++ii) {
		if (jobStatus == states[ii].status) {
			result = states[ii].psz;
			return true;
		}
	}
	formatstr(result, "%d", jobStatus);
	return true;
}

bool render_hist_runtime (std::string & out, ClassAd * ad, Formatter & /*fmt*/)
{
	double utime;
	if(!ad->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK,utime)) {
		if(!ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU,utime)) {
			utime = 0;
		}
	}
	out = format_time((time_t)utime);
	return (time_t)utime != 0;
}

bool render_job_cmd_and_args (std::string & val, ClassAd * ad, Formatter & /*fmt*/)
{
	if ( ! ad->LookupString(ATTR_JOB_CMD, val))
		return false;

	std::string args;
	if (ad->LookupString (ATTR_JOB_ARGUMENTS1, args) || 
		ad->LookupString (ATTR_JOB_ARGUMENTS2, args)) {
		val += " ";
		val += args;
	}
	return true;
}

bool render_job_description (std::string & out, ClassAd *ad, Formatter &)
{
	if ( ! ad->LookupString(ATTR_JOB_CMD, out))
		return false;

	std::string description;
	if ( ! ad->LookupString("MATCH_EXP_" ATTR_JOB_DESCRIPTION, description)) {
		ad->LookupString(ATTR_JOB_DESCRIPTION, description);
	}
	if ( ! description.empty()) {
		formatstr(out, "(%s)", description.c_str());
	} else {
		std::string put_result = condor_basename(out.c_str());
		std::string args_string;
		ArgList::GetArgsStringForDisplay(ad,args_string);
		if ( ! args_string.empty()) {
			formatstr_cat(put_result, " %s", args_string.c_str());
		}
		out = put_result;
	}
	return true;
}

const char * format_job_factory_mode (const classad::Value &val, Formatter &)
{
	if (val.IsUndefinedValue()) return "";
	int pause_mode = 0;
	if (val.IsNumber(pause_mode)) {
		switch(pause_mode) {
		case -1: return "Errs";  // InvalidSubmit
		case 0:  return "Norm";  // Running
		case 1:  return "Held";  // Hold
		case 2:  return "Done";  // Done
		case 3:  return "Gone";  // Removed
		default: return "Unk";   // 
		}
	} else {
		return "????";
	}
}

bool render_job_id (std::string & result, ClassAd* ad, Formatter &)
{
	char str[PROC_ID_STR_BUFLEN];
	int cluster_id=0, proc_id=0;
	if ( ! ad->LookupInteger(ATTR_CLUSTER_ID, cluster_id))
		return false;
	ad->LookupInteger(ATTR_PROC_ID,proc_id);
	ProcIdToStr(cluster_id, proc_id, str);
	result = str;
	return true;
}

bool render_job_status_char (std::string & result, ClassAd*ad, Formatter &)
{
	int job_status;
	if ( ! ad->LookupInteger(ATTR_JOB_STATUS, job_status))
		return false;

	char put_result[3];
	put_result[1] = ' ';
	put_result[2] = 0;

	put_result[0] = encode_status(job_status);
	//Global Table assumes suspension is False
	// adjust status field to indicate file transfer status
	bool transferring_input = false;
	bool transferring_output = false;
	bool transfer_queued = false;
	ad->LookupBool(ATTR_TRANSFERRING_INPUT,transferring_input);
	ad->LookupBool(ATTR_TRANSFERRING_OUTPUT,transferring_output);
	ad->LookupBool(ATTR_TRANSFER_QUEUED,transfer_queued);
	if( transferring_input ) {
		put_result[0] = '<';
		put_result[1] = transfer_queued ? 'q' : ' ';
	}
	if( transferring_output || job_status == TRANSFERRING_OUTPUT) {
		put_result[0] = transfer_queued ? 'q' : ' ';
		put_result[1] = '>';
	}
	result = put_result;
	return true;
}

const char * format_job_status_raw (long long job_status, Formatter &)
{
	switch(job_status) {
	case IDLE:      return "Idle   ";
	case RUNNING:   return "Running";
	case REMOVED:   return "Removed";
	case COMPLETED: return "Complet";
	case HELD:      return "Held   ";
	case TRANSFERRING_OUTPUT: return "XFerOut";
	case SUSPENDED: return "Suspend";
	case JOB_STATUS_FAILED: return "Failed ";
	case JOB_STATUS_BLOCKED: return "Blocked";
	default:        return "Unk    ";
	}
}

const char * format_job_status_char (long long status, Formatter & /*fmt*/)
{
	const char * ret = " ";
	switch( status ) 
	{
	case IDLE:      ret = "I"; break;
	case RUNNING:   ret = "R"; break;
	case REMOVED:   ret = "X"; break;
	case COMPLETED: ret = "C"; break;
	case JOB_STATUS_FAILED: ret = "F"; break;
	case JOB_STATUS_BLOCKED: ret = "B"; break;
	case TRANSFERRING_OUTPUT: ret = ">"; break;
	}
	return ret;
}

const char * format_job_universe (long long job_universe, Formatter &)
{
	return CondorUniverseNameUcFirst(job_universe);
}

bool render_memory_usage (double & mem_used_mb, ClassAd *ad, Formatter &)
{
	long long  image_size;
	long long memory_usage;
	// print memory usage unless it's unavailable, then print image size
	// note that memory usage is megabytes but imagesize is kilobytes.
	if (ad->LookupInteger(ATTR_MEMORY_USAGE, memory_usage)) {
		mem_used_mb = memory_usage;
	} else if (ad->LookupInteger(ATTR_IMAGE_SIZE, image_size)) {
		mem_used_mb = image_size / 1024.0;
	} else {
		return false;
	}
	return true;
}

const char * format_real_date (long long dt, Formatter &) //Same function as other date functions
{
	return format_date(dt);
}

const char * format_readable_bytes (const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi;
	} else if (val.IsRealValue(kb)) {
		// nothing to do.
	} else {
		return "        ";
	}
	return metric_units(kb);
}

const char * format_readable_kb (const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi*1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

const char * format_readable_mb (const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi * 1024.0 * 1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0 * 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

bool render_remote_host (std::string & result, ClassAd *ad, Formatter &)
{
	//static char host_result[MAXHOSTNAMELEN];
	//static char unknownHost [] = "[????????????????]";
	condor_sockaddr addr;

	int universe = CONDOR_UNIVERSE_VANILLA;
	ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	if (universe == CONDOR_UNIVERSE_GRID) {
		if (ad->LookupString(ATTR_EC2_REMOTE_VM_NAME,result) == 1)
			return true;
		else if (ad->LookupString(ATTR_GRID_RESOURCE,result) == 1 )
			return true;
		else
			return false;
	}

	if (ad->LookupString(ATTR_REMOTE_HOST, result) == 1) {
		if( is_valid_sinful(result.c_str()) && addr.from_sinful(result.c_str()) == true ) {
			result = get_hostname(addr);
			return result.length() > 0;
		}
		return true;
	}
	return false;
}

bool render_goodput (double & goodput_time, ClassAd *ad, Formatter & /*fmt*/)
{
	int job_status;
	if ( ! ad->LookupInteger(ATTR_JOB_STATUS, job_status))
		return false;

	time_t ckpt_time = 0;
	time_t shadow_bday = 0;
	time_t last_ckpt = 0;
	double wall_clock = 0.0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time );
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	ad->LookupInteger( ATTR_LAST_CKPT_TIME, last_ckpt );
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock );
	if ((job_status == RUNNING || job_status == TRANSFERRING_OUTPUT || job_status == SUSPENDED) && shadow_bday && last_ckpt > shadow_bday) {
		wall_clock += last_ckpt - shadow_bday;
	}
	if (wall_clock <= 0.0) return false;

	goodput_time = ckpt_time/wall_clock*100.0;
	if (goodput_time > 100.0) goodput_time = 100.0;
	else if (goodput_time < 0.0) return false;
	//sprintf(put_result, " %6.1f%%", goodput_time);
	return true;
}

bool render_mbps (double & mbps, ClassAd *ad, Formatter & /*fmt*/)
{
	double bytes_sent;
	if ( ! ad->LookupFloat(ATTR_BYTES_SENT, bytes_sent))
		return false;

	double wall_clock=0.0, bytes_recvd=0.0, total_mbits;
	time_t shadow_bday = 0;
	time_t last_ckpt = 0;
	int job_status = IDLE;
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock );
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	ad->LookupInteger( ATTR_LAST_CKPT_TIME, last_ckpt );
	ad->LookupInteger( ATTR_JOB_STATUS, job_status );
	if ((job_status == RUNNING || job_status == TRANSFERRING_OUTPUT || job_status == SUSPENDED) && shadow_bday && last_ckpt > shadow_bday) {
		wall_clock += last_ckpt - shadow_bday;
	}
	ad->LookupFloat(ATTR_BYTES_RECVD, bytes_recvd);
	total_mbits = (bytes_sent+bytes_recvd)*8/(1024*1024); // bytes to mbits
	if (total_mbits <= 0) return false;
	mbps = total_mbits / wall_clock;
	// sprintf(result_format, " %6.2f", mbps);
	return true;
}

const char * format_utime_double (double utime, Formatter & /*fmt*/)
{
	return format_time((time_t)utime);
}

const char * format_real_time (long long t, Formatter &) //TODO: another time function
{
	return format_time( t );
}

bool render_activity_code (std::string & act, ClassAd *al, Formatter &)
{

	char sa[4] = "  ";
	bool ok = false;

	// Input might be state OR activity, so try looking up both
	State st = no_state;
	Activity ac = string_to_activity(act.c_str());
	if (ac > no_act && ac < _act_threshold_) {
		ok = true;
		// if it's a valid Activity, then fetch the state.
		al->LookupString(ATTR_STATE, act);
		st = string_to_state(act.c_str());
	} else {
		st = string_to_state(act.c_str());
		if (st > no_state && st < _state_threshold_) {
			ok = true;
			// if the input was a valid state, then it can't have been a valid activity
			// try and lookup the activity now
			al->LookupString(ATTR_ACTIVITY, act);
			ac = string_to_activity(act.c_str());
		}
	}
	digest_state_and_activity(sa, st, ac);
	act = sa;
	return ok;
}



bool render_activity_time (long long & atime, ClassAd *al, Formatter &)
{
	long long now = 0;
	if (al->LookupInteger(ATTR_MY_CURRENT_TIME, now) || al->LookupInteger(ATTR_LAST_HEARD_FROM, now)) {
		atime = now - atime; // format_time
		if (atime < 0) {
			atime = 0;
		}
		return true;
	}
	return false; // print "   [Unknown]"
}

bool render_condor_platform(std::string & str, ClassAd*, Formatter & /*fmt*/)
{
	if ( ! str.empty()) {
		size_t ix = str.find_first_of(' ');
		ix = str.find_first_not_of(' ', ix);
		size_t ixe = str.find_first_of(" .$", ix);
		str = str.substr(ix, ixe - ix);
		if (str[0] == 'X') str[0] = 'x';
		ix = str.find_first_of('-');
		while (ix != std::string::npos) {
			str[ix] = '_';
			ix = str.find_first_of('-');
		}
		ix = str.find("WINDOWS_");
		if (ix != std::string::npos) { str.erase(ix+7, std::string::npos); }
		return true;
	}
	return false;
}

bool render_version (std::string & str, ClassAd*, Formatter & fmt)
{
	if ( ! str.empty()) {
		str = format_version(str.c_str(), fmt);
		return true;
	}
	return false;
}

bool render_member_count (classad::Value & value, ClassAd*, Formatter &)
{
	const char * list = nullptr;
	classad::ExprList* ary = nullptr;
	if (value.IsStringValue(list) && list) {
		StringTokenIterator sti(list);
		value.SetIntegerValue(std::distance(sti.begin(), sti.end()));
		return true;
	} else if (value.IsListValue(ary) && ary) {
		value.SetIntegerValue(ary->size());
		return true;
	}
	return false;
}

bool render_due_date (long long & dt, ClassAd *al, Formatter &)
{
	long long now;
	if (al->LookupInteger(ATTR_LAST_HEARD_FROM , now)) {
		dt = now + dt; // format_date
		return true;
	}
	return false;
}

bool render_elapsed_time (long long & tm, ClassAd *al , Formatter &)
{
	long long now;
	if (al->LookupInteger(ATTR_LAST_HEARD_FROM, now)) {
		tm = now - tm; // format_time
		return true;
	}
	return false;
}

const char * format_load_avg (double fl, Formatter &)
{
	static char load_avg_buf[60];
	snprintf(load_avg_buf, sizeof(load_avg_buf), "%.3f", fl);
	return load_avg_buf;
}

// render Arch, OpSys, OpSysAndVer and OpSysShortName into a NMI style platform string
bool render_platform (std::string & str, ClassAd* al, Formatter & /*fmt*/)
{
	return format_platform_name(str, al);
}

bool render_strings_from_list ( classad::Value & value, ClassAd*, Formatter & fmt )
{
	if( ! value.IsListValue() ) {
		return false;
	}
	std::string prettyList;
	value.SetStringValue(extractStringsFromList(value, fmt, prettyList));
	return true;
}

bool render_unique_strings ( classad::Value & value, ClassAd*, Formatter & fmt )
{
	if( ! value.IsListValue() ) {
		return false;
	}
	std::string buffer;
	value.SetStringValue(extractUniqueStrings(value, fmt, buffer));
	return true;
}

//=================================Format Table======================================
//          !!! ENTRIES IN THIS TABLE MUST BE SORTED BY THE FIRST FIELD !!!
const CustomFormatFnTableItem GlobalPrintFormats[] = {
	{ "ACTIVITY_CODE",   ATTR_ACTIVITY, 0, render_activity_code, ATTR_STATE "\0" },
	{ "ACTIVITY_TIME",   ATTR_ENTERED_CURRENT_ACTIVITY, "%T", render_activity_time, ATTR_LAST_HEARD_FROM "\0" ATTR_MY_CURRENT_TIME "\0"  },
	{ "BATCH_NAME",      ATTR_JOB_CMD, 0, render_batch_name, ATTR_JOB_BATCH_NAME "\0" ATTR_JOB_CMD "\0" ATTR_DAGMAN_JOB_ID "\0" ATTR_DAG_NODE_NAME "\0" },
	{ "BUFFER_IO_MISC",  ATTR_JOB_UNIVERSE, 0, render_buffer_io_misc, ATTR_FILE_SEEK_COUNT "\0" ATTR_BUFFER_SIZE "\0" ATTR_BUFFER_BLOCK_SIZE "\0" ATTR_TRANSFERRING_INPUT "\0" ATTR_TRANSFERRING_OUTPUT "\0" ATTR_TRANSFER_QUEUED "\0" },
	{ "CONDOR_PLATFORM", "CondorPlatform", 0, render_condor_platform, NULL },
	{ "CONDOR_VERSION",  ATTR_CONDOR_VERSION, 0, render_version, NULL },
	{ "CPU_UTIL",        ATTR_JOB_REMOTE_USER_CPU, "%.1f", render_cpu_util, ATTR_JOB_COMMITTED_TIME "\0" },
	{ "DAG_OWNER",       ATTR_OWNER, 0, render_dag_owner, ATTR_NICE_USER_deprecated "\0" ATTR_DAGMAN_JOB_ID "\0" ATTR_DAG_NODE_NAME "\0"  },
#if 0
	//This was a collision between history table and prettyPrint table
	{ "DATE",            ATTR_Q_DATE, 0, format_real_date, NULL },
#endif
	{ "DATE",            NULL, 0, format_real_date, NULL },
	{ "DUE_DATE",        ATTR_CLASSAD_LIFETIME, "%Y", render_due_date, ATTR_LAST_HEARD_FROM "\0" },
	{ "ELAPSED_TIME",    ATTR_LAST_HEARD_FROM, "%T", render_elapsed_time, ATTR_LAST_HEARD_FROM "\0" },
	{ "GRID_JOB_ID",     ATTR_GRID_JOB_ID, 0, render_grid_job_id, ATTR_GRID_RESOURCE "\0" },
	{ "GRID_RESOURCE",   ATTR_GRID_RESOURCE, 0, render_grid_resource, ATTR_EC2_REMOTE_VM_NAME "\0" },
	{ "GRID_STATUS",     ATTR_GRID_JOB_STATUS, 0, render_grid_status, nullptr },
	{ "JOB_COMMAND",     ATTR_JOB_CMD, 0, render_job_cmd_and_args, ATTR_JOB_DESCRIPTION "\0MATCH_EXP_" ATTR_JOB_DESCRIPTION "\0" },
	{ "JOB_DESCRIPTION", ATTR_JOB_CMD, 0, render_job_description, ATTR_JOB_ARGUMENTS1 "\0" ATTR_JOB_ARGUMENTS2 "\0" ATTR_JOB_DESCRIPTION "\0MATCH_EXP_" ATTR_JOB_DESCRIPTION "\0" },
	{ "JOB_FACTORY_MODE",ATTR_JOB_MATERIALIZE_PAUSED, 0, format_job_factory_mode, NULL },
	{ "JOB_ID",          ATTR_CLUSTER_ID, 0, render_job_id, ATTR_PROC_ID "\0" },
	{ "JOB_STATUS",      ATTR_JOB_STATUS, 0, render_job_status_char, ATTR_LAST_SUSPENSION_TIME "\0" ATTR_TRANSFERRING_INPUT "\0" ATTR_TRANSFERRING_OUTPUT "\0" ATTR_TRANSFER_QUEUED "\0" },
#if 0
	//This was a collision between queue table and history table
	{ "JOB_STATUS",      ATTR_JOB_STATUS, 0, format_job_status_char, ATTR_LAST_SUSPENSION_TIME "\0" ATTR_TRANSFERRING_INPUT "\0" ATTR_TRANSFERRING_OUTPUT "\0" },
#endif
	{ "JOB_STATUS_RAW",  ATTR_JOB_STATUS, 0, format_job_status_raw, NULL },
	{ "JOB_UNIVERSE",    ATTR_JOB_UNIVERSE, 0, format_job_universe, NULL },
	{ "LOAD_AVG",        NULL, 0, format_load_avg, NULL },
	{ "MEMBER_COUNT",    NULL, "%d", render_member_count, NULL },
	{ "MEMORY_USAGE",    ATTR_IMAGE_SIZE, "%.1f", render_memory_usage, ATTR_MEMORY_USAGE "\0" },
	{ "OWNER",           ATTR_OWNER, 0, render_owner, ATTR_NICE_USER_deprecated "\0" },
	{ "PLATFORM",        ATTR_OPSYS, 0, render_platform, ATTR_ARCH "\0" ATTR_OPSYS_AND_VER "\0" ATTR_OPSYS_SHORT_NAME "\0" },
	{ "QDATE",           ATTR_Q_DATE, "%Y", format_real_date, NULL },
	{ "READABLE_BYTES",  ATTR_BYTES_RECVD, 0, format_readable_bytes, NULL },
#if 0
	//This was a collision between (queue & history) tables and prettyPrint table
	{ "READABLE_KB",     ATTR_REQUEST_DISK, 0, format_readable_kb, NULL },
#endif
	{ "READABLE_KB",     ATTR_DISK, 0, format_readable_kb, NULL },
#if 0
	//This was a collision between (queue & history) tables and prettyPrint table
	{ "READABLE_MB",     ATTR_REQUEST_MEMORY, 0, format_readable_mb, NULL },
#endif
	{ "READABLE_MB",     ATTR_MEMORY, 0, format_readable_mb, NULL },
	{ "REMOTE_HOST",     ATTR_OWNER, 0, render_remote_host, ATTR_JOB_UNIVERSE "\0" ATTR_REMOTE_HOST "\0" ATTR_EC2_REMOTE_VM_NAME "\0" ATTR_GRID_RESOURCE "\0" },
	{ "RUNTIME",         ATTR_JOB_REMOTE_WALL_CLOCK, 0, format_utime_double, NULL },
	{ "STDU_GOODPUT",    ATTR_JOB_STATUS, "%.1f", render_goodput, ATTR_JOB_REMOTE_WALL_CLOCK "\0" ATTR_SHADOW_BIRTHDATE "\0" ATTR_LAST_CKPT_TIME "\0" },
	{ "STDU_MPBS",       ATTR_BYTES_SENT, "%.2f", render_mbps, ATTR_JOB_REMOTE_WALL_CLOCK "\0" ATTR_SHADOW_BIRTHDATE "\0" ATTR_LAST_CKPT_TIME "\0" ATTR_JOB_STATUS "\0" ATTR_BYTES_RECVD "\0"},
	{ "STRINGS_FROM_LIST", NULL, 0, render_strings_from_list, NULL },
	{ "TIME",            ATTR_KEYBOARD_IDLE, 0, format_real_time, NULL },
	{ "UNIQUE",          NULL, 0, render_unique_strings, NULL },
};

const CustomFormatFnTableItem * getGlobalPrintFormats() { return GlobalPrintFormats; }
const CustomFormatFnTable getGlobalPrintFormatTable() { return SORTED_TOKENER_TABLE(GlobalPrintFormats); }

