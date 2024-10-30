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
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_state.h"
#include "sig_install.h"
#include "daemon.h"
#include "dc_collector.h"
#include "match_prefix.h"    // is_arg_colon_prefix
#include "ad_printmask.h"
#include "condor_string.h"
#include "directory.h"
#include "format_time.h"
#include "console-utils.h"
#include "ipv6_hostname.h"
#include <my_popen.h>

#include "condor_distribution.h"
#include "condor_version.h"
#include "condor_environ.h"
#include "setenv.h"

#include <vector>
#include <map>

#include "backward_file_reader.h"

using std::vector;
using std::map;
using std::string;

// stuff that we can scrape from the log directory about various daemons.
//
typedef struct {
	std::string name;  // Daemon name, (note Starters are named SlotN)
	std::string addr;  // sinful string from .address file, if available
	std::string log_dir;// log directory
	std::string log;     // log filename
	std::string log_old; // previous (.old) log file full path name.
	std::string exe_path; // full path to executable (from master log)
	std::string pid;      // latest process id (from master log, starterlog, etc)
	std::string exit_code; // if daemon has exited, this is the exit code otherwise empty
} LOG_INFO;

typedef std::map<std::string, LOG_INFO*> LOG_INFO_MAP;
typedef std::map<std::string, pid_t> MAP_STRING_TO_PID;
typedef std::map<pid_t, std::string> MAP_STRING_FROM_PID;
typedef std::map<std::string, std::vector<std::string> > TABULAR_MAP;
typedef std::map<std::string, std::string> MAP_STRING_TO_STRING;

//char	*param();
static void query_log_dir(const char * log_dir, LOG_INFO_MAP & info, bool chatty);
static void print_log_info(LOG_INFO_MAP & info);
static void print_daemon_info(LOG_INFO_MAP & info, bool fQuick);
static void scan_logs_for_info(LOG_INFO_MAP & info, MAP_STRING_TO_PID & job_to_pid, bool fAddressesOnly);
static void scan_a_log_for_info(LOG_INFO_MAP & info, MAP_STRING_TO_PID & job_to_pid, LOG_INFO_MAP::const_iterator & it);

static void query_daemons_for_pids(LOG_INFO_MAP & info);
static void ping_all_known_addrs(LOG_INFO_MAP & info);
static char * get_daemon_param(const char * addr, const char * param_name);
static bool get_daemon_ready(const char * addr, const char * requirements, time_t sec_to_wait, ClassAd & ready_ad);

// app globals
static struct AppType {
	const char * Name; // tool name as invoked from argv[0]
	std::vector<const char *> target_names;    // list of target names to query
	std::vector<LOG_INFO *>   all_log_info;    // pool of info from scanning log directories.

	std::vector<const char *> print_head; // The list of headings for the mask entries
	AttrListPrintMask print_mask;
	classad::References projection;    // Attributes that we want the server to send us

	int    diagnostic; // output useful only to developers
	bool   verbose;    // extra output useful to users
	bool   wide;       // don't truncate to fit the screen
	bool   show_daemons;    // print a a table of daemons pids and addresses
	bool   daemon_mode;     // condor_who in 'daemon_mode' showing info about daemons rather than info about jobs.
	bool   show_full_ads;
	bool   show_job_ad;     // debugging
	bool   scan_pids;       // query all schedds found via ps/tasklist
	bool   quick_scan;      // do only the scanning that can be done quickly (i.e. no talking to daemons)
	bool   timed_scan;
	bool   ping_all_addrs;	 //
	time_t query_ready_timeout;
	time_t poll_for_master_time; // time spent polling for the master
	const char * startd_snapshot_opt; // CondorQuery extra attribute value to get internal snapshot from STARTD, use with show_full_ads
	const char * startd_statistics_opt; // CondorQuery extra attribute value to get internal snapshot from STARTD, use with show_full_ads
	std::string query_ready_requirements;
	int    test_backward;   // test backward reading code.
	vector<pid_t> query_pids;
	vector<const char *> query_log_dirs;
	vector<char *> query_addrs;
	vector<const char *> constraint;

	// hold temporary results.
	MAP_STRING_TO_STRING file_to_daemon; // map condor_xxxx_yyyy to daemon name XxxxxYyyy
	MAP_STRING_TO_STRING log_to_daemon;  // map XxxYyyLog to daemon name XxxYyy
	MAP_STRING_TO_PID   job_to_pid;
	MAP_STRING_FROM_PID pid_to_program;	// pid to program/command line
	MAP_STRING_FROM_PID pid_to_addr; // pid to sinful address
	std::string configured_logdir; // $(LOG) from condor_config (if any)

	// convert daemon subsys name e.g. "MASTER" to cannonical daemon name "Master"
	// the input need not be uppercase, but should contain _ for shared_port, etc.
	bool SubsysToDaemon(std::string & daemon) {
		lower_case(daemon);
		MAP_STRING_TO_STRING::const_iterator alt = this->file_to_daemon.find(daemon);
		if (alt != this->file_to_daemon.end()) { 
			daemon = alt->second; 
			return true;
		} else {
			daemon[0] = toupper(daemon[0]);
		}
		return false;
	}

	// convert cannonical daemon name e.g. "Master" to daemon subsys name "MASTER"
	// the output will be uppercase, and will contain _ for SHARED_PORT, etc.
	bool DaemonToSubsys(std::string & daemon) {
		bool found = false;
		MAP_STRING_TO_STRING::const_iterator it;
		for (it = this->file_to_daemon.begin(); it != this->file_to_daemon.end(); it++) {
			if (it->second == daemon) {
				daemon = it->first;
				found = true;
				break;
			}
		}
		upper_case(daemon);
		return found;
	}

	~AppType() {
		target_names.clear();
		all_log_info.clear();
		print_head.clear();
		for (char *p : query_addrs) {
			free(p);
		}
	}	
} App;

// init fields in the App structure that have no default initializers
void InitAppGlobals(const char * argv0)
{
	App.Name = argv0;
	App.diagnostic = 0; // output useful only to developers
	App.verbose = false;    // extra output useful to users
	App.show_daemons = false; 
	App.daemon_mode = false;
	App.show_full_ads = false;
	App.show_job_ad = false;     // debugging
	App.scan_pids = false;
	App.quick_scan = false;
	App.ping_all_addrs = false;
	App.test_backward = false;
	App.query_ready_timeout = 0;
	App.startd_snapshot_opt = nullptr;
	App.startd_statistics_opt = nullptr;

	// map Log name to daemon name for those that don't match the rule : 'remove Log'
	App.log_to_daemon["Sched"] = "Schedd";
	App.log_to_daemon["Start"] = "Startd";
	App.log_to_daemon["Match"] = "Negotiator";
	App.log_to_daemon["Cred"]  = "Credd";
	App.log_to_daemon["Kbd"] = "Kbdd";

	// map executable name to daemon name for those that don't match the rule : 'remove condor_ and capitalize'
	App.file_to_daemon["shared_port"] = "SharedPort";
	App.file_to_daemon["job_router"]  = "JobRouter";
	App.file_to_daemon["ckpt_server"] = "CkptServer";

}

	// Tell folks how to use this program
void usage(bool and_exit)
{
	fprintf (stderr, "Usage: %s [help-opt] [addr-opt] [display-opt]\n", App.Name);
	fprintf (stderr, "   or: %s -dae[mons] [daemon-opt]\n", App.Name);
	fprintf (stderr,
		"    where [help-opt] is one of\n"
		"\t-h[elp]\t\t\tDisplay this screen, then exit\n"
		"\t-v[erbose]\t\tAlso display pids and addresses for daemons\n"
		"\t-diag[nostic]\t\tDisplay extra information helpful for debugging\n"
		"    and [addr-opt] is one of\n"
		"\t-addr[ess] <host>\tlocal STARTD host address to query\n"
		"\t-log[dir] <dir>\t\tDirectory to seach for STARTD address to query\n"
		"\t-pid <pid>\t\tProcess ID of STARTD to query\n"
		"\t-al[lpids]\t\tQuery all local STARTDs\n"
		"   and [display-opt] is one or more of\n"
//		"\t-ps\t\t\tDisplay process tree\n"
		"\t-l[ong]\t\t\tDisplay entire classads\n"
		"\t-snap[shot]\t\tQuery internal startd classads.\n"
		"\t-stat[istics] <set>:<n>\tQuery startd statistics for <set> at level <n>\n"
		"\t-w[ide]\t\t\tdon't truncate fields to fit the screen\n"
		"\t-f[ormat] <fmt> <attr>\tPrint attribute with a format specifier\n"

		"\t-autoformat[:lhVr,tng] <attr> [<attr2> [...]]\n"
		"\t-af[:lhVr,tng] <attr> [attr2 [...]]\n"
		"\t    Print attr(s) with automatic formatting\n"
		"\t    the [lhVr,tng] options modify the formatting\n"
		"\t        l   attribute labels\n"
		"\t        h   attribute column headings\n"
		"\t        V   %%V formatting (string values are quoted)\n"
		"\t        r   %%r formatting (raw/unparsed values)\n"
		"\t        ,   comma after each value\n"
		"\t        t   tab before each value (default is space)\n"
		"\t        n   newline after each value\n"
		"\t        g   newline between ClassAds, no space before values\n"
		"\t    use -af:h to get tabular values with headings\n"
		"\t    use -af:lrng to get -long equivalent format\n"
		);
	fprintf (stderr,
		"    where [daemon-opt] is one of\n"
		"\t-dae[mons]\t\t Display pids and addressed for daemons then exit.\n"
		"\t-quic[k]\t\t Display master exit code or daemon readyness ad then exit\n"
		"\t-wait[-for-ready][:<time>] <expr> Query the MASTER for its daemon\n"
		"\t    readyness ad and Wait up to <time> seconds for the <expr> to become true.\n"
		);

	fprintf (stderr,
		"\n    In normal use, %s scans HTCondor log files to determine the adresse(s)\n"
		"Of STARTD(s), and then sends a command to each STARTD to determine what jobs\n"
		"are running on the local machine.  When the -pid or -allpids arguments are used\n"
		"STARTDs are located by scanning system process information and lists of open sockets\n"
		, App.Name);

	fprintf (stderr,
		"\n    When the -daemons, -quick, or -wait-for-ready arguments are used. %s only displays\n"
		"information about HTCondor daemons. It does not attempt to determine what jobs are running.\n"
		, App.Name);

	fprintf (stderr,
		"When the -quick or -wait-for-ready argument is used. %s scans the log directory to\n"
		"determine the MASTER address. If the master is alive, it will query its readyness ad and\n"
		"and display it. If the master is not alive it will report the master exit code and last\n"
		"known address.\n"
		, App.Name);


	if (and_exit)
		exit( 1 );
}

// return true if p1 is longer than p2 and it ends with p2
// if ppEnd is not NULL, return a pointer to the start of the end of p1
bool ends_with(const char * p1, const char * p2, const char ** ppEnd)
{
	size_t cch2 = p2 ? strlen(p2) : 0;
	if ( ! cch2)
		return false;

	size_t cch1 = p1 ? strlen(p1) : 0;
	if (cch2 >= cch1)
		return false;

	const char * p1e = p1+cch1-1;
	const char * p2e = p2+cch2-1;
	while (p2e >= p2) {
		if (*p2e != *p1e)
			return false;
		--p2e; --p1e;
	}
	if (ppEnd)
		*ppEnd = p1e+1;
	return true;
}

// return true if p1 starts with p2
// if ppEnd is not NULL, return a pointer to the first non-matching char of p1
bool starts_with(const char * p1, const char * p2, const char ** ppEnd)
{
	if ( ! p2 || ! *p2)
		return false;

	const char * p1e = p1;
	const char * p2e = p2;
	while (*p2e) {
		if (*p1e != *p2e)
			return false;
		++p2e; ++p1e;
	}
	if (ppEnd)
		*ppEnd = p1e;
	return true;
}

