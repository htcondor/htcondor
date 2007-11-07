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
   the script should be installed into the database that's used by quill
   and installed as the quillwriter user the quill daemon will connect to 
   for updating the database. The right permissions shoud have been 
   set up for the user for the creation of the following schema objects.
*/

CREATE TABLE cdb_users (
username varchar(30),
password character(32),
admin varchar(5));

CREATE TABLE  transfers (
globaljobid  	varchar(4000),
src_name  	varchar(4000),
src_host  	varchar(4000),
src_port	integer,
src_path 	varchar(4000),
src_daemon      varchar(30),
src_protocol	varchar(30),
src_credential_id integer,
src_acl_id        integer, -- we dont know what this is yet, so normalized id
dst_name  	varchar(4000),
dst_host  	varchar(4000),
dst_port        integer,
dst_path  	varchar(4000),
dst_daemon  	varchar(30),
dst_protocol    varchar(30),
dst_credential_id integer,
dst_acl_id        integer,
transfer_intermediary_id integer, -- a foreign key for potential proxies
transfer_size_bytes   numeric(38),
elapsed  	numeric(38),
checksum    	varchar(256),
transfer_time	timestamp(3) with time zone,
last_modified	timestamp(3) with time zone,
is_encrypted    varchar(5),
delegation_method_id integer,
completion_code integer
);

CREATE TABLE files (
file_id  	int NOT NULL,
name  		varchar(4000), 
host  		varchar(4000),
path  		varchar(4000),
acl_id      integer,
lastmodified    timestamp(3) with time zone,
filesize  	numeric(38),
checksum  	varchar(32), 
PRIMARY KEY (file_id)
);

CREATE TABLE fileusages (
globaljobid  	varchar(4000),
file_id         int REFERENCES files(file_id),
usagetype  	varchar(4000));

CREATE sequence condor_seqfileid;

-- Added by pachu

CREATE TABLE machines_horizontal (
machine_id             varchar(4000) NOT NULL,
opsys                  varchar(4000),
arch                   varchar(4000),
state                  varchar(4000),
activity               varchar(4000),
keyboardidle           integer,
consoleidle            integer,
loadavg                real,
condorloadavg          real,
totalloadavg           real,
virtualmemory          integer,
memory                 integer,
totalvirtualmemory     integer,
cpubusytime            integer,
cpuisbusy              varchar(5),
currentrank            real,
clockmin               integer,
clockday               integer,
lastreportedtime       timestamp(3) with time zone,
enteredcurrentactivity timestamp(3) with time zone,
enteredcurrentstate    timestamp(3) with time zone,
updatesequencenumber   integer,
updatestotal           integer,
updatessequenced       integer,
updateslost            integer,
globaljobid            varchar(4000),
lastreportedtime_epoch    integer,
Primary Key (machine_id)
);

CREATE TABLE machines_horizontal_history (
machine_id             varchar(4000),
opsys                  varchar(4000),
arch                   varchar(4000),
state                  varchar(4000),
activity               varchar(4000),
keyboardidle           integer,
consoleidle            integer,
loadavg                real,
condorloadavg          real,
totalloadavg           real,
virtualmemory          integer,
memory                 integer,
totalvirtualmemory     integer,
cpubusytime            integer,
cpuisbusy              varchar(5),
currentrank            real,
clockmin               integer,
clockday               integer,
lastreportedtime       timestamp(3) with time zone,
enteredcurrentactivity timestamp(3) with time zone,
enteredcurrentstate    timestamp(3) with time zone,
updatesequencenumber   integer,
updatestotal           integer,
updatessequenced       integer,
updateslost            integer,
globaljobid            varchar(4000),
end_time	       timestamp(3) with time zone
);

-- END Added by Pachu

-- BEGIN Added by Ameet

CREATE SEQUENCE seqrunid;

