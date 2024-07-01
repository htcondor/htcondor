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


 

/*********************************************************************
  qusers command-line tool
*********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_distribution.h"
#include "condor_attributes.h"
//#include "command_strings.h"
//#include "enum_utils.h"
#include "condor_version.h"
#include "ad_printmask.h"
#include "internet.h"
#include "compat_classad_util.h"
#include "daemon.h"
#include "dc_schedd.h"
//#include "dc_collector.h"
#include "basename.h"
#include "match_prefix.h"
#include "console-utils.h"

#define USE_DC_SCHEDD_USERS_METHODS 1

// For schedd commands
#include "condor_commands.h"
#include "CondorError.h"

#ifdef USE_DC_SCHEDD_USERS_METHODS
#include "condor_q.h"
#else

enum
{
	Q_NO_SCHEDD_IP_ADDR = 20,
	Q_SCHEDD_COMMUNICATION_ERROR,
	Q_INVALID_REQUIREMENTS,
	Q_INTERNAL_ERROR,
	Q_REMOTE_ERROR,
	Q_UNSUPPORTED_OPTION_ERROR
};

int makeUsersRequestAd(
	classad::ClassAd & request_ad,
	const char * constraint,
	classad::References & attrs,
	int fetch_opts=0,
	int match_limit=-1);

int fetchUsersFromSchedd (
	DCSchedd & schedd,
	const classad::ClassAd & request_ad,
	bool (*process_func)(void*, ClassAd *ad),
	void * process_func_data,
	int connect_timeout,
	CondorError *errstack,
	ClassAd ** psummary_ad);

int actOnUsers (
	int cmd,
	DCSchedd & schedd,
	const char * usernames[],
	int num_usernames,
	const char * reason,
	CondorError *errstack,
	int connect_timeout = 20);

#endif

// Global variables
int dash_verbose = 0;
int dash_long = 0;
int dash_add = 0;

const char * my_name = nullptr;
static ClassAdFileParseType::ParseType dash_long_format = ClassAdFileParseType::Parse_auto;

// pass the exit code through dprintf_SetExitCode so that it knows
// whether to print out the on-error buffer or not.
#define exit(n) (exit)((dprintf_SetExitCode(n==1), n))

// protoypes of interest
void usage(FILE * out, const char* appname);

static const struct Translation MyCommandTranslation[] = {
	{ "add", ENABLE_USERREC },
	{ "delete", DELETE_USERREC },
	{ "disable", DISABLE_USERREC },
	{ "edit", EDIT_USERREC },
	{ "enable", ENABLE_USERREC },
	{ "reset", RESET_USERREC },
	{ "rm", DELETE_USERREC },
	{ "query", QUERY_USERREC_ADS },
	{ "", 0 }
};

#if 0
static const char * cmd_to_str(int cmd) { return getNameFromNum(cmd, MyCommandTranslation); }
#endif
static int str_to_cmd(const char * str) { return getNumFromName(str, MyCommandTranslation); }
static void print_results(ClassAd * ad, const char * op);

typedef std::map<std::string, MyRowOfValues> ROD_MAP_BY_KEY;
typedef std::map<std::string, ClassAd*> AD_MAP_BY_KEY;

const char * const userDefault_PrintFormat = "SELECT\n"
"   User         AS USER      WIDTH AUTO\n"
"   Owner        AS OWNER     WIDTH AUTO\n"
"   NTDomain     AS NTDOMAIN  WIDTH AUTO PRINTF %s OR ''\n"
"   {\"no\",\"yes\"}[Enabled?:0] AS ENABLED   WIDTH AUTO OR _\n"
//"   MaxJobs?:\"default\" AS MAX_JOBS WIDTH AUTO\n"
"   MaxJobsRunning?:\"default\" AS MAX_RUN WIDTH AUTO\n"
"   NumIdle    AS 'JOBS:Idle' WIDTH AUTO PRINTF %9d\n"
"   NumRunning AS Running WIDTH AUTO PRINTF %7d\n"
"   NumHeld    AS '   Held' WIDTH AUTO PRINTF %7d\n"
"   TotalRemovedJobs + NumRemoved AS Removed WIDTH AUTO PRINTF %7d\n"
"   TotalCompletedJobs + NumCompleted AS Completed WIDTH AUTO PRINTF %9d\n"
"SUMMARY STANDARD\n";

static  int  testing_width = 0;
int getDisplayWidth() {
	if (testing_width <= 0) {
		testing_width = getConsoleWindowSize();
		if (testing_width <= 0)
			testing_width = 80;
	}
	return testing_width;
}

static void initOutputMask(AttrListPrintMask & prmask, int qdo_mode, bool wide_mode)
{
	static const struct {
		const int mode;
		const char * tag;
		const char * fmt;
	} info[] = {
		{ 0,      "",         userDefault_PrintFormat },
	};

	int ixInfo = -1;
	for (int ix = 0; ix < (int)COUNTOF(info); ++ix) {
		if (info[ix].mode == qdo_mode) {
			ixInfo = ix;
			break;
		}
	}

	if (ixInfo < 0)
		return;

	const char * fmt = info[ixInfo].fmt;
	const char * tag = info[ixInfo].tag;

	if ( ! wide_mode) {
		int display_wid = getDisplayWidth();
		prmask.SetOverallWidth(display_wid-1);
	}

	PrintMaskMakeSettings propt;
	std::vector<GroupByKeyInfo> grkeys; // future
	std::string messages;

	StringLiteralInputStream stream(fmt);
	if (SetAttrListPrintMaskFromStream(stream, nullptr, prmask, propt, grkeys, nullptr, messages) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

int store_ads(void*pv, ClassAd* ad)
{
	AD_MAP_BY_KEY & ads = *(AD_MAP_BY_KEY*)pv;

	std::string key;
	if (!ad->LookupString(ATTR_USER, key)) {
		formatstr(key, "%06d", (int)ads.size()+1);
	}
	ads.emplace(key, ad);
	return 0; // return 0 means we are keeping the ad
}

// arguments passed to the process_ads_callback
struct _render_ads_info {
	ROD_MAP_BY_KEY * pmap=nullptr;
	unsigned int  ordinal=0;
	int           columns=0;
	FILE *        hfDiag=nullptr; // write raw ads to this file for diagnostic purposes
	unsigned int  diag_flags=0;
	AttrListPrintMask * pm = nullptr;
	_render_ads_info(ROD_MAP_BY_KEY* map, AttrListPrintMask * _pm) 
		: pmap(map)
		, pm(_pm)
	{}
};

int render_ads(void* pv, ClassAd* ad)
{
	int done_with_ad = 1; // return 1 means we allow the ad to be deleted or re-used
	struct _render_ads_info * pi = (struct _render_ads_info*)pv;
	ROD_MAP_BY_KEY * pmap = pi->pmap;

	int ord = pi->ordinal++;

	std::string key;
	if (!ad->LookupString(ATTR_USER, key)) {
		formatstr(key, "%06d", ord);
	}

	// if diagnose flag is passed, unpack the key and ad and print them to the diagnostics file
	if (pi->hfDiag) {
		if (pi->diag_flags & 1) {
			fprintf(pi->hfDiag, "#Key: %s\n", key.c_str());
		}

		if (pi->diag_flags & 2) {
			fPrintAd(pi->hfDiag, *ad);
			fputc('\n', pi->hfDiag);
		}

		if (pi->diag_flags & 4) {
			return true; // done processing this ad
		}
	}

	auto pp = pmap->emplace(key,MyRowOfValues());
	if (!pp.second) {
		fprintf( stderr, "Error: Two results with the same key.\n" );
		done_with_ad = 1;
	} else {
		MyRowOfValues & rov = pp.first->second;
		int cols = MAX(pi->columns, pi->pm->ColCount());
		rov.SetMaxCols(cols); // reserve space for column data
		pi->pm->render(rov, ad);
		done_with_ad = 1;
	}

	return done_with_ad;
}

/*********************************************************************
   main()
*********************************************************************/

