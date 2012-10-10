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
#include "condor_debug.h"
#include "condor_attributes.h"
#include "CondorError.h"

// platform includes
#include <libgen.h> // dirname
#include "directory.h"
#include "stat_wrapper.h"

// local includes
#include "ODSProcessors.h"
#include "ODSDBNames.h"
#include "ODSUtils.h"
#include "ODSHistoryFile.h"

using namespace std;
using namespace compat_classad;
using namespace mongo;
using namespace plumage::etl;
using namespace plumage::util;

// helpers, note expected bob & p vars
#define STRING(X,Y) \
const char* X = p.getStringField(Y); \
if (strcmp(X,"")) bob.append(#X,X);

#define INTEGER(X,Y) bob.append(#X,p.getIntField(Y));
#define DOUBLE(X,Y) bob.appendAsNumber(#X,formatReal(p.getField(Y).Double()));
#define DATE(X,Y) bob.appendDate(#X,Y);

void
plumage::etl::processSubmitterStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processSubmitterStats called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Submitter" ) );
    conn->ensureIndex(DB_STATS_SAMPLES_SUB, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_SUB, BSON( "sn" << 1 ));
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to submitter samples
        BSONObjBuilder bob;
        DATE(ts,ts);
        STRING(sn,ATTR_NAME);
        STRING(ma,ATTR_MACHINE);
        INTEGER(jr,ATTR_RUNNING_JOBS);
        // TODO: weird...HeldJobs isn't always there in the raw submitter ad
        int h = p.getIntField(ATTR_HELD_JOBS); h = (h>0) ? h : 0;
        bob.append("jh",h);
        INTEGER(ji,ATTR_IDLE_JOBS);
        conn->insert(DB_STATS_SAMPLES_SUB,bob.obj());
    }
}

void
plumage::etl::processMachineStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processMachineStats() called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Machine" ) );
    conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "m" << 1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "n" << 1 ));
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to machine samples
        BSONObjBuilder bob;
        DATE(ts,ts);
        STRING(m,ATTR_MACHINE);
        STRING(n,ATTR_NAME);
        STRING(ar,ATTR_ARCH);
        STRING(os,ATTR_OPSYS);
        STRING(req,ATTR_REQUIREMENTS);
        INTEGER(ki,ATTR_KEYBOARD_IDLE);
        DOUBLE(la,ATTR_LOAD_AVG);
        STRING(st,ATTR_STATE);
        INTEGER(cpu,ATTR_CPUS);
        INTEGER(mem,ATTR_MEMORY);
        // TODO: these might be moved to another collection
//         STRING(gjid,ATTR_GLOBAL_JOB_ID);
//         STRING(ru,ATTR_REMOTE_USER);
//         STRING(ag,ATTR_ACCOUNTING_GROUP);
        conn->insert(DB_STATS_SAMPLES_MACH,bob.obj());
    }
}

void
plumage::etl::processSchedulerStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processSchedulerStats() called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Scheduler" ) );
    conn->ensureIndex(DB_STATS_SAMPLES_SCHED, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_SCHED, BSON( "n" << 1 ));
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to scheduler samples
        BSONObjBuilder bob;
        DATE(ts,ts);
        STRING(n,ATTR_NAME);
        INTEGER(mjr,ATTR_MAX_JOBS_RUNNING);
        INTEGER(nu,ATTR_NUM_USERS);
        INTEGER(tja,ATTR_TOTAL_JOB_ADS);
        INTEGER(trun,ATTR_TOTAL_RUNNING_JOBS);
        INTEGER(thj,ATTR_TOTAL_HELD_JOBS);
        INTEGER(tij,ATTR_TOTAL_IDLE_JOBS);
        INTEGER(trem,ATTR_TOTAL_REMOVED_JOBS);
        INTEGER(tsr,ATTR_TOTAL_SCHEDULER_RUNNING_JOBS);
        INTEGER(tsi,ATTR_TOTAL_SCHEDULER_IDLE_JOBS);
        INTEGER(tlr,ATTR_TOTAL_LOCAL_RUNNING_JOBS);
        INTEGER(tli,ATTR_TOTAL_LOCAL_IDLE_JOBS);
        INTEGER(tfj,ATTR_TOTAL_FLOCKED_JOBS);
        conn->insert(DB_STATS_SAMPLES_SCHED,bob.obj());
    }
}

