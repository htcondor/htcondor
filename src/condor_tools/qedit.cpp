/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_qmgr.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "condor_attributes.h"
#include "condor_distribution.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "util_lib_proto.h"   // for blankline()
#include "condor_query.h"     // for CondorQuery class (constraint building)

static const char *MyName = NULL;
static classad::References immutable_attrs;
static classad::References protected_attrs;
static classad::References secure_attrs;
enum {
	NORMAL_ATTR=0,
	IMMUTABLE_ATTR,
	PROTECTED_ATTR,
	SECURE_ATTR,
};

void usage(int exit_code)
{
	FILE * out = exit_code ? stderr : stdout;
	fprintf(out,
	"Usage: %s [help-opts] [general-opts] <restriction-list> <edit-list>\n" , MyName);

	fprintf(out,
	"\n    [help-opts] are:\n"
	"\t-help\t\t   Display this screen\n"
	"\t-debug\t\t   Display debugging info to console\n"
	"\t-dry-run\t   Display whether edit is allowed\n"
	"\n    [general-opts] are:\n"
	"\t-name <name>\t   Name of Scheduler\n"
	"\t-pool <pool>\t   Use host as the central manager to query\n"
	);

	fprintf(out,
	"\n    <restriction-list> is one or more of:\n"
	"\t<cluster>\t   Edit jobs in a specific cluster\n"
	"\t<cluster>.<proc>   Edit a specific job\n"
	"\t<owner>\t\t   Edit jobs owned by <owner>\n"
	"\t-jobids <id-list>  Edit jobs in <id-list>\n"
	"\t-owner <owner>     Edit jobs owned by <owner>\n"
	"\t-constraint <expr> Edit jobs that match <expr>\n"
//	"\t-allusers\t  Consider jobs from all users\n"
	);

	fprintf(out,
	"\n    <edit-list> is a set of <attr> and <value> pairs, in one of two forms\n"
	"\t<attr> <value>\t   Two argument form, alternate <attr> and <value>\n"
	"\t<attr>=<value>\t   Single argument form, = separates <attr> and <value>\n"
	"\t-edits[:<form>] <file> Read edits from <file> in ClassAd format.\n"
	"\t           where <form> is one of:\n"
	"\t       auto    default, guess the format from reading the input stream\n"
	"\t       long    The traditional -long form\n"
	"\t       xml     XML form, the same as -xml\n"
	"\t       json    JSON classad form, the same as -json\n"
	"\t       new     'new' classad form without newlines\n"
	);

	fprintf(out,
	"\nAt least one restriction and one edit must be specified, restrictions must be\n"
	"first. The restriction-list should be: a list of job and/or cluster ids, a\n"
	"single owner name, or one or more of -owner, -jobids, -constraint arguments.\n"
	"If both owner and jobids are specified, only jobs that match both will be\n"
	"edited. If a -constraint argument is also provided, only jobs that also match\n"
	"the constraint will be edited.\n"
	"If only a -constraint argument is provided, jobs not owned by the current user\n"
	"will be ignored unless the current user is a queue superuser.\n"
	"\nOnce a restriction has been specified, all remaining arguments that do not\n"
	"start with a - are part of the <edit-list>.  An argument that contains = will\n"
	"be treated as an <attr>=<value> pair, otherwise arguments should alternate\n"
	"between <attr> and <value>\n"
	);
	exit(exit_code);
}

const char * getProtectedTypeString(int prot)
{
	switch (prot) {
	case NORMAL_ATTR: return "normal";
	case IMMUTABLE_ATTR: return "immutable";
	case PROTECTED_ATTR: return "protected";
	case SECURE_ATTR: return "secure";
	}
	return "invalid";
}

