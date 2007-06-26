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

#include "condor_ttdb.h"
#include <time.h>
#include "misc_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "MyString.h"
#if HAVE_ORACLE
#include "oracledatabase.h"
#endif

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
#if HAVE_ORACLE
	case T_ORACLE:
		rv.sprintf("TO_TIMESTAMP_TZ('%s', '%s')", tsv, 
				   QUILL_ORACLE_TIMESTAMP_FORAMT);
		break;
#endif

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
	case T_ORACLE:
		rv = tsv;
		break;
	default:
		break;
	}

	return rv;
}

MyString condor_ttdb_buildseq(dbtype dt, char *seqName)
{
	MyString rv;

	switch(dt) {
	case T_ORACLE:
		rv.sprintf("%s.nextval", seqName);
		break;
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
	case T_ORACLE:
		rv.sprintf(" and ROWNUM <= 1");
		break;
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
			} else {
					/* other database only include oracle, which doesn't
					   need to escape backslash */
				newstr += str[i];
			}
            break;
        case '\'':
				/* both oracle and postgres can escape a single quote with 
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
	case T_ORACLE:
		rv.sprintf("dbms_lob.compare(%s, '%s') != 0", col_nam, literal);
		break;
	case T_PGSQL:
		rv.sprintf("%s != '%s'", col_nam, literal);
		break;
	default:
		break;
	}

	return rv;
}