void AddPrintColumn(const char * heading, int width, const char * expr)
{
	ClassAd ad;
	if(!GetExprReferences(expr, ad, NULL, &App.projection)) {
		fprintf( stderr, "Error:  Parse error of: %s\n", expr);
		exit(1);
	}

	App.print_head.emplace_back(heading);

	int wid = (int)(width ? width : strlen(heading));
	int opts = FormatOptionNoTruncate | FormatOptionAutoWidth;
	App.print_mask.registerFormat("%v", wid, opts, expr);
}

// print a utime in human readable form
static const char *
format_int_runtime (long long utime, Formatter & /*fmt*/)
{
	return format_time(utime);
}

// print out static or dynamic slot id.
static bool
render_slot_id (std::string & out, ClassAd * ad, Formatter & /*fmt*/)
{
	int slotid;
	if ( ! ad->LookupInteger(ATTR_SLOT_ID, slotid))
		return false;

	static char outstr[10];
	outstr[0] = 0;
	//bool from_name = false;
	bool is_dynamic = false;
	if (/*from_name || */(ad->LookupBool(ATTR_SLOT_DYNAMIC, is_dynamic) && is_dynamic)) {
		std::string name;
		if (ad->LookupString(ATTR_NAME, name) && (0 == name.find("slot"))) {
			size_t cch = sizeof(outstr)/sizeof(outstr[0]);
			strncpy(outstr, name.c_str()+4, cch-1);
			outstr[cch-1] = 0;
			char * pat = strchr(outstr, '@');
			if (pat)
				pat[0] = 0;
		} else {
			snprintf(outstr, sizeof(outstr), "%u_?", slotid);
		}
	} else {
		snprintf(outstr, sizeof(outstr), "%u", slotid);
	}
	out = outstr;
	return true;
}

// print the pid for a jobid.
static const char *
format_jobid_pid (const char *jobid, Formatter & /*fmt*/)
{
	static char outstr[16];
	outstr[0] = 0;
	if (App.job_to_pid.find(jobid) != App.job_to_pid.end()) {
		snprintf(outstr, sizeof(outstr), "%u", App.job_to_pid[jobid]);
	}
	return outstr;
}


// takes the output of ps or lsof or tasklist.
// returns a map that uses headings as the key, and has a vector of strings for each field under that heading.
// if fMultiSep is true, then 2 or more separator characters are required to represent a field separation.
//
int get_fields_from_tabular_stream(FILE * stream, TABULAR_MAP & out, bool fMultiWord = false)
{
	int rows = 0;
	out.clear();

	// stream contains
	// HEADINGS over DATA with optional ==== or --- line under headings.
	char * line = getline_trim(stream);
	if (line && (0 == strlen(line))) line = getline_trim(stream);
	if ( ! line || (0 == strlen(line)))
		return 0;

	std::string headings = line;
	std::string subhead;
	std::string data;

	line = getline_trim(stream);
	if (line) data = line;

	if (data.find("====") == 0 || data.find("----") == 0) {
		subhead = line ? line : "";
		data.clear();

		// first line after headings is not data, but underline
		line = getline_trim(stream);
		if (line) data = line;
	}

	// If headers have spaces in them, change to underline
	size_t lpos = headings.find("Local Address");
	size_t rpos = headings.find("Foreign Address");
	size_t ppos = headings.find("PID/Program name");

	if (lpos != std::string::npos) {
			headings[lpos + 5] = '_';
	}

	if (rpos != std::string::npos) {
			headings[rpos + 7] = '_';
	}

	if (ppos != std::string::npos) {
			headings = headings.substr(0, ppos + 3);
	}

	if (App.diagnostic > 2) { printf("HD: %s\n", headings.c_str()); printf("SH: %s\n", subhead.c_str()); }

	while ( ! data.empty()) {
		const char WS[] = " \t\r\n";
		if (App.diagnostic > 2) { printf(" D: %s\n", data.c_str()); }
		if (subhead.empty()) {
			// use whitespace as a field separator
			// assume all but last field headings/data have no spaces in them.
			size_t ixh=0, ixd=0, ixh2, ixd2=0, ixhN;
			for (;;) {
				if (ixh == string::npos)
					break;
				ixhN = ixh2 = ixh = headings.find_first_not_of(WS, ixh);
				if (ixh2 != string::npos) {
					ixh2 = headings.find_first_of(WS, ixh2);
					if (ixh2 != string::npos) {
						ixhN = headings.find_first_not_of(WS, ixh2);
						if (fMultiWord && (ixhN == ixh2 + 1)) {
							ixh2 = headings.find_first_of(WS, ixhN);
						}
					}
				}
				// so the last field can contain spaces, we notice when we get to the last heading
				// an use that to trigger a 'read to end of line' for the data
				bool to_eol = ((ixh2 == string::npos) || (headings.find_first_not_of(WS, ixh2) == string::npos));

				//if (App.diagnostic > 3) { printf(" hs(%d,%d)", (int)ixh, (int)ixh2); }
				std::string hd;
				if (ixh2 != string::npos)
					hd = headings.substr(ixh, ixh2-ixh);
				else
					hd = headings.substr(ixh);

				std::string val;
				if (fMultiWord) {
					// if multi word headings, use the heading offsets strictly to parse the underlying data
					// the windows netstat tool does this.
					ixd = ixh;
					if (to_eol || (ixh2 == string::npos))
						val = data.substr(ixd);
					else {
						ixd2 = data.find_first_of(WS, ixh2);
						val = data.substr(ixd, ixd2-ixd);
					}
				} else {
					ixd2 = ixd = data.find_first_not_of(WS, ixd);
					if (ixd2 != string::npos) ixd2 = data.find_first_of(WS, ixd2);
					if (to_eol || ixd2 == string::npos) {
						//if (App.diagnostic > 3) { printf(" ds(%d)", (int)ixd); }
						val = data.substr(ixd);
					} else {
						if (ixd >= ixhN) {
							// if the data starts AFTER the colunm header for the next colum
							// pretend that this colum is empty.
							val = "";
							ixd = ixd2 = ixh2;
						} else {
							//if (App.diagnostic > 3) { printf(" ds(%d,%d)", (int)ixd, (int)ixd2); }
							val = data.substr(ixd, ixd2-ixd);
						}
					}
				}
				trim(hd);
				trim(val);
				out[hd].push_back(val);

				ixh = ixh2;
				ixd = ixd2;
				//if (App.diagnostic > 3) { printf(" h=%d d=%d\n", (int)ixh, (int)ixd); }
			}
		} else {
			// use subhead to get field widths
			size_t ixh = 0, ixh2;
			for (;;) {
				if (ixh == string::npos)
					break;

				std::string hd;
				ixh2 = ixh = subhead.find_first_not_of(WS, ixh);
				if (ixh2 != string::npos)
					ixh2 = subhead.find_first_of(WS, ixh2);
				if (ixh2 != string::npos)
					hd = headings.substr(ixh, ixh2-ixh);
				else
					hd = headings.substr(ixh);

				std::string val;
				if (ixh2 != string::npos)
					val = data.substr(ixh, ixh2-ixh);
				else
					val = data.substr(ixh);

				trim(hd);
				trim(val);
				out[hd].push_back(val);

				ixh = ixh2;
			}
		}

		// get next line.
		line = getline_trim(stream);
		if (line) data = line; else data.clear();
	}

	return rows;
}

class tabular_map_constraint {
public:
	typedef bool (tabular_map_constraint::*FNConstraint)(TABULAR_MAP & table, size_t index) const;
	FNConstraint m_include_row;
	tabular_map_constraint() : m_include_row(NULL) {}
	bool include_row(TABULAR_MAP & table, size_t index) const {
		return m_include_row ? (this->*m_include_row)(table, index) : true;
	}
};

class tm_cmd_contains : public tabular_map_constraint {
public:
	tm_cmd_contains(std::string name_to_match) : name(name_to_match) {
		m_include_row = (tabular_map_constraint::FNConstraint)&tm_cmd_contains::query;
	}
	//operator const tabular_map_constraint&() const { return (const tabular_map_constraint&)(*this); }
	//operator tabular_map_constraint&() { return (tabular_map_constraint&)(*this); }

	bool query(TABULAR_MAP & table, size_t index) const {
		#ifdef WIN32
			const char * cmd_key = "Image Name";
		#else
			const char * cmd_key = "CMD";
		#endif
		if (table[cmd_key][index].find(name) != std::string::npos) return true;
		return false;
	}
	std::string name;
};

class tm_pid_is_known : public tabular_map_constraint {
public:
	tm_pid_is_known() {
		m_include_row = (tabular_map_constraint::FNConstraint)&tm_pid_is_known::query;
	}
	bool query(TABULAR_MAP & table, size_t index) const {
		const char * cmd_key = "PID";
		pid_t pid = atoi(table[cmd_key][index].c_str());
		if (App.pid_to_addr.find(pid) != App.pid_to_addr.end() ||
			App.pid_to_program.find(pid) != App.pid_to_program.end()) {
			return true;
		} else if (App.query_pids.size() > 0) {
			for (size_t ix = 0; ix < App.query_pids.size(); ++ix) {
				if (App.query_pids[ix] == pid) return true;
			}
		}
		return false;
	}

};

typedef bool (*TABULAR_MAP_CALLBACK)(void* pUser, TABULAR_MAP & table, size_t index);

// iterate a tabular map, calling pfn for each row that matches map_constraint until
// pfn returns false.
//
void iterate_tabular_map(TABULAR_MAP & table, TABULAR_MAP_CALLBACK pfn, void* pUser, const tabular_map_constraint & map_constraint)
{
	TABULAR_MAP::const_iterator it = table.begin();
	if (it != table.end()) {
		size_t cRows = it->second.size();

		for (size_t ix = 0; ix < cRows; ++ix) {
			if (map_constraint.include_row(table, ix)) {
				if ( ! pfn(pUser, table, ix))
					break;
			}
		}
	}
}

void dump_tabular_map(TABULAR_MAP & table, const tabular_map_constraint & map_constraint)
{
	size_t cRows = 0;
	std::string headings;
	for (TABULAR_MAP::const_iterator it = table.begin(); it != table.end(); ++it) {
		if ( ! headings.empty()) { headings += "; "; }
		headings += it->first;
		cRows = MAX(cRows, it->second.size());
	}
	printf("H [%s]\n", headings.c_str());

	for (size_t ix = 0; ix < cRows; ++ix) {
		if (map_constraint.include_row(table, ix)) {

			std::string row;
			for (TABULAR_MAP::const_iterator it = table.begin(); it != table.end(); ++it) {
				if ( ! row.empty()) { row += "; "; }
				row += it->second[ix];
			}
			printf(" D[%s]\n", row.c_str());

		}
	}
}