CREATE TABLE runs (
run_id 	                NUMERIC(12) NOT NULL,
machine_id              varchar(4000),
scheddname	        varchar(4000),
cluster_id              integer,
proc_id			integer,
spid                    integer,
globaljobid		varchar(4000),
startts                 timestamp(3) with time zone,
endts                   timestamp(3) with time zone,
endtype                 smallint,
endmessage              varchar(4000),
wascheckpointed         varchar(7),
imagesize               numeric(38),
runlocalusageuser       integer,
runlocalusagesystem     integer,
runremoteusageuser      integer,
runremoteusagesystem    integer,
runbytessent            numeric(38),
runbytesreceived        numeric(38), 
PRIMARY KEY (run_id));

CREATE INDEX runs_idx1 ON runs(scheddname, cluster_id, proc_id);

-- END Added by Ameet

-- BEGIN Srinath

CREATE TABLE rejects (
reject_time     timestamp(3) with time zone, -- Time the job was rejected
username        varchar(4000),
scheddname      varchar(4000),
cluster_id      integer,
proc_id		integer,
globaljobid     varchar(4000));

CREATE TABLE matches (
match_time      timestamp(3) with time zone, -- Time the match was made
username        varchar(4000),
scheddname      varchar(4000),
cluster_id      integer,
proc_id		integer,
globaljobid     varchar(4000), 
machine_id      varchar(4000),
remote_user     varchar(4000),   -- The user that was preempted
remote_priority real       -- Preempted user's priority
);

--END Srinath

CREATE TABLE l_jobstatus (
jobstatus	integer NOT NULL,
abbrev	        char(1),
description     varchar(4000),
primary key	(jobstatus)
);

INSERT INTO l_jobstatus VALUES(0, 'U', 'UNEXPANDED');
INSERT INTO l_jobstatus VALUES(1, 'I', 'IDLE');
INSERT INTO l_jobstatus VALUES(2, 'R', 'RUNNING');
INSERT INTO l_jobstatus VALUES(3, 'X', 'REMOVED');
INSERT INTO l_jobstatus VALUES(4, 'C', 'COMPLETED');
INSERT INTO l_jobstatus VALUES(5, 'H', 'HELD');
INSERT INTO l_jobstatus VALUES(6, 'E', 'SUBMISSION_ERROR');

--END Eric

CREATE TABLE throwns(
filename       varchar(4000),
machine_id     varchar(4000),
log_size       numeric(38),
throwtime      timestamp(3) with time zone
);

CREATE TABLE events (
scheddname      varchar(4000),
cluster_id      integer,
proc_id		integer,
globaljobid varchar(4000),
run_id     	numeric(12, 0),
eventtype       integer,
eventtime       timestamp(3) with time zone,
description     varchar(4000)
);

CREATE TABLE l_eventtype (
eventtype     integer,
description   varchar(4000)
);

INSERT INTO l_eventtype values (0, 'Job submitted');
INSERT INTO l_eventtype values (1, 'Job now running');
INSERT INTO l_eventtype values (2, 'Error in executable');
INSERT INTO l_eventtype values (3, 'Job was checkpointed');
INSERT INTO l_eventtype values (4, 'Job evicted from machine');
INSERT INTO l_eventtype values (5, 'Job terminated');
INSERT INTO l_eventtype values (6, 'Image size of job updated');
INSERT INTO l_eventtype values (7, 'Shadow threw an exception');
INSERT INTO l_eventtype values (8, 'Generic Log Event');
INSERT INTO l_eventtype values (9, 'Job Aborted');
INSERT INTO l_eventtype values (10, 'Job was suspended');
INSERT INTO l_eventtype values (11, 'Job was unsuspended');
INSERT INTO l_eventtype values (12, 'Job was held');
INSERT INTO l_eventtype values (13, 'Job was released');
INSERT INTO l_eventtype values (14, 'Parallel Node executed');
INSERT INTO l_eventtype values (15, 'Parallel Node terminated');
INSERT INTO l_eventtype values (16, 'POST script terminated');
INSERT INTO l_eventtype values (17, 'Job Submitted to Globus');
INSERT INTO l_eventtype values (18, 'Globus Submit failed');
INSERT INTO l_eventtype values (19, 'Globus Resource Up');
INSERT INTO l_eventtype values (20, 'Globus Resource Down');
INSERT INTO l_eventtype values (21, 'Remote Error');
INSERT INTO l_eventtype values (22, 'RSC socket lost');
INSERT INTO l_eventtype values (23, 'RSC socket re-established');
INSERT INTO l_eventtype values (24, 'RSC reconnect failure');

