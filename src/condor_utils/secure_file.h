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


#ifndef SECURE_FILE_H
#define SECURE_FILE_H

#include "condor_common.h"

void simple_scramble(char* scrambled,  const char* orig, int len);
int write_password_file(const char* path, const char* password);
int write_binary_password_file(const char* path, const char* password, size_t password_len);
FILE* open_secure_file_for_write(const char* path, bool as_root, bool group_readable = false);
bool write_secure_file(const char* path, const void* data, size_t len, bool as_root, bool group_readable = false);

// write a secure temp file (using the function above), then rename/replace it over the given filename
//   path - file to replace
//   tmpext - appended to path to create the temp filename
bool replace_secure_file(const char* path, const char * tmpext, const void* data, size_t len, bool as_root, bool group_readable = false);

#define SECURE_FILE_VERIFY_NONE      0x00
#define SECURE_FILE_VERIFY_OWNER     0x01
#define SECURE_FILE_VERIFY_ACCESS    0x02
#define SECURE_FILE_VERIFY_ALL       0xFF
bool read_secure_file(const char *fname, void **buf, size_t *len, bool as_root, int verify_mode = SECURE_FILE_VERIFY_ALL);



#endif // SECURE_FILE_H

