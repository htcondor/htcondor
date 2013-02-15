/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "compat_classad_util.h"
#include "condor_attributes.h"
#include "condor_qmgr.h"
#include "get_daemon_name.h"

// c++ includes
#include <map>

// axis
#include "axutil_log.h"

// local includes
#include "AviaryUtils.h"
#include "AviaryCommon_Attribute.h"
#include "AviaryCommon_AttributeType.h"

using namespace std;
using namespace compat_classad;
using namespace aviary::codec;
using namespace AviaryCommon;

const char* RESERVED[] = {"error", "false", "is", "isnt", "parent", "true","undefined", NULL};
#define AVIARY_UTILS_FIXBUF 500

typedef std::map<std::string,int> LogLevelMapType;
LogLevelMapType log_level_map;

int
aviary::util::getLogLevel() {
	char *tmp = NULL;

	if (log_level_map.empty()) {
		log_level_map["AXIS2_LOG_LEVEL_CRITICAL"] = AXIS2_LOG_LEVEL_CRITICAL;
		log_level_map["AXIS2_LOG_LEVEL_ERROR"] = AXIS2_LOG_LEVEL_ERROR;
		log_level_map["AXIS2_LOG_LEVEL_WARNING"] = AXIS2_LOG_LEVEL_WARNING;
		log_level_map["AXIS2_LOG_LEVEL_INFO"] = AXIS2_LOG_LEVEL_INFO;
		log_level_map["AXIS2_LOG_LEVEL_DEBUG"] = AXIS2_LOG_LEVEL_DEBUG;
		log_level_map["AXIS2_LOG_LEVEL_USER"] = AXIS2_LOG_LEVEL_USER;
		log_level_map["AXIS2_LOG_LEVEL_TRACE"] = AXIS2_LOG_LEVEL_TRACE;
	}

	tmp = param("AXIS2_DEBUG_LEVEL");
	if (!tmp) {
		return AXIS2_LOG_LEVEL_CRITICAL;
	}

	LogLevelMapType::iterator it = log_level_map.find(tmp);
	free(tmp); tmp = NULL;

	if (it != log_level_map.end()) {
		return (*it).second;
	}

	return AXIS2_LOG_LEVEL_CRITICAL;
}

string
aviary::util::getPoolName()
{
    string poolName;
    char *tmp = NULL;

    tmp = param("COLLECTOR_HOST");
    if (!tmp) {
        tmp = strdup("NO COLLECTOR_HOST, NOT GOOD");
    }
    poolName = tmp;
    free(tmp); tmp = NULL;

    return poolName;
}

string
aviary::util::getScheddName() {
	string scheddName;
	char* tmp = NULL;

	tmp = param("SCHEDD_NAME");
	if (!tmp) {
		scheddName = default_daemon_name();
	} else {
		scheddName = build_valid_daemon_name(tmp);
		free(tmp); tmp = NULL;
	}

	return scheddName;
}

// cleans up the quoted values from the job log reader
string aviary::util::trimQuotes(const char* str) {
	string val = str;

	size_t endpos = val.find_last_not_of("\\\"");
	if( string::npos != endpos ) {
		val = val.substr( 0, endpos+1 );
	}
	size_t startpos = val.find_first_not_of("\\\"");
	if( string::npos != startpos ) {
		val = val.substr( startpos );
	}

	return val;
}

// validate that an incoming group/user name is
// alphanumeric, underscores, or a dot separator
bool aviary::util::isValidGroupUserName(const string& _name, string& _text) {
	const char* ptr = _name.c_str();
	while( *ptr ) {
		char c = *ptr++;
		if (	('a' > c || c > 'z') &&
			('A' > c || c > 'Z') &&
			('0' > c || c > '9') &&
			(c != '_' ) &&
			(c != '.' ) ) {
			_text = "Invalid name for group/user - alphanumeric, underscore and dot characters only";
			return false;
		}
	}
	return true;
}

// validate that an incoming attribute name is
// alphanumeric, or underscores
bool aviary::util::isValidAttributeName(const string& _name, string& _text) {
	const char* ptr = _name.c_str();
	while( *ptr ) {
		char c = *ptr++;
		if (	('a' > c || c > 'z') &&
			('A' > c || c > 'Z') &&
			('0' > c || c > '9') &&
			(c != '_' ) ) {
			_text = "Invalid name for attribute - alphanumeric and underscore characters only";
			return false;
		}
	}
	return true;
}

bool aviary::util::checkRequiredAttrs(compat_classad::ClassAd& ad, const char* attrs[], string& missing) {
	bool status = true;
	int i = 0;

        while (NULL != attrs[i]) {
		if (!ad.Lookup(attrs[i])) {
			missing += " "; missing += attrs[i];
			status = false;
		}
		i++;
	}
	return status;
}

