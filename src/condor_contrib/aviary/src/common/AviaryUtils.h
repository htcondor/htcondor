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

#ifndef _AVIARYUTILS_H
#define _AVIARYUTILS_H

// condor includes
#include "condor_classad.h"

// wso2 include
#include "AviaryCommon_Attributes.h"

// axis includes
#include "axutil_date_time.h"

// local includes
#include "ClassadCodec.h"

using namespace std;
using namespace compat_classad;

namespace aviary {
namespace util {

const char SEPARATOR[] = "#";

int getLogLevel();

string getPoolName();

string getScheddName();

string trimQuotes(const char* value);

bool isValidGroupUserName(const string& _name, string& _text);

bool isValidAttributeName(const string& _name, string& _text);

bool checkRequiredAttrs(ClassAd& ad, const char* attrs[], string& missing);

bool isKeyword(const char* kw);

bool isSubmissionChange(const char* attr);

axutil_date_time_t* encodeDateTime(const time_t& ts, const axutil_env_t* env);

void mapToXsdAttributes(const aviary::codec::AttributeMapType& _map, AviaryCommon::Attributes* _attrs);

int aviUtilFmt(std::string& s, const char* format, ...);

}}

#endif /* _AVIARYUTILS_H */
