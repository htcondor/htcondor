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

DROP TABLE cdb_users;
DROP TABLE transfers;
DROP TABLE files CASCADE;
DROP TABLE fileusages;
DROP SEQUENCE condor_seqfileid;
DROP TABLE machines_vertical;
DROP TABLE machines_vertical_history;
DROP TABLE machines_horizontal CASCADE;
DROP TABLE machines_horizontal_history CASCADE;
DROP TABLE clusterads_vertical CASCADE;
DROP TABLE clusterads_horizontal CASCADE;
DROP TABLE procads_vertical CASCADE;
DROP TABLE procads_horizontal CASCADE;
DROP TABLE jobs_vertical_history CASCADE;
DROP TABLE jobs_horizontal_History CASCADE;
DROP TABLE runs CASCADE;
DROP SEQUENCE seqrunid;
DROP TABLE rejects;
DROP TABLE matches;
DROP TABLE l_jobstatus;
DROP TABLE throwns;
DROP TABLE events;
DROP TABLE l_eventtype;
DROP TABLE generic_messages;
DROP TABLE jobqueuepollinginfo CASCADE;
DROP TABLE currencies;
DROP TABLE daemons_horizontal;
DROP TABLE daemons_horizontal_history;
DROP TABLE daemons_vertical;
DROP TABLE daemons_vertical_history;
DROP TABLE submitters_horizontal;
DROP TABLE submitters_horizontal_history;
DROP TABLE dummy_single_row_table;
DROP TABLE error_sqllogs CASCADE;
DROP TABLE history_jobs_to_purge;
DROP TABLE quilldbmonitor;
DROP TABLE quill_schema_version;
DROP TABLE maintenance_log;
DROP TABLE maintenance_events;
DROP FUNCTION quill_purgehistory(resourceHistoryDuration integer, runHistoryDuration integer, jobHistoryDuration integer);
