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


#ifndef VM_UNIV_UTILS_H
#define VM_UNIV_UTILS_H

#include "condor_common.h"
#include "condor_classad.h"

// Removes leading/tailing single(') or double(") quote
std::string delete_quotation_marks(const char *value);

// Create the list of all files in the directory 'dirpath'
void find_all_files_in_dir(const char *dirpath, std::vector<std::string> &file_list, bool use_fullname);

// Checks if filename is in the given file_list.
// If use_base is true, we will compare two files with basenames. 
bool filelist_contains_file(const char *filename, const std::vector<std::string> &file_list, bool use_base);

// Parse the string like "Name = Value" or "Name=Value"
void parse_param_string(const char *line, std::string &name, std::string &value, bool del_quotes);

// Create name for virtual machine
// name consists of user + cluster id + proc id
bool create_name_for_VM(ClassAd *ad, std::string& vmname);

#endif /* VM_UNIV_UTILS_H */