// liberally cribbed from user_prio.cpp
void 
plumage::etl::processAccountantStats(ClassAd* ad, ODSMongodbOps* ops, Date_t& ts)
{
    // attr%d holders...sadly reverting back to MyString for convenience of sprintf
    MyString  attrName, attrPrio, attrResUsed, attrWtResUsed, attrFactor, attrBeginUsage, attrAccUsage;
    MyString  attrLastUsage, attrAcctGroup, attrIsAcctGroup;
    MyString  attrConfigQuota, attrEffectiveQuota, attrSubtreeQuota, attrSurplusPolicy;
    
    // values
    string  name, acctGroup, surplusPolicy;
    float priority, factor, wtResUsed, configQuota, effectiveQuota, subtreeQuota, accUsage = -1;
    int   resUsed, beginUsage, lastUsage;
    resUsed = beginUsage = lastUsage = 0;
    bool isAcctGroup;

    DBClientConnection* conn = ops->m_db_conn;
    conn->ensureIndex(DB_STATS_SAMPLES_ACCOUNTANT, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_ACCOUNTANT, BSON( "lu" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_ACCOUNTANT, BSON( "n" << 1 ));
    unsigned long long acct_count = conn->count(DB_STATS_SAMPLES_ACCOUNTANT);

    // eventhough the Accountant doesn't forget
    // we don't care about stale submitters (default: last 24 hours)
    int cfg_last_usage = param_integer("ODS_ACCOUNTANT_LAST_USAGE", 60*60*24);
    int minLastUsageTime = time(0)-cfg_last_usage;
    int numElem = -1;
    ad->LookupInteger( "NumSubmittors", numElem );

    for( int i=1; i<=numElem; i++) {
        priority=0;
        isAcctGroup = false;

        // skip stale records unless we have none
        attrLastUsage.sprintf("LastUsageTime%d", i );
        ad->LookupInteger  ( attrLastUsage.Value(), lastUsage );
        if (lastUsage < minLastUsageTime && acct_count > 0)
            continue;

        // parse the horrid classad
        attrName.sprintf("Name%d", i );
        attrPrio.sprintf("Priority%d", i );
        attrResUsed.sprintf("ResourcesUsed%d", i );
        attrWtResUsed.sprintf("WeightedResourcesUsed%d", i );
        attrFactor.sprintf("PriorityFactor%d", i );
        attrBeginUsage.sprintf("BeginUsageTime%d", i );
        attrAccUsage.sprintf("WeightedAccumulatedUsage%d", i );
        attrAcctGroup.sprintf("AccountingGroup%d", i);
        attrIsAcctGroup.sprintf("IsAccountingGroup%d", i);
        attrConfigQuota.sprintf("ConfigQuota%d", i);
        attrEffectiveQuota.sprintf("EffectiveQuota%d", i);
        attrSubtreeQuota.sprintf("SubtreeQuota%d", i);
        attrSurplusPolicy.sprintf("SurplusPolicy%d", i);

        ad->LookupString   ( attrName.Value(), name );
        ad->LookupFloat    ( attrPrio.Value(), priority );
        ad->LookupFloat    ( attrFactor.Value(), factor );
        ad->LookupFloat    ( attrAccUsage.Value(), accUsage );
        ad->LookupInteger  ( attrBeginUsage.Value(), beginUsage );
        ad->LookupInteger  ( attrResUsed.Value(), resUsed );
        ad->LookupBool     ( attrIsAcctGroup.Value(), isAcctGroup);
        ad->LookupFloat    ( attrConfigQuota.Value(), configQuota );
        ad->LookupFloat    ( attrEffectiveQuota.Value(), effectiveQuota );
        ad->LookupFloat    ( attrSubtreeQuota.Value(), subtreeQuota );
        ad->LookupString   ( attrSurplusPolicy.Value(), surplusPolicy );
        
        if( !ad->LookupFloat( attrWtResUsed.Value(), wtResUsed ) ) {
            wtResUsed = resUsed;
        }
        if (!ad->LookupString(attrAcctGroup.Value(), acctGroup)) {
            acctGroup = "<none>";
        }

        BSONObjBuilder bob;
        bob.appendDate("ts",ts);
        bob.append("n",name);
        bob.append("ag",acctGroup);
        bob.appendAsNumber("prio",formatReal(priority));
        bob.appendAsNumber("fac",formatReal(factor));
        bob.append("ru",resUsed);
        bob.append("wru",wtResUsed);
        // condor timestamps need massaging when going in the db
        bob.appendDate("bu",static_cast<unsigned long long>(beginUsage)*1000);
        bob.appendDate("lu",static_cast<unsigned long long>(lastUsage)*1000);
        bob.appendAsNumber("au",formatReal(accUsage));
        bob.appendAsNumber("cq",formatReal(configQuota));
        bob.appendAsNumber("eq",formatReal(effectiveQuota));
        bob.appendAsNumber("sq",formatReal(subtreeQuota));
        if (!surplusPolicy.empty()) bob.append("sp",surplusPolicy);
        
        conn->insert(DB_STATS_SAMPLES_ACCOUNTANT,bob.obj());
    }
    
}

typedef set<long unsigned int> HistoryFileListType;
static HistoryFileListType m_historyFiles;
MyString m_path;

// force a reset of history processing
void plumage::etl::initHistoryFiles() {
    m_historyFiles.clear();
    processHistoryDirectory();
    processCurrentHistory(true);
}

/**
 * Process the history directory and maintain the history file map
 *
 * Only handle rotated history files. 
 * For each one that is not in the history file map, create a
 * new HistoryFile, poll it for entries to process, and add it to the
 * map.
 */
void
plumage::etl::processHistoryDirectory()
{
    const char *file = NULL;

    Directory dir ( m_path.Value() );
    dir.Rewind();
    while ( ( file = dir.Next() ) )
    {
        // Skip all non-history files, e.g. not history.<datestamp>
        if ( strncmp ( file, "history.", 8 ) ) {
            continue;
        }

        ODSHistoryFile h_file ( ( m_path + DIR_DELIM_STRING + file ).Value() );
        CondorError errstack;
        if ( !h_file.init ( errstack ) )
        {
            dprintf ( D_ALWAYS, "%s\n", errstack.getFullText() );
            return;
        }
        errstack.clear();

        long unsigned int id;
        ASSERT ( h_file.getId ( id ) );
        HistoryFileListType::iterator entry = m_historyFiles.find ( id );
        if ( m_historyFiles.end() == entry )
        {
            h_file.poll ( errstack );
            m_historyFiles.insert ( id );
        }
    }
}

/**
 * Process the current history file.
 *
 * 1) check to see if it is properly initialized, recording id (inode)
 * 2) stat the current history file
 * 3) poll for new entries and process them
 * 4) detect rotations
 */
void
plumage::etl::processCurrentHistory(bool force_reset)
{
    static MyString currentHistoryFilename = m_path + DIR_DELIM_STRING + "history";
    static ODSHistoryFile currentHistory ( currentHistoryFilename.Value() );

    CondorError errstack;

    if (force_reset) {
       currentHistory.cleanup();
    }

    // (1)
    long unsigned int id;
    if ( !currentHistory.getId ( id ) || force_reset)
    {
        if ( !currentHistory.init ( errstack ) )
        {
            dprintf ( D_ALWAYS, "%s\n", errstack.getFullText() );
            return;
        }
        ASSERT ( currentHistory.getId ( id ) );
        m_historyFiles.insert ( id );
    }

    // (2)
    // Stat before poll to handle race of: poll + write + rotate + stat
    StatWrapper stat_wrapper;
    if ( stat_wrapper.Stat ( currentHistoryFilename ) )
    {
        dprintf ( D_ALWAYS, "Failed to stat %s: %d (%s)\n",
                  currentHistoryFilename.Value(),
                  stat_wrapper.GetErrno(), strerror ( stat_wrapper.GetErrno() ) );
        return;
    }
    const StatStructType *stat = stat_wrapper.GetBuf();
    ASSERT ( currentHistory.getId ( id ) );

    // (3)
    errstack.clear();
    currentHistory.poll ( errstack );

    // (4)
    // If different the file has rotated
    if ( id != stat->st_ino )
    {
        currentHistory = ODSHistoryFile ( currentHistoryFilename.Value() );
        if ( !currentHistory.init ( errstack ) )
        {
            dprintf ( D_ALWAYS, "%s\n", errstack.getFullText() );
            return;
        }
        ASSERT ( currentHistory.getId ( id ) );
        m_historyFiles.insert ( id );
    }
}
