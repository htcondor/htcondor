/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _QUILL_SCHEMA_DEF_H_
#define _QUILL_SCHEMA_DEF_H_

#define SCHEMA_CHECK_STR "SELECT relname FROM pg_class WHERE relname = 'clusterads_num' OR relname = 'clusterads_str' OR relname = 'history_horizontal' OR relname = 'history_vertical' OR relname = 'procads_num' OR relname = 'procads_str' OR relname = 'jobqueuepollinginfo' OR relname='procads' OR relname='clusterads';"

//this includes the 2 views
#define SCHEMA_SYS_TABLE_NUM 	9

/* 
	Definition of Delete String 

	DELETE FROM ProcAds_Str;
	DELETE FROM ProcAds_Num; 
	DELETE FROM ClusterAds_Str;
	DELETE FROM ClusterAds_Num; 
	DELETE FROM JobQueuePollingInfo;
*/

#define SCHEMA_DELETE_STR "DELETE FROM ProcAds_Str; DELETE FROM ProcAds_Num; DELETE FROM ClusterAds_Str; DELETE FROM ClusterAds_Num; DELETE FROM JobQueuePollingInfo;"


/*
	Definition of Drop Table String 

	DROP TABLE ProcAds_Str;
	DROP TABLE ProcAds_Num;
	DROP TABLE ClusterAds_Str;
	DROP TABLE ClusterAds_Num;
	DROP TABLE History_Horizontal;
	DROP TABLE History_Vertical;
	DROP TABLE JobQueuePollingInfo;
        DROP VIEW ProcAds;
        DROP VIEW ClusterAds;
*/

#define SCHEMA_DROP_TABLE_STR "DROP TABLE ProcAds_Str; DROP TABLE ProcAds_Num; DROP TABLE ClusterAds_Str; DROP TABLE ClusterAds_Num; DROP TABLE History_Horizontal; DROP TABLE History_Vertical; DROP TABLE JobQueuePollingInfo;DROP VIEW ProcAds; DROP VIEW ClusterAds;"

/*
	Definition of Create ProcAds Table String 

	CREATE TABLE ProcAds_Str (
		cid		int,
		pid		int,
		attr		text,
		val		text,
		primary key (cid, pid, attr)
	);

#	CREATE INDEX ProcAds_Str_I_cid ON ProcAds_Str (cid);
#	CREATE INDEX ProcAds_Str_I_pid ON ProcAds_Str (pid);
#	CREATE INDEX ProcAds_Str_I_attr ON ProcAds_Str (attr);
#	CREATE INDEX ProcAds_Str_I_val ON ProcAds_Str (val);
#	CREATE INDEX ProcAds_Str_I_attr_val ON ProcAds_Str (attr, val);
all indices are removed.  Only the pkey index created by default exists

	CREATE TABLE ProcAds_Num (
		cid		int,
		pid		int,
		attr		text,
		val		double precision,
		primary key (cid, pid, attr)
	);

#	CREATE INDEX ProcAds_Num_I_cid ON ProcAds_Num (cid);
#	CREATE INDEX ProcAds_Num_I_pid ON ProcAds_Num (pid);
#	CREATE INDEX ProcAds_Num_I_attr ON ProcAds_Num (attr);
#	CREATE INDEX ProcAds_Num_I_val ON ProcAds_Num (val);
#	CREATE INDEX ProcAds_Num_I_attr_val ON ProcAds_Num (attr, val);
all indices are removed.  Only the pkey index created by default exists

    Definition of ProcAds view
    CREATE VIEW ProcAds as 
	select cid, pid, attr, val from ProcAds_Str UNION ALL
	select cid, pid, attr, cast(val as text) from ProcAds_Num;
*/

#define SCHEMA_CREATE_PROCADS_TABLE_STR "CREATE TABLE ProcAds_Str (cid int, pid int, attr text, val text, primary key(cid, pid, attr)); CREATE TABLE ProcAds_Num (cid int, pid int, attr text, val double precision, primary key(cid,pid,attr)); CREATE VIEW ProcAds as select cid, pid, attr, val from ProcAds_Str UNION ALL select cid, pid, attr, cast(val as text) from ProcAds_Num; GRANT SELECT ON ProcAds_Str TO quillreader; GRANT SELECT ON ProcAds_Num TO quillreader; GRANT SELECT ON ProcAds TO quillreader;"

/*
	Definition of Create ClusterAds Table String 

CREATE TABLE ClusterAds_Str (
	cid		int,
	attr		text,
	val		text,
	primary key (cid, attr)
);

#CREATE INDEX ClusterAds_Str_I_cid ON ClusterAds_Str (cid);
#CREATE INDEX ClusterAds_Str_I_attr ON ClusterAds_Str (attr);
#CREATE INDEX ClusterAds_Str_I_val ON ClusterAds_Str (val);
#CREATE INDEX ClusterAds_Str_I_attr_val ON ClusterAds_Str (attr, val);
all indices are removed.  Only the pkey index created by default exists

CREATE TABLE ClusterAds_Num (
	cid		int,
	attr		text,
	val		double precision,
	primary key (cid, attr)
);

#CREATE INDEX ClusterAds_Num_I_cid ON ClusterAds_Num (cid);
#CREATE INDEX ClusterAds_Num_I_attr ON ClusterAds_Num (attr);
#CREATE INDEX ClusterAds_Num_I_val ON ClusterAds_Num (val);
#CREATE INDEX ClusterAds_Num_I_attr_val ON ClusterAds_Num (attr, val);
all indices are removed.  Only the pkey index created by default exists


    Definition of ClusterAds view
    CREATE VIEW ClusterAds as 
	select cid, attr, val from ClusterAds_Str UNION ALL
	select cid, attr, cast(val as text) from ClusterAds_Num;
*/