// parse a file stream and return the first field value from the column named fld_name.
//
// stream is expected to be the output of a tool like ps or tasklist that returns
// data in tabular form with a header (parse_type==0),  or one labeled value per line
// form like a long form classad (parse_type=1)
int get_field_from_stream(FILE * stream, int parse_type, const char * fld_name, std::string & outstr)
{
	int cch = 0;
	outstr.clear();

	if (0 == parse_type) {
		// stream contains
		// HEADINGS over DATA with optional ==== or --- line under headings.
		char * line = getline_trim(stream);
		if (line && ! strlen(line)) line = getline_trim(stream);
		if (line) {
			std::string headings = line;
			std::string subhead;
			std::string data;

			line = getline_trim(stream);
			if (line) data = line;

			if (data.find("====") == 0 || data.find("----") == 0) {
				subhead = line ? line : "";
				data.clear();

				// first line after headings is not data, but underline
				line = getline_trim(stream);
				if (line) data = line;
			}

			const char WS[] = " \t\r\n";
			if (subhead.empty()) {
				// use field index, assume all but last field have no spaces in them.
				size_t ixh=0, ixd=0, ixh2, ixd2;
				for (;;) {
					if (ixh == string::npos || ixd == string::npos)
						break;
					ixh2 = ixh = headings.find_first_not_of(WS, ixh);
					if (ixh2 != string::npos) ixh2 = headings.find_first_of(WS, ixh2);
					ixd2 = ixd = data.find_first_not_of(WS, ixd);
					if (ixd2 != string::npos) ixd2 = data.find_first_of(WS, ixd);
					std::string hd;
					if (ixh2 != string::npos)
						hd = headings.substr(ixh, ixh2-ixh);
					else
						hd = headings.substr(ixh);

					if (hd == fld_name) {
						ixd = data.find_first_not_of(WS, ixd2);
						bool to_eol = headings.find_first_not_of(WS, ixh2) != string::npos;
						if (to_eol || ixd == string::npos) {
							outstr = data.substr(ixd2);
						} else {
							outstr = data.substr(ixd, ixd2-ixd);
						}
						cch = (int)outstr.size();
						break;
					}
					ixh = ixh2;
					ixd = ixd2;
				}
			} else {
				// use subhead to get field widths
				size_t ixh = 0, ixh2;
				for (;;) {
					if (ixh == string::npos)
						break;

					std::string hd;
					ixh2 = ixh = subhead.find_first_not_of(WS, ixh);
					if (ixh2 != string::npos)
						ixh2 = subhead.find_first_of(WS, ixh2);
					if (ixh2 != string::npos)
						hd = headings.substr(ixh, ixh2-ixh);
					else
						hd = headings.substr(ixh);

					trim(hd);
					if (hd == fld_name) {
						if (ixh2 != string::npos)
							outstr = data.substr(ixh, ixh2-ixh);
						else
							outstr = data.substr(ixh);
						cch = (int)outstr.size();
						break;
					}

					ixh = ixh2;
				}
			}
		}
	} else if (1 == parse_type) {
		// stream contains lines with
		// Label: value
		//  or
		// Label = value

	}

	return cch;
}

#ifdef WIN32
#include <psapi.h>
#endif

static void get_process_table(TABULAR_MAP & table)
{
	ArgList cmdargs;
#ifdef WIN32
	cmdargs.AppendArg("tasklist");
	//cmdargs.AppendArg("/V");
#elif defined(LINUX)
	cmdargs.AppendArg("ps");
	cmdargs.AppendArg("-eF");
#else
	cmdargs.AppendArg("ps");
	cmdargs.AppendArg("-ef");
#endif

	FILE * stream = my_popen(cmdargs, "r", 0);
	if (stream) {
		get_fields_from_tabular_stream(stream, table);
		my_pclose(stream);
	}

	if (App.diagnostic > 1) {
		std::string args;
		cmdargs.GetArgsStringForDisplay(args);
		printf("Parsed command output> %s\n", args.c_str());

		printf("with filter> %s\n", "cmd_contains(\"condor\")");
		dump_tabular_map(table, tm_cmd_contains("condor"));
		printf("[eos]\n");

		printf("with filter> %s\n", "pid_is_known()");
		dump_tabular_map(table, tm_pid_is_known());
		printf("[eos]\n");
	}
}

static void get_address_table(TABULAR_MAP & table)
{
	ArgList cmdargs;
#ifdef WIN32
	cmdargs.AppendArg("netstat");
	cmdargs.AppendArg("-ano");
#else
	cmdargs.AppendArg("netstat");
	cmdargs.AppendArg("-n");
	cmdargs.AppendArg("-t");
	cmdargs.AppendArg("-p");
	cmdargs.AppendArg("-w");
#endif

#ifndef WIN32
	fclose(stderr);
#endif

	FILE * stream = my_popen(cmdargs, "r", 0);
	if (stream) {
		bool fMultiWord = false; // output has multi-word headings.
		#ifdef WIN32
		// netstat begins with the line "Active Connections" followed by a blank line
		// followed by the actual data, but that confuses the table parser, so skip the first 2 lines.
		char * line = getline_trim(stream);
		while (MATCH != strcasecmp(line, "Active Connections")) {
			if (App.diagnostic > 1) { printf("skipping: %s\n", line); }
			line = getline_trim(stream);
		}
		if (App.diagnostic > 1) { printf("skipping: %s\n", line); }
		fMultiWord = true;
		#else
			// on Linux skip first line of netstat output
		getline_trim(stream);
		#endif
		get_fields_from_tabular_stream(stream, table, fMultiWord);
		my_pclose(stream);
	}

	if (App.diagnostic > 1) {
		std::string args;
		cmdargs.GetArgsStringForDisplay(args);
		printf("Parsed command output> %s\n", args.c_str());

		printf("with filter> %s\n", "true");
		dump_tabular_map(table, tabular_map_constraint());

		printf("with filter> %s\n", "pid_is_known()");
		dump_tabular_map(table, tm_pid_is_known());
		printf("[eos]\n");
	}
}

// convert from tabular map data into a usable sinful string
// this involves recognising various ways of expressing all-interfaces
// in the UI and converting them to a local IP address.
//
void convert_to_sinful_addr(std::string & out, const std::string & str)
{
	int port_offset = 0;
	if (starts_with(str.c_str(), "*:", NULL)) { // lsof does this
		port_offset = 2;
	} else if (starts_with(str.c_str(), "0.0.0.0:", NULL)) {  // netstat does this
		port_offset = 8;
	} else if (starts_with(str.c_str(), "[::]:", NULL)) { // netstat does this (ipv6?)
		port_offset = 5;
	}

	if (port_offset) {
		// TODO Picking IPv4 arbitrarily.
		std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();
		formatstr(out, "<%s:%s>", my_ip.c_str(), str.substr(port_offset).c_str());
	} else {
		formatstr(out, "<%s>", str.c_str());
	}
}

#ifdef WIN32

// search through the process_table (returned from invoking tasklist) to get
// the module name associated with a PID.
// returns a pointer into the process_table, so it's only valid until the process table is freed
//
const char * get_module_for_pid(const std::string & pidstr, TABULAR_MAP & process_table)
{
	if (App.diagnostic > 1) { printf("searching process_table for PID %s", pidstr.c_str()); }

	TABULAR_MAP::const_iterator itPID  = process_table.find("PID");
	TABULAR_MAP::const_iterator itModule = process_table.find("Image Name");
	if (itPID != process_table.end() &&
		itModule != process_table.end()) {
		for (size_t ix = 0; ix < itPID->second.size(); ++ix) {
			if (itPID->second[ix] == pidstr) {
				if (App.diagnostic > 1) { printf(" %s\n", itModule->second[ix].c_str()); }
				return itModule->second[ix].c_str();
			}
		}
	}
	if (App.diagnostic > 1) { printf(" NULL\n"); }
	return NULL;
}

// On Windows, the address_table is build from invoking netstat.
// it give us the CREATOR pid for a given listen port, not the current owner.
// in practice that means that all of the daemons' command ports are associated
// with the master's pid.  so we have to send a "condor_config_val PID" query
// to every port assocated with condor_master to figure out what the PID of the
// listening deamon is.
//
bool determine_address_for_pid(pid_t pid, TABULAR_MAP & address_table, TABULAR_MAP & process_table, std::string & addr)
{
	addr.clear();

	TABULAR_MAP::const_iterator itPID  = address_table.find("PID");
	TABULAR_MAP::const_iterator itAddr = address_table.find("Local Address");
	TABULAR_MAP::const_iterator itState = address_table.find("State");

	std::string temp;
	if (itPID != address_table.end() &&
		itAddr != address_table.end() &&
		itState != address_table.end()) {

		for (size_t ix = 0; ix < itAddr->second.size(); ++ix) {
			if (itState->second[ix] != "LISTENING")
				continue;

			const char * pszModule = get_module_for_pid(itPID->second[ix], process_table);
			if ( ! pszModule || MATCH != strcasecmp(pszModule, "condor_master.exe")) {
				continue;
			}

			convert_to_sinful_addr(temp, itAddr->second[ix]);
			if (App.diagnostic > 1) { printf("Query \"%s\" for PID\n", temp.c_str()); }

			// now ask the daemon listening at this port what his PID is,
			// and return the address if the PID matches what we are searching for.
			//
			char * pidstr = get_daemon_param(temp.c_str(), "PID");
			if (pidstr) {
				bool match = (atoi(pidstr) == pid);
				free(pidstr);
				if (match)
					break;
			}
			temp.clear();
		}
	}

	if ( ! temp.empty()) {
		addr = temp;
		return true;
	}

	return false;
}

#else // ! windows

// on *nix, the address table is built from lsof -nPi, the listen ports will be under the heading NAME
// with the address & port followed by the text " (LISTEN)". LISTEN is not in a separate field, so
// we have to parse the address out of the name.
//
bool determine_address_for_pid(pid_t pid, TABULAR_MAP & address_table, TABULAR_MAP & /*process_table*/, std::string & addr)
{
	addr.clear();

	TABULAR_MAP::const_iterator itPID  = address_table.find("PID");
	TABULAR_MAP::const_iterator itAddr = address_table.find("Local_Address");

	std::string temp;
	if (itPID != address_table.end() && itAddr != address_table.end()) {
		for (size_t ix = 0; ix < itPID->second.size(); ++ix) {
			pid_t pid2 = atoi(itPID->second[ix].c_str());
			if (pid2 == pid) {
				std::string str = itAddr->second[ix];
				size_t cch = str.find_first_of(" ", 0);
				if (cch == string::npos || str.substr(cch+1) != "(LISTEN)") {
					if (App.diagnostic) { printf("Ignoring addr \"%s\" for pid %d\n", str.c_str(), pid); }
					continue;
				}

				convert_to_sinful_addr(temp, itAddr->second[ix].substr(0,cch));
				break;
			}
		}
	}

	if ( ! temp.empty()) {
		addr = temp;
		return true;
	}

	return false;
}
#endif


static void init_program_for_pid(pid_t pid)
{
	App.pid_to_program[pid] = "";

	ArgList cmdargs;
#ifdef WIN32
	cmdargs.AppendArg("tasklist");
	cmdargs.AppendArg("/FI");
	std::string eqpid;
	formatstr(eqpid, "PID eq %u", pid);
	cmdargs.AppendArg(eqpid.c_str());
	const char * fld_name = "Image Name";
	const int    parse_type = 0;
	//const int    parse_type = 1;
#else
	cmdargs.AppendArg("ps");
	cmdargs.AppendArg("-F");
	std::string eqpid;
	formatstr(eqpid, "%u", pid);
	cmdargs.AppendArg(eqpid.c_str());
	const char * fld_name = "CMD";
	const  int    parse_type = 0;
#endif

	FILE * stream = my_popen(cmdargs, "r", 0);
	if (stream) {
		std::string program;
		if (get_field_from_stream(stream, parse_type, fld_name, program))
			App.pid_to_program[pid] = program;
		my_pclose(stream);
	}
}

// alternative for windows
#if 0
static void init_program_for_pid(pid_t pid)
{
	App.pid_to_program[pid] = "";

	#ifdef WIN32
		extern DWORD WINAPI GetProcessImageFileNameA(HANDLE, LPSTR, DWORD);
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
		if (hProcess) {
			char outstr[MAX_PATH];
			if (GetProcessImageFileNameA(hProcess, outstr, sizeof(outstr)))
				App.pid_to_program[pid] = outstr;
			CloseHandle(hProcess);
		}
	#endif
}
#endif


static const char *
format_jobid_program (const char *jobid, Formatter & /*fmt*/)
{
	const char * outstr = NULL;

	if (App.job_to_pid.find(jobid) != App.job_to_pid.end()) {
		pid_t pid = App.job_to_pid[jobid];
		if (App.pid_to_program.find(pid) == App.pid_to_program.end()) {
			init_program_for_pid(pid);
		}
		outstr = App.pid_to_program[pid].c_str();
	}

	return outstr;
}

