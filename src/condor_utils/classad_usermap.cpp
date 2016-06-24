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

 
/*
  This file holds utility functions that the class userMap function depends on
*/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_config.h"
#include "MapFile.h"

// userMap stuff
class MapHolder {
public:
	MyString filename;
	time_t   file_timestamp; // last modify time of the file 
	MapFile * mf;
	MapHolder(MapFile * _mf=NULL) : file_timestamp(0), mf(_mf) {}
	~MapHolder() { delete mf; mf = NULL; }
};
typedef std::map<std::string, MapHolder, classad::CaseIgnLTStr> STRING_MAPS;
static STRING_MAPS * g_user_maps = NULL;

void clear_user_maps(StringList * keep_list) {
	if ( ! g_user_maps) return;
	if ( ! keep_list || keep_list->isEmpty()) {
		g_user_maps->clear();
		return;
	}
	for (STRING_MAPS::iterator it = g_user_maps->begin(); it != g_user_maps->end(); /* advance in the loop */) {
		STRING_MAPS::iterator tmp = it++;
		if (keep_list->find(tmp->first.c_str(), true)) {
			g_user_maps->erase(tmp);
		}
	}
	if (g_user_maps->empty()) {
		delete g_user_maps;
		g_user_maps = NULL;
	}
}

static time_t get_file_timestamp(const char * file)
{
	if ( ! file) return 0;

	struct stat sbuf;
	if (stat(file, &sbuf) < 0) {
		return 0;
	}
	
	return sbuf.st_mtime;
}

// Add a mapfile or pre-loaded map into the userMap lookup table.
// either the filename or the input MapFile may be null, but not both
int add_user_map(const char * mapname, const char * filename, MapFile * mf /*=NULL*/)
{
	if ( ! g_user_maps) {
		g_user_maps = new STRING_MAPS;
	}
	STRING_MAPS::iterator found = g_user_maps->find(mapname);
	if (found != g_user_maps->end()) {
		// map exists, remove it if the filename does not match
		// and a MapFile was not supplied as an argument
		MapHolder * pmh = &found->second;
		if (filename && !mf && (pmh->filename == filename)) {
			// if the filename is the same, and the modify time is also the same
			// skip reloading the file. 
			time_t ts = get_file_timestamp(filename);
			if (ts && ts == pmh->file_timestamp) {
				return 0;
			}
		}
		// we have an entry, but we want to reload it. so delete the old one.
		g_user_maps->erase(found);
	}

	// if mapfile was not supplied, load it now.
	time_t ts = filename ? get_file_timestamp(filename) : 0;
	if ( ! mf) {
		mf = new MapFile();
		ASSERT(mf);
		int rval = mf->ParseCanonicalizationFile(filename);
		if (rval < 0) {
			return rval;
		}
	}
	MapHolder * pmh = &((*g_user_maps)[mapname]);
	pmh->filename = filename;
	pmh->file_timestamp = ts;
	pmh->mf = mf;
	return 0;
}

int add_user_mapping(const char * mapname, char * mapdata)
{
	MapFile * mf = new MapFile();
	MyStringCharSource src(mapdata, false);
	mf->ParseCanonicalization(src, mapname);
	return add_user_map(mapname, NULL, mf);
}


int delete_user_map(const char * mapname)
{
	if ( ! g_user_maps) return 0;
	STRING_MAPS::iterator found = g_user_maps->find(mapname);
	if (found != g_user_maps->end()) {
		g_user_maps->erase(found);
		return 1;
	}
	return 0;
}

// map the input string using the given mapname
// if the mapname contains a . it is treated as mapname.method
// otherise the method is "*" which should match all methods.
// return is true if the mapname exists and mapping was found within it, false if not.
bool user_map_do_mapping(const char * mapname, const char * input, MyString & output) {
	if ( ! g_user_maps) return false;

	std::string name(mapname);
	// split input mapname into name and method if it contains a '.'
	const char * method = strchr(mapname, '.');
	if (method) {
		name.erase(method-mapname, std::string::npos);
		++method;
	} else {
		method = "*";
	}
	STRING_MAPS::iterator found = g_user_maps->find(name);
	if (found != g_user_maps->end()) {
		MapHolder * pmh = &found->second;
		if (pmh->mf) {
			return pmh->mf->GetCanonicalization(method, input, output) >= 0;
		}
	}
	return false;
}


