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
#include "condor_classad.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_secman.h"
#include "command_strings.h"
#include "daemon.h"
#include "safe_sock.h"
#include "condor_distribution.h"
#include "daemon_list.h"
#include "dc_collector.h"
#include "my_hostname.h"
#include "match_prefix.h"
#include "ad_printmask.h"

 // for -af 
static struct AppType {
	const char * Name{nullptr}; // tool name as invoked from argv[0]
	bool diagnostic{false};
	bool long_format{false};
	bool printed_headings{false};
	AttrListPrintMask prmask;   // -af and -format printing 
	classad::References prattrs;  // Attributes that we want to print for -long
} App;

// moved here from condor_perms.cpp for the purpose of comparing the new
// implementation to the previous one.
class DCpermissionHierarchyOld {

private:
	DCpermission m_base_perm; // the specified permission level

	// [0] is base perm, [1] is implied by [0], ...
	// Terminated by an entry with value LAST_PERM.
	DCpermission m_implied_perms[LAST_PERM+1];

	// List of perms that imply base perm, not including base perm,
	// and not including things that indirectly imply base perm, such
	// as the things that imply the members of this list.
	// Example: for base perm WRITE, this list includes DAEMON and
	// ADMINISTRATOR.
	// Terminated by an entry with value LAST_PERM.
	DCpermission m_directly_implied_by_perms[LAST_PERM+1];

	// [0] is base perm, [1] is perm to param for if [0] is undefined, ...
	// The list ends with DEFAULT_PERM, followed by LAST_PERM.
	DCpermission m_config_perms[LAST_PERM+1];

public:

	// [0] is base perm, [1] is implied by [0], ...
	// Terminated by an entry with value LAST_PERM.
	DCpermission const * getImpliedPerms() const { return m_implied_perms; }

	// List of perms that imply base perm, not including base perm,
	// and not including things that indirectly imply base perm, such
	// as the things that imply the members of this list.
	// Example: for base perm WRITE, this list includes DAEMON and
	// ADMINISTRATOR.
	// Terminated by an entry with value LAST_PERM.
	DCpermission const * getPermsIAmDirectlyImpliedBy() const { return m_directly_implied_by_perms; }

	// [0] is base perm, [1] is perm to param for if [0] is undefined, ...
	// The list ends with DEFAULT_PERM, followed by LAST_PERM.
	DCpermission const * getConfigPerms() const { return m_config_perms; }

	DCpermissionHierarchyOld(DCpermission perm) {
		m_base_perm = perm;
		unsigned int i = 0;

		m_implied_perms[i++] = m_base_perm;

		// Add auth levels implied by specified perm
		bool done = false;
		while(!done) {
			switch( m_implied_perms[i-1] ) {
			case DAEMON:
			case ADMINISTRATOR:
				m_implied_perms[i++] = WRITE;
				break;
			case WRITE:
			case NEGOTIATOR:
			case CONFIG_PERM:
			case ADVERTISE_STARTD_PERM:
			case ADVERTISE_SCHEDD_PERM:
			case ADVERTISE_MASTER_PERM:
				m_implied_perms[i++] = READ;
				break;
			default:
				// end of hierarchy
				done = true;
				break;
			}
		}
		m_implied_perms[i] = LAST_PERM;

		i=0;
		switch(m_base_perm) {
		case READ:
			m_directly_implied_by_perms[i++] = WRITE;
			m_directly_implied_by_perms[i++] = NEGOTIATOR;
			m_directly_implied_by_perms[i++] = CONFIG_PERM;
			m_directly_implied_by_perms[i++] = ADVERTISE_STARTD_PERM;
			m_directly_implied_by_perms[i++] = ADVERTISE_SCHEDD_PERM;
			m_directly_implied_by_perms[i++] = ADVERTISE_MASTER_PERM;
			break;
		case WRITE:
			m_directly_implied_by_perms[i++] = ADMINISTRATOR;
			m_directly_implied_by_perms[i++] = DAEMON;
			break;
		default:
			break;
		}
		m_directly_implied_by_perms[i] = LAST_PERM;

		i=0;
		m_config_perms[i++] = m_base_perm;
		done = false;
		while( !done ) {
			switch(m_config_perms[i-1]) {
			case DAEMON:
				if (param_boolean("LEGACY_ALLOW_SEMANTICS", false)) {
					m_config_perms[i++] = WRITE;
				} else {
					done = true;
				}
				break;
			case ADVERTISE_STARTD_PERM:
			case ADVERTISE_SCHEDD_PERM:
			case ADVERTISE_MASTER_PERM:
				m_config_perms[i++] = DAEMON;
				break;
			default:
				// end of config hierarchy
				done = true;
				break;
			}
		}
		m_config_perms[i++] = DEFAULT_PERM;
		m_config_perms[i] = LAST_PERM;
	}

};


static const char * PermSuffix(DCpermission perm) {
	if (perm == CONFIG_PERM || perm >= SOAP_PERM) return "_PERM";
	return "";
}

