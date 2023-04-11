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
#include "string_list.h"
#include "ad_printmask.h"
#include "internet.h"
#include "compat_classad_util.h"
#include "daemon.h"
#include "dc_schedd.h"
//#include "dc_collector.h"
#include "basename.h"
#include "match_prefix.h"

// For schedd commands
#include "condor_commands.h"
#include "CondorError.h"
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
	CondorError *errstack,
	int connect_timeout = 20);

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

static const char * cmd_to_str(int cmd) { return getNameFromNum(cmd, MyCommandTranslation); }
static int str_to_cmd(const char * str) { return getNumFromName(str, MyCommandTranslation); }

typedef std::map<std::string, MyRowOfValues> ROD_MAP_BY_KEY;
typedef std::map<std::string, ClassAd*> AD_MAP_BY_KEY;

bool store_ads(void*pv, ClassAd* ad)
{
	AD_MAP_BY_KEY & ads = *(AD_MAP_BY_KEY*)pv;

	std::string key;
	if (!ad->LookupString(ATTR_USER, key)) {
		formatstr(key, "%06d", (int)ads.size()+1);
	}
	ads.emplace(key, ad);
	return false; // we are keeping the ad
}

// arguments passed to the process_ads_callback
struct _render_ads_info {
	ROD_MAP_BY_KEY * pmap=nullptr;
	unsigned int  ordinal=0;
	int           columns=0;
	FILE *        hfDiag=nullptr; // write raw ads to this file for diagnostic purposes
	unsigned int  diag_flags=0;
	AttrListPrintMask * pm = nullptr;
	_render_ads_info(ROD_MAP_BY_KEY* map, AttrListPrintMask * _pm) : pmap(map), pm(_pm) {}
};

