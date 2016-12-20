/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "dc_collector.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"
#include "condor_threads.h"
#include "condor_state.h" // for state_to_string

#include "clerk_utils.h"
#include "clerk_collect.h"

const char * print_bits(char *buf, int cch, unsigned int bits[], int cbBits, int cbWord, int pos)
{
	unsigned int outbit = 0; // Current bit # to output
	unsigned int outword = 0x0; // Current word to ouput

	// Calculate the "last" offset
	int offset = pos; // History offset
	if (--offset < 0) {
		offset = cbBits*8;
	}

	char *p = buf;
	if (cch < 1) return NULL;
	if (cch < 2) { buf[0] = 0; return buf; }
	char *pe = buf+cch-1;

	// And, the starting "word" and bit numbers
	int word_num = offset / cbWord;
	int bit_num = offset % cbWord;

	// Walk through 1 bit at a time...
	for (int loop = 0; loop < cbBits*8;  loop++ ) {
		unsigned int mask = (1 << bit_num);

		if (bits[word_num] & mask) {
			outword |= (0x8 >> outbit);
		}
		if (--bit_num < 0) {
			bit_num = (cbWord - 1);
			if (--word_num < 0) {
				word_num = (cbWord - 1);
			}
		}

		// Convert to a char
		if (++outbit == 4) {
			if (p+1 >= pe) break;
			*p++ = outword + (outword < 10) ? '0' : ('A' - 10);
			outbit = 0;
			outword = 0x0;
		}

	}
	if (outbit && p+1 < pe) {
		*p++ = outword + (outword < 10) ? '0' : ('A' - 10);
	}
	*p = 0;
	return buf;
}


void remove_self_from_collector_list(CollectorList * colist, bool check_default_id, bool im_the_collector)
{
	DCCollector * daemon = NULL;
	colist->rewind();
	const char * myself = daemonCore->InfoCommandSinfulString();
	if( myself == NULL ) {
		EXCEPT( "Unable to determine my own address, aborting rather than hang.  You may need to make sure the shared port daemon is running first." );
	}
	Sinful mySinful( myself );
	Sinful mySharedPortDaemonSinful = mySinful;
	mySharedPortDaemonSinful.setSharedPortID( NULL );
	while( colist->next( daemon ) ) {
		const char * current = daemon->addr();
		if( current == NULL ) { continue; }

		Sinful currentSinful( current );
		if( mySinful.addressPointsToMe( currentSinful ) ) {
			dprintf( D_FULLDEBUG, "removing '%s' from collector list because it points to me.\n",  currentSinful.getSinful() );
			colist->deleteCurrent();
			continue;
		}

		if ( ! check_default_id)
			continue;

		// addressPointsToMe() doesn't know that the shared port daemon
		// forwards connections that otherwise don't ask to be forwarded
		// to the collector.  This means that COLLECTOR_HOST doesn't need
		// to include ?sock=collector, but also that mySinful has a
		// shared port address and currentSinful may not.  Since we know
		// that we're trying to contact the collector here -- that is, we
		// can tell we're not contacting the shared port daemon in the
		// process of doing something else -- we can safely assume that
		// any currentSinful without a shared port ID intends to connect
		// to the default collector.
		dprintf( D_FULLDEBUG, "checking for self: '%s', '%s, '%s'\n", mySinful.getSharedPortID(), mySharedPortDaemonSinful.getSinful(), currentSinful.getSinful() );
		if( mySinful.getSharedPortID() != NULL && mySharedPortDaemonSinful.addressPointsToMe( currentSinful ) ) {
			// Check to see if I'm the default collector.
#if 1
			dprintf( D_FULLDEBUG, "Shared port default ID points to me. ImTheCollector=%d.\n", im_the_collector );
			if (im_the_collector) {
				dprintf( D_FULLDEBUG, "ImTheCollector so Skipping sending update to myself via my shared port daemon.\n" );
				colist->deleteCurrent();
			}
#else
			std::string collectorSPID;
			param( collectorSPID, "SHARED_PORT_DEFAULT_ID" );
			if(! collectorSPID.size()) { collectorSPID = "collector"; }
			if( strcmp( mySinful.getSharedPortID(), collectorSPID.c_str() ) == 0 ) {
				dprintf( D_FULLDEBUG, "Skipping sending update to myself via my shared port daemon.\n" );
				colist->deleteCurrent();
			}
#endif
		}
	}
}

