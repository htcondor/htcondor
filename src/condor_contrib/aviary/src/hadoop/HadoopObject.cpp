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

// Global Scheduler object, used for needReschedule etc.
extern Scheduler scheduler;
extern char * Name;

using namespace aviary::hadoop;
using namespace aviary::util;
using namespace aviary::codec;

string quote_it (const char *  pszIn)
{
    string ret;
    aviUtilFmt(ret,"\"%s\"", pszIn);
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
    //if (DebugFlags & D_FULLDEBUG) {
	dPrintAd(D_FULLDEBUG|D_NOHEADER, const_cast<ClassAd&>(ad));
    //}
}

int HadoopObject::start( tHadoopInit & hInit )
{ 
    int cluster, proc;
    
    dprintf( D_FULLDEBUG, "Called HadoopObject::start count:%d owner:(%s) unmanaged(%s) tarball:%s \n",  
             hInit.count, 
             hInit.owner.c_str(),
             hInit.unmanaged?"TRUE":"FALSE",
             hInit.idref.tarball.size()?hInit.idref.tarball.c_str():"EMPTY"
           );
         
    // check input tarball.
    if ( 0 == hInit.idref.tarball.size() && !hInit.unmanaged )
    {
        if (hInit.unmanaged)
        {
            hInit.idref.tarball = "EMPTY";
        }
        else
        {
            char * binball = param("HADOOP_BIN_TARBALL");
            if (!binball )
            {
                m_lasterror = "No hadoop tarball specified.";
                return false;
            }
            else
            {
                hInit.idref.tarball = binball;
                delete binball;
            }
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
    MyString IPCAddress, HTTPAddress="", ManagedState;
    bool hasInputScript=false, bValidId=false;
    string Iwd="/tmp";
    int iStatus=1;
    PROC_ID id = getProcByString( hInit.idref.id.c_str() );
    char * requirements = 0;
    
    if (id.cluster > 0 && id.proc >= 0) 
    {
        bValidId = true;

        ::GetAttributeInt( id.cluster, id.proc, ATTR_JOB_STATUS, &iStatus);
        ::GetAttributeString( id.cluster, id.proc,  "GridoopManaged", ManagedState);

        dprintf(D_FULLDEBUG, "Valid ClusterId Ref: %s status: %d\n", hInit.idref.id.c_str(), iStatus);
    }
    
    args = hInit.idref.tarball;
    bool wantsHadoopRequest = false;
    
    switch (hInit.idref.type)
    {
        case NAME_NODE:
        hadoopType = ATTR_NAME_NODE;
        
        hasInputScript = param(inputscript, "HADOOP_HDFS_NAMENODE_SCRIPT");
        ::SetAttribute(cluster, proc, ATTR_RANK, "memory");
        ::SetAttribute(cluster, proc, ATTR_REQUEST_MEMORY, "floor(.50 * Target.Memory)");  // TODO: --> Target.Memory
        
        requirements = param(HADOOP_NAMENODE_REQUIREMENTS);
        if(requirements)
        {
            ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, requirements);
            delete requirements;
        }
        else
        {
            ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, "( HasJava =?= TRUE ) && ( TARGET.OpSys == \"LINUX\" ) && ( TARGET.Memory >= RequestMemory ) && ( TARGET.HasFileTransfer )");
        }
        
        // "spindle" configuration
        wantsHadoopRequest = param_boolean("HADOOP_ENABLE_REQUEST_NAMENODE",false);
        if (wantsHadoopRequest) {
            ::SetAttributeInt(cluster, proc, ATTR_HADOOP_REQUEST_NAMENODE, 1);
        }

        break;
        case JOB_TRACKER:
        hadoopType = ATTR_JOB_TRACKER;
        hasInputScript = param(inputscript, "HADOOP_MAPR_JOBTRACKER_SCRIPT");
        ::SetAttribute(cluster, proc, ATTR_RANK, "memory");
        
        requirements = param(HADOOP_JOBTRACKER_REQUIREMENTS);
        if(requirements)
        {
            ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, requirements);
            delete requirements;
        }
        else
        {
            ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, "( HasJava =?= TRUE ) && ( TARGET.OpSys == \"LINUX\" ) && ( TARGET.HasFileTransfer )");
        }
        
        // "spindle" configuration
        wantsHadoopRequest = param_boolean("HADOOP_ENABLE_REQUEST_JOBTRACKER",false);
        if (wantsHadoopRequest) {
            ::SetAttributeInt(cluster, proc, ATTR_HADOOP_REQUEST_JOBTRACKER, 1);
        }

        // fall through
        case DATA_NODE:
        // special case case only a small part the rest is common.
        if (hInit.idref.type == DATA_NODE)
        {
            hadoopType = ATTR_DATA_NODE; 
            hasInputScript = param(inputscript, "HADOOP_HDFS_DATANODE_SCRIPT");
            ::SetAttribute(cluster, proc, ATTR_RANK, "disk");

            requirements = param(HADOOP_DATANODE_REQUIREMENTS);
            if(requirements)
            {
                ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, requirements);
                delete requirements;
            }
            else
            {
                ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, "( HasJava =?= TRUE ) && ( TARGET.OpSys == \"LINUX\" ) && ( TARGET.HasFileTransfer )");
            }
        }
        
        ///////////////////////////////////////////////////////////////////////////////////////
        // NOTE: This section is common to both JOB_TRACKER && DATA_NODES
        // It could possibly be refactored into a function but it's not really pretty/clean.
        if (bValidId && (iStatus == RUNNING || ManagedState == "UNMANAGED" ))
        {
            ::GetAttributeString( id.cluster, id.proc,  "IPCAddress", IPCAddress);
            ::GetAttributeString( id.cluster, id.proc,  "HTTPAddress", HTTPAddress);
            
            //TODO: there could be extra checks here.
            ::SetAttribute(cluster, proc, "NameNode", quote_it(hInit.idref.id.c_str()).c_str() );
        }
        else if (hInit.idref.ipcid.length())
        {
            // TODO: there could be extra checks here, validate it's up. 
            IPCAddress = hInit.idref.ipcid.c_str();
            ::SetAttribute(cluster, proc, "NameNode", "0" );
        }
        else
        {
            AbortTransaction();
            aviUtilFmt ( m_lasterror, "Name Node %s Invalid or not running status %d", hInit.idref.id.c_str(), iStatus );
            return false;
        }
            
        ::SetAttribute(cluster, proc, "NameNodeHTTPAddress", quote_it(HTTPAddress.Value()).c_str() );
        
        args += " ";
        args += IPCAddress.Value();
        ::SetAttribute(cluster, proc, "NameNodeIPCAddress", quote_it(IPCAddress.Value()).c_str());
        ///////////////////////////////////////////////////////////////////////////////////////
        
        // "spindle" configuration
        wantsHadoopRequest = param_boolean("HADOOP_ENABLE_REQUEST_DATANODE",false);
        if (wantsHadoopRequest) {
            ::SetAttributeInt(cluster, proc, ATTR_HADOOP_REQUEST_DATANODE, 1);
        }

        break;
        
        case TASK_TRACKER:
        
        hadoopType = ATTR_TASK_TRACKER;
        hasInputScript = param(inputscript, "HADOOP_MAPR_TASKTRACKER_SCRIPT");
        ::SetAttribute(cluster, proc, ATTR_RANK, "Mips");
        
        requirements = param(HADOOP_TASKTRACKER_REQUIREMENTS);
        if(requirements)
        {
            ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, requirements);
            delete requirements;
        }
        else
        {
            ::SetAttribute(cluster, proc, ATTR_REQUIREMENTS, "( HasJava =?= TRUE ) && ( TARGET.OpSys == \"LINUX\" ) && ( TARGET.HasFileTransfer )");
        }
        
        if (bValidId && iStatus == RUNNING)
        {   
            ::GetAttributeString( id.cluster, id.proc,  "IPCAddress", IPCAddress);
            ::GetAttributeString( id.cluster, id.proc,  "HTTPAddress", HTTPAddress);
            
            //TODO: there could be extra checks here.
            ::SetAttribute(cluster, proc, "JobTracker", quote_it(hInit.idref.id.c_str()).c_str() );
        }
        else if (hInit.idref.ipcid.length())
        {
            // TODO: there could be extra checks here, validate it's up. 
            IPCAddress = hInit.idref.ipcid.c_str();
            ::SetAttribute(cluster, proc, "JobTracker", "0" );
        }
        else
        {
            AbortTransaction();
            m_lasterror = "No valid Job Tracker ";
            aviUtilFmt ( m_lasterror, "ID %s Invalid or not running status %d", hInit.idref.id.c_str(), iStatus );
            return false;
        }
            
        ::SetAttribute(cluster, proc, "JobTrackerHTTPAddress", quote_it(HTTPAddress.Value()).c_str() );
        
        args += " ";
        args += IPCAddress.Value();
        ::SetAttribute(cluster, proc, "JobTrackerIPCAddress", quote_it(IPCAddress.Value()).c_str());
        
        // "spindle" configuration
        wantsHadoopRequest = param_boolean("HADOOP_ENABLE_REQUEST_TASKTRACKER",false);
        if (wantsHadoopRequest) {
            ::SetAttributeInt(cluster, proc, ATTR_HADOOP_REQUEST_TASKTRACKER, 1);
        }

        break;
    }

    // verify that there is an input script, without it you won't get far.
    if (!hasInputScript)
    {
        AbortTransaction();
        aviUtilFmt(m_lasterror, "Missing Script Input KNOB for type %s", hadoopType.c_str() );
        return false;
    }

    // Set the owner attribute
    ::SetAttribute(cluster, proc, ATTR_OWNER, quote_it(hInit.owner.c_str()).c_str());
    
    
    if (hInit.unmanaged)
    {
        ::SetAttribute(cluster, proc, "GridoopManaged", quote_it("UNMANAGED").c_str());
        ::SetAttribute(cluster, proc, "IPCAddress", quote_it(hInit.idref.ipcid.c_str()).c_str());
        ::SetAttribute(cluster, proc, "HTTPAddress", quote_it(hInit.idref.http.c_str()).c_str());
        
        // EARLY SET: These attribute are set early so the incoming ad
        // has a change to override them.
        ::SetAttribute(cluster, proc, ATTR_JOB_STATUS, "5"); // 1 = held
        
    }
    else
    {
        ::SetAttribute(cluster, proc, "GridoopManaged", quote_it("MANAGED").c_str());
        ::SetAttribute(cluster, proc, ATTR_TRANSFER_INPUT_FILES, quote_it(hInit.idref.tarball.c_str()).c_str());
        ::SetAttribute(cluster, proc, ATTR_HADOOP_BIN_VERSION, quote_it(hInit.idref.tarball.c_str()).c_str());
        
        // EARLY SET: These attribute are set early so the incoming ad
        // has a change to override them.
        ::SetAttribute(cluster, proc, ATTR_JOB_STATUS, "1"); // 1 = idle
        
    }
    
    if ( !hInit.description.length() )
    {
        hInit.description="N/A";
    }
    
    ::SetAttribute(cluster, proc, ATTR_HADOOP_DESCRIPTION, quote_it(hInit.description.c_str()).c_str());
    
    param(Iwd, "HADOOP_IWD", "/tmp");
    ::SetAttribute(cluster, proc, ATTR_JOB_IWD, quote_it(Iwd.c_str()).c_str() );
    
    ::SetAttribute(cluster, proc, ATTR_JOB_CMD, quote_it(inputscript.c_str()).c_str());
    ::SetAttribute(cluster, proc, ATTR_JOB_ARGUMENTS1, quote_it(args.c_str()).c_str());
    ::SetAttribute(cluster, proc, ATTR_HADOOP_TYPE, quote_it(hadoopType.c_str()).c_str() );
    ::SetAttribute(cluster, proc, ATTR_SHOULD_TRANSFER_FILES, quote_it("YES").c_str());
    ::SetAttribute(cluster, proc, ATTR_WANT_IO_PROXY, "true");
    
    // TODO - Handle input & output files (kind of debugging only)
    ::SetAttribute(cluster, proc, ATTR_WHEN_TO_TRANSFER_OUTPUT, quote_it("ON_EXIT").c_str());
    
    ::SetAttribute(cluster, proc, ATTR_KILL_SIG, quote_it("SIGTERM").c_str());
    
    
    // Junk that condor_q wants, but really shouldn't be necessary
    ::SetAttribute(cluster, proc, ATTR_JOB_REMOTE_USER_CPU, "0.0"); // float
    ::SetAttribute(cluster, proc, ATTR_JOB_PRIO, "0");              // int
    ::SetAttribute(cluster, proc, ATTR_IMAGE_SIZE, "0");            // int  
    ::SetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA );    
    
    // more stuff - without these our idle stats are whack
    ::SetAttribute(cluster, proc, ATTR_MAX_HOSTS, "1");
    ::SetAttribute(cluster, proc, ATTR_MIN_HOSTS, "1");

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
    aviUtilFmt(hInit.newcluster,"%d",cluster);
    
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