CREATE TABLE jobqueuepollinginfo (
scheddname           varchar(4000),
last_file_mtime      INTEGER, 
last_file_size       numeric(38), 
last_next_cmd_offset INTEGER, 
last_cmd_offset      INTEGER, 
last_cmd_type        SMALLINT, 
last_cmd_key         varchar(4000), 
last_cmd_mytype      varchar(4000), 
last_cmd_targettype  varchar(4000), 
last_cmd_name        varchar(4000), 
last_cmd_value       varchar(4000)
); 

CREATE INDEX jq_i_schedd ON jobqueuepollinginfo(scheddname);

CREATE TABLE currencies(
datasource varchar(4000),
lastupdate timestamp(3) with time zone
);

CREATE INDEX currencies_idx ON currencies(datasource);

CREATE TABLE daemons_horizontal (
mytype				VARCHAR(100) NOT NULL,
name				VARCHAR(500) NOT NULL,
lastreportedtime		TIMESTAMP(3) WITH TIME ZONE,
monitorselftime			TIMESTAMP(3) WITH TIME ZONE,
monitorselfcpuusage		numeric(38),
monitorselfimagesize		numeric(38),
monitorselfresidentsetsize	numeric(38),
monitorselfage			INTEGER,
updatesequencenumber		INTEGER,
updatestotal			INTEGER,
updatessequenced		INTEGER,
updateslost			INTEGER,
updateshistory			VARCHAR(4000),
lastreportedtime_epoch          integer,
PRIMARY KEY (MyType, Name)
);

CREATE TABLE daemons_horizontal_history (
mytype				VARCHAR(100),
name				VARCHAR(500),
lastreportedtime		TIMESTAMP(3) WITH TIME ZONE,
monitorselftime			TIMESTAMP(3) WITH TIME ZONE,
monitorselfcpuusage		numeric(38),
monitorselfimagesize		numeric(38),
monitorselfresidentsetsize	numeric(38),
monitorselfage			INTEGER,
updatesequencenumber		INTEGER,
updatestotal			INTEGER,
updatessequenced		INTEGER,
updateslost			INTEGER,
updateshistory			VARCHAR(4000),
endtime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE submitters_horizontal (
name				VARCHAR(500) NOT NULL,
scheddname			VARCHAR(4000),
lastreportedtime		TIMESTAMP(3) WITH TIME ZONE,
idlejobs			INTEGER,
runningjobs			INTEGER,
heldjobs			INTEGER,
flockedjobs			INTEGER,
PRIMARY KEY (Name)
);

CREATE TABLE submitters_horizontal_history (
name				VARCHAR(500),
scheddname			VARCHAR(4000),
lastreportedtime		TIMESTAMP(3) WITH TIME ZONE,
idlejobs			INTEGER,
runningjobs			INTEGER,
heldjobs			INTEGER,
flockedjobs			INTEGER,
endtime				TIMESTAMP(3) WITH TIME ZONE
);


 

-- this table is used internally by the quill daemon for constructing a 
-- single tuple in a sql statement for updating database, end users 
-- don't need to access this table
CREATE TABLE dummy_single_row_table(a varchar(1));
INSERT INTO dummy_single_row_table VALUES ('x');