int
main( int argc, const char *argv[] )
{
	const char * pcolon = nullptr;
	std::vector<const char*> usernames;
	std::vector<const char*> edit_args;
	const char* pool = nullptr;
	const char* name = nullptr;
	const char* disableReason = nullptr;
	int cmd = 0;
	int rval = 0;
	classad::References attrs;
	const char * constraint=nullptr;
	AttrListPrintMask prmask;

	my_name = argv[0];
	set_priv_initialize(); // allow uid switching if root
	config();
	dprintf_config_tool_on_error(0);
	dprintf_OnExitDumpOnErrorBuffer(stderr);

	for (int i=1; i < argc; ++i) {
		if (*argv[i] != '-') {
			// a bare argument is a username or edit unless we do not yet have a command
			// and it matches a command name
			if ( ! cmd) {
				int tmp = str_to_cmd(argv[i]);
				if (tmp >= QUERY_USERREC_ADS && tmp <= DELETE_USERREC) {
					cmd = tmp;
					continue;
				}
			}
			if (strchr(argv[i], '=')) {
				edit_args.push_back(argv[i]);
			} else {
				usernames.push_back(argv[i]);
			}
			continue;
		}

		if (is_dash_arg_prefix(argv[i], "help", 1)) {
			usage(stdout, my_name);
			exit(0);
		}
		else
		if (is_dash_arg_prefix(argv[i], "version", 1)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit( 0 );
		}
		else
		if (is_dash_arg_colon_prefix(argv[i], "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
			if (pcolon && pcolon[1]) {
				set_debug_flags( ++pcolon, 0 );
			}
		}
		else
		if (is_dash_arg_prefix(argv[i], "verbose", 4)) {
			dash_verbose = 1;
		} 
		else
		if (is_dash_arg_prefix(argv[i], "user", 1)) {
			if( i+1 >= argc ) {
				fprintf( stderr, "Error: -user requires a username argument\n"); 
				exit(1);
			}
			usernames.push_back(argv[++i]);
		}
		else
		if (is_dash_arg_prefix(argv[i], "add", 3) || is_dash_arg_prefix(argv[i], "enable", 2)) {
			if (cmd && cmd != ENABLE_USERREC) {
				usage(stderr, my_name);
				exit(1);
			}
			if (is_dash_arg_prefix(argv[i], "add", 3)) { dash_add = true; }
			cmd = ENABLE_USERREC;
		}
		else
		if (is_dash_arg_prefix(argv[i], "disable", 2)) {
			if (cmd && cmd != DISABLE_USERREC) {
				usage(stderr, my_name);
				exit(1);
			}
			cmd = DISABLE_USERREC;
		}
		else
		if (is_dash_arg_prefix(argv[i], "reason", 2)) {
			if( i+1 >= argc ) {
				fprintf( stderr, "Error: -reason requires a argument\n"); 
				exit(1);
			}
			disableReason = argv[++i];
		}
		else
		if (is_dash_arg_prefix(argv[i], "delete", 2)) {
			if (cmd && cmd != DELETE_USERREC) {
				usage(stderr, my_name);
				exit(1);
			}
			cmd = DELETE_USERREC;
		}
		else
		if (is_dash_arg_prefix(argv[i], "reset", 2)) {
			if (cmd && cmd != RESET_USERREC) {
				usage(stderr, my_name);
				exit(1);
			}
			cmd = RESET_USERREC;
		}
		else
		if (is_dash_arg_prefix(argv[i], "edit", 2)) {
			if (cmd && cmd != EDIT_USERREC) {
				usage(stderr, my_name);
				exit(1);
			}
			cmd = EDIT_USERREC;
		}
		else
		if (is_dash_arg_colon_prefix (argv[i], "long", &pcolon, 1)) {
			dash_long = 1;
			if (pcolon) {
				dash_long_format = parseAdsFileFormat(++pcolon, dash_long_format);
			}
			if ( ! prmask.IsEmpty()) {
				fprintf( stderr, "Error: -long cannot be used with -format or -af\n" );
				exit(1);
			}
		}
		else
		if (is_dash_arg_prefix (argv[i], "format", 1)) {
				// make sure we have at least two more arguments
			if( argc <= i+2 ) {
				fprintf( stderr, "Error: -format requires format and attribute parameters\n" );
				exit( 1 );
			}
			if (dash_long) {
				fprintf( stderr, "Error: -format and -long cannot be used together\n" );
				exit( 1 );
			}
			prmask.registerFormatF( argv[i+1], argv[i+2], FormatOptionNoTruncate );
			if ( ! IsValidClassAdExpression(argv[i+2], &attrs, NULL)) {
				fprintf(stderr, "Error: -format arg '%s' is not a valid expression\n", argv[i+2]);
				exit(2);
			}
			i+=2;
		}
		else
		if (is_dash_arg_colon_prefix(argv[i], "autoformat", &pcolon, 5) ||
			is_dash_arg_colon_prefix(argv[i], "af", &pcolon, 2)) {
			// make sure we have at least one more argument
			if ( (i+1 >= argc)  || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: -autoformat requires at least one attribute parameter\n" );
				exit(1);
			}
			if (dash_long) {
				fprintf( stderr, "Error: -af and -long cannot be used together\n" );
				exit(1);
			}
			int ixNext = parse_autoformat_args(argc, argv, i+1, pcolon, prmask, attrs, false);
			if (ixNext < 0) {
				fprintf(stderr, "Error: -autoformat arg '%s' is not a valid expression\n", argv[-ixNext]);
				exit(2);
			}
			if (ixNext > i) {
				i = ixNext-1;
			}
			// if autoformat list ends in a '-' without any characters after it, just eat the arg and keep going.
			if (i+1 < argc && '-' == (argv[i+1])[0] && 0 == (argv[i+1])[1]) {
				++i;
			}
		}
		else
		if (is_dash_arg_prefix(argv[i], "pool", 1)) {
			if( i+1 >= argc ) {
				fprintf( stderr, "Error: -pool requires a hostname argument\n"); 
				exit(1);
			}
			pool = argv[++i];
		}
		else
		if (is_dash_arg_prefix (argv[i], "name", 1)) {
			if( i+1 >= argc ) {
				fprintf( stderr, "ERROR: -name requires a schedd-name argument\n"); 
				exit(1);
			}
			name = argv[++i];
		}
		else
		if (is_dash_arg_prefix (argv[i], "constraint", 1)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: -constraint requires and expresion argument\n");
				exit(1);
			}
			constraint = argv[++i];
		}
		else {
			fprintf(stderr,"Error: unexpected argument: %s\n", argv[i]);
			usage(stderr, my_name);
			exit(2);
		}
	}

	if (!cmd) cmd = QUERY_USERREC_ADS;

	if ( ! edit_args.empty() && cmd != EDIT_USERREC) {
		fprintf(stderr, "<attr>=<expr> arguments only work with -edit");
		usage(stderr, my_name);
		exit(2);
	}

	if ((cmd == QUERY_USERREC_ADS) && ! dash_long && prmask.IsEmpty()) {
		initOutputMask(prmask, 0, false);
	}

	DCSchedd schedd(name, pool);
	if ( ! schedd.locate()) {
		const char * msg = schedd.error();
		fprintf(stderr, "Error: %d %s\n", schedd.errorCode(), msg?msg:"");
		exit(1);
	}

	if (schedd.version()) {
		const char * fullname = schedd.fullHostname();
		const char * scheddName = fullname ? fullname : (name?name:"LOCAL");
		const char * ver = schedd.version();
		CondorVersionInfo v(ver);
		if ( ! v.built_since_version(10,6,0)) {
			fprintf(stderr, "Schedd %s version %d.%d too old for this query\n", scheddName, v.getMajorVer(), v.getMinorVer());
			exit(1);
		}
		if (dash_verbose) {
			fprintf(stdout, "Schedd %s %s\n", scheddName, ver?ver:"unknown version");
		}
	}

	// -reason requires -disable
	if (disableReason && cmd != DISABLE_USERREC) {
		fprintf(stdout, "-reason argument can only be used with -disable argument\n");
		exit(1);
	}

	if (cmd == QUERY_USERREC_ADS) {

		classad::ClassAd req_ad;
	#ifdef USE_DC_SCHEDD_USERS_METHODS
		// make sure we ask for the key attribute when we query
		if ( ! attrs.empty()) { attrs.insert(ATTR_USER); }

		int rval = schedd.makeUsersQueryAd(req_ad, constraint, attrs);
		if (rval == Q_PARSE_ERROR) {
			fprintf(stderr, "Error: invalid constraint expression: %s\n", constraint);
			exit(1);
		} else if (rval != Q_OK) {
			fprintf(stderr, "Error: %d while making query ad\n", rval);
			exit(1);
		}
	#else
		if (makeUsersRequestAd(req_ad, constraint, attrs)) {
			fprintf(stderr, "Error: invalid constraint expression\n");
			exit(1);
		}

		if ( ! attrs.empty()) {
			// make sure we ask for the key attribute when we query
			attrs.insert(ATTR_USER);
		}
	#endif

		ROD_MAP_BY_KEY rods;
		struct _render_ads_info render_info(&rods, &prmask);
		AD_MAP_BY_KEY ads;
		ClassAd *summary_ad=nullptr;
		CondorError errstack;

		int (*process_ads)(void*, ClassAd *ad) = nullptr;
		void* process_ads_data = nullptr;
		if (dash_long) {
			process_ads = store_ads;
			process_ads_data = &ads;
		} else {
			process_ads = render_ads;
			process_ads_data = &render_info;
		}

		int connect_timeout = param_integer("Q_QUERY_TIMEOUT");
		rval = schedd.queryUsers(req_ad, process_ads, process_ads_data, connect_timeout, &errstack, &summary_ad);
		if (rval != Q_OK) {
			fprintf(stderr, "Error: query failed - %s\n", errstack.getFullText().c_str());
			exit(1);
		} else if (dash_long) {
			CondorClassAdListWriter writer(dash_long_format);
			for (const auto &[key, ad]: ads) { writer.writeAd(*ad, stdout); }
			if (writer.needsFooter()) { writer.writeFooter(stdout); }
		} else {
			std::string line; line.reserve(1024);
			// render once to set column widths
			for (auto it : rods) { prmask.display(line, it.second); line.clear(); }
			if (prmask.has_headings()) prmask.display_Headings(stdout);
			for (auto it : rods) {
				prmask.display(line, it.second);
				fputs(line.c_str(), stdout);
				line.clear();
			}
		}

	} else if (cmd == ENABLE_USERREC) {
		CondorError errstack;
	#if 1
		const char * opname = dash_add ? "add" : "enable";
		const bool create_if = dash_add;
		ClassAd * ad = schedd.enableUsers(&usernames[0], (int)usernames.size(), create_if, &errstack);
		if ( ! ad) {
			fprintf(stderr, "Error: %s failed - %s\n", opname, errstack.getFullText().c_str());
			rval = 1;
		} else {
			rval = 0;
			print_results(ad, opname);
			delete ad;
		}
	#else
		rval = actOnUsers (cmd, schedd, &usernames[0], (int)usernames.size(), nullptr, &errstack);
		if (rval != 0) {
			fprintf(stderr, "Error: %s failed - %s\n", cmd_to_str(cmd), errstack.getFullText().c_str());
		}
	#endif
	} else if (cmd == DISABLE_USERREC) {
		CondorError errstack;
	#if 1
		const char * opname = "disable";
		ClassAd * ad = schedd.disableUsers(&usernames[0], (int)usernames.size(), disableReason, &errstack);
		if ( ! ad) {
			fprintf(stderr, "Error: %s failed - %s\n", opname, errstack.getFullText().c_str());
			rval = 1;
		} else {
			rval = 0;
			print_results(ad, opname);
			delete ad;
		}
	#else
		rval = actOnUsers (cmd, schedd, &usernames[0], (int)usernames.size(), disableReason, &errstack);
		if (rval != 0) {
			fprintf(stderr, "Error: %s failed - %s\n", cmd_to_str(cmd), errstack.getFullText().c_str());
		}
	#endif
	} else if (cmd == EDIT_USERREC) {
		ClassAd * ad = new ClassAd();
		for (auto & line : edit_args) {
			if ( ! ad->Insert(line)) {
				fprintf(stderr, "Error: not a valid classad assigment: %s\n", line);
				rval = 1;
				ad->Clear();
				break;
			}
		}
		if (ad && ad->size() > 0) {
			ClassAdList adlist;
			rval = 1; // assume failure

			if (constraint) {
				if (ad->AssignExpr(ATTR_REQUIREMENTS, constraint)) {
					adlist.Insert(ad);
					rval = 0;
				} else {
					fprintf(stderr, "Error: invalid constraint : %s\n", constraint);
				}
			} else if (usernames.size()) {
				// we need a separate ad for each user, each should contain all of the edit_args
				for (auto & name : usernames) {
					// make a copy of the edit_args attributes for each user beyond the first
					if (adlist.Length() > 0) { ad = new ClassAd(*ad); }
					ad->Assign(ATTR_USER, name);
					adlist.Insert(ad);
					rval = 0;
				}
			} else {
				fprintf(stderr, "Error: no username for constraint - don't know which user(s) to edit\n");
				rval = 1;
			}

			// if we got to here with a valid adlist, send it on to the schedd
			if (rval == 0) {
				CondorError errstack;
				ClassAd * resultAd = schedd.updateUserAds(adlist, &errstack);
				if ( ! resultAd) {
					fprintf(stderr, "Error: edit failed - %s\n", errstack.getFullText().c_str());
					rval = 1;
				} else {
					rval = 0;
					print_results(resultAd, "edit");
					delete resultAd;
				}
			}
		}
	} else {
		fprintf(stderr, "Unsupported command %d\n", cmd);
		rval = 1;
	}

	dprintf_SetExitCode(rval);
	return rval;
}

