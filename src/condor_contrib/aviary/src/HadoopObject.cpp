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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_qmgr.h"
#include "../condor_schedd.V6/scheduler.h"
#include "stl_string_utils.h"

// local includes
#include "AviaryUtils.h"
#include "AviaryConversionMacros.h"
#include "HadoopObject.h"
#include "Codec.h"

// Global Scheduler object, used for needReschedule etc.
extern Scheduler scheduler;
extern char * Name;

using namespace aviary::hadoop;
using namespace aviary::util;
using namespace aviary::codec;

string quote_it (const char *  pszIn)
{
	string ret;
	sprintf(ret,"\"%s\"", pszIn);
	return ret;
}

HadoopObject* HadoopObject::m_instance = NULL;

HadoopObject::HadoopObject()
{
    m_pool = getPoolName();
	m_name = getScheddName();
    m_codec = new BaseCodec();
}

HadoopObject::~HadoopObject()
{
	delete m_codec;
}

HadoopObject* HadoopObject::getInstance()
{
    if (!m_instance) {
        m_instance = new HadoopObject();
    }
    return m_instance;
}

void
HadoopObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

	m_stats.Pool = getPoolName();
	STRING(CondorPlatform);
	STRING(CondorVersion);
	TIME_INTEGER(DaemonStartTime);
	TIME_INTEGER(JobQueueBirthdate);
	STRING(Machine);
	INTEGER(MaxJobsRunning);
	INTEGER(MonitorSelfAge);
	DOUBLE(MonitorSelfCPUUsage);
	DOUBLE(MonitorSelfImageSize);
	INTEGER(MonitorSelfRegisteredSocketCount);
	INTEGER(MonitorSelfResidentSetSize);
	TIME_INTEGER(MonitorSelfTime);
	STRING(MyAddress);
    //TIME_INTEGER(MyCurrentTime);
	STRING(Name);
	INTEGER(NumUsers);
	STRING(MyAddress);
	INTEGER(TotalHeldJobs);
	INTEGER(TotalIdleJobs);
	INTEGER(TotalJobAds);
	INTEGER(TotalRemovedJobs);
	INTEGER(TotalRunningJobs);
	m_stats.System = m_stats.Machine;

	// debug
	if (DebugFlags & D_FULLDEBUG) {
		const_cast<ClassAd*>(&ad)->dPrint(D_FULLDEBUG|D_NOHEADER);
	}
}

