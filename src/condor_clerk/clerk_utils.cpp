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

#include "clerk_utils.h"
#include "clerk_collect.h"

const char * AdTypeName(AdTypes whichAds)
{
	return AdTypeToString(whichAds);
}

AdTypes AdTypeByName(const char * name)
{
	return AdTypeFromString(name);
}

void remove_self_from_collector_list(CollectorList * colist, bool ImTheCollector)
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
			colist->deleteCurrent();
			continue;
		}

		if ( ! ImTheCollector)
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
			std::string collectorSPID;
			param( collectorSPID, "SHARED_PORT_DEFAULT_ID" );
			if(! collectorSPID.size()) { collectorSPID = "collector"; }
			if( strcmp( mySinful.getSharedPortID(), collectorSPID.c_str() ) == 0 ) {
				dprintf( D_FULLDEBUG, "Skipping sending update to myself via my shared port daemon.\n" );
				colist->deleteCurrent();
			}
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

	CondorClassAdFileIterator adIter;
	if ( ! adIter.begin(fh, true, ftype)) {
		fclose(fh); fh = NULL;
		dprintf(D_ALWAYS, "Failed to create CondorClassAdFileIterator for %s\n", file);
		return;
	} else {
		ClassAd * ad;
		while ((ad = adIter.next(constr.Expr()))) {
			collect(operation, adtype, ad, NULL);
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


bool param_and_populate_map(const char * param_name, NOCASE_STRING_MAP & smap)
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

