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
#include "condor_ttdb.h"
#include <time.h>
#include "misc_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "MyString.h"

MyString condor_ttdb_buildts(time_t *tv, dbtype dt)
{
	char tsv[100];
	struct tm *tm;	
	MyString rv;

	tm = localtime(tv);		

	snprintf(tsv, 100, "%d/%d/%d %02d:%02d:%02d %s", 
			 tm->tm_mon+1,
			 tm->tm_mday,
			 tm->tm_year+1900,
			 tm->tm_hour,
			 tm->tm_min,
			 tm->tm_sec,
			 my_timezone(tm->tm_isdst));	

	switch(dt) {
	case T_PGSQL:
		rv.sprintf("'%s'", tsv);		
		break;
	default:
		break;
	}

	return rv;
}

MyString condor_ttdb_buildtsval(time_t *tv, dbtype dt)
{
	char tsv[100];
	struct tm *tm;	
	MyString rv;

	tm = localtime(tv);		

	snprintf(tsv, 100, "%d/%d/%d %02d:%02d:%02d %s", 
			 tm->tm_mon+1,
			 tm->tm_mday,
			 tm->tm_year+1900,
			 tm->tm_hour,
			 tm->tm_min,
			 tm->tm_sec,
			 my_timezone(tm->tm_isdst));	

	switch(dt) {
	default:
		break;
	}

	return rv;
}

MyString condor_ttdb_buildseq(dbtype dt, char *seqName)
{
	MyString rv;

	switch(dt) {
	case T_PGSQL:
		rv.sprintf("nextval('%s')", seqName);		
		break;
	default:
		break;
	}

	return rv;
}

MyString condor_ttdb_onerow_clause(dbtype dt)
{
	MyString rv;

	switch(dt) {
	case T_PGSQL:
		rv.sprintf(" LIMIT 1");
		break;
	default:
		break;
	}

	return rv;	
}

MyString condor_ttdb_fillEscapeCharacters(const char * str, dbtype dt) {
	int i;
	
	int len = strlen(str);

	MyString newstr;
        
	for (i = 0; i < len; i++) {
		switch(str[i]) {
        case '\\':
			if (dt == T_PGSQL) {
					/* postgres need to escape backslash */
				newstr += '\\';
				newstr += '\\';
			}
            break;
        case '\'':
				/* postgres can escape a single quote with 
				   another single quote */
            newstr += '\'';
            newstr += '\'';
            break;
        default:
            newstr += str[i];
            break;
		}
	}
    return newstr;
}


MyString condor_ttdb_compare_clob_to_lit(dbtype dt, const char* col_nam, const char* literal)
{
	MyString rv;

	switch(dt) {
	case T_PGSQL:
		rv.sprintf("%s != '%s'", col_nam, literal);
		break;
	default:
		break;
	}

	return rv;
}
