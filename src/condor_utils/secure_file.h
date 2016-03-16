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
bool write_secure_file(const char* path, const void* data, size_t len, bool as_root);
bool read_secure_file(const char *fname, void **buf, size_t *len, bool as_root);

#endif // SECURE_FILE_H