bool render_ads(void* pv, ClassAd* ad)
{
	bool done_with_ad = true;
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
		done_with_ad = true;
	} else {
		MyRowOfValues & rov = pp.first->second;
		// set this to truncate output to window width
		//rov.SetMaxCols(pi->columns);
		pi->pm->render(rov, ad);
		done_with_ad = true;
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
	int cmd = 0;
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
			if (cmd == EDIT_USERREC) {
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
			cmd = RESET_USERREC;
		}
		else
		if (is_dash_arg_colon_prefix (argv[i], "long", &pcolon, 1)) {
			dash_long = 1;
			if (pcolon) {
				dash_long_format = parseAdsFileFormat(++pcolon, dash_long_format);
			}
		}
		else
		if (is_dash_arg_prefix (argv[i], "format", 1)) {
				// make sure we have at least two more arguments
			if( argc <= i+2 ) {
				fprintf( stderr, "Error: -format requires format and attribute parameters\n" );
				exit( 1 );
			}
			prmask.registerFormatF( argv[i+1], argv[i+2], FormatOptionNoTruncate );
			if ( ! IsValidClassAdExpression(argv[i+2], &attrs, NULL)) {
				return i+2;
			}
			i+=2;
		}
		else
		if (is_dash_arg_colon_prefix(argv[i], "autoformat", &pcolon, 5) ||
			is_dash_arg_colon_prefix(argv[i], "af", &pcolon, 2)) {
			// make sure we have at least one more argument
			if ( (i+1 >= argc)  || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: -autoformat requires at last one attribute parameter\n" );
				exit(1);
			}
			int ixNext = parse_autoformat_args(argc, argv, i+1, pcolon, prmask, attrs, false);
			if (ixNext < 0) {
				return -ixNext;
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
		if ( ! v.built_since_version(10,4,0)) {
			fprintf(stderr, "Schedd %s version %d.%d too old for this query\n", scheddName, v.getMajorVer(), v.getMinorVer());
			exit(1);
		}
		if (dash_verbose) {
			fprintf(stdout, "Schedd %s %s\n", scheddName, ver?ver:"unknown version");
		}
	}

	if (cmd == QUERY_USERREC_ADS) {

		classad::ClassAd req_ad;
		if (makeUsersRequestAd(req_ad, constraint, attrs)) {
			fprintf(stderr, "Error: invalid constraint expression\n");
			exit(1);
		}

		StringList white;
		StringList * whitelist = nullptr;
		if ( ! attrs.empty()) {
			for (auto attr : attrs) { white.insert(attr.c_str()); }
			whitelist = &white;
			// make sure we ask for the key attribute when we query
			attrs.insert(ATTR_USER);
		}

		ROD_MAP_BY_KEY rods;
		struct _render_ads_info render_info(&rods, &prmask);
		AD_MAP_BY_KEY ads;
		ClassAd *summary_ad=nullptr;
		CondorError errstack;

		bool (*process_ads)(void*, ClassAd *ad) = nullptr;
		void* process_ads_data = nullptr;
		if (dash_long) {
			process_ads = store_ads;
			process_ads_data = &ads;
		} else {
			process_ads = render_ads;
			process_ads_data = &render_info;
		}

		int connect_timeout = param_integer("Q_QUERY_TIMEOUT");
		if (fetchUsersFromSchedd (schedd, req_ad, process_ads, process_ads_data, connect_timeout, &errstack, &summary_ad)) {
			fprintf(stderr, "Error: query failed - %s\n", errstack.getFullText().c_str());
			exit(1);
		} else if (dash_long) {
		#if 0
			std::string buf; buf.reserve(1024);
			for (auto it : ads) {
				classad::References ad_attrs;
				sGetAdAttrs(ad_attrs, *it.second, false, whitelist);
				sPrintAdAttrs(buf, *it.second, ad_attrs);
				buf += "\n";
				fputs(buf.c_str(), stdout);
				buf.clear();
			}
		#else
			CondorClassAdListWriter writer(dash_long_format);
			for (const auto it : ads) { writer.writeAd(*it.second, stdout); }
			if (writer.needsFooter()) { writer.writeFooter(stdout); }
		#endif
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
		if (actOnUsers (cmd, schedd, &usernames[0], (int)usernames.size(), &errstack)) {
			fprintf(stderr, "Error: %s failed - %s\n", cmd_to_str(cmd), errstack.getFullText().c_str());
			exit(1);
		}
	} else {
		fprintf(stderr, "Unsupported command %d\n", cmd);
	}

	dprintf_SetExitCode(0);
	return 0;
}

int makeUsersRequestAd(
	classad::ClassAd & request_ad,
	const char * constraint,
	classad::References &attrs,
	int fetch_opts /*= 0*/,
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
		if (dash_add) { cmd_ad.Assign("Create", true); }

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



void
usage(FILE *out, const char *appname)
{
	if( ! appname ) {
		fprintf( stderr, "Use -help to see usage information\n" );
		exit(1);
	}
	fprintf(out, "Usage: %s [OPTIONS] <username>\n", appname );
	fprintf(out, "\nOPTIONS:\n" );
	fprintf(out, "-cancel           Stop draining.\n" );
	fprintf(out, "-debug            Display debugging info to console\n" );
	fprintf(out, "-graceful         (the default) Honor MaxVacateTime and MaxJobRetirementTime.\n" );
	fprintf(out, "-quick            Honor MaxVacateTime but not MaxJobRetirementTime.\n" );
	fprintf(out, "-fast             Honor neither MaxVacateTime nor MaxJobRetirementTime.\n" );
	fprintf(out, "-reason <text>    While draining, advertise <text> as the reason.\n" );
	fprintf(out, "-resume-on-completion    When done draining, resume normal operation.\n" );
	fprintf(out, "-exit-on-completion      When done draining, STARTD should exit and not restart.\n" );
	fprintf(out, "-restart-on-completion   When done draining, STARTD should restart.\n" );
	fprintf(out, "-request-id <id>  Specific request id to cancel (optional).\n" );
	fprintf(out, "-check <expr>     Must be true for all slots to be drained or request is aborted.\n" );
	fprintf(out, "-start <expr>     Change START expression to this while draining.\n" );
}