void update_ads_from_file (
	int operation,
	AdTypes adtype,
	const char * file,
	CondorClassAdFileParseHelper::ParseType ftype,
	ConstraintHolder & constr)
{
	FILE* fh;
	bool close_file = false;
	if (MATCH == strcmp(file, "-")) {
		fh = stdin;
		close_file = false;
	} else {
		fh = safe_fopen_wrapper_follow(file, "r");
		if (fh == NULL) {
			dprintf(D_ALWAYS, "Can't open file of ClassAds: %s\n", file);
		}
		close_file = true;
	}

	CollectStatus status;
	CondorClassAdFileIterator adIter;
	if ( ! adIter.begin(fh, close_file, ftype)) {
		fclose(fh); fh = NULL;
		dprintf(D_ALWAYS, "Failed to create CondorClassAdFileIterator for %s\n", file);
		return;
	} else {
		ClassAd * ad;
		while ((ad = adIter.next(constr.Expr()))) {
			collect(operation, adtype, ad, NULL, status);
		}
	}
}

class ClerkMacroStreamFile : public MacroStreamFile
{
public:
	ClerkMacroStreamFile() {};
	FILE * handle() { return fp; }
	const char * filename() { return macro_source_filename(src, ClerkVars); }
};

class CollectionIterFromFile : public CollectionIterator<ClassAd*>
{
public:
	ClerkMacroStreamFile msf;
	CondorClassAdFileIterator adIter;
	ConstraintHolder constr;
	ClassAd * cur;
	bool is_empty;
	bool at_start;
	bool at_end;

	CollectionIterFromFile(char * constraint)
		: constr(constraint)
		, cur(NULL)
		, is_empty(true), at_start(true), at_end(false)
	{}

	virtual ~CollectionIterFromFile() {}

	void UseFile(FILE * fh, bool close_fh, CondorClassAdFileParseHelper::ParseType ftype) {
		adIter.begin(fh, close_fh, ftype);
		is_empty = false;
		at_start = true;
		at_end = false;
	}

	bool OpenFile(const char * file, bool is_command, CondorClassAdFileParseHelper::ParseType ftype) {
		std::string errmsg;
		if ( ! msf.open(file, is_command, ClerkVars, errmsg))
			return false;
		UseFile(msf.handle(), false, ftype);
		return true;
	}

	virtual bool IsEmpty() const { return ! is_empty; }
	virtual void Rewind() { ASSERT(at_start); }
	virtual ClassAd* Next () {
		cur = adIter.next(constr.Expr());
		at_start = false;
		at_end = !cur;
		return cur;
	}
	virtual ClassAd* Current() const { return cur; }
	virtual bool AtEnd() const { return at_end; }
};

CollectionIterator<ClassAd*>* clerk_get_file_iter (
	const char * file,
	bool is_command,
	char * constraint /*=NULL*/,
	CondorClassAdFileParseHelper::ParseType ftype /*= CondorClassAdFileParseHelper::Parse_long*/)
{
	CollectionIterFromFile * it = new CollectionIterFromFile(constraint);
	if ( ! it->OpenFile(file, is_command, ftype)) {
		delete it; it = NULL;
	}
	return it;
}

void collect_free_file_iter(CollectionIterator<ClassAd*> * it)
{
	if (it) {
		CollectionIterFromFile * fit = static_cast<CollectionIterFromFile *>(it);
		delete fit;
	}
}


bool param_and_populate_smap(const char * param_name, NOCASE_STRING_MAP & smap)
{
	char *lines = param(param_name);
	if ( ! lines)
		return false;

	int cItems = 0;
	MyString line;
	MyStringCharSource src(lines, true);
	while (line.readLine(src)) {
		line.trim();
		if (line[0] == '#') continue;
		int ix = line.FindChar('=', 0);
		if (ix > 0) {
			MyString key = line.Substr(0, ix-1); key.trim();
			MyString rhs = line.Substr(ix+1, INT_MAX); rhs.trim();
			smap[key.Value()] = rhs.Value();
			++cItems;
		}
	}
	return cItems > 0;
}

