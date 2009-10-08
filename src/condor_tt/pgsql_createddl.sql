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

/*
Before installing this script, the following must have been prepared
1. quillreader account has been created
2. pl/pgSQL language has been created with "createlang plpgsql [dbname]"
*/

CREATE TABLE maintenance_events(
id                              INTEGER,
msg                             VARCHAR(4000),
PRIMARY KEY (id)
);

CREATE TABLE maintenance_log(
eventid                         integer REFERENCES maintenance_events(id),
eventts                         TIMESTAMP(3) WITH TIME ZONE,
eventdur                        INTERVAL
);

CREATE TABLE machines_vertical (
machine_id varchar(4000) NOT NULL,
attr       varchar(2000) NOT NULL, 
val        text, 
start_time timestamp(3) with time zone, 
Primary Key (machine_id, attr)
);

CREATE TABLE machines_vertical_history (
machine_id varchar(4000),
attr       varchar(4000), 
val        text, 
start_time timestamp(3) with time zone, 
end_time   timestamp(3) with time zone
);

CREATE TABLE clusterads_horizontal(
scheddname          varchar(4000) NOT NULL,
cluster_id             integer NOT NULL,
owner               varchar(30),
jobstatus           integer,
jobprio             integer,
imagesize           numeric(38),
qdate               timestamp(3) with time zone,
remoteusercpu       numeric(38),
remotewallclocktime numeric(38),
cmd                 text,
args                text,
jobuniverse         integer,
primary key(scheddname,cluster_id)
);

CREATE INDEX ca_h_i_owner ON clusterads_horizontal (owner);

CREATE TABLE procads_horizontal(
scheddname		varchar(4000) NOT NULL,
cluster_id	 	integer NOT NULL,
proc_id 			integer NOT NULL,
jobstatus 		integer,
imagesize 		numeric(38),
remoteusercpu	        numeric(38),
remotewallclocktime 	numeric(38),
remotehost              varchar(4000),
globaljobid        	varchar(4000),
jobprio            	integer,
args                    text,
shadowbday              timestamp(3) with time zone,
enteredcurrentstatus    timestamp(3) with time zone,
numrestarts             integer,
primary key(scheddname,cluster_id,proc_id)
);

CREATE TABLE jobs_horizontal_history (
scheddname   varchar(4000) NOT NULL,
scheddbirthdate     integer NOT NULL,
cluster_id              integer NOT NULL,
proc_id                 integer NOT NULL,
qdate                   timestamp(3) with time zone,
owner                   varchar(30),
globaljobid             varchar(4000),
numckpts                integer,
numrestarts             integer,
numsystemholds          integer,
condorversion           varchar(4000),
condorplatform          varchar(4000),
rootdir                 varchar(4000),
Iwd                     varchar(4000),
jobuniverse             integer,
cmd                     text,
minhosts                integer,
maxhosts                integer,
jobprio                 integer,
negotiation_user_name   varchar(4000),
env                     text,
userlog                 varchar(4000),
coresize                numeric(38),
killsig                 varchar(4000),
stdin	              	varchar(4000),
transferin              varchar(5),
stdout                  varchar(4000),
transferout             varchar(5),
stderr                  varchar(4000),
transfererr             varchar(5),
shouldtransferfiles     varchar(4000),
transferfiles           varchar(4000),
executablesize          numeric(38),
diskusage               integer,
filesystemdomain        varchar(4000),
args                    text,
lastmatchtime           timestamp(3) with time zone,
numjobmatches           integer,
jobstartdate            timestamp(3) with time zone,
jobcurrentstartdate     timestamp(3) with time zone,
jobruncount             integer,
filereadcount           numeric(38),
filereadbytes           numeric(38),
filewritecount          numeric(38),
filewritebytes          numeric(38),
fileseekcount           numeric(38),
totalsuspensions        integer,
imagesize               numeric(38),
exitstatus              integer,
localusercpu            numeric(38),
localsyscpu             numeric(38),
remoteusercpu           numeric(38),
remotesyscpu            numeric(38),
bytessent      	        numeric(38),
bytesrecvd              numeric(38),
rscbytessent            numeric(38),
rscbytesrecvd           numeric(38),
exitcode                integer,
jobstatus               integer,
enteredcurrentstatus    timestamp(3) with time zone,
remotewallclocktime     numeric(38),
lastremotehost          varchar(4000),
completiondate          timestamp(3) with time zone,
enteredhistorytable     timestamp(3) with time zone,
primary key		(scheddname,scheddbirthdate, cluster_id, proc_id)
);

