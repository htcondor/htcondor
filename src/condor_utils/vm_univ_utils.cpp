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
#include "condor_debug.h"
#include "condor_config.h"
#include "basename.h"
#include "directory.h"
#include "condor_attributes.h"
#include "setenv.h"
#include "vm_univ_utils.h"

// Removes leading/tailing single(') or double(") quote
std::string 
delete_quotation_marks(const char *value)
{
	std::string fixedvalue;

	if( !value || (value[0] == '\0')) {
		return fixedvalue;
	}

	char *tmpvalue = strdup(value);
	char *ptr = tmpvalue;

	// Delete leading ones
	while( ( *ptr == '\"') || (*ptr == '\'' ) ) {
		*ptr = ' ';
		ptr++;
	}

	ptr = tmpvalue + strlen(tmpvalue) - 1;
	// Delete tailing ones
	while( ( ptr > tmpvalue ) && 
			( ( *ptr == '\"') || (*ptr == '\'' ) )) {
		*ptr = ' ';
		ptr--;
	}
		   
	fixedvalue = tmpvalue;
	trim(fixedvalue);
	free(tmpvalue);
	return fixedvalue;
}

// Create the list of all files in the given directory
void 
find_all_files_in_dir(const char *dirpath, std::vector<std::string> &file_list, bool use_fullname)
{
	Directory dir(dirpath);

	file_list.clear();

	const char *f = NULL;

	dir.Rewind();
	while( (f=dir.Next()) ) {

		if( dir.IsDirectory() ) {
			continue;
		}

		if( use_fullname ) {
			file_list.emplace_back(dir.GetFullPath());
		}else {
			file_list.emplace_back(f);
		}
	}
	return;
}

// This checks if filename is in the given file_list.
// If use_base is true, we will compare two files with basenames. 
bool
filelist_contains_file(const char *filename, const std::vector<std::string> &file_list, bool use_base)
{
	if( !filename ) {
		return false;
	}

	if( !use_base ) {
		return contains(file_list, filename);
	}

	const char* file_basename = condor_basename(filename);
	for (const auto& tmp_file : file_list) {
		if( strcmp(file_basename,
		           condor_basename(tmp_file.c_str())) == MATCH ) {
			return true;
		}
	}
	return false;
}

void
parse_param_string(const char *line, std::string &name, std::string &value, bool del_quotes)
{
	std::string one_line;
	size_t pos=0;

	name = "";
	value = "";

	if( !line || (line[0] == '\0') ) {
		return;
	}

	one_line = line;
	chomp(one_line);
	pos = one_line.find('=');
	if( pos == 0 || pos == std::string::npos ) {
		return;
	}

	name = one_line.substr(0, pos);
	if( pos == (one_line.length() - 1) ) {
		value = "";
	}else {
		value = one_line.substr( pos+1 );
	}

	trim(name);
	trim(value);

	if( del_quotes ) {
		value = delete_quotation_marks(value.c_str());
	}
	return;
}

bool 
create_name_for_VM(ClassAd *ad, std::string& vmname)
{
	if( !ad ) {
		return false;
	}

	int cluster_id = 0;
	if( ad->LookupInteger(ATTR_CLUSTER_ID, cluster_id) != 1 ) {
		dprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
				ATTR_CLUSTER_ID); 
		return false;
	}

	int proc_id = 0;
	if( ad->LookupInteger(ATTR_PROC_ID, proc_id) != 1 ) {
		dprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
				ATTR_PROC_ID); 
		return false;
	}

	std::string stringattr;
	if( ad->LookupString(ATTR_USER, stringattr) != 1 ) {
		dprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
				ATTR_USER); 
		return false;
	}

	// replace '@' with '_'
	size_t pos = std::string::npos;
	while( (pos = stringattr.find("@") ) != std::string::npos ) {
		stringattr[pos] = '_';
	}

	formatstr( vmname, "%s_%d.%d", stringattr.c_str(), cluster_id, proc_id );
	return true;
}