bool HadoopObject::status (ClassAd* cAd, const tHadoopType & type, tHadoopJobStatus & hStatus)
{
    
    int cluster=0, proc=0, JobStatus=0, EnteredStatus=0;

    // so the following checks are pretty excessive and could likely be removed
    if (!cAd->LookupString( ATTR_OWNER, hStatus.owner))
    {
    m_lasterror = "Could not find Owner";
    return false;
    }
    
    if (!cAd->LookupInteger(ATTR_CLUSTER_ID, cluster))
    {
    m_lasterror = "Could not find cluster id";
    return false;
    }
    
    if (!cAd->LookupInteger(ATTR_PROC_ID, proc))
    {
    m_lasterror = "Could not find proc id";
    return false;
    }
    
    if (!cAd->LookupInteger(ATTR_JOB_STATUS, JobStatus))
    {
    m_lasterror = "Could not find job status";
    return false;
    }
    
    if (!cAd->LookupString( ATTR_HADOOP_BIN_VERSION, hStatus.idref.tarball))
    {
        hStatus.idref.tarball = "UNMANAGED";
    }
     
    aviUtilFmt(hStatus.idref.id,"%d.%d", cluster, proc);
    
    if (!cAd->LookupString( ATTR_HADOOP_DESCRIPTION, hStatus.description))
    {
        hStatus.description = "N/A";
    }
   
    cAd->LookupInteger( ATTR_Q_DATE, hStatus.qdate );
    
    if (!cAd->LookupString( ATTR_HTTP_ADDRESS, hStatus.http))
    {
        hStatus.http="N/A";
    }
 
    hStatus.uptime = 0;
    
    // 1st check to see if we are unmanaged.
    cAd->LookupString( "GridoopManaged", hStatus.state);
    
    // if we are not unmanaged then fill in the state
    if ( strcmp("UNMANAGED", hStatus.state.c_str()) )
    {
        dprintf( D_ALWAYS, "ANything but 0 on comparison\n");
        
            switch (JobStatus)
            {
                case 1:
                    hStatus.state = "PENDING";
                    break;
                case 2:
                    hStatus.state = "RUNNING";
                    
                    if ( cAd->LookupInteger(ATTR_ENTERED_CURRENT_STATUS, EnteredStatus) )
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
        
    }  

    if (!cAd->LookupString( "IPCAddress", hStatus.idref.ipcid))
    {
       hStatus.idref.ipcid="N/A";
    }
    
    if (!cAd->LookupString( "HTTPAddress", hStatus.idref.http))
    {
        hStatus.idref.http="N/A";
    }
    
    // default the parent data
    hStatus.idparent.ipcid="N/A";
    hStatus.idparent.id="N/A";
    hStatus.idparent.http="N/A";
    
    switch (type)
    {
        case DATA_NODE:
        case JOB_TRACKER:
            cAd->LookupString("NameNodeIPCAddress", hStatus.idparent.ipcid);
            cAd->LookupString("NameNode", hStatus.idparent.id);
            break;
        case TASK_TRACKER:
            cAd->LookupString("JobTrackerIPCAddress", hStatus.idparent.ipcid);
            cAd->LookupString("JobTracker", hStatus.idparent.id);
           break;
        default:
          break;
    }

    
    dprintf( D_ALWAYS, "Called HadoopObject::status() STATUS:%s, ID:%d.%d OWNER:%s PARENT:(%s,%s) DESCRIPTION:%s\n", 
             hStatus.state.c_str(), 
             cluster, proc, 
             hStatus.owner.c_str(),
             hStatus.idparent.id.c_str(),
             hStatus.idparent.ipcid.c_str(), 
             hStatus.description.c_str()
           );   
 
    return true;
    
}

bool HadoopObject::query (const tHadoopRef & hRef, std::vector<tHadoopJobStatus> & vhStatus)
{   
    dprintf( D_FULLDEBUG, "Called HadoopObject::query()\n");
    
    vhStatus.clear();
    ClassAd* cAd;
    string constraint;
    
    switch (hRef.type)
    {
    case NAME_NODE:
        // just list all the name nodes
        constraint = "HadoopType =?= \"NameNode\"";
        break;
    case DATA_NODE:
        // list all name nodes bound query on hRef.idref.id;
        constraint = "HadoopType =?= \"DataNode\"";
        break;
    case JOB_TRACKER:
        //just list all job trackers.
        constraint = "HadoopType =?= \"JobTracker\"";
        break;
    case TASK_TRACKER:
        // list all name nodes bound query on hRef.idref.id;
        constraint = "HadoopType =?= \"TaskTracker\"";
        break;
    }
    
    // if an id is given use, use reference id.
    if (hRef.id.length())
    {
        size_t pos = hRef.id.find(".");
        string cid,pid;
        
        if (pos != string::npos)
        {
            cid = hRef.id.substr(0,pos);
            pid = hRef.id.substr(pos+1);
        }
        else 
        {
            cid = hRef.id;
        }
        
        constraint+= " && ClusterId =?= ";
        constraint+=cid;
        
        if (pid.length())
        {
            constraint+= " && ProcId =?= ";
            constraint+=pid;
        }
            
    }
    else if (hRef.ipcid.length())
    {
        // if not given a id but given an ipc, try that. 
        constraint+= " && IPCAddress =?= ";
        constraint+= hRef.ipcid;
    }
    
    // get all adds that match the above constraint.
    if ( 0 == ( cAd = ::GetJobByConstraint(constraint.c_str()) ) )
    {
        m_lasterror = "Empty query";
        dprintf( D_FULLDEBUG, "HadoopObject::status() - FAILED Constraint query(%s)\n", constraint.c_str());
        return false;
    } 
    
    // loop through the ads
    while ( cAd )
    {
        tHadoopJobStatus hStatus;
        if ( status ( cAd, hRef.type, hStatus ) )
        {
            // last error should be set.
            vhStatus.push_back(hStatus);
        }
        else
        {
            dprintf( D_FULLDEBUG, "HadoopObject::status() - FAILED status parse\n");
            return false;
        }
    
        cAd = ::GetNextJobByConstraint( constraint.c_str(), 0 );
    }
    
    return true;
    
}