CREATE INDEX jobs_hor_his_ix1 ON jobs_horizontal_history (owner);
CREATE INDEX jobs_hor_his_ix2 ON jobs_horizontal_history (enteredhistorytable);

CREATE TABLE clusterads_vertical (
scheddname	varchar(4000) NOT NULL,
cluster_id		integer NOT NULL,
attr	        varchar(2000) NOT NULL,
val		text,
primary key (scheddname,cluster_id, attr)
);

CREATE TABLE procads_vertical (
scheddname	varchar(4000) NOT NULL,
cluster_id	integer NOT NULL,
proc_id		integer NOT NULL,
attr	        varchar(2000) NOT NULL,
val		text,
primary key (scheddname,cluster_id, proc_id, attr)
);

CREATE TABLE jobs_vertical_history (
scheddname	varchar(4000) NOT NULL,
scheddbirthdate integer NOT NULL,
cluster_id	integer NOT NULL,
proc_id		integer NOT NULL,
attr		varchar(2000) NOT NULL,
val		text,
primary key (scheddname,scheddbirthdate, cluster_id, proc_id, attr)
);

CREATE TABLE generic_messages (
eventtype	varchar(4000),
eventkey	varchar(4000),
eventtime	timestamp(3) with time zone,
eventloc        varchar(4000),
attname	        varchar(4000),
attvalue	text,
atttype	varchar(4000)
);

CREATE TABLE daemons_vertical (
mytype				VARCHAR(100) NOT NULL,
name				VARCHAR(500) NOT NULL,
attr				VARCHAR(4000) NOT NULL,
val				text,
lastreportedtime			TIMESTAMP(3) WITH TIME ZONE,
PRIMARY KEY (MyType, Name, attr)
);

CREATE TABLE daemons_vertical_history (
mytype				VARCHAR(100),
name				VARCHAR(500),
lastreportedtime			TIMESTAMP(3) WITH TIME ZONE,
attr				VARCHAR(4000),
val				text,
endtime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE error_sqllogs (
logname   varchar(100),
host      varchar(50),
lastmodified timestamp(3) with time zone,
errorsql  text,
logbody   text,
errormessage varchar(4000)
);

CREATE INDEX error_sqllog_idx ON error_sqllogs (logname, host, lastmodified);

CREATE VIEW agg_user_jobs_waiting AS
  SELECT c.owner, count(*) AS jobs_waiting
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus IS NULL OR p.jobstatus = 0 OR p.jobstatus = 1)
    GROUP BY c.owner; 

CREATE VIEW agg_user_jobs_held AS
  SELECT c.owner, count(*) as jobs_held
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus=5)
    GROUP BY c.owner;

CREATE VIEW agg_user_jobs_running AS
  SELECT c.owner, count(*) as jobs_running
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus=2)
    GROUP BY c.owner;

CREATE VIEW agg_user_jobs_fin_last_day AS
  SELECT h.owner, count(*) as jobs_completed 
    FROM jobs_horizontal_history h 
    WHERE h.jobstatus = 4 
      AND h.completiondate >= (current_timestamp - interval '24 hour')
    GROUP BY h.owner;