void
print_permission_heirarchy()
{
	bool legacy_allow = param_boolean("LEGACY_ALLOW_SEMANTICS", false);

	printf("=== Permission Heirarchy ===\n");
	printf("%sConfig perms:\n", legacy_allow ? "Legacy " : "");
	for (auto perm = FIRST_PERM; perm != LAST_PERM; perm = NEXT_PERM(perm)) {
		DCpermissionHierarchyOld heir(perm);
		auto lst = heir.getConfigPerms();
		printf("\t/* %20s */ {", PermString(perm));
		const char * sep = "";
		while (*lst != LAST_PERM) {
			printf("%s%s%s", sep, PermString(*lst), PermSuffix(*lst));
			sep = ", ";
			++lst;
		}
		printf("},\n");
	}

	printf("%s nextConfig perms:\n", legacy_allow ? "Legacy " : "");
	for (auto perm = FIRST_PERM; perm != LAST_PERM; perm = NEXT_PERM(perm)) {
		printf("\t/* %20s */ {", PermString(perm));
		const char * sep = "";
		const char * suffix = " /**/";
		auto p2 = perm;
		while (p2 != LAST_PERM) {
			printf("%s%s%s%s", sep, PermString(p2), PermSuffix(p2), suffix);
			sep = ", "; suffix = "";
			p2 = DCpermissionHierarchy::nextConfig(p2, legacy_allow);
		}
		printf("},\n");
	}

	///
	///
	printf("\nImplied perms:\n");
	for (auto perm = FIRST_PERM; perm != LAST_PERM; perm = NEXT_PERM(perm)) {
		DCpermissionHierarchyOld heir(perm);
		auto lst = heir.getImpliedPerms();
		printf("\t/* %20s */ {", PermString(perm));
		const char * sep = "";
		while (*lst != LAST_PERM) {
			printf("%s%s%s", sep, PermString(*lst), PermSuffix(*lst));
			sep = ", ";
			++lst;
		}
		printf("},\n");
	}

	printf("\nnextImplied:\n");
	for (auto perm = FIRST_PERM; perm != LAST_PERM; perm = NEXT_PERM(perm)) {
		printf("\t/* %20s */ {", PermString(perm));
		const char * sep = "";
		const char * suffix = " /**/";
		auto p2 = perm;
		while (p2 != LAST_PERM) {
			printf("%s%s%s%s", sep, PermString(p2), PermSuffix(p2), suffix);
			sep = ", "; suffix = "";
			p2 = DCpermissionHierarchy::nextImplied(p2);
		}
		printf("},\n");
	}

	///
	///
	printf("\nImplied by:\n");
	for (auto perm = FIRST_PERM; perm != LAST_PERM; perm = NEXT_PERM(perm)) {
		DCpermissionHierarchyOld heir(perm);
		auto lst = heir.getPermsIAmDirectlyImpliedBy();
		printf("\t/* %20s */ {", PermString(perm));
		const char * sep = "";
		while (*lst != LAST_PERM) {
			printf("%s%s%s", sep, PermString(*lst), PermSuffix(*lst));
			sep = ", ";
			++lst;
		}
		printf("},\n");
	}

	printf("\nnextImpliedBy:\n");
	for (auto perm = FIRST_PERM; perm != LAST_PERM; perm = NEXT_PERM(perm)) {
		printf("\t/* %20s */ {", PermString(perm));
		const char * sep = "";
		for (auto p2 : DCpermissionHierarchy::DirectlyImpliedBy(perm)) {
			printf("%s%s%s", sep, PermString(p2), PermSuffix(p2));
			sep = ", ";
		}
		printf("},\n");
	}
}


enum { usage_Error=-1, usage_Help=0, usage_AuthLevels=1, usage_Config=2, usage_PermHeir=0x100, usage_HelpAll=0xFF };
void print_security_config(bool include_soap, const char * subsys);

