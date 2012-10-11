/*
 * Copyright 2009-2012 Red Hat, Inc.
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

#ifndef _ODS_PROCESSORS_H
#define _ODS_PROCESSORS_H

#include "compat_classad.h"
#include "ODSMongodbOps.h"

namespace plumage {
namespace etl {

// stat processors
void processSubmitterStats(plumage::etl::ODSMongodbOps* conn, mongo::Date_t& ts);
void processMachineStats(plumage::etl::ODSMongodbOps* conn, mongo::Date_t& ts);
void processSchedulerStats(plumage::etl::ODSMongodbOps* conn, mongo::Date_t& ts);
void processAccountantStats(compat_classad::ClassAd* ad, plumage::etl::ODSMongodbOps* conn, mongo::Date_t& ts);

// history file processors
void initHistoryFiles();
void processHistoryDirectory();
void processCurrentHistory(bool force_reset = false);


}}

#endif /* _ODS_PROCESSORS_H */