void AddPrintColumn(const char * heading, int width, const char * attr, const CustomFormatFn & fmt)
{
	App.projection.insert(attr);
	App.print_head.emplace_back(heading);

	int wid = width ? width : (int)strlen(heading);
	int opts = FormatOptionNoTruncate | FormatOptionAutoWidth;
	if ( ! width && ! fmt.IsNumber()) opts |= FormatOptionLeftAlign; // strings default to left align.
	App.print_mask.registerFormat(NULL, wid, opts, fmt, attr);
}

#define IsArg is_arg_prefix
#define IsArgColon is_arg_colon_prefix

void parse_args(int /*argc*/, char *argv[])
{
	const char * pcolon;
	for (int ixArg = 0; argv[ixArg]; ++ixArg)
	{
		const char * parg = argv[ixArg];
		if (is_dash_arg_prefix(parg, "help", 1)) {
			usage(true);
			continue;
		}
		// condor_who is unusual in that -debug can be followed by an optional second argument
		// with debug flags.  We still allow that for backward compat.
		if (is_dash_arg_colon_prefix(parg, "debug", &pcolon, 1)) {
			const char * pflags = argv[ixArg+1];
			if (pflags && (pflags[0] == '"' || pflags[0] == '\'')) ++pflags;
			if (pflags && pflags[0] == 'D' && pflags[1] == '_') {
				++ixArg;
			} else {
				pflags = NULL;
			}
			if (pflags || ! pcolon) { set_debug_flags(pflags, D_NOHEADER | D_FULLDEBUG); }
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
			continue;
		}

		if (*parg != '-') {
			// arg without leading - is a target
			App.target_names.emplace_back(parg);
		} else {
			// arg with leading '-' is an option
			const char * pcolon = NULL;
			++parg;
			if (IsArgColon(parg, "diagnostic", &pcolon, 4)) {
				App.diagnostic = 1;
				if (pcolon) App.diagnostic = atoi(++pcolon);
			} else if (IsArg(parg, "verbose", 4)) {
				App.verbose = true;
			} else if (IsArg(parg, "daemons", 3)) {
				App.show_daemons = true;
				App.daemon_mode = true;
			} else if (IsArg(parg, "quick", 4)) {
				App.quick_scan = true;
				App.daemon_mode = true;
			} else if (IsArgColon(parg, "wait-for-ready", &pcolon, 4)) {
				App.quick_scan = true;
				App.daemon_mode = true;
				if (pcolon) App.query_ready_timeout = atoi(++pcolon);
				if ( ! argv[ixArg+1] || *argv[ixArg+1] == '-') {
					fprintf(stderr, "Error: Argument %s requires a requirements expression\n", argv[ixArg]);
					exit(1);
				}
				++ixArg;
				App.query_ready_requirements = argv[ixArg];
				// fprintf(stderr, "got wait-for-ready expr='%s'\n", App.query_ready_requirements.c_str());
			} else if (IsArg(parg, "timed", 5)) {
				App.timed_scan = true;
			} else if (IsArg(parg, "address", 4)) {
				if ( ! argv[ixArg+1] || *argv[ixArg+1] == '-') {
					fprintf(stderr, "Error: Argument %s requires a host address\n", argv[ixArg]);
					exit(1);
				}
				++ixArg;
				App.query_addrs.push_back(strdup(argv[ixArg]));
			} else if (IsArg(parg, "logdir", 3)) {
				if ( ! argv[ixArg+1] || *argv[ixArg+1] == '-') {
					fprintf(stderr, "Error: Argument %s requires a directory\n", argv[ixArg]);
					exit(1);
				}
				++ixArg;
				App.query_log_dirs.push_back(argv[ixArg]);
			} else if (IsArg(parg, "pid", 3)) {
				if ( ! argv[ixArg+1] || *argv[ixArg+1] == '-') {
					fprintf(stderr, "Error: Argument %s requires a process id parameter\n", argv[ixArg]);
					exit(1);
				}
				++ixArg;
				char * p;
				pid_t pid = strtol(argv[ixArg], &p, 10);
				App.query_pids.push_back(pid);
			} else if (IsArg(parg, "allpids", 2)) {
				App.scan_pids = true;
			} else if (IsArg(parg, "wide", 1)) {
				App.wide = true;
			} else if (IsArg(parg, "long", 1)) {
				App.show_full_ads = true;
			} else if (IsArgColon(parg, "snapshot", &pcolon, 4)) {
				App.startd_snapshot_opt = "1";
				if (pcolon && pcolon[1]) { App.startd_snapshot_opt = pcolon + 1; }
			} else if (IsArg(parg, "statistics", 4)) {
				if ( ! argv[ixArg+1] || *argv[ixArg+1] == '-') {
					fprintf(stderr, "Error: Argument %s requires an argument\n", argv[ixArg]);
					exit(1);
				}
				App.startd_statistics_opt = argv[++ixArg];
			} else if (IsArg(parg, "format", 1)) {
			} else if (IsArgColon(parg, "autoformat", &pcolon, 5) ||
			           IsArgColon(parg, "af", &pcolon, 2)) {
				// make sure we have at least one more argument
				if ( !argv[ixArg+1] || *(argv[ixArg+1]) == '-') {
					fprintf (stderr, "Error: Argument %s requires "
					         "at least one attribute parameter\n", argv[ixArg]);
					fprintf (stderr, "Use \"%s -help\" for details\n", App.Name);
					exit(1);
				}


				bool flabel = false;
				bool fCapV  = false;
				bool fRaw = false;
				bool fheadings = false;
				const char * prowpre = NULL;
				const char * pcolpre = " ";
				const char * pcolsux = NULL;
				if (pcolon) {
					++pcolon;
					while (*pcolon) {
						switch (*pcolon)
						{
							case ',': pcolsux = ","; break;
							case 'n': pcolsux = "\n"; break;
							case 'g': pcolpre = NULL; prowpre = "\n"; break;
							case 't': pcolpre = "\t"; break;
							case 'l': flabel = true; break;
							case 'V': fCapV = true; break;
							case 'r': case 'o': fRaw = true; break;
							case 'h': fheadings = true; break;
						}
						++pcolon;
					}
				}
				App.print_mask.SetAutoSep(prowpre, pcolpre, pcolsux, "\n");

				while (argv[ixArg+1] && *(argv[ixArg+1]) != '-') {
					++ixArg;

					parg = argv[ixArg];
					const char * pattr = parg;
					CustomFormatFn cust_fmt;

					// If the attribute/expression begins with # treat it as a magic
					// identifier for one of the derived fields that we normally display.
					if (*parg == '#') {
						++parg;
						if (MATCH == strcasecmp(parg, "SLOT") || MATCH == strcasecmp(parg, "SlotID")) {
							cust_fmt = render_slot_id;
							pattr = ATTR_SLOT_ID;
							App.projection.insert(pattr);
							App.projection.insert(ATTR_SLOT_DYNAMIC);
							App.projection.insert(ATTR_NAME);
						} else if (MATCH == strcasecmp(parg, "PID")) {
							cust_fmt = format_jobid_pid;
							pattr = ATTR_JOB_ID;
							App.projection.insert(pattr);
						} else if (MATCH == strcasecmp(parg, "PROGRAM")) {
							cust_fmt = format_jobid_program;
							pattr = ATTR_JOB_ID;
							App.projection.insert(pattr);
						} else if (MATCH == strcasecmp(parg, "RUNTIME")) {
							cust_fmt = format_int_runtime;
							pattr = ATTR_TOTAL_JOB_RUN_TIME;
							App.projection.insert(pattr);
						} else {
							parg = argv[ixArg];
						}
					}

					if ( ! cust_fmt) {
						ClassAd ad;
						if(!GetExprReferences(parg, ad, NULL, &App.projection)) {
							fprintf( stderr, "Error:  Parse error of: %s\n", parg);
							exit(1);
						}
					}

					std::string lbl = "";
					int wid = 0;
					int opts = FormatOptionNoTruncate;
					if (fheadings || App.print_head.size() > 0) {
						const char * hd = fheadings ? parg : "(expr)";
						wid = 0 - (int)strlen(hd);
						opts = FormatOptionAutoWidth | FormatOptionNoTruncate;
						App.print_head.emplace_back(hd);
					}
					else if (flabel) { formatstr(lbl,"%s = ", parg); wid = 0; opts = 0; }

					lbl += fRaw ? "%r" : (fCapV ? "%V" : "%v");
					if (App.diagnostic) {
						printf ("Arg %d --- register format [%s] width=%d, opt=0x%x for %llx[%s]\n",
							ixArg, lbl.c_str(), wid, opts, (long long)(StringCustomFormat)cust_fmt, pattr);
					}
					if (cust_fmt) {
						App.print_mask.registerFormat(NULL, wid, opts, cust_fmt, pattr);
					} else {
						App.print_mask.registerFormat(lbl.c_str(), wid, opts, pattr);
					}
				}
			} else if (IsArg(parg, "job", 3)) {
				App.show_job_ad = true;
			} else if (IsArg(parg, "ping_all_addrs", 4)) {
				App.ping_all_addrs = true;
			} else if (IsArgColon(parg, "test_backwards", &pcolon, 6)) {
				App.test_backward = pcolon ? atoi(pcolon) : 1;
			} else {
				fprintf(stderr, "Error: %s is not a valid argument\n", argv[ixArg]);
				exit(1);
			}
		}
	}

	// done parsing args, now interpret the results.
	//

	// if no output format has yet been specified, choose a default.
	//
	if (App.daemon_mode) {
		// if no autoformat, leave the print mask empty
	} else if ( ! App.show_full_ads && App.print_mask.IsEmpty() ) {
		App.print_mask.SetAutoSep(NULL, " ", NULL, "\n");

		if (App.show_job_ad) {
			AddPrintColumn("USER", 0, ATTR_USER);
			AddPrintColumn("CMD", 0, ATTR_JOB_CMD);
			AddPrintColumn("MEMORY", 0, ATTR_MEMORY_USAGE);
		} else {

			//SLOT OWNER   PID   RUNTIME  MEMORY  COMMAND
			AddPrintColumn("OWNER", 0, ATTR_REMOTE_OWNER);
			AddPrintColumn("CLIENT", 0, ATTR_CLIENT_MACHINE);
			AddPrintColumn("SLOT", 0, ATTR_SLOT_ID, render_slot_id);
			App.projection.insert(ATTR_SLOT_DYNAMIC);
			App.projection.insert(ATTR_NAME);
			AddPrintColumn("JOB", -6, ATTR_JOB_ID);
			AddPrintColumn("  RUNTIME", 12, ATTR_TOTAL_JOB_RUN_TIME, format_int_runtime);
			AddPrintColumn("PID", -6, ATTR_JOB_ID, format_jobid_pid);
			AddPrintColumn("PROGRAM", 0, ATTR_JOB_ID, format_jobid_program);
		}
	}

	if ( ! App.wide && ! App.print_mask.IsEmpty()) {
		int console_width = getConsoleWindowSize()-1; // -1 because we get double spacing if we use the full width.
		if (console_width < 0) console_width = 1024;
		App.print_mask.SetOverallWidth(console_width);
	}

	if (App.show_job_ad) {
		// constraint?
	} else if ( ! App.startd_snapshot_opt && ! App.startd_statistics_opt) {
		App.constraint.push_back("JobID=!=UNDEFINED");
	}

	if (App.scan_pids) {
		#ifdef WIN32
		#else
		if (! is_root()) {
			fprintf (stderr, "Warning: only the root account can use -allpids "
			         "to query condor_startd's not owned by the current user\n");
		}
		#endif
	}
}

bool contains_pid(vector<pid_t> & pids, pid_t pid)
{
	for (size_t ix = 0; ix < pids.size(); ++ix) {
		if (pids[ix] == pid) return true;
	}
	return false;
}

