/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "interpreter.h"
#include "condor_config.h"
#include "match_prefix.h"
#include "condor_classad.h"
#include "condor_debug.h"

int InterpreterFind( const char *language, char *interpreter )
{
	char *line;
	char lang[ATTRLIST_MAX_EXPRESSION];
	char lang_ver[ATTRLIST_MAX_EXPRESSION];
	char vendor[ATTRLIST_MAX_EXPRESSION];
	char vendor_ver[ATTRLIST_MAX_EXPRESSION];
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char interp[ATTRLIST_MAX_EXPRESSION];
	int fields;
	int pos;

	line = param("INTERPRETERS");
	if(!line) return 0;

	while(1) {
		fields = sscanf(line,"%s %s %s %s %s%n",lang,lang_ver,vendor,vendor_ver,interp,&pos);
		if( fields==0 ) {
			break;
		} else if( fields<5 ) {
			return 0;
		}

		sprintf(buffer,"%s%s%s%s",lang,lang_ver,vendor,vendor_ver);

		if( match_prefix(language,buffer) ) {
			strcpy(interpreter,interp);
			return 1;
		}

		line = &line[pos];
	}

	return 0;
}

int InterpreterPublish( ClassAd *ad )
{
	char *line;
	char lang[ATTRLIST_MAX_EXPRESSION];
	char lang_ver[ATTRLIST_MAX_EXPRESSION];
	char vendor[ATTRLIST_MAX_EXPRESSION];
	char vendor_ver[ATTRLIST_MAX_EXPRESSION];
	char interp[ATTRLIST_MAX_EXPRESSION];
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char temp[ATTRLIST_MAX_EXPRESSION];
	int fields;
	int pos;

	line = param("INTERPRETERS");
	if(!line) return 0;

	while(1) {
		fields = sscanf(line,"%s %s %s %s %s%n",lang,lang_ver,vendor,vendor_ver,interp,&pos);
		if( fields==0 ) {
			break;
		} else if( fields<5 ) {
			return 0;
		}

		sprintf(buffer,"%sInterpreter = \"%s\"",lang,interp);
		ad->Insert(buffer);

		sprintf(buffer,"%s%sInterpreter = \"%s\"",lang,lang_ver,interp);
		ad->Insert(buffer);

		sprintf(buffer,"%s%s%sInterpreter = \"%s\"",lang,lang_ver,vendor,interp);
		ad->Insert(buffer);

		sprintf(buffer,"%s%s%s%sInterpreter = \"%s\"",lang,lang_ver,vendor,vendor_ver,interp);
		ad->Insert(buffer);

		line = &line[pos];
	}

	return 1;	
}

