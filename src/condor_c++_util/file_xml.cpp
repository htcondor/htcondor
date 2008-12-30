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
#include "condor_api.h"
#include "condor_config.h"
#include <errno.h>
#include <string.h>
#include "file_xml.h"
#include "subsystem_info.h"
#include <sys/stat.h>

#define FILESIZELIMT 1900000000L


AttrList *FILEXML::file_readAttrList() 
{
	AttrList *ad = 0;

    if (is_dummy) return ad;

	dprintf(D_ALWAYS, "file_readAttrList: Method not implemented for XML log files\n" );

	return ad;
}

QuillErrCode FILEXML::file_newEvent(const char * /*eventType*/, AttrList *info) {
	int retval = 0;
	struct stat file_status;
    int xml_log_limit;

    if (is_dummy) return QUILL_SUCCESS;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open");
		return QUILL_FAILURE;
	}

	if(file_lock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	fstat(outfiledes, &file_status);

		// only write to the log if it's not exceeding the log size limit
	xml_log_limit = param_integer("MAX_XML_LOG", FILESIZELIMT);
    if (file_status.st_size < xml_log_limit) {

        MyString temp("<event>\n");
        const char *tag;
        char *val_temp = NULL;

        info->ResetName();
        while (NULL != (tag = info->NextNameOriginal())) {
            temp += "\t<";
            temp += tag;
            temp += ">";
            ExprTree *value = info->Lookup(tag);
			if (value->RArg()) 
				value->RArg()->PrintToNewStr(&val_temp);
			else 
				val_temp = strdup("NULL");
            temp += val_temp;
            temp += "</";
            temp += tag;
            temp += ">\n";
            free(val_temp);
        }
        temp += "</event>\n";

        retval = write(outfiledes, temp.Value(), temp.Length());
	}
	
	if(file_unlock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	if (retval < 0) {
		return QUILL_FAILURE;
	} else {
		return QUILL_SUCCESS;	
	}
}

QuillErrCode FILEXML::file_updateEvent(const char * /*eventType*/, 
									   AttrList * /*info*/, 
									   AttrList * /*condition*/) {

    if (is_dummy) return QUILL_SUCCESS;

	dprintf(D_ALWAYS,"file_updateEvent: Method not implemented for XML log files\n");
	return QUILL_FAILURE;


}

#if 0
QuillErrCode FILEXML::file_deleteEvent(const char *eventType, 
									   AttrList *condition) {

	dprintf(D_ALWAYS,"file_deleteEvent: Method not implemented for XML log files\n");
	return QUILL_FAILURE;

}
#endif

/* We put XMLObj definition here because almost everything uses XMLObj. This way we 
 * won't get the XMLObj undefined error during compilation of any code which 
 * needs cplus_lib.a. Notice the XMLObj is just a pointer, the real object 
 * should be created only when the process wants to dump XML logs. E.g. 
 * most daemons have data which we want to dump as XML, hence we can create an 
 * instance of XMLObj in the  main function of daemon process.
 */

FILEXML *XMLObj = NULL;

/*static */FILEXML *
FILEXML::createInstanceXML() { 
	FILEXML *ptr = NULL;
	char *tmp, *outfilename;
	char *tmpParamName;
	const char *daemon_name;
    bool want_xml_log = false;

    want_xml_log = param_boolean("WANT_XML_LOG", false);

    if ( ! want_xml_log ) {  // XML Logging is turned off, hence return a dummy FILEXML Object
        ptr = new FILEXML();
        return ptr;
    }
	daemon_name = get_mySubSystem()->getName();

	tmpParamName = (char *)malloc(10+strlen(daemon_name));

		/* build parameter name based on the daemon name */
	sprintf(tmpParamName, "%s_XMLLOG", daemon_name);
	tmp = param(tmpParamName);
	free(tmpParamName);

	if( tmp ) {
		outfilename = tmp;
	}
	else {
		tmp = param ("LOG");		

		if (tmp) {
			outfilename = (char *)malloc(strlen(tmp) + 12);
			sprintf(outfilename, "%s/Events.xml", tmp);
			free(tmp);
		}
		else {
			outfilename = (char *)malloc(11);
			sprintf(outfilename, "Events.xml");
		}
	}

	ptr = new FILEXML(outfilename, O_WRONLY|O_CREAT|O_APPEND, true);

	free(outfilename);

	if (ptr->file_open() == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "FILEXML createInstance failed\n");
	}

	return ptr;
}