int IsProtectedAttribute(const char *attr)
{
	if (immutable_attrs.empty()) {
		param_and_insert_attrs("IMMUTABLE_JOB_ATTRS", immutable_attrs);
		param_and_insert_attrs("SYSTEM_IMMUTABLE_JOB_ATTRS", immutable_attrs);
		param_and_insert_attrs("PROTECTED_JOB_ATTRS", protected_attrs);
		param_and_insert_attrs("SYSTEM_PROTECTED_JOB_ATTRS", protected_attrs);
		param_and_insert_attrs("SECURE_JOB_ATTRS", secure_attrs);
		param_and_insert_attrs("SYSTEM_SECURE_JOB_ATTRS", secure_attrs);
	}

	if (immutable_attrs.find(attr) != immutable_attrs.end()) return IMMUTABLE_ATTR;
	if (protected_attrs.find(attr) != protected_attrs.end()) return PROTECTED_ATTR;
	if (secure_attrs.find(attr) != secure_attrs.end()) return SECURE_ATTR;
	return NORMAL_ATTR;
}

static void appendProcsForCluster(std::string & out, int cluster, std::set<int> & procs)
{
	if (cluster <= 0)
		return;

	if (procs.empty()) {
		formatstr_cat(out, ATTR_CLUSTER_ID " == %d ", cluster);
	} else {
		formatstr_cat(out, "( " ATTR_CLUSTER_ID " == %d && ", cluster);
		if (procs.size() > 1) {
			out += "(";
			const char * op = "";
			for (std::set<int>::iterator it = procs.begin(); it != procs.end(); ++it) {
				out += op;
				formatstr_cat(out, ATTR_PROC_ID " == %d ", *it);
				op = " || ";
			}
			out += ")";
		} else {
			formatstr_cat(out, ATTR_PROC_ID " == %d ", *procs.begin());
		}
		out += ")";
	}
}

const char * makeJobidConstraint(std::string & out, std::set<JOB_ID_KEY> & jobids)
{
	if (jobids.empty()) return out.c_str();
	int cluster = -1;
	std::set<int> procs;
	for (std::set<JOB_ID_KEY>::iterator jid = jobids.begin(); jid != jobids.end(); ++jid) {
		if (jid->cluster != cluster) {
			if ( ! out.empty()) { out += "|| "; }
			appendProcsForCluster(out, cluster, procs);
			procs.clear();
			cluster = jid->cluster;
		}
		if (jid->proc >= 0) { procs.insert(jid->proc); }
	}
	if ( ! out.empty()) { out += "|| "; }
	appendProcsForCluster(out, cluster, procs);
	return out.c_str();
}


