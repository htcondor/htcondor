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
DROP TABLE files CASCADE CONSTRAINTS;
DROP TABLE fileusages;
DROP SEQUENCE condor_seqfileid;
DROP TABLE machines_vertical;
DROP TABLE machines_vertical_history;
DROP TABLE machines_horizontal CASCADE CONSTRAINTS;
DROP TABLE machines_horizontal_history CASCADE CONSTRAINTS;
DROP TABLE clusterads_vertical CASCADE CONSTRAINTS;
DROP TABLE clusterads_horizontal CASCADE CONSTRAINTS;
DROP TABLE procads_vertical CASCADE CONSTRAINTS;
DROP TABLE procads_horizontal CASCADE CONSTRAINTS;
DROP TABLE jobs_vertical_history CASCADE CONSTRAINTS;
DROP TABLE jobs_horizontal_History CASCADE CONSTRAINTS;
DROP TABLE runs CASCADE CONSTRAINTS;
DROP SEQUENCE seqrunid;
DROP TABLE rejects;
DROP TABLE matches;
DROP VIEW agg_user_jobs_waiting;
DROP VIEW agg_user_jobs_held;
DROP VIEW agg_user_jobs_running;
DROP VIEW agg_user_jobs_fin_last_day;
DROP TABLE l_jobstatus;
DROP TABLE throwns;
DROP TABLE events;
DROP TABLE l_eventtype;
DROP TABLE generic_messages;
DROP TABLE jobqueuepollinginfo CASCADE CONSTRAINTS;
DROP TABLE currencies;
DROP TABLE daemons_horizontal;
DROP TABLE daemons_horizontal_history;
DROP TABLE daemons_vertical;
DROP TABLE daemons_vertical_history;
DROP TABLE submitters_horizontal;
DROP TABLE submitters_horizontal_history;
DROP VIEW history_jobs_flocked_in;
DROP VIEW current_jobs_flocked_in;
DROP VIEW history_jobs_flocked_out;
DROP VIEW current_jobs_flocked_out;
DROP TABLE dummy_single_row_table;
DROP TABLE error_sqllogs CASCADE CONSTRAINTS;
DROP PROCEDURE quill_purgehistory;
DROP TABLE history_jobs_to_purge;
DROP TABLE quilldbmonitor;
DROP TABLE quill_schema_version;
DROP TABLE maintenance_log;
DROP TABLE maintenance_events;