#define SCHEMA_CREATE_CLUSTERADS_TABLE_STR "CREATE TABLE ClusterAds_Str (cid int, attr text, val text, primary key(cid, attr)); CREATE TABLE ClusterAds_Num (cid int, attr text, val double precision, primary key(cid, attr)); CREATE VIEW ClusterAds as select cid, attr, val from ClusterAds_Str UNION ALL select cid, attr, cast(val as text) from ClusterAds_Num; GRANT SELECT ON ClusterAds_Str TO quillreader; GRANT SELECT ON ClusterAds_Num TO quillreader; GRANT SELECT ON ClusterAds TO quillreader;"

/*
	Definition of Create JobQueuePollingInfo Table String 

CREATE TABLE JobQueuePollingInfo (
	last_file_mtime		BIGINT,
	last_file_size		BIGINT,
	last_next_cmd_offset	BIGINT,
	last_cmd_offset		BIGINT,
	last_cmd_type		SMALLINT,
	last_cmd_key		text,
	last_cmd_mytype 	text,
	last_cmd_targettype 	text,
	last_cmd_name		text,
	last_cmd_value		text
	log_seq_num		BIGINT,
	log_creation_time	BIGINT
);

INSERT INTO JobQueuePollingInfo (last_file_mtime, last_file_size,log_seq_num,log_creation_time) VALUES (0,0,0,0);
*/

#define SCHEMA_CREATE_JOBQUEUEPOLLINGINFO_TABLE_STR "CREATE TABLE JobQueuePollingInfo (last_file_mtime BIGINT, last_file_size BIGINT, last_next_cmd_offset BIGINT, last_cmd_offset BIGINT, last_cmd_type SMALLINT, last_cmd_key text, last_cmd_mytype text, last_cmd_targettype text, last_cmd_name text, last_cmd_value text, log_seq_num BIGINT, log_creation_time BIGINT); INSERT INTO JobQueuePollingInfo (last_file_mtime, last_file_size, log_seq_num, log_creation_time) VALUES (0,0,0,0);"

/*
        Definition of Create History Tables String
  
CREATE TABLE History_Vertical (
        cid     int,
	pid     int,
        attr    text,
        val     text,
        primary key (cid, pid, attr)
);
  
CREATE INDEX History_Vertical_I_attr_val ON History_Vertical (attr, val);

CREATE TABLE History_Horizontal(
        cid                  int, 
        pid                  int, 
	EnteredHistoryTable  timestamp with time zone
        Owner                text, 
        QDate                int, 
        RemoteWallClockTime  int, 
        RemoteUserCpu        float, 
        RemoteSysCpu         float, 
        ImageSize            int, 
        JobStatus            int, 
        JobPrio              int, 
        Cmd                  text, 
        CompletionDate       int, 
        LastRemoteHost       text, 
        primary key(cid,pid)
); 

CREATE INDEX History_Horizontal_I_Owner on History_Horizontal(Owner);      
CREATE INDEX History_Horizontal_I_CompletionDate on History_Horizontal(CompletionDate);      
CREATE INDEX History_Horizontal_I_EnteredHistoryTable on History_Horizontal(EnteredHistoryTable);      

Note that some column names in the horizontal table are surrounded
by quotes in order to make them case-sensitive.  This is so that when
we convert the column names to their corresponding classad attribute name,
they look the same as the corresponding ad stored in the history file
*/

#define SCHEMA_CREATE_HISTORY_TABLE_STR "CREATE TABLE History_Vertical (cid int, pid int, attr text, val text, primary key (cid, pid, attr)); CREATE INDEX History_Vertical_I_attr_val ON History_Vertical (attr, val); CREATE TABLE History_Horizontal(cid int, pid int, \"EnteredHistoryTable\" timestamp with time zone, \"Owner\" text, \"QDate\" int, \"RemoteWallClockTime\" int, \"RemoteUserCpu\" float, \"RemoteSysCpu\" float, \"ImageSize\" int, \"JobStatus\" int, \"JobPrio\" int, \"Cmd\" text, \"CompletionDate\" int, \"LastRemoteHost\" text, primary key(cid,pid)); CREATE INDEX History_Horizontal_I_Owner on History_Horizontal(\"Owner\");CREATE INDEX History_Horizontal_I_CompletionDate on History_Horizontal(\"CompletionDate\");CREATE INDEX History_Horizontal_I_EnteredHistoryTable on History_Horizontal(\"EnteredHistoryTable\"); GRANT SELECT ON History_Horizontal TO quillreader; GRANT SELECT ON History_Vertical TO quillreader;"



#endif