int
main(int argc, const char *argv[])
{
	const char * schedd_name = NULL;
	const char * pool_name = NULL;

	MyName = argv[0];
	if (argc < 2) {
		usage(1);
	}

	GenericQuery gquery;
	const char * ownername = NULL;
	bool dash_dryrun = false;
	bool transaction_aborted = false;
	bool only_my_jobs = true;
	bool bare_arg_must_identify_jobs = true;
	bool has_arbitrary_constraint = false;
	int dash_diagnostic = 0;
	StringList job_list; // list of job ids (or mixed job & cluster id's to modify)
	std::map<std::string, std::string> kvp_list; // Classad of attr=value pairs to set

	myDistro->Init( argc, argv );
	set_priv_initialize(); // allow uid switching if root
	config();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	const char * pcolon;

	only_my_jobs = param_boolean("CONDOR_Q_ONLY_MY_JOBS", only_my_jobs);

	// prior to 8.5.8, argument parsing was strictly ordered, but starting with 8.5.8 it will not be.
	// general argument parsing policy:
	//   the first argument not preceeded by a - must be either a job-id or username (this is for backward compatibility)
	//     this rule applies only if -constraint, -owner, -jobids, or -edits arguments have not yet be used.
	//   non-numeric arguments will be treated to as attribute names (unless preceeded by a -).
	//   the next argument after an attribute name will be treated as an expression, (even if it begins with -)
	//   If it does not follow an attribute, a numeric argument will be presumed to be a jobid or cluster id.
	for (int ixarg = 1; ixarg < argc; ++ixarg) {
		const char * parg = argv[ixarg];
		if (is_dash_arg_prefix(parg, "help", 1)) {
			usage(0);
			exit(0);
		}
		else
		if (is_dash_arg_prefix(parg, "name", 1)) {
			if (++ixarg >= argc || *argv[ixarg] == '-') {
				fprintf(stderr, "%s: %s requires a SCHEDD name as an argument\n", MyName, parg);
				exit(1);
			}
			schedd_name = argv[ixarg];
		}
		else
		if (is_dash_arg_prefix(parg, "pool", 1)) {
			if (++ixarg >= argc || *argv[ixarg] == '-') {
				fprintf(stderr, "%s: %s requires a HTCondor collector name or address as an argument\n", MyName, parg);
				exit(1);
			}
			pool_name = argv[ixarg];
		}
		else
		if (is_dash_arg_colon_prefix(parg, "debug", &pcolon, 1)) {
			// output dprintf messages to stderror at TOOL_DEBUG level
			dprintf_set_tool_debug("TOOL", 0);
			if (pcolon && pcolon[1]) {
				set_debug_flags( ++pcolon, 0 );
			}
		}
		else
		if (is_dash_arg_colon_prefix(parg, "diagnostic", &pcolon, 4)) {
			dash_diagnostic = 1;
			if (pcolon) { dash_diagnostic = atoi(++pcolon); }
		}
		else
		if (is_dash_arg_prefix(parg, "dry-run", 3)) {
			dash_dryrun = true;
		}
		else
		if (is_dash_arg_prefix(parg, "constraint", 1)) {
			if (++ixarg >= argc) {
				fprintf(stderr, "%s: %s requires a ClassAd expression an argument\n", MyName, parg);
				exit(1);
			}
			ExprTree * expr = NULL;
			if (0 != ParseClassAdRvalExpr(argv[ixarg], expr)) {
				fprintf(stderr, "%s: invalid constraint: %s\n", MyName, argv[ixarg]);
				exit(1);
			}
			delete expr;
			gquery.addCustomAND(argv[ixarg]);
			bare_arg_must_identify_jobs = false;
			has_arbitrary_constraint = true;
		}
		else
		if (is_dash_arg_prefix(parg, "owner", 1)) {
			if (++ixarg >= argc || *argv[ixarg] == '-') {
				fprintf(stderr, "%s: %s requires a owner name argument\n", MyName, parg);
				exit(1);
			}
			ownername = argv[ixarg]; // remember this in case we need it later
			std::string expr; formatstr(expr, ATTR_OWNER "==\"%s\"", ownername);
			gquery.addCustomAND(expr.c_str());
			bare_arg_must_identify_jobs = false;
			only_my_jobs = false;
		}
		else
		if (is_dash_arg_prefix(parg, "jobids", 1)) {
			if (++ixarg >= argc || *argv[ixarg] == '-') {
				fprintf(stderr, "%s: %s requires a list of job ids as an argument\n", MyName, parg);
				exit(1);
			}
			job_list.initializeFromString(argv[ixarg]);
			bare_arg_must_identify_jobs = false;
			only_my_jobs = false;
		}
		else
		if (is_dash_arg_colon_prefix(parg, "edits", &pcolon, 2)) {
			if (++ixarg >= argc || (argv[ixarg][0] == '-' && argv[ixarg][1] != 0)) {
				fprintf(stderr, "%s: %s requires a filename as an argument\n", MyName, parg);
				exit(1);
			}

			// load attributes and values to set from classad file.
			//
			const char * file = argv[ixarg];
			ClassAdFileParseType::ParseType file_format = ClassAdFileParseType::Parse_auto;
			if (pcolon) {
				file_format = parseAdsFileFormat(++pcolon, file_format);
			}

			FILE* fh;
			bool close_file = false;
			if (MATCH == strcmp(file, "-")) {
				fh = stdin;
				close_file = false;
			} else {
				fh = safe_fopen_wrapper_follow(file, "r");
				if (fh == NULL) {
					fprintf(stderr, "Can't open file of edits: %s\n", file);
					exit(1);
				}
				close_file = true;
			}
			CondorClassAdFileIterator adIter;
			adIter.begin(fh, close_file, file_format);
			ClassAd * ad = adIter.next(NULL);
			if ( ! ad) {
				fprintf(stderr, "Unable to read file of edits: %s\n", file);
				exit(1);
			}

			// convert the ClassAd of edits into our kvp_list data structure.
			for (ClassAd::iterator it = ad->begin(); it != ad->end(); ++it) {
				const char * attr = it->first.c_str();
				kvp_list[attr] = ExprTreeToString(it->second);
			}
			delete ad;
		}
		else
		if (*parg == '-') {
			fprintf(stderr, "%s: %s is not a valid option\n", MyName, parg);
			usage(1);
			exit(1);
		}
		else if (bare_arg_must_identify_jobs) {
			bare_arg_must_identify_jobs = false;
			only_my_jobs = false;
			// arg is bare, and the first one of that type, so it must be a jobid or owner name
			if (isdigit(*parg)) {
				job_list.initializeFromString(parg);
				// treat all of the following args that start with a digit as job ids also
				while (ixarg+1 < argc && isdigit(*argv[ixarg+1])) {
					job_list.initializeFromString(argv[++ixarg]);
				}
			} else {
				// assume that this argument is an ownername
				ownername = argv[ixarg];
				std::string expr; formatstr(expr, ATTR_OWNER "==\"%s\"", ownername);
				gquery.addCustomAND(expr.c_str());
			}
		}
		else {
			// if we get to here we have an arg not prefixed by - and we already have a jobid list or owner
			// so bare arg must be an attribute or attribute=value
			std::string attr(parg), value;
			const char * pequal = strchr(parg, '=');
			if (pequal) {
				// arg is actually a attr=value pair, so no second argument is needed
				attr.erase(pequal-parg, std::string::npos); trim(attr);
				value = pequal+1; trim(value);
				//fprintf(stdout, "single: %s = %s\n", attr.c_str(), value.c_str());
			} else {
				// arg has no =, so the next argument must be the value
				if (++ixarg >= argc) {
					fprintf(stderr, "%s : expression required after attribute %s\n", MyName, parg);
					usage(1); exit(1);
				}
				value = argv[ixarg];
				//fprintf(stdout, "split: %s = %s\n", attr.c_str(), value.c_str());
			}

			// grab the attribute part and validate it.
			if (blankline(attr.c_str()) || ! IsValidAttrName(attr.c_str())) {
				fprintf(stderr, "Update aborted, illegal attribute-name specified for attribute \"%s\".\n", attr.c_str());
				transaction_aborted = true;
				if (dash_diagnostic <= 1) // keep going for some diagnostic levels, but not normally
					break;
			}

			// grab the value part and validate it.
			ExprTree * expr = NULL;
			if (blankline(value.c_str()) || ! IsValidAttrValue(value.c_str()) || 0 != ParseClassAdRvalExpr(value.c_str(), expr)) {
				fprintf(stderr, "Update aborted, illegal value for : %s\n", attr.c_str());
				transaction_aborted = true;
				if (dash_diagnostic <= 1) // keep going for some diagnostic levels, but not normally
					break;
			}
			delete expr;
			//fprintf(stdout, "final: %s = %s\n", attr.c_str(), value.c_str());
			kvp_list[attr] = value;
		}

	} // end of argument parsing for loop

	//
	// cook and validiate arguments
	//

	ConstraintHolder constraint;
	std::string query_string;
	gquery.makeQuery(query_string);

	// cook jobid strings into either a set of job ids, or more constraints
	bool have_cluster_id = false;
	bool can_do_cluster_edits = ! has_arbitrary_constraint; // if the gquery as things other than Owner== or Job== constraints, we cannot safely do cluster edits.
	std::set<JOB_ID_KEY> jobids;
	for (const char * jobid = job_list.first(); jobid; jobid = job_list.next()) {
		JOB_ID_KEY jid(jobid);
		jobids.insert(jid);
		if (jid.proc < 0) { have_cluster_id = true; }
		else              { can_do_cluster_edits = false; }
	}

	// if we have a set of full job id's we can optimize the query, but only if we don't also have a constraint.
	if ( ! jobids.empty()) {
		bool always_ammend = true; // prior to 8.5.8 this was false
		if ( ! always_ammend && ! have_cluster_id && ! query_string.empty()) {
			fprintf(stderr, "%s : Error, cannot use both constraint and jobids\n", MyName);
			exit(1);
		}
		// ammend the constraint to match the job id's also.
		if (have_cluster_id || ! query_string.empty()) {
			query_string.clear();
			gquery.addCustomAND(makeJobidConstraint(query_string, jobids));
			gquery.makeQuery(query_string);
			jobids.clear(); // we use this as a signal to use the constrained query
		}
	}
	if ( ! query_string.empty()) {
		constraint.set(strdup(query_string.c_str()));
	}

	// got the args, now check for consistency
	if ( ! transaction_aborted) {
		if (jobids.empty() && constraint.empty()) {
			// no jobs identified as the target - just print usage
			if ( ! kvp_list.empty()) {
				fprintf(stderr, "%s : Error - there were no jobs specified.\n\tuse -constraint, jobid or owner argument to indicate which jobs to edit\n", MyName);
			}
			usage(1);
		}
		if (kvp_list.empty()) {
			if ( ! jobids.empty() || ! constraint.empty()) {
				fprintf(stderr, "%s : Error - no attributes specifed.\n", MyName);
			}
			usage(1);
		}
	}

	if (dash_diagnostic) {
		fprintf(stdout, "--- effective query ----\n");
		fprintf(stdout, "FROM %s (%s)\n", schedd_name ? schedd_name : "local", pool_name ? pool_name : "local");
		// echo back the arguments
		if ( ! jobids.empty()) {
			std::string buffer, id;
			for (std::set<JOB_ID_KEY>::iterator jid = jobids.begin(); jid != jobids.end(); ++jid) {
				jid->sprint(id);
				buffer += id;
				buffer += " ";
			}
			fprintf(stdout, "QEDIT BY JOBID : %s\n", buffer.c_str());
		}
		if ( ! constraint.empty()) {
			fprintf(stdout, "QEDIT BY CONSTRAINT : %s\n", constraint.c_str());
		}
		fprintf(stdout, "ONLY_MY_JOBS = %s\n", only_my_jobs ? "true" : "false");
		fprintf(stdout, "CAN_DO_CLUSTER_EDITS = %s\n", can_do_cluster_edits ? "true" : "false");
		for (std::map<std::string, std::string>::iterator it = kvp_list.begin(); it != kvp_list.end(); ++it) {
			const char * attr = it->first.c_str();
			const char * value = it->second.c_str();
			fprintf(stdout, "%s = %s\n", attr, value);
			int prot = IsProtectedAttribute(attr);
			if ( ! prot && (blankline(attr) || !IsValidAttrName(attr))) { prot = -1; }
			if (prot) {
				fprintf(stdout, "  %s: %s attribute\n", (prot==PROTECTED_ATTR) ? "WARNING" : "ERROR", getProtectedTypeString(prot));
			}
			ExprTree * expr = NULL;
			if (blankline(value) || ! IsValidAttrValue(value) || 0 != ParseClassAdRvalExpr(value, expr)) {
				fprintf(stdout, "  ERROR: invalid expression\n");
			}
			delete expr;
		}
		fprintf(stdout, "------------------------\n");
		exit(0);
	}

	// check for attempt to change protected attributes
	if ( ! transaction_aborted) {
		for (std::map<std::string, std::string>::iterator it = kvp_list.begin(); it != kvp_list.end(); ++it) {
			const char * attr = it->first.c_str();
			int prot = IsProtectedAttribute(attr);
			// allow attempts to edit protected attributes because of queue superusers. see gt6647
			if (prot && (prot != PROTECTED_ATTR)) {
				fprintf(stderr, "Update of %s attribute \"%s\" is not allowed.\n", getProtectedTypeString(prot), attr);
				transaction_aborted = true;
			}
		}
	}

	if (transaction_aborted) {
		fprintf(stderr, "Transaction failed.  No attributes were set.\n");
		exit(1);
	}

	DCSchedd schedd(schedd_name, pool_name);
	if ( schedd.locate(Daemon::LOCATE_FOR_LOOKUP) == false ) {
		if ( ! schedd_name) {
			fprintf( stderr, "%s: ERROR: Can't find address of local schedd\n", argv[0] );
			exit(1);
		}

		fprintf( stderr, "%s: No such schedd named %s in %s pool\n", argv[0], schedd_name, pool_name ? pool_name : "local" );
		exit(1);
	}

	// Open job queue
	Qmgr_connection *q = ConnectQ( schedd.addr(), 0, false, NULL, NULL, schedd.version() );
	if( !q ) {
		fprintf( stderr, "Failed to connect to queue manager %s\n",
				 schedd.addr() );
		exit(1);
	}

	// now that we know the schedd version. decide whether we can do cluster edits or not.
	if ( ! schedd.version()) {
		can_do_cluster_edits = false;
	} else {
		CondorVersionInfo v(schedd.version());
		if ( ! v.built_since_version(8,7,7)) {
			can_do_cluster_edits = false;  // schedd side support for cluster edits added in 8.7.7
		}
	}

	// TODO: do the transaction
	const char * dry_tag = "";
	SetAttributeFlags_t setflags = SETDIRTY;
	if (only_my_jobs) setflags |= SetAttribute_OnlyMyJobs;
	if (dash_dryrun) {
		if (schedd.version()) {
			CondorVersionInfo v(schedd.version());
			if (v.built_since_version(8,5,8)) {
				setflags |= SetAttribute_QueryOnly;
				dry_tag = "Can ";
			}
		}
		if ( ! (setflags & SetAttribute_QueryOnly)) {
			fprintf(stderr, "Error : SCHEDD %s does not support -dry-run", schedd.name());
			transaction_aborted = true;
			goto bail;
		}
	}
	if (can_do_cluster_edits) setflags |= SetAttribute_PostSubmitClusterChange;

	for (std::map<std::string, std::string>::iterator it = kvp_list.begin(); it != kvp_list.end(); ++it) {
		const char * attr = it->first.c_str();
		const char * value = it->second.empty() ? "undefined" : it->second.c_str();
		int match_count = 0;

		if ( ! jobids.empty()) {
			for (std::set<JOB_ID_KEY>::iterator jid = jobids.begin(); jid != jobids.end(); ++jid) {
				int rval = SetAttribute(jid->cluster, jid->proc, attr, value, setflags);
				if (rval < 0) {
					fprintf(stderr, "Failed to set attribute \"%s\" for job %d.%d.\n",
							attr, jid->cluster, jid->proc);
					transaction_aborted = true;
					break;
				}
				++match_count;
			}
		} else {
			int rval = SetAttributeByConstraint(constraint.Str(), attr, value, setflags);
			if (rval < 0) {
				fprintf(stderr, "Failed to set attribute \"%s\" by constraint: %s\n",
						attr, constraint.c_str());
				transaction_aborted = true;
				break;
			}
			if (setflags & SetAttribute_QueryOnly) {
				// when queryonly is passed, there return value is the match count or a negative error code.
				if ( ! rval) {
					fprintf(stderr, "No jobs match the constraint: %s\n", constraint.c_str());
					transaction_aborted = true;
					break;
				}
			}
			match_count += rval;
		}
		if (transaction_aborted) break;
		std::string count("all");
		if (match_count > 0) formatstr(count, "%d", match_count);
		printf("%sSet attribute \"%s\" for %s matching jobs.\n", dry_tag, attr, count.c_str());
	}

	if ( ! transaction_aborted && jobids.empty() && (setflags & SetAttribute_QueryOnly)) {
		#if 0 // this doesn't get only-my-jobs... <sigh>
		ClassAdList ads;
		GetAllJobsByConstraint(constraint.Str(), "ClusterId\nProcId", ads);
		#endif
	}

bail:

	if (!DisconnectQ(q) || transaction_aborted) {
		fprintf(stderr,
				"Queue transaction failed.  No attributes were set.\n");
		exit(1);
	}

	return 0;
}