bool fn_add_to_query_pids(void* /*pUser*/, TABULAR_MAP & table, size_t index)
{
	pid_t pid = atoi(table["PID"][index].c_str());
	if (pid && ! contains_pid(App.query_pids, pid)) {
		if (App.verbose) {
			#ifdef WIN32
				const char * cmd_key = "Image Name";
			#else
				const char * cmd_key = "CMD";
			#endif
			printf("Found PID %d : %s\n", pid, table[cmd_key][index].c_str());
		}
		App.query_pids.push_back(pid);
	}
	return true;
}

// initialize our config params, this is more complicated than most most tools
// because condor_who needs to work even if it doesn't have the same config
// as the daemons that it is trying to query.
//
void init_condor_config()
{
#ifdef WIN32
#else
	// Condor will try not to run as root, not even as a tool.
	// if we are are currently running as root it will look for a condor account
	// or look in the CONDOR_IDS environment variable, if neither
	// exist, then it will fail to config.  so if we are root and we don't see
	// a CONDOR_UIDS environment variable, set one to keep config happy.
	//
	if (is_root()) {
		const char *env_name = ENV_CONDOR_UG_IDS;
		if (env_name) {
			bool fSetUG_IDS = false;
			char * env = getenv(env_name);
			if ( ! env) {
				//env = param(env_name);
				//if (env) {
				//	free(env);
				//} else {
					fSetUG_IDS = true;
				//}
			}

			if (fSetUG_IDS) {
				const char * ug_ids = "0.0"; // use root as the condor id.
				if (App.diagnostic) { printf("setting %s to %s\n", env_name, ug_ids); }
				SetEnv(env_name, ug_ids);
			}
		}
	}
#endif

	const char *env_name = ENV_CONDOR_CONFIG;
	char * env = getenv(env_name);
	if ( ! env) {
		// If no CONDOR_CONFIG environment variable, we don't want to fail if there
		// is no global config file, so tell the config subsystem that.
		config_continue_if_no_config(true);
	}
	set_priv_initialize(); // allow uid switching if root
	config();
}

bool poll_log_dir_for_active_master(
	const char * logdir,
	LOG_INFO_MAP &info, 
	time_t retry_timeout,
	bool   print_dots)
{
	int sleep_time = 1;
	time_t start_time = time(NULL);
	for (int iter=0;;++iter) {
		query_log_dir(logdir, info, App.diagnostic && (iter > 0));
		LOG_INFO_MAP::const_iterator it = info.find("Master");
		if (it != info.end()) {
			scan_a_log_for_info(info, App.job_to_pid, it);
			LOG_INFO * pli = it->second;
			if ( ! pli->addr.empty() && pli->exit_code.empty()) {
				// got a master address, we can quit scanning now...
				time_t time_spent = time(nullptr) - start_time;
				App.poll_for_master_time = std::min(retry_timeout, time_spent);
				return true;
			}
		}

		time_t now = time(NULL);
		if ((now >= start_time + retry_timeout) || (now < start_time)) {
			// if we exceeded the time limit, or time went backward, quit the loop.
			break;
		}
		if (print_dots) { printf("."); }
		sleep(sleep_time);
		time_t max_sleep = MIN(10, start_time + retry_timeout - now - sleep_time);
		sleep_time = MIN(sleep_time*2, max_sleep);
		if (it != info.end()) {
			// if we found an exited master, we should forget about it and scan again
			// in case there is a new master starting.
			LOG_INFO * pli = it->second;
			if ( ! pli->exit_code.empty()) {
				pli->exit_code.clear();
				pli->addr.clear();
				pli->pid.clear();
			}
		}
	}

	return false;
}

int
main( int argc, char *argv[] )
{
	time_t begin_time = time(NULL);
	InitAppGlobals(argv[0]);

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	if( argc < 1 ) {
		usage(true);
	}

	// parse command line arguments here.
	parse_args(argc, argv);
	bool fAddressesOnly = App.daemon_mode && ! App.verbose;

	// setup initial config params from the default condor_config
	// TODO: change config based on daemon that we are trying to query?
	init_condor_config();

	// of no -logdir's or -pid's listed on the command line, try the LOG config knob.
	// note that we can't depend on having a valid condor_config here, but we _might_ have one.
	if ( ! App.scan_pids && (App.query_log_dirs.size() == 0 && App.query_pids.size() == 0)) {
		if (param(App.configured_logdir, "LOG") &&
			IsDirectory(App.configured_logdir.c_str())) {
			App.query_log_dirs.push_back(App.configured_logdir.c_str());
		}
	}

	// if any log directories were specified, scan them now.
	//
	if (App.query_log_dirs.size() > 0) {
		if (App.diagnostic) {
			printf("Query log dirs:\n");
			for (size_t ii = 0; ii < App.query_log_dirs.size(); ++ii) {
				printf("    [%3d] %s\n", (int)ii, App.query_log_dirs[ii]);
			}
		}


		// scrape the log directory, scanning master, startd and slot logs
		// to pick up addresses, pids & exit codes.
		//
		for (size_t ii = 0; ii < App.query_log_dirs.size(); ++ii) {
			LOG_INFO_MAP info;

			if (App.quick_scan) {
				bool chatty = App.diagnostic > 0;
				if (chatty) { printf("\nScanning '%s' for address of Master", App.query_log_dirs[ii]); }
				int poll_timeout = App.query_ready_timeout*2/3;
				poll_log_dir_for_active_master(App.query_log_dirs[ii], info, poll_timeout, chatty);
				if (chatty) { printf(" took %d seconds\n", (int)(time(NULL) - begin_time)); }

				// if we found a master log then we know whether the master is alive or not.
				// if it's alive, we can ask it about the readiness of its children
 				LOG_INFO_MAP::const_iterator it = info.find("Master");
				if (it != info.end()) {
					LOG_INFO * pli = it->second;
					if ( ! pli->addr.empty() && pli->exit_code.empty()) {
						if (App.query_ready_requirements.empty()) {
							if (App.verbose) printf("\nQuerying Master %s for daemon states.\n", pli->addr.c_str());
						} else {
							if (App.verbose) printf("\nQuerying Master %s for daemon states and waiting for: %s\n", pli->addr.c_str(), App.query_ready_requirements.c_str());
						}
						ClassAd ready_ad;
						// if we spent a significant amount of time waiting for the master to start up, but we have one now.
						// we want to give a bit of extra time for things to become 'ready', so we adjust the timeouts a bit.
						time_t remain_timeout = App.query_ready_timeout - App.poll_for_master_time;
						bool got_ad = get_daemon_ready(pli->addr.c_str(), App.query_ready_requirements.c_str(), remain_timeout, ready_ad);
						if (got_ad) {
							std::string tmp;
							if (App.print_mask.IsEmpty()) {
								classad::References attrs;
								sGetAdAttrs(attrs, ready_ad);
								sPrintAdAttrs(tmp, ready_ad, attrs);
							} else {
								if (App.print_mask.has_headings()) {
									// render once to calculate column widths.
									App.print_mask.display(tmp, &ready_ad);
									tmp.clear();
									// now print headings
									App.print_mask.display_Headings(stdout, App.print_head);
								}
								// and render the data for real.
								App.print_mask.display(tmp, &ready_ad);
							}
							tmp += "\n";
							fputs(tmp.c_str(), stdout);
						}
					} else {
						const char * addr = pli->addr.empty() ? "NULL" : pli->addr.c_str();
						const char * exit_code = pli->exit_code.empty() ? "" : pli->exit_code.c_str();
						printf("\nMaster %s Exited with code=%s\n", addr, exit_code);
					}
				}
				if ( ! App.show_daemons) {
					// if we aren't printing the daemons table, just quit now.
					for (const auto &keyvalue : info) {
						delete keyvalue.second;
					}
					info.clear();
					continue;
				}
			}

			query_log_dir(App.query_log_dirs[ii], info, false);
			scan_logs_for_info(info, App.job_to_pid, fAddressesOnly);

			// if we got a STARTD address, push it's address onto the list
			// that we want to query.
			LOG_INFO_MAP::const_iterator it = info.find("Startd");
			if (it != info.end()) {
				LOG_INFO * pli = it->second;
				if ( ! pli->addr.empty() && pli->exit_code.empty()) {
					App.query_addrs.push_back(strdup(pli->addr.c_str()));
				}
			}

			if(App.ping_all_addrs) { ping_all_known_addrs(info); }

			if ( ! App.quick_scan) {
				// fill in missing PIDS for daemons by sending them DC_CONFIG_VAL commands
				query_daemons_for_pids(info);
			}

			if (App.verbose || (App.diagnostic && ! App.daemon_mode)) {
				printf("\nLOG directory \"%s\"\n", App.query_log_dirs[ii]);
				print_log_info(info);
			} else if (App.show_daemons) {
				printf("\nLOG directory \"%s\"\n", App.query_log_dirs[ii]);
				print_daemon_info(info, App.quick_scan);
			}
			for (const auto &keyvalue : info) {
				delete keyvalue.second;
			}
			info.clear();
		}
	}

	if (App.daemon_mode && App.quick_scan) {
		if (App.timed_scan) { printf("%d seconds\n", (int)(time(NULL) - begin_time)); }
		exit(0);
	}
	if ( ! App.scan_pids && App.query_pids.empty() && App.query_log_dirs.empty()) {
		if (App.verbose || App.diagnostic) { printf("\nNo STARTDs to query - exiting\n"); }
		if (App.timed_scan) { printf("%d seconds\n", (int)(time(NULL) - begin_time)); }
		exit(0);
	}

	// build a table of info from the output of ps or tasklist
	TABULAR_MAP process_table;
	get_process_table(process_table);
	TABULAR_MAP address_table;
	get_address_table(address_table);

	// if no specified logdirs and no specified pids.
	// build a list of pids by examining the process_table.
	if (App.scan_pids || (App.query_log_dirs.size() == 0 && App.query_pids.size() == 0)) {
		iterate_tabular_map(process_table, fn_add_to_query_pids, NULL, tm_cmd_contains("condor_startd"));
	}

	// if there are PIDs to query, do that now.
	// use the address_table (lsof/netstat) and process_table (ps/tasklist) to
	// figure out the cmd port of the given PID, then add those addresses
	// to the list of startd's to query.
	//
	if (App.query_pids.size() > 0) {
		size_t cPids = App.query_pids.size();
		for (size_t ix = 0; ix < cPids; ++ix) {
			pid_t pid = App.query_pids[ix];
			if (App.pid_to_addr.find(pid) == App.pid_to_addr.end()) {
				std::string addr;
				if (determine_address_for_pid(pid, address_table, process_table, addr)) {

					if (App.diagnostic || App.verbose) {
						printf("\nAddr for PID %d is %s\n", pid, addr.c_str());
					}
					App.query_addrs.push_back(strdup(addr.c_str()));

					// if no log directories specifed on the command line, ask the daemon
					// for it's log directory and scan it.
					//
					if ( ! App.query_log_dirs.size()) {
						char * logdir = get_daemon_param(addr.c_str(), "LOG");
						if (logdir) {
							if (App.diagnostic || App.verbose || App.show_daemons) {
								printf("LOG directory for PID %d is \"%s\"\n", pid, logdir);
							}

							LOG_INFO_MAP info;
							query_log_dir(logdir, info, false);
							scan_logs_for_info(info, App.job_to_pid, fAddressesOnly);

							if (App.verbose || (App.diagnostic && ! App.daemon_mode)) {
								print_log_info(info);
							} else if (App.show_daemons) {
								print_daemon_info(info, false);
							}
							free(logdir);
							for (const auto &keyvalue : info) {
								delete keyvalue.second;
							}
							info.clear();
						}
					}
				}
			}
		}
	}

	if (App.daemon_mode) {
		if (App.timed_scan) { printf("%d seconds\n", (int)(time(NULL) - begin_time)); }
		exit(0);
	}

	// query any detected startd's for running jobs.
	//
	CondorQuery *query;
	if (App.show_job_ad)
		query = new CondorQuery(ANY_AD);
	else
		query = new CondorQuery(STARTD_AD);

	if ( ! query) {
		fprintf (stderr, "Error:  Out of memory\n");
		exit (1);
	}

	if (App.constraint.size() > 0) {
		for (size_t ii = 0; ii < App.constraint.size(); ++ii) {
			query->addANDConstraint (App.constraint[ii]);
		}
	}

	if (App.projection.size() > 0) {
		query->setDesiredAttrs(App.projection);
	}

	if (App.startd_snapshot_opt) {
		query->addExtraAttribute("Snapshot", App.startd_snapshot_opt);
	}
	if (App.startd_statistics_opt) {
		query->addExtraAttributeString("STATISTICS_TO_PUBLISH", App.startd_statistics_opt);
	}

	// if diagnose was requested, just print the query ad
	if (App.diagnostic) {

		// print diagnostic information about inferred internal state
		printf ("----------\n");

		ClassAd queryAd;
		QueryResult qr = query->getQueryAd (queryAd);
		fPrintAd (stdout, queryAd);

		printf ("----------\n");
		fprintf (stderr, "Result of making query ad was:  %d\n", qr);
		//exit (1);
	}

	// if we have accumulated no query addresses, then query the default
	if (App.query_addrs.empty()) {
		App.query_addrs.push_back(strdup("NULL"));
	}

	bool identify_schedd = App.query_addrs.size() > 1;
	for (size_t ixAddr = 0; ixAddr < App.query_addrs.size(); ++ixAddr) {

		const char * direct = NULL; //"<128.105.136.32:7977>";
		const char * addr = App.query_addrs[ixAddr];

		if (App.diagnostic) { printf("querying addr = %s\n", addr); }

		if (strcasecmp(addr,"NULL") == MATCH) {
			addr = NULL;
		} else {
			direct = addr;
		}


		Daemon *dae = new Daemon( DT_STARTD, direct, addr );
		if ( ! dae) {
			fprintf (stderr, "Error:  Could not create Daemon object for %s\n", direct);
			exit (1);
		}

		if( dae->locate() ) {
			addr = dae->addr();
		}
		if (App.diagnostic) { printf("got a Daemon, addr = %s\n", addr); }

		ClassAdList result;
		if (addr || App.diagnostic) {
			CondorError errstack;
			QueryResult qr = query->fetchAds (result, addr, &errstack);
			if (Q_OK != qr) {
				fprintf( stderr, "Error: %s\n", getStrQueryResult(qr) );
				fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );
				exit(1);
			}
			else if (App.diagnostic) {
				printf("QueryResult is %d : %s\n", qr, errstack.getFullText(true).c_str());
				printf("    %d records\n", result.Length());
			}
		}


		// extern int mySortFunc(ClassAd*,ClassAd*,void*);
		// result.Sort((SortFunctionType)mySortFunc);

		if (App.show_full_ads) {

			std::string tmp;

			result.Open();
			while (ClassAd	*ad = result.Next()) {
				tmp.clear();
				classad::References attrs;
				sGetAdAttrs(attrs, *ad);
				sPrintAdAttrs(tmp, *ad, attrs);
				tmp += "\n";
				fputs(tmp.c_str(), stdout);
			}
			result.Close();

		} else if (result.Length() > 0) {

			// render the data once to calcuate column widths.
			result.Open();
			std::string tmp;
			while (ClassAd	*ad = result.Next()) {
				App.print_mask.display(tmp, ad);
				tmp.clear();
			}
			result.Close();

			if (identify_schedd) {
				printf("\n%s has %d job(s) running\n", addr, result.Length());
			} else {
				printf("\n");
			}

			// now print headings
			App.print_mask.display_Headings(stdout, App.print_head);

			// now render the data for real.
			result.Open();
			while (ClassAd	*ad = result.Next()) {
				if (App.diagnostic) {
					printf("    result Ad has %d attributes\n", ad->size());
				}
				App.print_mask.display (stdout, ad);
			}
			result.Close();

			printf("\n");
		}

		delete dae;
	}

	delete query;

	return 0;
}