const char * smap_string(const char * name, const NOCASE_STRING_MAP & smap) {
	NOCASE_STRING_MAP::const_iterator found = smap.find(name);
	if (found == smap.end()) return NULL;
	return found->second.c_str();
}
bool smap_bool(const char * name, const NOCASE_STRING_MAP & smap, bool def_val) {
	const char * str = smap_string(name, smap);
	if ( ! str || ! str[0]) return def_val;
	bool bval = def_val;
	if ( ! string_is_boolean_param(str, bval)) bval = def_val;
	return bval;
}
bool smap_number(const char * name, const NOCASE_STRING_MAP & smap, long long & lval) {
	const char * str = smap_string(name, smap);
	if ( ! str || ! str[0]) return false;
	return string_is_long_param(str, lval);
}
bool smap_number(const char * name, const NOCASE_STRING_MAP & smap, double & dval) {
	const char * str = smap_string(name, smap);
	if ( ! str || ! str[0]) return false;
	return string_is_double_param(str, dval);
}

bool ExprTreeIsScopedAttrRef(classad::ExprTree * expr, std::string & attr, std::string & scope)
{
	if ( ! expr) return false;

	classad::ExprTree::NodeKind kind = expr->GetKind();
	if (kind == classad::ExprTree::ATTRREF_NODE) {
		classad::ExprTree *e2=NULL;
		bool absolute=false;
		scope.clear();
		((classad::AttributeReference*)expr)->GetComponents(e2, attr, absolute);
		return !e2 || ExprTreeIsAttrRef(e2, scope, &absolute);
	}
	return false;
}

const classad::ClassAd* ExprTreeIsClassAd(classad::ExprTree * expr)
{
	if ( ! expr) return NULL;
	classad::ExprTree::NodeKind kind = expr->GetKind();
	if (kind == classad::ExprTree::CLASSAD_NODE) {
		return dynamic_cast<const classad::ClassAd*>(expr);
	}
	return NULL;
}

bool ExprTreeIsFnCall(classad::ExprTree * expr, std::string &fnName, std::vector<classad::ExprTree*>& args)
{
	if ( ! expr) return NULL;
	classad::ExprTree::NodeKind kind = expr->GetKind();
	if (kind == classad::ExprTree::FN_CALL_NODE) {
		((const classad::FunctionCall*)expr)->GetComponents(fnName, args);
		return true;
	}
	return false;
}


/*
bool ExprTreeIsClassad(classad::ExprTree * expr, std::vector< std::pair<std::string, classad::ExprTree*> > & attrs)
{
	const classad::ClassAd* ad = ExprTreeIsClassAd(expr);
	if (ad) {
		ad->GetComponents(attrs);
		return true;
	}
	return false;
}
*/