-- Jobs that have historically flocked in to this pool for execution
-- (an anti-join between machine_classad history and jobs)
CREATE VIEW history_jobs_flocked_in AS 
SELECT DISTINCT globaljobid
FROM machines_horizontal_history 
WHERE SUBSTRING(globaljobid FROM 1 FOR (POSITION('#' IN globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM jobs_horizontal_history UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that are currently flocking in to this pool for execution 
-- (an anti-join between machine_classad and jobs)
CREATE VIEW current_jobs_flocked_in AS 
SELECT DISTINCT globaljobid 
FROM machines_horizontal
WHERE SUBSTRING(globaljobid FROM 1 FOR (POSITION('#' IN globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM jobs_horizontal_history UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that have historically flocked out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- The predicate "R.machine_id != R.scheddname" is added because some
-- jobs are executed locally on the schedd machine even if it's not 
-- a normal executing host.
CREATE VIEW history_jobs_flocked_out AS
SELECT DISTINCT scheddname, cluster_id, proc_id
FROM runs R 
WHERE R.endts IS NOT NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT substring(M.machine_id from (position('@' in M.machine_id)+1)) FROM machines_horizontal M);

-- Jobs that are currently flocking out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- machines must have reported less than 10 minutes ago to be counted
-- toward this pool.
CREATE VIEW current_jobs_flocked_out AS
SELECT DISTINCT R.scheddname, R.cluster_id, R.proc_id
FROM runs R, clusterads_horizontal C 
WHERE R.endts IS NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT substring(M.machine_id from (position('@' in M.machine_id)+1)) FROM machines_horizontal M where M.lastreportedtime >= now() - interval '10 minutes') AND R.scheddname = C.scheddname AND R.cluster_id = C.cluster_id;

/*
quill_purgehistory for PostgresSQL database.

quill_purgehistory does the following:
1. purge resource history data (e.g. machine history) that are older than 
   resourceHistoryDuration days

2. purge job run history data (e.g. runs, matchs, ...) that are older than 
   runHistoryDuration days

3. purge job history data that are older than 
   jobHistoryDuration days

4. check total size of all tables and see if it's bigger than 
   75% of quillDBSpaceLimit and if so, update the flag spaceShortageWarning
   to true and set the exact percentage of usage.

-- resource history data: no need to keep them for long
--   machine_history, machine_classad_history, 
--   daemon_horizontal_history, daemon_vertical_history, 

-- job run history data: purge when they are very old
--   transfers, fileusages, files, runs, events, rejects, matches

-- important job history data should be kept as long as possible
--   history_vertical, history_horizontal, thrown (log thrown events)

-- never purge current "operational data": 
--   machine, machine_classad, clusterads_horizontal, procads_horizontal, 
--   clusterads_vertical, procads_vertical, thrown, daemon_horizontal
--   daemon_vertical

-- resourceHistoryDuration, runHistoryDuration, jobHistoryDuration 
-- parameters are all in number of days
-- quillDBSpaceLimit parameter is in number of gigabytes

*/

-- dbsize is in unit of megabytes
CREATE TABLE quilldbmonitor (
dbsize    integer
);

DELETE FROM quilldbmonitor;
INSERT INTO quilldbmonitor (dbsize) VALUES (0);

CREATE TABLE history_jobs_to_purge(
scheddname   varchar(4000),
scheddbirthdate integer,
cluster_id   integer, 
proc_id         integer,
globaljobid  varchar(4000),
Primary key (scheddname, scheddbirthdate, cluster_id, proc_id)
);

INSERT INTO maintenance_events(id,msg) VALUES (1, 'issued purge command');
INSERT INTO maintenance_events(id,msg) VALUES (2, 'issued reindex command');

CREATE OR REPLACE FUNCTION
quill_purgehistory(
resourceHistoryDuration integer,
runHistoryDuration integer,
jobHistoryDuration integer) RETURNS  int AS $$
DECLARE
totalUsedMB NUMERIC;
start_time timestamp with time zone;
end_time timestamp with time zone;
	
BEGIN

-- lock tables in a specific order so we do not deadlock with Quill
-- more tables may be needed, but at least these are needed to avoid
-- deadlocking with the initial job queue load
LOCK TABLE clusterads_horizontal, clusterads_vertical, procads_horizontal, procads_vertical, JobQueuePollingInfo;

-- start timer
select into start_time timeofday();

/* first purge resource history data */

-- purge maintenance log older than resourceHistoryDuration days
DELETE FROM maintenance_log 
WHERE eventts < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge machine vertical attributes older than resourceHistoryDuration days
DELETE FROM machines_vertical_history
WHERE start_time < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge machine classads older than resourceHistoryDuration days
DELETE FROM machines_horizontal_history
WHERE lastreportedtime < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge daemon vertical attributes older than certain days
DELETE FROM daemons_vertical_history
WHERE lastreportedtime < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge daemon classads older than certain days
DELETE FROM daemons_horizontal_history
WHERE lastreportedtime < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge submitters classads older than certain days
DELETE FROM submitters_horizontal_history
WHERE lastreportedtime < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

/* second purge job run history data */

-- find the set of jobs for which the run history are going to be purged
INSERT INTO history_jobs_to_purge 
SELECT scheddname, scheddbirthdate, cluster_id, proc_id, globaljobid
FROM jobs_horizontal_history
WHERE enteredhistorytable < 
      (current_timestamp - 
       cast (runHistoryDuration || ' day' as interval));

-- purge transfers data related to jobs older than certain days
DELETE FROM transfers 
WHERE globaljobid IN (SELECT globaljobid 
                      FROM history_jobs_to_purge);

-- purge fileusages related to jobs older than certain days
DELETE FROM fileusages
WHERE globaljobid IN (SELECT globaljobid 
                      FROM history_jobs_to_purge);

-- purge files that are not referenced any more
DELETE FROM files
WHERE file_id IN (SELECT distinct(file_id) 
		FROM files 
		EXCEPT 
		SELECT f.file_id FROM files f, fileusages fu WHERE f.file_id = fu.file_id);

-- purge run data for jobs older than certain days
DELETE FROM runs 
WHERE run_id IN (SELECT distinct run_id 
		FROM runs R, history_jobs_to_purge H
		WHERE 	h.scheddname = r.scheddname AND 
			h.cluster_id = r.cluster_id AND 
			h.proc_id = r.proc_id);

-- purge rejects data for jobs older than certain days
DELETE FROM rejects
WHERE exists (SELECT * 
              FROM history_jobs_to_purge AS H
              WHERE H.scheddname = Rejects.scheddname AND
                    H.cluster_id = Rejects.cluster_id AND
                    H.proc_id = Rejects.proc_id);

-- purge matches data for jobs older than certain days
DELETE FROM matches
WHERE exists (SELECT * 
              FROM history_jobs_to_purge AS H
              WHERE H.scheddname = Matches.scheddname AND
                    H.cluster_id = Matches.cluster_id AND
                    H.proc_id = Matches.proc_id);

-- purge events data for jobs older than certain days
DELETE FROM events
WHERE exists (SELECT * 
              FROM history_jobs_to_purge AS H
              WHERE H.scheddname = Events.scheddname AND
                    H.cluster_id = Events.cluster_id AND
                    H.proc_id = Events.proc_id);

TRUNCATE TABLE history_jobs_to_purge;

/* third purge job history data */

-- find the set of jobs for which history data are to be purged
INSERT INTO history_jobs_to_purge 
SELECT scheddname, scheddbirthdate, cluster_id, proc_id, globaljobid
FROM jobs_horizontal_history
WHERE enteredhistorytable < 
      (current_timestamp - 
       cast (jobHistoryDuration || ' day' as interval));

-- purge vertical attributes for jobs older than certain days
DELETE FROM jobs_vertical_history 
WHERE exists (SELECT * 
              FROM history_jobs_to_purge AS H
              WHERE H.scheddname = jobs_vertical_history.scheddname AND
	    	    H.scheddbirthdate = jobs_vertical_history.scheddbirthdate AND
                    H.cluster_id = jobs_vertical_history.cluster_id AND
                    H.proc_id = jobs_vertical_history.proc_id);

-- purge classads for jobs older than certain days
DELETE FROM jobs_horizontal_history 
WHERE exists (SELECT * 
              FROM history_jobs_to_purge AS H2
              WHERE jobs_horizontal_history.scheddname = H2.scheddname AND
		    jobs_horizontal_history.scheddbirthdate = H2.scheddbirthdate AND
                    jobs_horizontal_history.cluster_id = H2.cluster_id AND
                    jobs_horizontal_history.proc_id = H2.proc_id);


-- purge log thrown events older than jobHistoryDuration
-- The thrown table doesn't fall precisely into any of the categories 
-- but we don't want the table to grow unbounded either.
DELETE FROM throwns
WHERE throwns.throwtime < 
     (current_timestamp - 
       cast (jobHistoryDuration || ' day' as interval));

-- purge sql error events older than jobHistoryDuration
-- The error_sqllogs table doesn't fall precisely into any of the categories, 
-- it may contain information about job history log that causes a sql error.
-- We don't want the table to grow unbounded either.
DELETE FROM error_sqllogs 
WHERE error_sqllogs.lastmodified < 
     (current_timestamp - 
       cast (jobHistoryDuration || ' day' as interval));

TRUNCATE TABLE history_jobs_to_purge;

/* lastly check if db size is above 75 percentage of specified limit */
-- one caveat: index size is not counted in the usage calculation
-- analyze tables first to have correct statistics 

-- without any parameters, meaning, analyze all tables in database
ANALYZE;

SELECT ROUND(SUM(relpages)*8192/(1024*1024)) INTO totalUsedMB
FROM pg_class
WHERE relname IN ('procads_vertical', 'jobs_vertical_history', 'clusterads_vertical', 'procads_horizontal', 'clusterads_horizontal', 'jobs_horizontal_history', 'files', 'fileusages', 'submitters_horizontal', 'runs', 'machines_vertical', 'machines_horizontal', 'daemons_vertical', 'daemons_horizontal', 'transfers', 'submitters_horizontal_history', 'rejects', 'matches', 'machines_vertical_history', 'machines_horizontal_history', 'events', 'daemons_vertical_history', 'daemons_horizontal_history');

RAISE NOTICE 'totalUsedMB=% MegaBytes', totalUsedMB;

UPDATE quilldbmonitor SET dbsize = totalUsedMB;

-- end timer
select into end_time timeofday();


-- finally record this in the maintenance_log table 
INSERT INTO maintenance_log(eventid,eventts,eventdur) 
VALUES(1, timestamp with time zone 'now', end_time-start_time);

return 1;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION quill_reindextables() RETURNS  int AS $$
DECLARE
start_time timestamp with time zone;
end_time timestamp with time zone;
BEGIN

-- lock tables in a specific order so we do not deadlock with Quill
-- more tables may be needed, but at least these are needed to avoid
-- deadlocking with the initial job queue load
LOCK TABLE clusterads_horizontal, clusterads_vertical, procads_horizontal, procads_vertical, JobQueuePollingInfo;

-- start timer
select into start_time timeofday();

reindex table error_sqllogs;
reindex table jobqueuepollinginfo;
reindex table currencies;
reindex table daemons_horizontal;
reindex table daemons_vertical;
reindex table submitters_horizontal;
reindex table machines_horizontal;
reindex table machines_vertical;
reindex table clusterads_horizontal;
reindex table clusterads_vertical;
reindex table procads_horizontal;
reindex table procads_vertical;
reindex table jobs_horizontal_history;
reindex table runs;
-- end timer
select into end_time timeofday();

-- finally record this in the maintenance_log table 
INSERT INTO maintenance_log(eventid,eventts,eventdur) 
VALUES(2, timestamp with time zone 'now', end_time-start_time);

return 1;
END;
$$ LANGUAGE plpgsql;


-- grant read access on relevant tables to quillreader
grant select on cdb_users to quillreader;
grant select on transfers to quillreader;
grant select on files to quillreader;
grant select on fileusages to quillreader;
grant select on machines_vertical to quillreader;
grant select on machines_vertical_history to quillreader;
grant select on machines_horizontal_history to quillreader;
grant select on machines_horizontal to quillreader;
grant select on clusterads_horizontal to quillreader;
grant select on procads_horizontal to quillreader;
grant select on clusterads_vertical to quillreader;
grant select on procads_vertical to quillreader;
grant select on jobs_vertical_history to quillreader;
grant select on jobs_horizontal_history to quillreader;
grant select on runs to quillreader;
grant select on rejects to quillreader;
grant select on matches to quillreader;
grant select on agg_user_jobs_waiting to quillreader;
grant select on agg_user_jobs_held to quillreader;
grant select on agg_user_jobs_running to quillreader;
grant select on l_jobstatus to quillreader;
grant select on throwns to quillreader;
grant select on events to quillreader;
grant select on l_eventtype to quillreader;
grant select on generic_messages to quillreader;
grant select on jobqueuepollinginfo to quillreader;
grant select on currencies to quillreader;
grant select on daemons_horizontal to quillreader;
grant select on daemons_vertical to quillreader;
grant select on daemons_horizontal_history to quillreader;
grant select on daemons_vertical_history to quillreader;
grant select on submitters_horizontal to quillreader;
grant select on submitters_horizontal_history to quillreader;
grant select on error_sqllogs to quillreader;
grant select on agg_user_jobs_fin_last_day to quillreader;
grant select on history_jobs_flocked_in to quillreader;
grant select on current_jobs_flocked_in to quillreader;
grant select on history_jobs_flocked_out to quillreader;
grant select on current_jobs_flocked_out to quillreader;
grant select on quilldbmonitor to quillreader;
grant select on maintenance_log to quillreader;

-- the creation of the schema version table should be the last step 
-- because it is used by quill daemon to decide whether we have the 
-- right schema objects for it to operate correctly
CREATE TABLE quill_schema_version (
major int, 
minor int, 
back_to_major int, 
back_to_minor int);

grant select on quill_schema_version to quillreader;

DELETE FROM quill_schema_version;
INSERT INTO quill_schema_version (major, minor, back_to_major, back_to_minor) VALUES (2,0,2,0);