/*
static LOG_INFO * find_log_info(LOG_INFO_MAP & info, std::string name)
{
	LOG_INFO_MAP::const_iterator it = info.find(name);
	if (it == info.end()) {
		return NULL;
	}
	return it->second;
}
*/

static LOG_INFO * find_or_add_log_info(LOG_INFO_MAP & info, std::string name, std::string log_dir)
{
	LOG_INFO * pli = NULL;
	LOG_INFO_MAP::const_iterator it = info.find(name);
	if (it == info.end()) {
		pli = new LOG_INFO();
		pli->name = name;
		pli->log_dir = log_dir;
		info[name] = pli;
		App.all_log_info.emplace_back(pli);
	} else {
		pli = it->second;
	}
	return pli;
}

static void read_address_file(const char * filename, std::string & addr)
{
	addr.clear();
	int fd = safe_open_wrapper_follow(filename, O_RDONLY);
	if (fd < 0)
		return;

	// read the address file into a local buffer
	char buf[4096];
	int cbRead = read(fd, buf, sizeof(buf));

	if (cbRead < 0) {
		close(fd);
		return;
	}

	// parse out the address string. it should be the first line of data
	char * peol = buf;
	while (peol < buf+cbRead) {
		if (*peol == '\r' || *peol == '\n') {
			*peol = 0;
			break;
		}
		++peol;
	}

	close(fd);

	// return the address
	if (cbRead > 0) addr.assign(buf, cbRead);
}


