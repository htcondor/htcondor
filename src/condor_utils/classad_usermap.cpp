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
#include "subsystem_info.h"
#include "MapFile.h"


// userMap stuff
class MapHolder {
public:
	std::string filename;
	time_t   file_timestamp; // last modify time of the file 
	MapFile * mf;
	MapHolder(MapFile * _mf=NULL) : file_timestamp(0), mf(_mf) {}
	~MapHolder() { delete mf; mf = NULL; }
};
typedef std::map<std::string, MapHolder, classad::CaseIgnLTStr> STRING_MAPS;
static STRING_MAPS * g_user_maps = NULL;

void clear_user_maps(std::vector<std::string> * keep_list) {
	if ( ! g_user_maps) return;
	if ( ! keep_list || keep_list->empty()) {
		g_user_maps->clear();
		return;
	}
	for (STRING_MAPS::iterator it = g_user_maps->begin(); it != g_user_maps->end(); /* advance in the loop */) {
		STRING_MAPS::iterator tmp = it++;
		// remove the map if it is not in the keep list
		if ( ! contains_anycase(*keep_list, tmp->first)) {
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
		// map exists, and the new map is to be read from a file, and 
		// the file timestamp is the same as when we originally read it
		// then skip reloading the file.
		MapHolder * pmh = &found->second;
		if (filename && !mf && (pmh->filename == filename)) {
			// if the filename is the same, and the modify time is also the same
			// skip reloading the file. 
			time_t ts = get_file_timestamp(filename);
			if (ts && ts == pmh->file_timestamp) {
				return 0; // map already loaded.
			}
		}
		// we have an entry, but we want to reload it. so delete the old one.
		g_user_maps->erase(found);
	}

	// if mapfile was not supplied, load it now.
	// note that this code assumes that either filename will be non-null or
	// a MapFile class will be passed in.
	time_t ts = filename ? get_file_timestamp(filename) : 0;
	dprintf (D_ALWAYS, "Loading classad userMap '%s' ts=%lld from %s\n", mapname, (long long)ts, filename ? filename : "knob");
	if ( ! mf) {
		ASSERT(filename);

		mf = new MapFile();
		ASSERT(mf);

		std::string parameter_name;
		formatstr( parameter_name, "CLASSAD_USER_MAP_PREFIX_%s", mapname );
		bool is_prefix_map = param_boolean( parameter_name.c_str(), false );
		int rval = mf->ParseCanonicalizationFile(filename, true, true, is_prefix_map);
		if (rval < 0) {
			dprintf(D_ALWAYS, "PARSE ERROR %d in classad userMap '%s' from file %s\n", rval, mapname, filename);
			delete mf;
			return rval;
		}
	}
	MapHolder * pmh = &((*g_user_maps)[mapname]);
	pmh->filename = filename ? filename : "";
	pmh->file_timestamp = ts;
	pmh->mf = mf;
	return 0;
}

int add_user_mapping(const char * mapname, const char * mapdata)
{
	MapFile * mf = new MapFile();
	// TODO Modify MyStringCharSource to take a const char* and remove this cast
	MyStringCharSource src(mapdata);
//	MyStringCharSource src(const_cast<char*>(mapdata), false);
	std::string parameter_name;
	formatstr( parameter_name, "CLASSAD_USER_MAP_PREFIX_%s", mapname );
	bool is_prefix_map = param_boolean( parameter_name.c_str(), false );
	int rval = mf->ParseCanonicalization(src, mapname, true, true, is_prefix_map);
	if (rval < 0) {
		dprintf(D_ALWAYS, "PARSE ERROR %d in classad userMap '%s' from knob\n", rval, mapname);
	} else {
		rval = add_user_map(mapname, NULL, mf);
	}
	if (rval < 0) { delete mf; }
	return rval;
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

// load standard usermaps from standard configuration knobs
// note that if a mapfile has already been loaded, then on 
// reconfig this code will only reread the file if it's modify
// time is more recent than the time it was last read.
//
int reconfig_user_maps()
{
	int cMaps = 0;

	const char * subsys = get_mySubSystem()->getName();
	if ( ! subsys) {
		// no subsys? leave usermaps alone...
		if (g_user_maps) {
			return (int)g_user_maps->size();
		}
		return 0;
	}

	std::string param_name(subsys); param_name += "_CLASSAD_USER_MAP_NAMES";
	std::string user_map_names;
	if (param(user_map_names, param_name.c_str())) {
		std::vector<std::string> names = split(user_map_names);

		// clear user maps that are no longer in the names list
		clear_user_maps(&names);

		// load/refresh the user maps that are in the list
		std::string user_map;
		for (auto& name: names) {
			param_name = "CLASSAD_USER_MAPFILE_"; param_name += name;
			if (param(user_map, param_name.c_str())) {
				add_user_map(name.c_str(), user_map.c_str(), nullptr);
			} else {
				param_name = "CLASSAD_USER_MAPDATA_"; param_name += name;
				if (param(user_map, param_name.c_str())) {
					add_user_mapping(name.c_str(), user_map.c_str());
				}
			}
		}

		// we will return the number of active maps
		if (g_user_maps) { cMaps = (int)g_user_maps->size(); }
	} else {
		clear_user_maps(NULL);
		cMaps = 0;
	}
	return cMaps;
}


// map the input string using the given mapname
// if the mapname contains a . it is treated as mapname.method
// otherise the method is "*" which should match all methods.
// return is true if the mapname exists and mapping was found within it, false if not.
bool user_map_do_mapping(const char * mapname, const char * input, std::string & output) {
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


