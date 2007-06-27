/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef VM_UNIV_UTILS_H
#define VM_UNIV_UTILS_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "string_list.h"

// Removes leading/tailing single(') or double(") quote
MyString delete_quotation_marks(const char *value);

/*
  Find all suffix-matched files in the directory 'dirpath'.
  Suffix is case-insensitive.
  If there is at least one matched file, this returns true.
*/
bool suffix_matched_files_in_dir(const char *dirpath, StringList &file_list, const char *suffix, bool use_fullname);

// Create the list of all files in the directory 'dirpath'
void find_all_files_in_dir(const char *dirpath, StringList &file_list, bool use_fullname);

// Checks if filename is in the given file_list.
// If use_base is true, we will compare two files with basenames. 
bool filelist_contains_file(const char *filename, StringList *file_list, bool use_base);

// delete all files in the given filelist.
void delete_all_files_in_filelist(StringList *file_list);

// Check whether a file has the given suffix.
// Filename will be compared with the suffix string from the end.
// suffix is case-insensitive
bool has_suffix(const char *filename, const char *suffix);

// Parse the string like "Name = Value" or "Name=Value"
void parse_param_string(const char *line, MyString &name, MyString &value, bool del_quotes);

// Create name for virtual machine
// name consists of user + cluster id + proc id
bool create_name_for_VM(ClassAd *ad, MyString& vmname);

// put a temporary value into current environment
// Old value will stored into oldValue
bool set_temporary_env(const char *envName, const char* envValue, MyString *oldValue);

// If prog has setuid-root, true will be returned.
bool has_root_setuid(const char *prog);
#endif /* VM_UNIV_UTILS_H */