#ifdef USE_DC_SCHEDD_USERS_METHODS
static void print_results(ClassAd * ad, const char * op)
{
	std::string buf; buf.reserve(200);
	fprintf(stdout, "%s succeeded:\n%s\n", op, formatAd(buf, *ad, "    ", nullptr, false));
}
#else

int makeUsersRequestAd(
	classad::ClassAd & request_ad,
	const char * constraint,
	classad::References &attrs,
	int /* fetch_opts = 0*/,
	int match_limit /*=-1*/)
{
	if (constraint && constraint[0]) {
		classad::ClassAdParser parser;
		classad::ExprTree *expr = NULL;
		parser.ParseExpression(constraint, expr);
		if (!expr) return Q_INVALID_REQUIREMENTS;

		request_ad.Insert(ATTR_REQUIREMENTS, expr);
	}

	if ( ! attrs.empty()) {
		std::string projection;
		for (auto attr : attrs) {
			if (projection.empty()) projection += "\n";
			projection += attr;
		}
		request_ad.InsertAttr(ATTR_PROJECTION, projection);
		if (attrs.count(ATTR_SERVER_TIME)) {
			request_ad.Assign(ATTR_SEND_SERVER_TIME, true);
		}
	}

	if (match_limit >= 0) {
		request_ad.InsertAttr(ATTR_LIMIT_RESULTS, match_limit);
	}
	return 0;
}