int HadoopObject::start( tHadoopInit & hInit )
{ 
    int cluster, proc;
    
    dprintf( D_FULLDEBUG, "Called HadoopObject::start w/%s\n", hInit.tarball.c_str() );
         
    // check input tarball.
    if ( 0 == hInit.tarball.size() )
    {
	char * binball = param("HADOOP_BIN_TARBALL");
	if (!binball)
	{
	    m_lasterror = "No hadoop tarball specified.";
	    return false;
	}
	else
	{
	    hInit.tarball = binball;
	    delete binball;
	}
    }
    
    // Create transaction
    BeginTransaction();

    // Create cluster
    if ( -1 == (cluster = NewCluster()) ) 
    {
	AbortTransaction();
	m_lasterror = "Failed to create new cluster";
	return false;
    }

    // loop through adding sub-elements for a cluster/proc
    for (unsigned int iCtr=0; iCtr<hInit.count; iCtr++)
    {
	
	// create proc
	if ( -1 == (proc = NewProc(cluster)) ) 
	{
	    AbortTransaction();
	    m_lasterror = "Failed to create new proc";
	    return false;
	}

	// now we will 
	string hadoopType, inputscript, args;
	MyString HTTPAddress, IPCAddress;
	bool hasInputScript=false, bValidId=false;
	string Iwd="/tmp";
	PROC_ID id = getProcByString( hInit.idref.id.c_str() );
	
	
	if (id.cluster > 0 && id.proc > 0) 
	{
	    bValidId = true;
	    dprintf(D_FULLDEBUG, "Valid ClusterId Ref: %s\n", hInit.idref.id.c_str());
	}
	
	args = hInit.tarball;
	
	switch (hInit.idref.type)
	{
	    case NAME_NODE:
		hadoopType = ATTR_NAME_NODE;
		hasInputScript = param(inputscript, "HADOOP_HDFS_NAMENODE_SCRIPT");
		::SetAttribute(cluster, proc, ATTR_RANK, "memory");
		::SetAttribute(cluster, proc, ATTR_REQUEST_MEMORY, "floor(.50 * Target.Memory)");  // TODO: --> Target.Memory
		break;
	    case JOB_TRACKER:
		hadoopType = ATTR_JOB_TRACKER;
		hasInputScript = param(inputscript, "HADOOP_MAPR_JOBTRACKER_SCRIPT");
		::SetAttribute(cluster, proc, ATTR_RANK, "memory"); 
		// fall through
	    case DATA_NODE:
		// special case case only a small part the rest is common.
		if (hInit.idref.type == DATA_NODE)
		{
		    hadoopType = ATTR_DATA_NODE; 
		    hasInputScript = param(inputscript, "HADOOP_HDFS_DATANODE_SCRIPT");
		    ::SetAttribute(cluster, proc, ATTR_RANK, "disk");
		    ::SetAttribute(cluster, proc, ATTR_REQUEST_DISK, "floor(.50 * Target.TotalDisk)");
		}
		
		///////////////////////////////////////////////////////////////////////////////////////
		// NOTE: This section is common to both JOB_TRACKER && DATA_NODES
		// It could possibly be refactored into a function but it's not really pretty/clean.
		if (bValidId)
		{
		    ::GetAttributeString( id.cluster, id.proc,  "NameNodeIPCAddress", IPCAddress);
		    ::GetAttributeString( id.cluster, id.proc,  "NameNodeHTTPAddress", HTTPAddress);
		    
		    //TODO: there could be extra checks here.
		    ::SetAttribute(cluster, proc, "NameNode", hInit.idref.id.c_str() );
		}
		else if (hInit.idref.ipcid.length())
		{
		    // TODO: there could be extra checks here, validate it's up. 
		    IPCAddress = hInit.idref.ipcid.c_str();
		    ::SetAttribute(cluster, proc, "NameNode", hInit.idref.ipcid.c_str() );
		}
		else
		{
		    AbortTransaction();
		    m_lasterror = "No valid Name Node ";
		    return false;
		}
		    
		// not always valid
		if ( HTTPAddress.Length() )
		{
		    ::SetAttribute(cluster, proc, "NameNodeHTTPAddress", quote_it(HTTPAddress.Value()).c_str() );
		}
		
		args += " ";
		args += IPCAddress.Value();
		::SetAttribute(cluster, proc, "NameNodeIPCAddress", quote_it(IPCAddress.Value()).c_str());
		///////////////////////////////////////////////////////////////////////////////////////
		
		break;
	    
	    case TASK_TRACKER:
		
		hadoopType = ATTR_TASK_TRACKER;
		hasInputScript = param(inputscript, "HADOOP_MAPR_TASKTRACKER_SCRIPT");
		::SetAttribute(cluster, proc, ATTR_RANK, "Mips");
		
		if (bValidId)
		{   
		    ::GetAttributeString( id.cluster, id.proc,  "JobTrackerIPCAddress", IPCAddress);
		    ::GetAttributeString( id.cluster, id.proc,  "JobTrackerHTTPAddress", HTTPAddress);
		    
		    //TODO: there could be extra checks here.
		    ::SetAttribute(cluster, proc, "JobTracker", hInit.idref.id.c_str() );
		}
		else if (hInit.idref.ipcid.length())
		{
		    // TODO: there could be extra checks here, validate it's up. 
		    IPCAddress = hInit.idref.ipcid.c_str();
		    ::SetAttribute(cluster, proc, "JobTracker", hInit.idref.ipcid.c_str() );
		}
		else
		{
		    AbortTransaction();
		    m_lasterror = "No valid Job Tracker ";
		    return false;
		}
		    
		// not always valid
		if ( HTTPAddress.Length() )
		{
		    ::SetAttribute(cluster, proc, "JobTrackerHTTPAddress", quote_it(HTTPAddress.Value()).c_str() );
		}
		
		args += " ";
		args += IPCAddress.Value();
		::SetAttribute(cluster, proc, "JobTrackerIPCAddress", quote_it(IPCAddress.Value()).c_str());
		
		break;
	}

	// verify that there is an input script, without it you won't get far.
	if (!hasInputScript)
	{
	    AbortTransaction();
	    sprintf(m_lasterror, "Missing Script Input KNOB for type %s", hadoopType.c_str() );
	    return false;
	}

	// TODO - Owner ?
	::SetAttribute(cluster, proc, ATTR_OWNER, "\"condor\"");
	
	param(Iwd, "HADOOP_IWD", "/tmp");
	::SetAttribute(cluster, proc, ATTR_JOB_IWD, quote_it(Iwd.c_str()).c_str() );
	
	::SetAttribute(cluster, proc, ATTR_JOB_CMD, quote_it(inputscript.c_str()).c_str());
	::SetAttribute(cluster, proc, ATTR_JOB_ARGUMENTS1, quote_it(args.c_str()).c_str());
	::SetAttribute(cluster, proc, ATTR_TRANSFER_INPUT_FILES, quote_it(hInit.tarball.c_str()).c_str());
	::SetAttribute(cluster, proc, ATTR_HADOOP_TYPE, quote_it(hadoopType.c_str()).c_str() );
	::SetAttribute(cluster, proc, ATTR_SHOULD_TRANSFER_FILES, quote_it("YES").c_str());
	::SetAttribute(cluster, proc, ATTR_WANT_IO_PROXY, "true");
	::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, "( HasJava =?= TRUE ) && ( TARGET.OpSys == \"LINUX\" ) && ( TARGET.Memory >= RequestMemory ) && ( TARGET.HasFileTransfer )");
	
	// TODO - Handle input & output files (kind of debugging only)
	::SetAttribute(cluster, proc, ATTR_WHEN_TO_TRANSFER_OUTPUT, quote_it("ON_EXIT").c_str());
	
	::SetAttribute(cluster, proc, ATTR_KILL_SIG, quote_it("SIGTERM").c_str());
	
	// EARLY SET: These attribute are set early so the incoming ad
	// has a change to override them.
	::SetAttribute(cluster, proc, ATTR_JOB_STATUS, "1"); // 1 = idle

	// Junk that condor_q wants, but really shouldn't be necessary
	::SetAttribute(cluster, proc, ATTR_JOB_REMOTE_USER_CPU, "0.0"); // float
	::SetAttribute(cluster, proc, ATTR_JOB_PRIO, "0");              // int
	::SetAttribute(cluster, proc, ATTR_IMAGE_SIZE, "0");            // int  
	::SetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA );
	::SetAttribute(cluster, proc, ATTR_CURRENT_HOSTS, "0"); // int

        // LATE SET: These attributes are set late, after the incoming
	// ad, so they override whatever the incoming ad set.
	char buf[22]; // 22 is max size for an id, 2^32 + . + 2^32 + \0
	snprintf(buf, 22, "%d", cluster);
	::SetAttribute(cluster, proc, ATTR_CLUSTER_ID, buf);
	snprintf(buf, 22, "%d", proc);
	::SetAttribute(cluster, proc, ATTR_PROC_ID, buf);
	snprintf(buf, 22, "%ld", time(NULL));
	::SetAttribute(cluster, proc, ATTR_Q_DATE, buf);

    }
    
    // 5. Commit transaction
    CommitTransaction();

    // 6. Reschedule
    scheduler.needReschedule();
    
    // fill in the new cluster id
    sprintf(hInit.newcluster,"%d",cluster);
    
    return true;
	
}