// scan backward through a daemon log file and extract PID and Address information
// this code looks as the LAST daemon startup banner and a few lines after it to get
// this information.  It scans backwards from the end of the file and quits as soon
// as it gets to the startup banner. this code looks for:
//		" (CONDOR_DAEMON) STARTING UP"  (where DAEMON is MASTER, SCHEDD, etc)
//		" ** Log last touched "
//		" ** PID = NNNN"                (between Log last touched and STARTING UP)
//		" DaemonCore: command socket at <"
//
// for the master log, it also parses some other lines to more efficiently pick up
// pids and addresses when they are mentioned in the master log.  this code looks for:
//		"Sent signal NNN to DAEMON (pid NNNN)"
//		"Started DaemonCore process PATH, pid and pgroup = NNNN"
//		"The DAEMON (pid NNNN) exited with status NNNN"		(daemon exit code and pid is scraped)
//		"(condor_MASTER) pid NNNN EXITING WITH STATUS N"    (scan does not stop, but exit code and pide is scraped)
//
// for starter log, it also builds a job_to_pid map for jobs that are still alive
// according to the log file.  this code looks for:
//		" (condor_STARTER) pid NNNN EXITING WITH STATUS N"  (this stops scanning of the starter log)
//		" Starting a UUUUUU universe job with ID: JJJJJ"    (JJJJ scraped from this line and mapped to possible_pid)
//		"Create_Process succeeded, pid=NNNN"                (NNNN is possible_pid if NNNN is not in dead pids list)
//		"Process exited, pid=NNNN"                          (NNNN added to dead_pids list)
//
//
static void scan_a_log_for_info(
	LOG_INFO_MAP & info,
	MAP_STRING_TO_PID & job_to_pid,
	LOG_INFO_MAP::const_iterator & it)
{
	std::string daemon_name_all_caps;  // e.g. "MASTER"
	std::string startup_banner_text;  // e.g. " (CONDOR_MASTER) STARTING UP"

	// set flags to trigger special scraping rules for certain log files
	bool bSlotLog = false;
	bool bMasterLog = (it->first == "Master");
	if (bMasterLog) {
		daemon_name_all_caps = "MASTER";
		startup_banner_text = " (CONDOR_MASTER) STARTING UP";
	} else {
		bSlotLog = starts_with(it->first.c_str(),"Slot",NULL) || starts_with(it->first.c_str(),"Starter",NULL);
		if (bSlotLog) {
			daemon_name_all_caps = "STARTER";
			startup_banner_text = " (CONDOR_STARTER) STARTING UP";
		} else {
			daemon_name_all_caps = it->first;
			App.DaemonToSubsys(daemon_name_all_caps);
			startup_banner_text = " (CONDOR_";
			startup_banner_text += daemon_name_all_caps;
			startup_banner_text += ") STARTING UP";
		}
	}

	const char * pszDaemon = it->first.c_str();
	LOG_INFO * pliDaemon = it->second;

	std::string filename;
	formatstr(filename, "%s%c%s", pliDaemon->log_dir.c_str(), DIR_DELIM_CHAR, pliDaemon->log.c_str());
	if (App.diagnostic) {
		printf("scanning %s log file '%s' for pids and addresses\n", pszDaemon, filename.c_str());
		printf("using '%s' as banner\n", startup_banner_text.c_str());
	}

	BackwardFileReader reader(filename, O_RDONLY);
	if (reader.LastError()) {
		// report error??
		if (App.diagnostic) {
			fprintf(stderr,"Error opening %s: %s\n", filename.c_str(), strerror(reader.LastError()));
		}
	}

	std::map<pid_t,bool> dead_pids;
	std::string possible_job_id;
	std::string possible_job_pid;
	std::string possible_daemon_pid;
	std::string possible_daemon_addr;
	bool bInsideBanner = false;
	bool bInsideJobStartup = false;
	bool bEchoLines = App.test_backward != 0;

	std::string line;
	while (reader.PrevLine(line)) {
		if (bEchoLines) { printf("%s\n", line.c_str()); }

		/* the daemon startup banner looks like this.
		05/22/12 14:19:40 ******************************************************
		05/22/12 14:19:40 ** condor_master (CONDOR_MASTER) STARTING UP
		05/22/12 14:19:40 ** D:\scratch\condor\master\test\bin\condor_master.exe
		05/22/12 14:19:40 ** SubsystemInfo: name=MASTER type=MASTER(2) class=DAEMON(1)
		05/22/12 14:19:40 ** Configuration: subsystem:MASTER local:<NONE> class:DAEMON
		05/22/12 14:19:40 ** $CondorVersion: 7.8.0 Apr 30 2012 PRE-RELEASE-UWCS $
		05/22/12 14:19:40 ** $CondorPlatform: X86-WINDOWS_6.1 $
		05/22/12 14:19:40 ** PID = 5460
		05/22/12 14:19:40 ** Log last touched time unavailable (No such file or directory)
		05/22/12 14:19:40 ******************************************************
		*/

		if (line.find(startup_banner_text) != string::npos) {
			if (App.diagnostic) {
				printf("found %s startup banner with pid %s\n",
					pszDaemon, possible_daemon_pid.c_str());
			}
			if (pliDaemon->pid.empty()) {
				if (App.diagnostic) { printf("storing %s pid : %s\n", pszDaemon, possible_daemon_pid.c_str()); }
				pliDaemon->pid = possible_daemon_pid;
			}
			if (pliDaemon->addr.empty()) {
				if (App.diagnostic) { printf("storing %s addr : %s\n", pszDaemon, possible_daemon_addr.c_str()); }
				pliDaemon->addr = possible_daemon_addr;
			} else if (App.diagnostic) {
				const char * poss = possible_daemon_addr.c_str();
				printf("not storing %s addr : %s\n\tbecause addr = %s\n", pszDaemon, poss ? poss : "NULL", pliDaemon->addr.c_str());
			}

			bInsideBanner = false;
			// found daemon startup header, everything before this is from a different invocation
			// of the daemon, so stop searching.
			if (App.diagnostic) { printf("quitting scan of %s log file\n\n", pszDaemon); }
			break;
		}
		// are we inside the banner?
		if (line.find(" ** Log last touched ") != string::npos) {
			if (App.diagnostic) { printf("inside banner\n"); }
			bInsideBanner = true;
			continue;
		}
		if (bInsideBanner) {
			// parse PID from banner
			size_t ix = line.find(" ** PID = ");
			if (ix != string::npos) {
				possible_daemon_pid = line.substr(ix+10);
			}
			continue;
		}

		// if we get to here, we are not inside the banner.

		// DaemonCore: command socket at <sin>
		// DaemonCore: private command socket at <sin>
		size_t ix = line.find(" DaemonCore: command socket at <");
		if (ix == string::npos) ix = line.find(" DaemonCore: private command socket at <");
		if (ix != string::npos) {
			if (App.diagnostic) { printf("potential addr from: %s\n", line.c_str()); }
			possible_daemon_addr = line.substr(line.find("<",ix));
			continue;
		}

		// special parsing for master log.
		if (bMasterLog) {

			// parse "Sent signal NNN to DAEMON (pid NNNN)"
			ix = line.find("Sent signal");
			if (ix != string::npos) {
				size_t ix2 = line.find(" to ", ix);
				if (ix2 != string::npos) {
					ix2 += 4;  // 4 == sizeof " to "
					const size_t cch3 = sizeof(" (pid ")-1;
					size_t ix3 = line.find(" (pid ", ix2);
					if (ix3 != string::npos) {
						size_t ix4 = line.find(")", ix3);
						std::string daemon = line.substr(ix2, ix3-ix2);
						std::string pid = line.substr(ix3+cch3, ix4-ix3-cch3);
						App.SubsysToDaemon(daemon);
						if (info.find(daemon) != info.end()) {
							LOG_INFO * pliTemp = info[daemon];
							if (pliTemp->pid.empty()) {
								pliTemp->pid = pid;
								if (App.diagnostic)
									printf("From Sent Signal: %s = %s\n", daemon.c_str(), pid.c_str());
							}
						}
					}
				}
				continue;
			}

			// parse "Started DaemonCore process PATH, pid and pgroup = NNNN"
			ix = line.find("Started DaemonCore process");
			if (ix != string::npos) {
				const size_t cch2 = sizeof(", pid and pgroup = ")-1;
				size_t ix2 = line.find(", pid and pgroup = ", ix);
				if (ix2 != string::npos) {
					std::string pid = line.substr(ix2+cch2);
					const size_t cch3 = sizeof("condor_")-1;
					size_t ix3 = line.rfind("condor_");
					if (ix3 > ix && ix3 < ix2) {
						size_t ix4 = line.find_first_of("\".", ix3);
						if (ix4 > ix3 && ix4 < ix2) {
							std::string daemon = line.substr(ix3+cch3,ix4-ix3-cch3);
							App.SubsysToDaemon(daemon);
							if (info.find(daemon) != info.end()) {
								LOG_INFO * pliD = info[daemon];
								if (pliD->pid.empty()) {
									info[daemon]->pid = pid;
									if (App.diagnostic)
										printf("From Started DaemonCore process: %s = %s\n", daemon.c_str(), pid.c_str());
								}
							}
						}
					}
				}
			}

			//  The DAEMON (pid NNNN) exited with status NNNN
			ix = line.find(") exited with status ");
			if (ix != string::npos) {
				const size_t cch1 = sizeof(") exited with status ")-1;
				const size_t cch2 = sizeof(" (pid ")-1;
				size_t ix2 = line.rfind(" (pid ", ix);
				if (ix2 != string::npos) {
					const size_t cch3 = sizeof("The ")-1;
					size_t ix3 = line.rfind("The ", ix2);
					if (ix3 != string::npos) {
						std::string daemon = line.substr(ix3+cch3, ix2-ix3-cch3);
						App.SubsysToDaemon(daemon);

						std::string exited_pid = line.substr(ix2+cch2, ix-ix2-cch2);
						std::string exit_code = line.substr(ix+cch1);
						if (info.find(daemon) != info.end()) {
							LOG_INFO * pliD = info[daemon];
							if (pliD->pid.empty()) {
								pliD->pid = exited_pid;
								pliD->exit_code = exit_code;
								if (App.diagnostic) {
									printf("From exited with status: %s = %s (exit %s)\n",
											daemon.c_str(), exited_pid.c_str(), exit_code.c_str());
								}
							}
						}
					}
				}
			}

			ix = line.find("All daemons are gone.  Exiting.");
			if (ix != string::npos) {
				// mark all but master daemon as exited.
			}

			// **** condor_master (condor_MASTER) pid 6320 EXITING WITH STATUS 0
			ix = line.find(" (condor_MASTER) pid ");
			if (ix != string::npos) {
				const size_t cch1 = sizeof(" (condor_MASTER) pid ")-1;
				const size_t cch2 = sizeof(" EXITING WITH STATUS ")-1;
				size_t ix2 = line.find(" EXITING WITH STATUS ");
				if (ix2 != string::npos) {
					std::string pid = line.substr(ix+cch1, ix2-ix-cch1);
					std::string exit_code = line.substr(ix2+cch2);
					LOG_INFO * pliMaster = info["Master"];
					pliMaster->pid = pid;
					pliMaster->exit_code = exit_code;
					if (App.diagnostic) {
						printf("From EXITING WITH STATUS: Master = %s (exit %s)\n", pid.c_str(), exit_code.c_str());
					}
				}
			}

		} else if (bSlotLog) { // special parsing for StarterLog.slotN

			// if we find an exited message for the starter, then nothing before that in the log will be
			// 'live' anymore, so we can quit scanning.
			if (line.find(" (condor_STARTER) pid") != string::npos &&
				line.find("EXITING WITH STATUS") != string::npos) {
				if (App.diagnostic) {
					printf("found EXITING line for starter\nquitting scan of %s log file\n\n", it->first.c_str());
				}
				break;
			}

			// top line of JobStartup is " Communicating with shadow <"
			//
			ix = line.find(" Communicating with shadow <");
			if (ix != string::npos) {
				bInsideJobStartup = false;
				//possible_job_shadow_addr = line.substr(line.find("<",ix));
			}

			if (bInsideJobStartup) {

				// parse jobid  "Starting a %s universe job with ID: %d.%d\n"
				ix = line.find(" Starting a ");
				if (ix != string::npos) {
					size_t ix2 = line.find(" universe job with ID: ", ix);
					if (ix2 != string::npos) {
						possible_job_id = line.substr(line.find(": ", ix2)+2);
						if (App.diagnostic) { printf("found JobId %s\n", possible_job_id.c_str()); }
						if ( ! possible_job_pid.empty()) {
							pid_t pid = atoi(possible_job_pid.c_str());
							if (App.diagnostic) { printf("Adding %s = %s to job->pid map\n", possible_job_id.c_str(), possible_job_pid.c_str()); }
							job_to_pid[possible_job_id] = pid;
						}
					}
				}

				ix = line.find(" About to exec ");
				if (ix != string::npos) {
					pid_t pid = atoi(possible_job_pid.c_str());
					//possible_job_exe = line.substring(ix+sizeof(" About to exec ")-1);
					App.pid_to_program[pid] = line.substr(ix+sizeof(" About to exec ")-1);
				}
			}

			// bottom line of JobStartup is "Create_Process succeeded, pid=%d"
			//
			ix = line.find("Create_Process succeeded, pid=");
			if (ix != string::npos) {
				possible_job_pid = line.substr(line.find("=",ix)+1);
				if (App.diagnostic) { printf("found JobPID %s\n", possible_job_pid.c_str()); }
				pid_t pid = atoi(possible_job_pid.c_str());
				std::map<pid_t,bool>::iterator it_pids = dead_pids.find(pid);
				if (it_pids != dead_pids.end()) {
					possible_job_pid.clear();
					dead_pids.erase(it_pids);
				} else {
					// running_pids.push_back(pid);
					bInsideJobStartup = true;
				}
			}

			ix = line.find("Process exited, pid=");
			if (ix != string::npos) {
				std::string exited_pid = line.substr(line.find("=",ix)+1);
				if (App.diagnostic) { printf("found PID exited %s\n", exited_pid.c_str()); }
				pid_t pid = atoi(exited_pid.c_str());
				dead_pids[pid] = true;
			}

		}
	}

	if (App.test_backward > 1) {
		printf("------- scanning complete -------\n");
		while (reader.PrevLine(line)) {
			printf("%s\n", line.c_str());
		}
	}

	reader.Close();
}

// scan the log files found in the log info map for PIDS and ADDRESSES
// and use that information to populate the info map and to build a mapping
// of jobid to pid.
//
static void scan_logs_for_info(LOG_INFO_MAP & info, MAP_STRING_TO_PID & job_to_pid, bool fAddressesOnly)
{
	// scan the master log first
	LOG_INFO_MAP::const_iterator it = info.find("Master");
	if (it != info.end()) {
		scan_a_log_for_info(info, job_to_pid, it);
	}
	for (it = info.begin(); it != info.end(); ++it) {
		// scan starter logs.
		if (starts_with(it->first.c_str(),"Slot",NULL) || 
			starts_with(it->first.c_str(),"Starter", NULL)) {
			if ( ! fAddressesOnly) { scan_a_log_for_info(info, job_to_pid, it); }
			continue;
		}

		// scan schedd and startd logs only if we don't already
		// have their pid and/or addresses
		LOG_INFO * pliDaemon = it->second;
		if ( ! pliDaemon->pid.empty() && ! pliDaemon->addr.empty())
			continue;
		if (it->first == "Shadow") // shadows don't even have command ports...
			continue;
		if (it->first != "Schedd" && it->first != "Startd" && ! fAddressesOnly)
			continue;

		scan_a_log_for_info(info, job_to_pid, it);
	}
}

static void query_log_dir(const char * log_dir, LOG_INFO_MAP & info, bool chatty)
{

	Directory dir(log_dir);
	dir.Rewind();
	while (const char * file = dir.Next()) {

		// figure out what kind of file this is.
		//
		int filetype = 0; // nothing
		std::string name;
		const char * pu = NULL;
		if (file[0] == '.' && ends_with(file, "_address", &pu)) {
			name.insert(0, file+1, pu - file-1);
			name[0] = toupper(name[0]); // capitalize it.
			filetype = 1; // address
			if (chatty) printf("\t\tAddress file: %s\n", name.c_str());
		} else if (ends_with(file, "Log", &pu)) {
			name.insert(0, file, pu-file);
			MAP_STRING_TO_STRING::const_iterator alt = App.log_to_daemon.find(name);
			if (alt != App.log_to_daemon.end()) { name = alt->second; }
			/*
			if (name == "Sched") name = "Schedd"; // the schedd log is called SchedLog
			if (name == "Start") name = "Startd"; // the startd log is called StartLog
			if (name == "Match") name = "Negotiator"; // the Match log is a secondary Negotiator log
			if (name == "Cred") name = "Credd"; // the credd log is called CredLog
			if (name == "Kbd") name = "Kbdd"; // the kbdd log is called KbdLog
			*/
			filetype = 2; // log
		} else if (ends_with(file, "Log.old", &pu)) {
			name.insert(0, file, pu-file);
			MAP_STRING_TO_STRING::const_iterator alt = App.log_to_daemon.find(name);
			if (alt != App.log_to_daemon.end()) { name = alt->second; }
			/*
			if (name == "Sched") name = "Schedd"; // the schedd log is called SchedLog
			if (name == "Start") name = "Startd"; // the startd log is called StartLog
			if (name == "Match") name = "Negotiator"; // the Match log is a secondary Negotiator log
			if (name == "Cred") name = "Credd";
			if (name == "Kbd") name = "Kbdd";
			*/
			filetype = 3; // previous.log
		} else if (starts_with(file, "StarterLog.", &pu)) {
			const char * pname = pu;
			if (ends_with(pname, ".old", &pu)) {
				name.insert(0, pname, pu - pname);
				filetype = 3; // old slotlog
			} else {
				name.insert(0, pname);
				filetype = 2; // slotlog
			}
			name[0] = toupper(name[0]);
		}
		if ( ! filetype)
			continue;

		const char * fullpath = dir.GetFullPath();
		//if (chatty) printf("\t%d %-12s %s\n", filetype, name.c_str(), fullpath);

		if (filetype > 0) {
			LOG_INFO * pli = find_or_add_log_info(info, name, log_dir);
			switch (filetype) {
				case 1: // address file
					read_address_file(fullpath, pli->addr);
					if (chatty) printf("\t\t%-12s addr = %s\n", name.c_str(), pli->addr.c_str());
					if (App.diagnostic) printf("storing %s addr : %s from address file\n", name.c_str(), pli->addr.c_str());
					break;
				case 2: // log file
					pli->log = file;
					//if (chatty) printf("\t\t%-12s log = %s\n", name.c_str(), pli->log.c_str());
					break;
				case 3: // old log file
					pli->log_old = file;
					//if (chatty) printf("\t\t%-12s old = %s\n", name.c_str(), pli->log_old.c_str());
					break;
			}
		}
	}
}