// find (and optionally count) attribute references that match the given set
int has_specific_attr_refs (
	const classad::ExprTree * tree,
	NOCASE_STRING_TO_INT_MAP & mapping,
	bool stop_on_first_match)
{
	int iret = 0;
	if ( ! tree) return 0;
	switch (tree->GetKind()) {
		case ExprTree::LITERAL_NODE: {
			classad::ClassAd * ad;
			classad::Value val;
			classad::Value::NumberFactor	factor;
			((const classad::Literal*)tree)->GetComponents( val, factor );
			if (val.IsClassAdValue(ad)) {
				iret += has_specific_attr_refs(ad, mapping, stop_on_first_match);
			}
		}
		break;

		case ExprTree::ATTRREF_NODE: {
			const classad::AttributeReference* atref = reinterpret_cast<const classad::AttributeReference*>(tree);
			classad::ExprTree *expr;
			std::string ref;
			std::string tmp;
			bool absolute;
			atref->GetComponents(expr, ref, absolute);
			// if there is a non-trivial left hand side (something other than X from X.Y attrib ref)
			// then recurse it.
			if (expr && ! ExprTreeIsAttrRef(expr, tmp)) {
				iret += has_specific_attr_refs(expr, mapping, stop_on_first_match);
			} else {
				NOCASE_STRING_TO_INT_MAP::iterator found;
				if (expr) {
					found = mapping.find(tmp);
				} else {
					found = mapping.find(ref);
				}
				if (found != mapping.end()) {
					found->second += 1;
					iret += 1;
				}
			}
		}
		break;

		case ExprTree::OP_NODE: {
			classad::Operation::OpKind	op;
			classad::ExprTree *t1, *t2, *t3;
			((const classad::Operation*)tree)->GetComponents( op, t1, t2, t3 );
			if (t1) iret += has_specific_attr_refs(t1, mapping, stop_on_first_match);
			if (iret && stop_on_first_match) return iret;
			if (t2) iret += has_specific_attr_refs(t2, mapping, stop_on_first_match);
			if (iret && stop_on_first_match) return iret;
			if (t3) iret += has_specific_attr_refs(t3, mapping, stop_on_first_match);
		}
		break;

		case ExprTree::FN_CALL_NODE: {
			std::string fnName;
			std::vector<classad::ExprTree*> args;
			((const classad::FunctionCall*)tree)->GetComponents( fnName, args );
			for (std::vector<classad::ExprTree*>::iterator it = args.begin(); it != args.end(); ++it) {
				iret += has_specific_attr_refs(*it, mapping, stop_on_first_match);
				if (iret && stop_on_first_match) return iret;
			}
		}
		break;

		case ExprTree::CLASSAD_NODE: {
			std::vector< std::pair<std::string, classad::ExprTree*> > attrs;
			((const classad::ClassAd*)tree)->GetComponents(attrs);
			for (std::vector< std::pair<std::string, classad::ExprTree*> >::iterator it = attrs.begin(); it != attrs.end(); ++it) {
				iret += has_specific_attr_refs(it->second, mapping, stop_on_first_match);
				if (iret && stop_on_first_match) return iret;
			}
		}
		break;

		case ExprTree::EXPR_LIST_NODE: {
			std::vector<classad::ExprTree*> exprs;
			((const classad::ExprList*)tree)->GetComponents( exprs );
			for (std::vector<ExprTree*>::iterator it = exprs.begin(); it != exprs.end(); ++it) {
				iret += has_specific_attr_refs(*it, mapping, stop_on_first_match);
				if (iret && stop_on_first_match) return iret;
			}
		}
		break;

		case ExprTree::EXPR_ENVELOPE: {
			classad::ExprTree * expr = SkipExprEnvelope(const_cast<classad::ExprTree*>(tree));
			if (expr) iret += has_specific_attr_refs(expr, mapping, stop_on_first_match);
		}
		break;

		default:
			// unknown or unallowed node.
			ASSERT(0);
		break;
	}
	return iret;
}

void ConvertStartdAdToOffline(ClassAd & ad)
{
	/* reset any values in the ad that may interfere with
	a match in the future */

	/* Reset Condor state */
	ad.Assign ( ATTR_STATE, state_to_string ( unclaimed_state ) );
	ad.Assign ( ATTR_ACTIVITY, activity_to_string ( idle_act ) );
	ad.Assign ( ATTR_ENTERED_CURRENT_STATE, 0 );
	ad.Assign ( ATTR_ENTERED_CURRENT_ACTIVITY, 0 );

	/* Set the heart-beat time */
	int now = static_cast<int> ( time ( NULL ) );
	ad.Assign ( ATTR_MY_CURRENT_TIME, now );
	ad.Assign ( ATTR_LAST_HEARD_FROM, now );

	/* Reset machine load */
	ad.Assign ( ATTR_LOAD_AVG, 0.0 );
	ad.Assign ( ATTR_CONDOR_LOAD_AVG, 0.0 );
	ad.Assign ( ATTR_TOTAL_LOAD_AVG, 0.0 );
	ad.Assign ( ATTR_TOTAL_CONDOR_LOAD_AVG, 0.0 );

	/* Reset CPU load */
	ad.Assign ( ATTR_CPU_IS_BUSY, false );
	ad.Assign ( ATTR_CPU_BUSY_TIME, 0 );

	/* Reset keyboard and mouse times */
	ad.Assign ( ATTR_KEYBOARD_IDLE, INT_MAX );
	ad.Assign ( ATTR_CONSOLE_IDLE, INT_MAX );

	/* any others? */
}