void
usage( const char *cmd, int other=usage_Error, const char * subsystem=nullptr )
{
	bool include_soap=false;

	if (other & usage_PermHeir) {
		print_permission_heirarchy();
		if (other == usage_PermHeir) return;
	}

	FILE* out = (other == usage_Error) ? stderr : stdout;

	fprintf(out,"Usage: %s [options] TOKEN [TOKEN [TOKEN [...]]]\n",cmd);
	fprintf(out,"\nwhere TOKEN is ( ALL | <authorization-level> | <command-name> | <command-int> )\n\n");
	fprintf(out,"general options:\n");
	fprintf(out,"    -config <filename>              Add configuration from specified file\n");
	fprintf(out,"    -debug                          Show extra debugging info\n");
	//fprintf(out,"    -help                           Display this output\n");
	fprintf(out,"    -help [config|levels|all]       Display this output [and details]\n");
	fprintf(out,"    -version                        Display Condor version\n");
	fprintf(out,"\nspecifying target options:\n");
	fprintf(out,"    -address <sinful>               Use this sinful string\n");
	fprintf(out,"    -pool    <host>                 Query this collector\n");
	fprintf(out,"    -name    <name>                 Find a daemon with this name\n");
	fprintf(out,"    -subsystem <subsystem>          Type of daemon to contact (default: SCHEDD)\n");
	fprintf(out,"    -type    <subsystem>            Type of daemon to contact (default: SCHEDD)\n");
	fprintf(out,"\noutput options (specify only one):\n");
	fprintf(out,"    -quiet                          No output, only sets status\n");
	fprintf(out,"    -table                          Print one result per line\n");
	fprintf(out,"    -verbose                        Print all available information\n");
	fprintf(out,"    -long                           Print result classad\n");
	fprintf(out,"    -format <fmt> <attr>\tDisplay <attr> values with formatting\n");
	fprintf(out,"    -autoformat[:lhVr,tng] <attr> [<attr2> [...]]\n");
	fprintf(out,"    -af[:lhVr,tng] <attr> [attr2 [...]]\n");
	fprintf(out,"        Print attr(s) with automatic formatting\n");
	fprintf(out,"        the [lhVr,tng] options modify the formatting\n");
	fprintf(out,"            l   attribute labels\n");
	fprintf(out,"            h   attribute column headings\n");
	fprintf(out,"            V   %%V formatting (string values are quoted)\n");
	fprintf(out,"            r   %%r formatting (raw/unparsed values)\n");
	fprintf(out,"            ,   comma after each value\n");
	fprintf(out,"            t   tab before each value (default is space)\n");
	fprintf(out,"            n   newline after each value\n");
	fprintf(out,"            g   newline between ClassAds, no space before values\n");
	fprintf(out,"        use -af:h to get tabular values with headings\n");
	fprintf(out,"        use -af:lrng to get -long equivalent format\n");
	fprintf(out,"\nExample:\n    %s -addr \"<127.0.0.1:9618>\" -table READ WRITE DAEMON\n\n",cmd);

	if (other & usage_AuthLevels) {
		fprintf(out, "\nWhere <authorization-level> is one of:\n");
		for (DCpermission perm = FIRST_PERM; perm < LAST_PERM; perm = NEXT_PERM(perm)) {
			if (perm == SOAP_PERM && !include_soap) continue;
			fprintf(out, "    %-20s %s\n", PermString(perm), PermDescription(perm));
		}
	}

	if (other & usage_Config) {
		fprintf(out, "\nEffective security config:\n");
		print_security_config(include_soap, subsystem);
	}
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}



void process_err_stack(CondorError *errstack) {

	printf("%s\n", errstack->getFullText(true).c_str());

}

void print_security_config(bool include_soap, const char * subsys)
{
	const char * const knobs[] = {
		"SEC_%s_AUTHENTICATION_METHODS", "SEC_%s_CRYPTO_METHODS",
		"SEC_%s_AUTHENTICATION", "SEC_%s_NEGOTIATION",
		"ALLOW_%s", "DENY_%s",
	};

	std::set<std::string> reported; // so we don't repeat ourselves
	std::string param_name;
	const char * banner = nullptr;

	bool legacy = param_boolean("LEGACY_ALLOW_SEMANTICS", false);
	if (legacy) { printf("WARNING: LEGACY_ALLOW_SEMANTICS = true\n"); }

	for (auto knob : knobs) {
		bool is_allow_knob = false;
		if (starts_with(knob, "ALLOW_")) {
			banner = "ALLOW";
			is_allow_knob = true;
		} else if (starts_with(knob, "DENY_")) {
			banner = "DENY";
			is_allow_knob = true;
		} else {
			banner = knob + sizeof("SEC_%s");
		}

		for (DCpermission it = ALLOW_PERM; it < LAST_PERM; it = NEXT_PERM(it)) {
			// iterate so we do DEFAULT first, and skip ALLOW and SOAP
			DCpermission perm = it;
			if (it == DEFAULT_PERM) continue;
			else if (perm == ALLOW_PERM) perm = DEFAULT_PERM;
			else if (perm == SOAP_PERM && !include_soap) continue;

			formatstr(param_name, knob, PermString(perm));
			if (subsys) { param_name += "_"; param_name += subsys; }

			for (auto tag = perm; tag < LAST_PERM; tag = DCpermissionHierarchy::nextConfig(tag, legacy)) {
				std::string from_param, dotted_param;
				auto_free_ptr value(SecMan::getSecSetting(knob, tag, &from_param, subsys));
				if (value) {
					bool already_reported = (reported.count(from_param) > 0);
					bool skip_this = (already_reported && !is_allow_knob);
					if (skip_this) continue;
					const char* hash = skip_this ? "#" : "";

					if (banner) {
						printf("\n #--- %s ---#\n", banner);
						banner = nullptr;
					}

					printf("%s%s = %s\n", hash, param_name.c_str(), value.ptr());
					if (param_name != from_param) fprintf(stdout, "%s # from: %s\n", hash, from_param.c_str());
					reported.insert(from_param);

					const MACRO_META * pmet = nullptr;
					const char * def_val = nullptr;
					std::ignore = param_get_info(from_param.c_str(), subsys, nullptr, dotted_param, &def_val, &pmet);

					if (pmet && ! already_reported) {
						std::string location;
						param_get_location(pmet, location);
						fprintf(stdout, "%s # at: %s\n", hash, location.c_str());
					}
					break;
				}
			}
		}
		// If we had *no* config lookups for this knob, print the default value
		if (banner && ! is_allow_knob) {
			printf("\n #--- %s ---#\n", banner);
			banner = nullptr;

			formatstr(param_name, knob, PermString(DEFAULT_PERM));
			if (subsys) { param_name += "_"; param_name += subsys; }

			if (ends_with(knob, "_CRYPTO_METHODS")) {
				std::string crypto_methods = SecMan::getDefaultCryptoMethods();
				std::string filtered_methods = SecMan::filterCryptoMethods(crypto_methods);
				if (filtered_methods != crypto_methods) {
					fprintf(stdout, "%s = %s\n"
						" # from: <filtered-default>\n"
						" # default: %s\n",
						param_name.c_str(), filtered_methods.c_str(), crypto_methods.c_str()
					);
				} else {
					fprintf(stdout, "%s = %s\n"
						" # from: <compiled-default>\n",
						param_name.c_str(), crypto_methods.c_str()
					);
				}
			} else {
				const char * def_value = "OPTIONAL";
				if (ends_with(knob, "_NEGOTIATION")) def_value = "PREFERRED";
				fprintf(stdout, "%s = %s\n # from: <compiled-default>\n", param_name.c_str(), def_value);
			}
		}
	}
}


