
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

