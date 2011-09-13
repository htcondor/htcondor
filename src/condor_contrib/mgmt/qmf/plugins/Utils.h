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

#ifndef _UTILS_H
#define _UTILS_H

#include <qpid/types/Variant.h>

#include "condor_classad.h"

std::string TrimQuotes(const char* value);

bool AddAttribute(compat_classad::ClassAd &ad, const char *name, qpid::types::Variant::Map &_map);

bool IsValidGroupUserName(const std::string& _name, std::string& _text);

bool IsValidParamName(const std::string& _name, std::string& _text);

bool IsValidAttributeName(const std::string& _name, std::string& _text);

bool CheckRequiredAttrs(compat_classad::ClassAd& ad, const char* attrs[], std::string& missing);

bool IsKeyword(const char* kw);

bool IsSubmissionChange(const char* attr);

bool PopulateVariantMapFromAd(compat_classad::ClassAd &ad, qpid::types::Variant::Map &_map);

bool PopulateAdFromVariantMap(qpid::types::Variant::Map &_map, compat_classad::ClassAd &ad, std::string& text);

#endif /* _UTILS_H */