void print_useful_info_1(bool rv, const char* dname, const std::string & name, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *) {
	std::string  val;

	if(!rv) {
		printf("%s failed!  Use -verbose for more information.\n", name.c_str());
		return;
	}

	printf("%s command using (", name.c_str());

	std::string encryption_method;
	ad->LookupString("encryption", encryption_method);
	if (strcasecmp(encryption_method.c_str(), "no") == 0) {
		printf("no encryption");
	} else {
		ad->LookupString("cryptomethods", encryption_method);
		printf("%s", encryption_method.c_str());
	}

	printf(", ");

	ad->LookupString("integrity", val);
	if (strcasecmp(val.c_str(), "no") == 0) {
		printf("no integrity");
	} else if (encryption_method == "AES") {
		printf("AES");
	} else {
#ifdef FIPS_MODE
		printf("SHA");
#else
		printf("MD5");
#endif
	}

	printf(", and ");

	ad->LookupString("authentication", val);
	if (strcasecmp(val.c_str(), "no") == 0) {
		printf("no authentication");
	} else {
		ad->LookupString("authmethods", val);
		printf("%s", val.c_str());
	}

	printf(") ");

	bool bval;
	authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval);
	printf(bval ? "succeeded" : "failed");

	printf(" as ");

	ad->LookupString("myremoteusername", val);
	printf("%s", val.c_str());
	printf(" to %s.", dname);
	printf ("\n");
}


void print_useful_info_2(bool rv, const char* dname, int cmd, const std::string & name, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *errstack) {
	std::string  val;

	if(!rv) {
		printf("%s failed!\n", name.c_str());
		process_err_stack(errstack);
		printf("\n");
		return;
	}

	printf("Destination:                 %s\n", dname);
	ad->LookupString("remoteversion", val);
	printf("Remote Version:              %s\n", val.c_str());
	val = CondorVersion();
	printf("Local  Version:              %s\n", val.c_str());

	ad->LookupString("sid", val);
	printf("Session ID:                  %s\n", val.c_str());
	printf("Instruction:                 %s\n", name.c_str());
	printf("Command:                     %i\n", cmd);


	std::string encryption_method;
	ad->LookupString("encryption", encryption_method);
	if (strcasecmp(encryption_method.c_str(), "no") == 0) {
		printf("Encryption:                  none\n");
	} else {
		ad->LookupString("cryptomethods", encryption_method);
		printf("Encryption:                  %s\n", encryption_method.c_str());
	}

	ad->LookupString("integrity", val);
	if (strcasecmp(val.c_str(), "no") == 0) {
		printf("Integrity:                   none\n");
	} else if ("AES" == encryption_method) {
		printf("Integrity:                   AES\n");
	} else {
#ifdef FIPS_MODE
		printf("Integrity:                   SHA\n");
#else
		printf("Integrity:                   MD5\n");
#endif
	}

	ad->LookupString("authentication", val);
	if (strcasecmp(val.c_str(), "no") == 0) {
		printf("Authentication:              none\n");
	} else {
		ad->LookupString("authmethods", val);
		printf("Authenticated using:         %s\n", val.c_str());
		ad->LookupString("authmethodslist", val);
		printf("All authentication methods:  %s\n", val.c_str());
	}

	ad->LookupString("myremoteusername", val);
	printf("Remote Mapping:              %s\n", val.c_str());

	bool bval;
	authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval);
	printf("Authorized:                  %s\n", (bval ? "TRUE" : "FALSE"));

	printf ("\n");

	if (errstack->getFullText() != "") {
		printf ("Information about authentication methods that were attempted but failed:\n");
		process_err_stack(errstack);
		printf ("\n");
	}

}

