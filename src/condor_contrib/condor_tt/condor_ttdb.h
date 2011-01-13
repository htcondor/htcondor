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


#ifndef _CONDOR_TTDB_H_
#define _CONDOR_TTDB_H_

#include "condor_common.h"
#include "MyString.h"
#include <time.h>
#include "quill_enums.h"

MyString condor_ttdb_buildts(time_t *tv, dbtype dt);
MyString condor_ttdb_buildtsval(time_t *tv, dbtype dt);
MyString condor_ttdb_buildseq(dbtype dt, char *seqName);
MyString condor_ttdb_onerow_clause(dbtype dt);
MyString condor_ttdb_fillEscapeCharacters(const char * str, dbtype dt);
MyString condor_ttdb_compare_clob_to_lit(dbtype dt, const char* col_nam, const char* literal);

#endif /* _CONDOR_TTDB_H_ */