// checks if the attribute name is reserved
bool aviary::util::isKeyword(const char* kw) {
	int i = 0;
	while (NULL != RESERVED[i]) {
		if (strcasecmp(kw,RESERVED[i]) == 0) {
			return true;
		}
		i++;
	}
	return false;
}

// checks to see if ATTR_JOB_SUBMISSION is being changed post-submission
bool aviary::util::isSubmissionChange(const char* attr) {
	if (strcasecmp(attr,ATTR_JOB_SUBMISSION)==0) {
		return true;
	}
	return false;
}

// unfortunately no convenience functions from WS02 for dateTime
axutil_date_time_t* aviary::util::encodeDateTime(const time_t& ts, const axutil_env_t* env) {
    struct tm the_tm;

    // need the re-entrant version because axutil_date_time_create
    // calls time() again and overwrites static tm
    localtime_r(&ts,&the_tm);

    axutil_date_time_t* time_value = NULL;
    time_value = axutil_date_time_create(env);

    if (!time_value)
    {
        return NULL;
    }

    // play their game with adjusting the year and month offset
    axutil_date_time_set_date_time(time_value,env,
                                   the_tm.tm_year+1900,
                                   the_tm.tm_mon+1,
                                   the_tm.tm_mday,
                                   the_tm.tm_hour,
                                   the_tm.tm_min,
                                   the_tm.tm_sec,
                                   0);
    return time_value;
};

void aviary::util::mapToXsdAttributes(const aviary::codec::AttributeMapType& _map, AviaryCommon::Attributes* _attrs) {
    for (AttributeMapIterator i = _map.begin(); _map.end() != i; i++) {
        AviaryAttribute* codec_attr = (AviaryAttribute*)(*i).second;
        AviaryCommon::Attribute* attr = new AviaryCommon::Attribute;
        attr->setName((*i).first);
        AviaryCommon::AttributeType* attr_type = new AviaryCommon::AttributeType(AviaryCommon::AttributeType_UNDEFINED);
        if (codec_attr) {
            switch (codec_attr->getType()) {
                case AviaryAttribute::INTEGER_TYPE:
                    attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_INTEGER);
                    break;
                case AviaryAttribute::FLOAT_TYPE:
                    attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_FLOAT);
                    break;
                case AviaryAttribute::STRING_TYPE:
                    attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_STRING);
                    break;
                case AviaryAttribute::EXPR_TYPE:
                    attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_EXPRESSION);
                    break;
                default:
                    attr_type->setAttributeTypeEnum(AviaryCommon::AttributeType_UNDEFINED);
            }
            attr->setType(attr_type);
            attr->setValue(codec_attr->getValue());
        }
        else {
            //unknown/undefined attribute
            attr->setValue("UNDEFINED");
        }
        _attrs->addAttrs(attr);
    }
}

// too much pain with the 7.8/7.9 sprintf rename
// make our own locally solely for std::string
int aviary::util::aviUtilFmt(std::string& s, const char* format, ...) {
    char fixbuf[AVIARY_UTILS_FIXBUF];
    const int fixlen = sizeof(fixbuf)/sizeof(fixbuf[0]);
    int n;
    va_list  args,pargs;
    va_start(pargs, format);

    // Attempt to write to fixed buffer.  condor_snutils.{h,cpp}
    // provides an implementation of vsnprintf() in windows, so this
    // logic works cross platform 
#if !defined(va_copy)
    n = vsnprintf(fixbuf, fixlen, format, pargs);    
#else
    va_copy(args, pargs);
    n = vsnprintf(fixbuf, fixlen, format, args);
    va_end(args);
#endif

    // In this case, fixed buffer was sufficient so we're done.
    // Return number of chars written.
    if (n < fixlen) {
        s = fixbuf;
        return n;
    }

    // Otherwise, the fixed buffer was not large enough, but return from 
    // vsnprintf() tells us how much memory we need now.
    n += 1;
    char* varbuf = NULL;
    // Handle 'new' behavior mode of returning NULL or throwing exception
    try {
        varbuf = new char[n];
    } catch (...) {
        varbuf = NULL;
    }
    if (NULL == varbuf) EXCEPT("Failed to allocate char buffer of %d chars", n);

    // re-print, using buffer of sufficient size
#if !defined(va_copy)
    int nn = vsnprintf(varbuf, n, format, pargs);
#else
    va_copy(args, pargs);
    int nn = vsnprintf(varbuf, n, format, args);
    va_end(args);
#endif

    // Sanity check.  This ought not to happen.  Ever.
    if (nn >= n) EXCEPT("Insufficient buffer size (%d) for printing %d chars", n, nn);

    // safe to do string assignment
    s = varbuf;

    // clean up our allocated buffer
    delete[] varbuf;
    
    va_end(args);

    // return number of chars written
    return nn;
}