int fetchUsersFromSchedd (
	DCSchedd & schedd,
	const classad::ClassAd & request_ad,
	bool (*process_func)(void*, ClassAd *ad),
	void * process_func_data,
	int connect_timeout,
	CondorError *errstack,
	ClassAd ** psummary_ad)
{
	ClassAd *ad = NULL;	// job ad result

	int cmd = QUERY_USERREC_ADS;

	Sock* sock;
	if (!(sock = schedd.startCommand(cmd, Stream::reli_sock, connect_timeout, errstack))) return Q_SCHEDD_COMMUNICATION_ERROR;

	classad_shared_ptr<Sock> sock_sentry(sock);

	if (!putClassAd(sock, request_ad) || !sock->end_of_message()) return Q_SCHEDD_COMMUNICATION_ERROR;
	dprintf(D_FULLDEBUG, "Sent Users request classad to schedd\n");

	int rval = 0;
	do {
		ad = new ClassAd();
		if ( ! getClassAd(sock, *ad)) {
			rval = Q_SCHEDD_COMMUNICATION_ERROR;
			break;
		}
		dprintf(D_FULLDEBUG, "Got classad from schedd.\n");
		std::string mytype;
		if (ad->EvaluateAttrString(ATTR_MY_TYPE, mytype) && mytype == "Summary")
		{ // Last ad.
			dprintf(D_FULLDEBUG, "Ad was last one from schedd.\n");
			std::string errorMsg;
			int error_val;
			if (ad->EvaluateAttrInt(ATTR_ERROR_CODE, error_val) && error_val && ad->EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
			{
				if (errstack) errstack->push("TOOL", error_val, errorMsg.c_str());
				rval = Q_REMOTE_ERROR;
			} else if ( ! sock->end_of_message()) {
				rval = Q_SCHEDD_COMMUNICATION_ERROR;
			}
			sock->close();

			if (psummary_ad && rval == 0) {
				*psummary_ad = ad; // return the final ad, because it has summary information
				ad = NULL; // so we don't delete it below.
			}
			break;
		}
		// Note: process_func() will return false if taking
		// ownership of ad, so only delete if it returns true, else set to NULL
		// so we don't delete it here.  Either way, next set ad to NULL since either
		// it has been deleted or will be deleted later by process_func().
		if (process_func(process_func_data, ad)) {
			delete ad;
		}
		ad = NULL;
	} while (true);

	// Make sure ad is not leaked no matter how we break out of the above loop.
	delete ad;

	return rval;
}

int actOnUsers (
	int cmd,
	DCSchedd & schedd,
	const char * usernames[],
	int num_usernames,
	const char * reason,
	CondorError *errstack,
	int connect_timeout /*=20*/)
{
	ClassAd cmd_ad;

	Sock* sock;
	if (!(sock = schedd.startCommand(cmd, Stream::reli_sock, connect_timeout, errstack))) return Q_SCHEDD_COMMUNICATION_ERROR;

	int rval = 0;
	classad_shared_ptr<Sock> sock_sentry(sock);

	// send the number of ads
	sock->put(num_usernames);

	// send the ads
	for (int  ii = 0; ii < num_usernames; ++ii) {
		cmd_ad.Assign(ATTR_USER, usernames[ii]);
		if (dash_add) {
			cmd_ad.Assign(ATTR_USERREC_OPT_CREATE, true);
			cmd_ad.Assign(ATTR_USERREC_OPT_CREATE_DEPRECATED, true); // TODO: remove in late 23.x ?
		}
		if (reason) {
			if (cmd == DISABLE_USERREC) cmd_ad.Assign(ATTR_DISABLE_REASON, reason);
		}
		if (!putClassAd(sock, cmd_ad)) return Q_SCHEDD_COMMUNICATION_ERROR;
		dprintf(D_FULLDEBUG, "Sent %s User %s to schedd\n", cmd_to_str(cmd), usernames[ii]);
	}

	if ( ! sock->end_of_message()) {
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	ClassAd result_ad;
	if ( ! getClassAd(sock, result_ad) || ! sock->end_of_message()) {
		rval = Q_SCHEDD_COMMUNICATION_ERROR;
		if (errstack) errstack->push("TOOL", rval, "no result ad");
	} else {
		std::string errorMsg;
		int error_val;
		if (result_ad.EvaluateAttrInt(ATTR_RESULT, error_val) && error_val && result_ad.EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
		{
			if (errstack) errstack->push("TOOL", error_val, errorMsg.c_str());
			rval = Q_REMOTE_ERROR;
		}
	}

	return rval;
}

#endif

void
usage(FILE *out, const char *appname)
{
	if( ! appname ) {
		fprintf( stderr, "Use -help to see usage information\n" );
		exit(1);
	}
	fprintf(out, "Usage: %s [ADDRESS] [DISPLAY] [USERS]\n", appname );
	fprintf(out, "       %s [ADDRESS] [-add | -enable] [USERS]\n", appname );
	fprintf(out, "       %s [ADDRESS] -disable [USERS] [-reason <reason-string>]\n", appname );
	fprintf(out, "       %s [ADDRESS] [USERS] -edit <attr>=<value> [<attr>=<value> ...]\n", appname );

	fprintf(out, "\n  ADDRESS is:\n"
		"    -name <name>\t Name or address of Scheduler\n"
		"    -pool <host>\t Use host as the central manager to query\n"
		);

	fprintf(out, "\n  DISPLAY is:\n" );
	fprintf(out, "    -help\t\t Display this message to stdout and exit.\n" );
	fprintf(out, "    -version\t\t Display the HTCondor version and exit.\n" );
	fprintf(out, "    -debug[:<level>]\t Write a debug log to stderr. <level> overrides TOOL_DEBUG.\n");
	//fprintf(out, "    -verbose\t\t Narrate the process\n" );
	fprintf(out, "    -long[:format]\t Display full classads. format can long,json,xml,new or auto\n" );
	fprintf(out, "    -af[:jlhVr,tng] <attr> [attr2 [...]]\n"
		"        Print attr(s) with automatic formatting\n"
		"        the [jlhVr,tng] options modify the formatting\n"
		"            l   attribute labels\n"
		"            h   attribute column headings\n"
		"            V   %%V formatting (string values are quoted)\n"
		"            r   %%r formatting (raw/unparsed values)\n"
		"            ,   comma after each value\n"
		"            t   tab before each value (default is space)\n"
		"            n   newline after each value\n"
		"            g   newline between ClassAds, no space before values\n"
		"        use -af:h to get tabular values with headings\n"
		"        use -af:lrng to get -long equivalent format\n"
		"    -format <fmt> <attr> Print attribute attr using format fmt\n"
		);

	fprintf(out, "\n  USERS is zero or more of:\n"
		"    <user>\t\t Operate on <user>\n"
		"    -user <user>\t Operate on <user>\n"
//		"    -me\t\t\t Operate on the user running the command\n"
		"    -constraint <expr>\t Operate on users matching the <expr>. Cannot be used with -add.\n"
		);

	fprintf(out, "\n  At most one of the following operation args may be used:\n");
	fprintf(out, "    -add\t\t Add new, enabled user records\n" );
	fprintf(out, "    -enable\t\t Enable existing user records, Add new records as needed\n" );
	fprintf(out, "    -disable\t\t Disable existing user records, user cannot submit jobs\n" );
//	fprintf(out, "    -delete\t\t Delete user records\n" );
//	fprintf(out, "    -reset\t\t Reset user records to default settings and limits\n" );
	fprintf(out, "    -edit\t\t Edit fields of user records\n" );

	fprintf(out, "\n  Other arguments:\n");
	fprintf(out, "    -reason <string>\t Reason for disabling the user. Use with -disable\n" );
	fprintf(out, "    <attr>=<expr>\t Store <attr>=<expr> in the user record. Use with -edit\n" );

	fprintf(out, "\n"
		"  This tool is use to query, create and modify User/Owner records in the Schedd.\n"
		"  The default operation is to query and display users.\n" );
}