void print_policy_ad(bool rv, const char* dname, int cmd, const std::string & name, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *errstack) {
	FILE * out = stdout;
	std::string  buffer;

	const char * authok = "DENIED";
	if(!rv) {
		authok = "FAILED";
		buffer = "#";
		buffer += errstack->getFullText(false);
		buffer += "\n";
	} else {
		bool bval = false;
		if (authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval)) {
			if  (bval) authok = "ALLOWED";
		}
	}

	// for -long output print a comment before each ad showing the ALLOW/DENY result
	// unless a projection was specified. For the projection case, inject a few fake attrs
	if (App.prattrs.empty()) {
		formatstr_cat(buffer, "# %s command to %s is %s\n", name.c_str(), dname, authok);
	} else {
		// set some fake attributes about the query
		// they will only be printed if they are in the projection.
		ad->Assign(ATTR_NAME, getCommandStringSafe(cmd));
		ad->Assign("From", dname);
		ad->Assign("Result", authok);
	}

	if (rv) {
		classad::References * attrs = nullptr;
		if ( ! App.prattrs.empty()) attrs = &App.prattrs;
		formatAd(buffer, *ad, nullptr, attrs, false);
		buffer += "\n";
	}
	fputs(buffer.c_str(), out);
}

static bool simulate_policy_from_errstack(ClassAd & ad, CondorError * errstack)
{
	if ( ! errstack) return false;

	std::string tmp;
	int errcode = errstack->code();

	// fake up a policy ad from the errstack text for permission denied
	if (errcode == SECMAN_ERR_AUTHORIZATION_FAILED) {
		// SECMAN:2010:Received "DENIED" from server for user bob@host using method XYZ.
		int state = 0;
		for (auto &tok : StringTokenIterator(errstack->message(), ", \r\n\"" )) {
			switch(state) {
			case 0: if ("Received" == tok) { state = 1; } break;
			case 1: { ad.Assign("Result", tok); state = 2; } break;
			case 2: if ("user" == tok) { state = 3; } break;
			case 3: { ad.Assign("MyRemoteUserName", tok); state = 4; } break;
			case 4: if ("method" == tok) { state = 5; } break;
			case 5: {
				// get rid of trailing .
				tmp = tok; if (tmp.back() == '.') tmp.pop_back();
				ad.Assign("AuthMethods", tmp); state = 6;
			} break;
			}
		}
		return true;
	} else if (errcode == SECMAN_ERR_COMMUNICATIONS_ERROR) {
		// SECMAN:2007:Failed to end classad message.
	}

	return false;
}

void print_policy_attrs(bool rv, const char* dname, int cmd, const std::string & /*name*/, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *errstack) {
	FILE * out = stdout;
	ClassAd fake_policy;
	std::string  buffer;

	const char * authok = "DENIED";
	if(!rv) {
		authok = "FAILED";
		buffer = "#";
		buffer += errstack->getFullText(false);
		buffer += "\n";

		// fake a policy ad for the benefit of -af
		ad = &fake_policy;
		ad->Assign("AuthCommand", cmd);
		ad->Assign("Result", authok);
		if (simulate_policy_from_errstack(*ad, errstack)) {
			buffer.clear(); // don't print the errstack text.
		}
	} else {
		bool bval = false;
		if (authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval)) {
			if (bval) authok = "ALLOWED";
		}
		ad->Assign("Result", authok);
	}

	// inject some context attributes
	ad->Assign(ATTR_NAME, getCommandStringSafe(cmd));
	ad->Assign("From", dname);

	// print headings (if any) before the first output
	if (App.prmask.has_headings() && ! App.printed_headings) {
		// render an ad to calculate column widths.
		App.prmask.display(buffer, ad);
		buffer.clear();
		// now print headings
		App.prmask.display_Headings(out);
		App.printed_headings = true;
	}



	App.prmask.display(buffer, ad);

	fputs(buffer.c_str(), out);
}