bool HadoopObject::stop(const tHadoopRef & hRef)
{
    PROC_ID id = getProcByString( hRef.id.c_str() );
    
    dprintf( D_FULLDEBUG, "Called HadoopObject::stop()\n");
    
    if (id.cluster <= 0 || id.proc < 0) {
	dprintf(D_FULLDEBUG, "Remove: Failed to parse id: %s\n", hRef.id.c_str());
	m_lasterror = "Invalid Id";
	return false;
    }

    if (!abortJob(id.cluster, id.proc, "Aviary API stop",true )) 
    {
	m_lasterror = "Failed to remove job";
	return false;
    }

    return true;
}

bool status (const PROC_ID & id , tHadoopJobStatus & hStatus, string & m_lasterror)
{
    if (id.cluster <= 0 || id.proc < 0) {
	dprintf(D_FULLDEBUG, "Remove: INVALID ID\n");
	m_lasterror = "Invalid Id";
	return false;
    }
    
    // we are not sending it through yet.
    hStatus.owner = "condor";
    hStatus.uptime = 0;
    
    int JobStatus=0; 
    int EnteredStatus=0;
    if ( 0 > ::GetAttributeInt( id.cluster, id.proc, ATTR_JOB_STATUS, &JobStatus) )
    {
	dprintf(D_FULLDEBUG, "Remove: Failed to obtain JobStatus on: %d.%d\n", id.cluster, id.proc);
	m_lasterror = "Could no obtain JobStatus";
	return false;
    }
    
    // 
    switch (JobStatus)
    {
	case 1:
	    hStatus.state = "PENDING";
	    break;
	case 2:
	    hStatus.state = "RUNNING";
	    if ( 0 == ::GetAttributeInt( id.cluster, id.proc, ATTR_ENTERED_CURRENT_STATUS, &EnteredStatus))
	    {
		hStatus.uptime=((int)time(NULL)-EnteredStatus);
	    }
	    break;
	case 3:
	case 4:
	    hStatus.state = "EXITING";
	    break;
	default: 
	    hStatus.state = "ERROR";
    }
    
    return true;
    
}

bool HadoopObject::query (const tHadoopRef & hRef, std::vector<tHadoopJobStatus> & vhStatus)
{
    PROC_ID id = getProcByString( hRef.id.c_str() );
    
    dprintf( D_FULLDEBUG, "Called HadoopObject::status()\n");
    
    vhStatus.clear();
    
    switch (hRef.type)
    {
	case NAME_NODE:
	    // just list all the name nodes
	    break;
	case DATA_NODE:
	    // list all name nodes bound query on hRef.idref.id;
	    break;
	case JOB_TRACKER:
	    //just list all job trackers.
	    break;
	case TASK_TRACKER:
	    // list all name nodes bound query on hRef.idref.id;
	    
	    break;
    }
    
    return true;
    
}


