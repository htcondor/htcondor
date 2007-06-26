/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