void print_useful_info_10(bool rv, const char*, const std::string & name, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *) {
	std::string  val;

	printf("%20s", name.c_str());

	if(!rv) {
		printf ("           FAIL       FAIL      FAIL     FAIL FAIL  (use -verbose for more info)\n");
		return;
	}

	ad->LookupString("authentication", val);
	if (strcasecmp(val.c_str(), "no") == 0) {
		val = "none";
	} else {
		ad->LookupString("authmethods", val);
	}
	printf("%15s", val.c_str());

	std::string encryption_method;
	ad->LookupString("encryption", encryption_method);
	if (strcasecmp(encryption_method.c_str(), "no") == 0) {
		encryption_method = "none";
	} else {
		ad->LookupString("cryptomethods", encryption_method);
	}
	printf("%11s", encryption_method.c_str());

	ad->LookupString("integrity", val);
	if (strcasecmp(val.c_str(), "no") == 0) {
		val = "none";
	} else if (encryption_method == "AES") {
		val = encryption_method;
	} else {
#ifdef FIPS_MODE
		val = "SHA";
#else
		val = "MD5";
#endif
	}
	printf("%10s", val.c_str());

	bool bval;
	authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval);
	printf(bval ? "    ALLOW " : "     DENY ");

	ad->LookupString("myremoteusername", val);
	printf("%s", val.c_str());

	printf("\n");
}


void print_info(bool rv, const char* dname, const char * addr, Sock* s, const std::string & name, int cmd, ClassAd *authz_ad, CondorError *errstack, int output_mode) {
	std::string cmd_map_ent;
	const std::string &tag = SecMan::getTag();
	if (tag.size()) {
		formatstr(cmd_map_ent, "{%s,%s,<%i>}", tag.c_str(), addr, cmd);
	} else {
		formatstr(cmd_map_ent, "{%s,<%i>}", addr, cmd);
	}

	std::string session_id;
	ClassAd *policy = NULL;

	if(rv) {
		auto result_pair = SecMan::command_map.find(cmd_map_ent);
		if (result_pair == SecMan::command_map.end()) {
			printf("no cmd map!\n");
			return;
		}
		session_id = result_pair->second;

		// IMPORTANT: this hashtable returns 1 on success!
		auto itr = (SecMan::session_cache)->find(session_id);
		if (itr == (SecMan::session_cache)->end()) {
			printf("no session!\n");
			return;
		}

		policy = itr->second.policy();
	}


	if (output_mode == 0) {
		// print nothing!!
	} else if (output_mode == 1) {
		print_useful_info_1(rv, dname, name, s, policy, authz_ad, errstack);
	} else if (output_mode == 2) {
		print_useful_info_2(rv, dname, cmd, name, s, policy, authz_ad, errstack);
	} else if (output_mode == 3) {
		print_policy_ad(rv, dname, cmd, name, s, policy, authz_ad, errstack);
	} else if (output_mode == 4) {
		print_policy_attrs(rv, dname, cmd, name, s, policy, authz_ad, errstack);
	} else if (output_mode == 10) {
		print_useful_info_10(rv, dname, name, s, policy, authz_ad, errstack);
	}
}


int getSomeCommandFromString ( const char * cmdstring ) {

	int res = -1;

	res = getPermissionFromString( cmdstring );
	if (res != -1) {
		res = getSampleCommand( res );
		dprintf( D_ALWAYS, "recognized %s as authorization level, using command %i.\n", cmdstring, res );
		return res;
	}
	// CRUFT The old OWNER authorization level was merged into
	//   ADMINISTRATOR in HTCondor 9.9.0. For older daemons, we still
	//   recognize "OWNER" and issue the DC_NOP_OWNER command.
	//   We should remove it eventually.
	if (strcasecmp(cmdstring, "OWNER") == 0) {
		res = DC_NOP_OWNER;
		dprintf( D_ALWAYS, "recognized %s as old authorization level, using command %i.\n", cmdstring, res );
		return res;
	}

	res = getCommandNum( cmdstring );
	if (res != -1) {
		dprintf( D_ALWAYS, "recognized %s as command name, using command %i.\n", cmdstring, res );
		return res;
	}

	res = atoi ( cmdstring );
	if (res > 0 || (strcmp("0", cmdstring) == 0)) {
		char compare_conversion[20];
		snprintf(compare_conversion, sizeof(compare_conversion), "%i", res);
		if (strcmp(cmdstring, compare_conversion) == 0) {
			dprintf( D_ALWAYS, "recognized %i as command number.\n", res );
			return res;
		}
	}

	return -1;

}


bool do_item(Daemon* d, const std::string & name, int num, int output_mode) {

	CondorError errstack;
	ClassAd authz_ad;
	bool sc_success;
	ReliSock *sock = NULL;
	bool fn_success = false;

	sock = (ReliSock*) d->makeConnectedSocket( Stream::reli_sock, 0, 0, &errstack );
	if (sock) {

		sc_success = d->startSubCommand(DC_SEC_QUERY, num, sock, 0, &errstack);
		if (sc_success) {

			sock->decode();
			if (getClassAd(sock, authz_ad) &&
				sock->end_of_message()) {
				fn_success = true;
			}
		}
		const char* dname = d->idStr();
		print_info(fn_success, dname, sock->get_connect_addr(), sock, name, num, &authz_ad, &errstack, output_mode);
	} else {
		// we know that d->addr() is not null because we checked before
		// calling do_item.  but i'll be paranoid and check again.
		fprintf(stderr, "ERROR: failed to make connection to %s\n", d->addr()?d->addr():"(null)");
	}

	return fn_success;

}

