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

#ifndef _ODS_DBNAMES_H
#define _ODS_DBNAMES_H

namespace plumage {
namespace etl {

const char DB_RAW_ADS[] = "condor_raw.ads";
const char DB_STATS_SAMPLES[] = "condor_stats.samples";
const char DB_STATS_SAMPLES_SUB[] = "condor_stats.samples.submitter";
const char DB_STATS_SAMPLES_MACH[] = "condor_stats.samples.machine";
const char DB_STATS_SAMPLES_SCHED[] = "condor_stats.samples.scheduler";
const char DB_STATS_SAMPLES_ACCOUNTANT[] = "condor_stats.samples.accountant";
const char DB_JOBS_QUEUE[] = "condor_jobs.queue";
const char DB_JOBS_HISTORY[] = "condor_jobs.history";
}}

#endif /* _ODS_DBNAMES_H */