void print_log_info(LOG_INFO_MAP & info)
{
	printf("\n");
	printf("%-12s %-8s %-10s %-24s %s, %s\n", "Daemon", "PID", "Exit", "Addr", "Log", "Log.Old");
	printf("%-12s %-8s %-10s %-24s %s, %s\n", "------", "---", "----", "----", "---", "-------");
	for (LOG_INFO_MAP::const_iterator it = info.begin(); it != info.end(); ++it)
	{
		LOG_INFO * pli = it->second;
		printf("%-12s %-8s %-10s %-24s %s, %s\n",
			it->first.c_str(), pli->pid.c_str(), pli->exit_code.c_str(), pli->addr.c_str(),
			pli->log.c_str(), pli->log_old.c_str());
	}
}

void print_daemon_info(LOG_INFO_MAP & info, bool fQuick)
{
	printf("\n");
	if (fQuick) {
		printf("%-12s %-6s %-6s %s\n", "Daemon", "PID", "Exit", "Addr");
		printf("%-12s %-6s %-6s %s\n", "------", "---", "----", "----");
	} else {
		printf("%-12s %-6s %-6s %-6s %-6s %-24s %s\n", "Daemon", "Alive", "PID", "PPID", "Exit", "Addr", "Executable");
		printf("%-12s %-6s %-6s %-6s %-6s %-24s %s\n", "------", "-----", "---", "----", "----", "----", "----------");
	}
	for (LOG_INFO_MAP::const_iterator it = info.begin(); it != info.end(); ++it)
	{
		LOG_INFO * pli = it->second;
		if (pli->pid.empty() && pli->exit_code.empty() && pli->addr.empty())
			continue;
		bool active = false;
		const char * pid = "no";
		const char * last_pid = "no";
		const char * exit = pli->exit_code.c_str();
		if ( ! exit || !exit[0]) {
			exit = "no";
			pid = pli->pid.c_str();
			if ( ! pid || !pid[0]) pid = "?";
		} else {
			last_pid = pli->pid.c_str();
			if ( ! last_pid || !last_pid[0]) last_pid = "?";
		}

		char * pexe = NULL;
		char * ppid = NULL;
		if ( ! fQuick && ! pli->addr.empty() && ! pli->pid.empty() && pli->exit_code.empty()) {
			ppid = get_daemon_param(pli->addr.c_str(), "PPID");
			if (ppid) {
				active = true;
				std::string subsys = it->first;
				App.DaemonToSubsys(subsys);
				pexe = get_daemon_param(pli->addr.c_str(), subsys.c_str());
			}
		}

		if (fQuick) {
			printf("%-12s %-6s %-6s %s\n", it->first.c_str(), pid, exit, pli->addr.c_str());
		} else {
			printf("%-12s %-6s %-6s %-6s %-6s %-24s %s\n",
				it->first.c_str(),
				active ? "yes" : "no",
				pid, ppid ? ppid : "no", exit,
				pli->addr.c_str(),
				pexe ? pexe : pli->exe_path.c_str()
			);
		}

		if (pexe) free(pexe);
		if (ppid) free(ppid);
	}
}


// make  DC_QUERY_READY query
static bool get_daemon_ready(const char * addr, const char * requirements, time_t sec_to_wait, ClassAd & ready_ad)
{
	bool got_ad = false;

	Daemon dae(DT_ANY, addr, addr);

	ReliSock sock;
	sock.timeout(sec_to_wait + 2); // wait 2 seconds longer than the requested timeout.
	if (!sock.connect(addr)) {
		fprintf(stderr, "cannot connect to %s\n", addr);
		return false;
	}

	if ( ! dae.startCommand(DC_QUERY_READY, &sock, sec_to_wait + 2)) {
		if (App.diagnostic > 1) { fprintf(stderr, "Can't startCommand DC_QUERY_READY to %s\n", addr); }
		sock.close();
		return false;
	}

	ClassAd ad;
	ad.Assign("WaitTimeout", sec_to_wait);
	if (requirements && *requirements) {
		if ( ! ad.AssignExpr("ReadyRequirements", requirements)) {
			fprintf(stderr, "invalid expression for DC_QUERY_READY ReadyRequirements.  Expr = '%s'\n", requirements);
			return false;
		}
	}
	ad.Assign("WantAddrs", true);

	sock.encode();
	if ( ! putClassAd(&sock, ad)) {
		if (App.diagnostic > 1) { fprintf(stderr, "Can't send DC_QUERY_READY to %s\n", addr); }
	} else if ( ! sock.end_of_message()) {
		if (App.diagnostic > 1) { fprintf(stderr, "Can't send end of message to %s\n", addr); }
	} else {
		ready_ad.Clear();
		sock.decode();
		if ( ! getClassAd(&sock, ready_ad)) {
			if (App.diagnostic > 1) { fprintf(stderr, "Can't receive end of message from %s\n", addr); }
		} else if ( ! sock.end_of_message()) {
			if (App.diagnostic > 1) { fprintf(stderr, "Can't receive end of message from %s\n", addr); }
		} else {
			got_ad = true;
		}
	}
	sock.close();
	return got_ad;
}

// make a DC config val query for a particular daemon.
static char * get_daemon_param(const char * addr, const char * param_name)
{
	char * value = NULL;

	Daemon dae(DT_ANY, addr, addr);

	ReliSock sock;
	sock.timeout(20);   // years of research... :)
	if (!sock.connect(addr)) {
		fprintf(stderr, "Cannot connect to %s\n", addr);
		return value;
	}

	if ( ! dae.startCommand(CONFIG_VAL, &sock, 2)) {
		if (App.diagnostic > 1) { fprintf(stderr, "Can't startCommand CONFIG_VAL for %s to %s\n", param_name, addr); }
		sock.close();
		return value;
	}

	sock.encode();
	//if (App.diagnostic) { printf("Querying %s for $(%s) param\n", addr, param_name); }

	if ( ! sock.put(param_name)) {
		if (App.diagnostic > 1) { fprintf(stderr, "Can't send CONFIG_VAL for %s to %s\n", param_name, addr); }
	} else if ( ! sock.end_of_message()) {
		if (App.diagnostic > 1) { fprintf(stderr, "Can't send end of message to %s\n", addr); }
	} else {
		sock.decode();
		if ( ! sock.code_nullstr(value)) {
			if (App.diagnostic > 1) { fprintf(stderr, "Can't receive reply from %s\n", addr); }
		} else if( ! sock.end_of_message()) {
			if (App.diagnostic > 1) { fprintf(stderr, "Can't receive end of message from %s\n", addr); }
		} else if (App.diagnostic > 1) {
			printf("DC_CONFIG_VAL %s, %s = %s\n", addr, param_name, value);
		}
	}
	sock.close();
	return value;
}

#if 0
static char * query_a_daemon2(const char * addr, const char * /*name*/)
{
	char * value = NULL;
	Daemon dae(DT_ANY, addr, addr);

	ReliSock sock;
	sock.timeout(20);   // years of research... :)
	sock.connect(addr);
	char * param_name = "PID";
	dae.startCommand(DC_CONFIG_VAL, &sock, 2);
	sock.encode();
	if (App.diagnostic) {
		printf("Querying %s for $(%s) param\n", addr, param_name);
	}
	if ( ! sock.code(param_name)) {
		if (App.diagnostic) fprintf(stderr, "Can't send request (param_name) to %s\n", param_name, addr);
	} else if ( ! sock.end_of_message()) {
		if (App.diagnostic) fprintf(stderr, "Can't send end of message to %s\n", addr);
	} else {
		sock.decode();
		char * val = NULL;
		if ( ! sock.code(val)) {
			if (App.diagnostic) fprintf(stderr, "Can't receive reply from %s\n", addr);
		} else if( ! sock.end_of_message()) {
			if (App.diagnostic) fprintf(stderr, "Can't receive end of message from %s\n", addr);
		} else if (App.diagnostic) {
			printf("Received %s from %s for $(%s)\n", val, addr, param_name);
		}
		if (val) {
			value = strdup(val);
			free(val);
		}
	}
	sock.end_of_message();
	sock.close();
	return value;
}
#endif

//  use DC_CONFIG_VAL command to fill in daemon PIDs
//
static void query_daemons_for_pids(LOG_INFO_MAP & info)
{
	for (LOG_INFO_MAP::iterator it = info.begin(); it != info.end(); ++it) {
		LOG_INFO * pli = it->second;
		if (pli->name == "Kbdd") continue;
		if (pli->pid.empty() && ! pli->addr.empty()) {
			char * pid = get_daemon_param(pli->addr.c_str(), "PID");
			if (pid) {
				pli->pid.insert(0,pid);
				free(pid);
			}
		}
		// now put that in the daemon address table.
		if (pli->exit_code.empty() && ! pli->pid.empty()) {
			pid_t pid = atoi(pli->pid.c_str());
			if (pid) {
				App.pid_to_addr[pid] = pli->addr;
			}
		}
	}
}

/*
static void query_daemons_for_config_built_ins(LOG_INFO_MAP & info)
{
	for (LOG_INFO_MAP::iterator it = info.begin(); it != info.end(); ++it) {
		LOG_INFO * pli = it->second;
		if (pli->name == "Kbdd") continue;
		if ( ! pli->addr.empty()) {
			char * foo = get_daemon_param(pli->addr, "PID"); if (foo) free (foo);
			foo = get_daemon_param(pli->addr, "PPID"); if (foo) free (foo);
			foo = get_daemon_param(pli->addr, "REAL_UID"); if (foo) free (foo);
			foo = get_daemon_param(pli->addr, "REAL_GID"); if (foo) free (foo);
			foo = get_daemon_param(pli->addr, "USERNAME"); if (foo) free (foo);
			foo = get_daemon_param(pli->addr, "HOSTNAME"); if (foo) free (foo);
			foo = get_daemon_param(pli->addr, "FULL_HOSTNAME"); if (foo) free (foo);
			foo = get_daemon_param(pli->addr, "TILDE"); if (foo) free (foo);
		}
	}
}
*/

static bool ping_a_daemon(std::string addr, std::string /*name*/)
{
	bool success = false;
	Daemon dae(DT_ANY, addr.c_str(), addr.c_str());

	ReliSock sock;
	sock.timeout(20);   // years of research... :)
	if (!sock.connect(addr.c_str())) {
		return false;
	}
	dae.startCommand(DC_NOP, &sock, 20);
	success = sock.end_of_message();
	sock.close();
	return success;
}

static void ping_all_known_addrs(LOG_INFO_MAP & info)
{
	for (LOG_INFO_MAP::const_iterator it = info.begin(); it != info.end(); ++it) {
		LOG_INFO * pli = it->second;
		if ( ! pli->addr.empty()) {
			ping_a_daemon(pli->addr, it->first);
		}
	}
}