int main( int argc, const char *argv[] )
{
	const char *pcolon;
	const char *pool=nullptr;
	const char *name=nullptr;
	const char *address=nullptr;
	const char *optional_config=nullptr;
	const char *root_config=nullptr;
	const char *dash_subsystem=nullptr;
	int  output_mode = -1; // 0=quiet, 1=sentence, 2=paragraph, 3=long, 4=af, 10=table
	bool dash_help = false;
	int  help_details = 0;
	bool dash_debug = false;
	const char * debug_flags = nullptr;
	daemon_t dtype = DT_NONE;

	std::vector<std::string> worklist_name;
	std::vector<int> worklist_number;
	Daemon * daemon = NULL;

	App.Name = argv[0];

	for( int i=1; i<argc; i++ ) {
		if (is_dash_arg_prefix (argv[i], "help", 1)) {
			help_details = usage_Help;
			while ((i+1 < argc) && *(argv[i+1]) != '-') {
				++i;
				if (is_arg_prefix(argv[i], "levels", 3) || is_arg_prefix(argv[i], "auth", 2)) {
					help_details |= usage_AuthLevels;
				} else if (is_arg_prefix(argv[i], "config", 2)) {
					help_details |= usage_Config;
				} else if (is_arg_prefix(argv[i], "permheir", 2)) {
					help_details |= usage_PermHeir;
				} else if (is_arg_prefix(argv[i], "all", 2)) {
					help_details |= usage_HelpAll;
				}
			}
			dash_help = true;
		} else if (is_dash_arg_prefix(argv[i],"quiet")) {
			if(output_mode == -1) {
				output_mode = 0;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i],"table",2)) {
			if(output_mode == -1) {
				output_mode = 10;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i],"verbose")) {
			if(output_mode == -1) {
				output_mode = 2;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i],"long")) {
			if(output_mode == -1 || output_mode == 3) {
				output_mode = 3;
				App.long_format = true;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if (is_dash_arg_prefix (argv[i], "format", 1)) {
			if(output_mode == -1 || output_mode == 4) {
				output_mode = 4;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if ( ! argv[i+1] || ! argv[i+2]) {
				fprintf(stderr,"ERROR: -format requires 2 arguments\n\n");
				usage(App.Name);
				exit(1);
			}
			App.prmask.registerFormatF(argv[i+1], argv[i+2], FormatOptionNoTruncate);
			if ( ! IsValidClassAdExpression(argv[i+2], &App.prattrs, NULL)) {
				fprintf(stderr,"ERROR: %s is not a valid expression\n\n", argv[i+2]);
				usage(App.Name);
				exit(1);
			}
			i+=2;
		} else if (is_dash_arg_colon_prefix(argv[i], "af", &pcolon, 2) ||
				is_dash_arg_colon_prefix(argv[i], "autoformat", &pcolon, 5)) {
			if (pcolon) ++pcolon; // of there are options, skip over the :
			int ixNext = parse_autoformat_args(argc, argv, i+1, pcolon, App.prmask, App.prattrs, App.diagnostic);
			if (ixNext < 0) {
				fprintf(stderr,"ERROR: could not parse -af option\n\n");
				usage(App.Name);
				exit(1);
			}
			if (ixNext > i) {
				i = ixNext-1;
			}
			if(output_mode == -1  || output_mode == 4) {
				output_mode = 4;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i],"config")) {
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -config requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			optional_config = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "root-config", 4)) {
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -root-config requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			root_config = argv[i];
		} else if (is_dash_arg_prefix(argv[i],"pool")) {
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -pool requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if(address) {
				fprintf(stderr,"ERROR: -address cannot be used with -pool or -name.\n\n");
				usage(argv[0]);
				exit(1);
			}
			pool = argv[i];
		} else if (is_dash_arg_prefix(argv[i],"name")) {
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -name requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if(address) {
				fprintf(stderr,"ERROR: -address cannot be used with -pool or -name.\n\n");
				usage(argv[0]);
				exit(1);
			}
			name = argv[i];
		} else if (is_dash_arg_prefix(argv[i],"subsystem",3) || is_dash_arg_prefix(argv[i],"type",4)) {
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: %s requires an argument.\n\n", argv[i]);
				usage(argv[0]);
				exit(1);
			}
			dash_subsystem = argv[i];
			dtype = stringToDaemonType(argv[i]);
			if( dtype == DT_NONE) {
				fprintf(stderr,"ERROR: unrecognized daemon type: %s\n\n", argv[i]);
				usage(argv[0]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i],"address")) {
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -address requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if(pool || name) {
				fprintf(stderr,"ERROR: -address cannot be used with -pool or -name.\n\n");
				usage(argv[0]);
				exit(1);
			}
			address = argv[i];
		} else if (is_dash_arg_prefix(argv[i],"version", 4)) {
			version();
			exit(0);
		} else if (is_dash_arg_colon_prefix(argv[i],"debug",&pcolon)) {
			dash_debug = true;
			debug_flags = (pcolon && pcolon[1]) ? pcolon+1 : nullptr;
		} else if(argv[i][0]!='-' || !strcmp(argv[i],"-")) {
			// a special case
			// CRUFT The old OWNER authorization level was merged into
			//   ADMINISTRATOR in HTCondor 9.9.0. For older daemons, we still
			//   recognize "OWNER" and issue the DC_NOP_OWNER command.
			//   We should remove it eventually.
			if(strcasecmp("ALL", argv[i]) == 0) {
				worklist_name.push_back("ALLOW");
				worklist_name.push_back("READ");
				worklist_name.push_back("WRITE");
				worklist_name.push_back("NEGOTIATOR");
				worklist_name.push_back("ADMINISTRATOR");
				worklist_name.push_back("OWNER");
				worklist_name.push_back("CONFIG");
				worklist_name.push_back("DAEMON");
				worklist_name.push_back("ADVERTISE_STARTD");
				worklist_name.push_back("ADVERTISE_SCHEDD");
				worklist_name.push_back("ADVERTISE_MASTER");
			} else {
				// an individual item to act on
				worklist_name.push_back(argv[i]);
			}
		} else {
			fprintf(stderr,"ERROR: Unknown argument: %s\n\n",argv[i]);
			usage(argv[0]);
			exit(1);
		}
	}

	set_priv_initialize(); // allow uid switching if root

	// load the supplied config if specified
	if (optional_config) {
		// push the optional config into a global used config_host as the "last" local config file
		extern const char * simulated_local_config;
		simulated_local_config = optional_config;
	}

	int config_options = CONFIG_OPT_WANT_META;
	if (root_config) { config_options |= CONFIG_OPT_USE_THIS_ROOT_CONFIG | CONFIG_OPT_NO_EXIT; }
	config_host(NULL, config_options, root_config);

	// we do this after we load config so that "-help config" works correctly 
	if (dash_help) {
		usage(argv[0], help_details, dash_subsystem);
		exit(0);
	}

	// dprintf to console
	if (dash_debug) {
		dprintf_set_tool_debug("TOOL", debug_flags);
	}

	// 1 (normal) is the default
	if(output_mode == -1) {
		output_mode = 1;
	}

	// use some default
	if(worklist_name.size() == 0) {
		if(output_mode > 4) {
			fprintf( stderr, "WARNING: Missing <authz-level | command-name | command-int> argument, defaulting to WRITE.\n");
		}
		worklist_name.push_back("WRITE");
	}


	// convert each item
	bool all_okay = true;
	for (size_t i=0; i<worklist_name.size(); i++) {
		int c = getSomeCommandFromString(worklist_name[i].c_str());
		if (c == -1) {
			if(output_mode) {
				fprintf(stderr, "ERROR: Could not understand TOKEN \"%s\".\n", worklist_name[i].c_str());
			}
			all_okay = false;
		}
		worklist_number.push_back(c);
	}
	if (!all_okay) {
		exit(1);
	}


	//
	// LETS GET TO WORK!
	//

	if(dtype == DT_NONE && !address) {
		if(output_mode > 4) {
			fprintf( stderr, "WARNING: Missing daemon argument, defaulting to SCHEDD.\n");
		}
		dtype = DT_SCHEDD;
	}

	if(address) {
		daemon = new Daemon( DT_ANY, address, 0 );
	} else {
		if (pool) {
			DCCollector col( pool );
			if( ! col.addr() ) {
				fprintf( stderr, "ERROR: %s\n", col.error() );
				exit(1);
			}
			daemon = new Daemon( dtype, name, col.addr() );
		} else {
			daemon = new Daemon( dtype, name );
		}
	}

	if (!(daemon->locate(Daemon::LOCATE_FOR_LOOKUP))) {
		if(output_mode) {
			fprintf(stderr, "ERROR: couldn't locate %s!\n", address?address:name);
		}
		delete daemon;
		exit(1);
	}

	// add a check for address being null even though locate() succeeded.
	// this happens if the address doesn't parse.
	if(!daemon->addr()) {
		fprintf(stderr, "ERROR: unable to parse sinful string: %s\n", address);
		delete daemon;
		exit(1);
	}

	// do we need to print headers?
	if(output_mode == 10) {
		printf ("         Instruction Authentication Encryption Integrity Decision Identity\n");
	}

	all_okay = true;
	for(size_t i=0; i<worklist_name.size(); i++) {
		// any item failing induces failure of whole program
		if (!do_item(daemon, worklist_name[i], worklist_number[i], output_mode)) {
			all_okay = false;
		}
	}

	delete daemon;

	return (all_okay ? 0 : 1);

}

